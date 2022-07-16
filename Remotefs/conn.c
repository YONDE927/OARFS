#include <libssh/sftp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "connection.h"
#include "fileoperation.h"
#include "conn.h"
#include "config.h"

#define CHUNK_SIZE 16384

//ConfigはConnectorが後で参照する可能性があるので、解放しない。
ConnectConfig* loadConnConfig(char* path){
    char* host;
    char* port;
    FILE* file;
    ConnectConfig* config;
    int count = 0;

    file = fopen(path, "r");
    if(file == NULL){
        return NULL;
    }

    config = malloc(sizeof(ConnectConfig));
    if(config == NULL){
        fclose(file);
        return NULL;
    }
    bzero(config, sizeof(ConnectConfig));

    strncpy(config->path, path, strlen(path) + 1);

    if((host = searchOptionKey(file, "HOST")) != NULL){
        strncpy(config->host, host, strlen(host) + 1);
        free(host);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    if((port = searchOptionKey(file, "PORT")) != NULL){
        config->port = atoi(port);
        free(port);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    fclose(file);
    return config;
}

void freeConnectConfig(ConnectConfig* config){
    if(config != NULL){
        free(config);
    }
}

Connector* getConnector(ConnectConfig* config){
    Connector* connector = NULL;
   
    if(config == NULL){ return NULL;}

    connector = malloc(sizeof(Connector));
    if(connector == NULL){ return NULL; }

    connector->config = config;

    //init mutex
    connector->mutex = malloc(sizeof(pthread_mutex_t));
    if(connector->mutex == NULL){
        puts("connector mutex is not allocated");
        return NULL;
    }
    pthread_mutex_init(connector->mutex, NULL);

    connector->sockfd = getClientSock(config->host, config->port);
    if(connector->sockfd < 0){
        printf("connection cannot be established\n");
        return NULL;
    }

    if(requestHealth(connector->sockfd) == 0){
        return connector;
    }else{
        return NULL;
    }
}

int checkConnector(Connector* connector){
    int rc;
    if(connector == NULL){
        return -1;
    }

    pthread_mutex_lock(connector->mutex);
    rc = requestHealth(connector->sockfd);
    pthread_mutex_unlock(connector->mutex);

    return rc;
}

int reConnect(Connector* connector){
    int rc = 0;

    if(connector == NULL){
        return -1;
    }

    pthread_mutex_lock(connector->mutex);
    close(connector->sockfd);

    connector->sockfd = getClientSock(connector->config->host, connector->config->port);
    if(connector->sockfd < 0){
        pthread_mutex_unlock(connector->mutex);
        printf("connection cannot be established\n");
        return -1;
    }

    rc = requestHealth(connector->sockfd);
    pthread_mutex_unlock(connector->mutex);
    return rc;
}

void freeConnector(Connector* connector){
    if(connector == NULL){
        return;
    }
   
    close(connector->sockfd);
    
    if(connector->config != NULL){
        free(connector->config);
    }

    if(connector->mutex != NULL){
        pthread_mutex_destroy(connector->mutex);
    }

    free(connector);
}

/* connReaddirはpathを受け取り、AttributeのList構造体を領域確保ともにポインタを返却*/
List* connReaddir(Connector* connector, const char* path){
    int rc = 0;
    List* stats;
    Attribute* attr;

    if(connector == NULL){ return NULL;}

    rc = connStatus(connector);
    if(rc < 0){
        return NULL;
    }
    //open direcotry
    pthread_mutex_lock(connector->mutex);
    stats = requestReaddir(connector->sockfd, path); 
    pthread_mutex_unlock(connector->mutex);

    return stats;
}

/* connStatはAttributeのポインタを受け取りその領域を予約して取得した属性をコピーする。 */
Attribute* connStat(Connector* connector, const char* path){
    int rc = 0;
    Attribute* attr = NULL;
    struct stat stbuf;

    if(connector == NULL){ return NULL;}

    rc = connStatus(connector);
    if(rc < 0){
        return NULL;
    }

    pthread_mutex_lock(connector->mutex);
    rc = requestStat(connector->sockfd, path, &stbuf);
    pthread_mutex_unlock(connector->mutex);
    if(rc < 0){ return NULL;}

    attr = malloc(sizeof(Attribute));
    strncpy(attr->path, path, strlen(path) + 1);
    attr->st = stbuf;

    return attr;
}

FileSession* connOpen(Connector* connector, const char* path, int flag){
    int rc = 0;
    int path_size, fd;
    FileSession* file;

    if(connector == NULL){ return NULL;}

    rc = connStatus(connector);
    if(rc < 0){
        return NULL;
    }

    pthread_mutex_lock(connector->mutex);
    fd = requestOpen(connector->sockfd, path, flag);
    pthread_mutex_unlock(connector->mutex);
    if(fd < 0){
        return NULL;
    }

    file = malloc(sizeof(FileSession));

    strncpy(file->path, path, strlen(path) + 1);
    file->fh = fd;

    return file;
}

int connRead(Connector* connector, FileSession* file, off_t offset, void* buffer, int size){
    int read_sum = 0;
    int read_size = 0;
    int nbytes = 1;
    int rc;

    if(connector == NULL){ return -1;}

    rc = connStatus(connector);
    if(rc < 0){
        return -1;
    }

    if(file == NULL){
        printf("Remotefile* file not exist\n");
        return -1;
    }

    //読み込み
    for(; (nbytes != 0) & (size > 0) ;){
        if( size > CHUNK_SIZE ){
            read_size = CHUNK_SIZE;
        }
        else{
            read_size = size;
        }

        pthread_mutex_lock(connector->mutex);
        nbytes = requestRead(connector->sockfd, file->fh, buffer, offset, read_size);
        pthread_mutex_unlock(connector->mutex);
        if(nbytes < 0){
            puts("requestRead fail on connRead");
            return -1;
        }

        //次のreadのためにbufferのオフセットを更新して読み込み分サイズを減らす。
        buffer += nbytes;
        offset += nbytes;
        size -= nbytes;
        //総読み込みサイズの計算
        read_sum += nbytes;
    }
    return read_sum;
}

int connWrite(Connector* connector, FileSession* file, off_t offset, void* buffer, int size)
{
    /* charを予約して、sftp_writeしてコピーする。 */
    int rc = 0;
    int write_sum = 0;
    int write_size = 0;
    int nbytes = 1;
    
    if(connector == NULL){ return -1;}

    if(file == NULL){
        printf("FileSession not exist\n");
        return -1;
    }

    rc = connStatus(connector);
    if(rc < 0){
        return -1;
    }

    //書き込み
    for(; (nbytes != 0) & (size > 0) ;){
        if( size > CHUNK_SIZE ){
            write_size = CHUNK_SIZE;
        }else{
            write_size = size;
        }

        pthread_mutex_lock(connector->mutex);
        nbytes = requestWrite(connector->sockfd, file->fh, buffer, offset, write_size);
        pthread_mutex_unlock(connector->mutex);

        if(nbytes < 0){ return -1; }
        //次のreadのためにbufferのオフセットを更新して読み込み分サイズを減らす。
        buffer += nbytes;
        offset += nbytes;
        size -= nbytes;
        //総読み込みサイズの計算
        write_sum += nbytes;
    }
    return write_sum;
}

int connClose(Connector* connector, FileSession* file){
    int rc = 0;

    if(connector == NULL){ return -1;}

    if(file == NULL){
        printf("FileSession not exist\n");
        return -1;
    }

    rc = connStatus(connector);
    if(rc < 0){
        return -1;
    }

    pthread_mutex_lock(connector->mutex);
    rc = requestClose(connector->sockfd, file->fh); 
    pthread_mutex_unlock(connector->mutex);

    if(rc < 0){ return -1; }
    free(file);
    return 0;
}

int connStatus(Connector* connector){
    int rc = 0;

    if(connector == NULL){ 
        puts("Connector no exist");
        return -1;
    }
    rc = checkConnector(connector);
    if(rc < 0){
        rc = reConnect(connector);
    }
    return rc;
}
