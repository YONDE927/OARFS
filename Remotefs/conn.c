#include <libssh/sftp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "connection.h"
#include "fileoperation.h"
#include "conn.h"

#define CHUNK_SIZE 16384

#ifdef SFTP
//flock read write lock
//sessionをマルチスレッド
int loadConnOption(char* path,Authinfo* authinfo)
{
    char ch;
    FILE* file;
    int count = 0;
    file = fopen(path, "r");
    if(file == NULL)
    {
        return -1;
    }
    ch = getc(file);
    while((ch != EOF) && (ch != '\n'))
    {
        authinfo->host[count] = ch;    
        ch = getc(file);
        count ++;
    }
    authinfo->host[count] = '\0';

    ch = getc(file);
    count = 0;
    while((ch != EOF) && (ch != '\n'))
    {
        authinfo->username[count] = ch;    
        ch = getc(file);
        count ++;
    }
    authinfo->username[count] = '\0';

    ch = getc(file);
    count = 0;
    while((ch != EOF) && (ch != '\n'))
    {
        authinfo->password[count] = ch;    
        ch = getc(file);
        count ++;
    }
    authinfo->password[count] = '\0';
    return 0;
}

Connector* getConnector(char* configpath)
{
    static Connector* connector = NULL;
    static Authinfo* authinfo = NULL;
    if(connector==NULL)
    {
        connector = (Connector*)malloc(sizeof(Connector));
        authinfo = (Authinfo*)malloc(sizeof(Authinfo));

        //init mutex
        pthread_mutex_init(&(connector->mutex),NULL);
        pthread_mutex_lock(&(connector->mutex));

        if(loadConnOption(configpath, authinfo) < 0)
        {
            printf("loadConnOption failed.\n");
            free(connector);
            free(authinfo);
            connector = NULL;
            authinfo = NULL;
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }
        if(connInit(connector, authinfo) < 0)
        {
            printf("connection cannot be established\n");
            free(connector);
            free(authinfo);
            connector = NULL;
            authinfo = NULL;
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }
        pthread_mutex_unlock(&(connector->mutex));
    }
    if(ssh_is_connected(connector->m_ssh))
    {
        return connector;
    }
    else
    {
        pthread_mutex_lock(&(connector->mutex));
        if(connInit(connector, authinfo) < 0)
        {
            printf("connection cannot be established\n");
            free(connector);
            free(authinfo);
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }
        else
        {
            pthread_mutex_unlock(&(connector->mutex));
            return connector;
        }
    }
}

int connInit(Connector* connector,Authinfo* authinfo)
{
    connector->m_ssh = ssh_new();
    ssh_options_set(connector->m_ssh, SSH_OPTIONS_HOST, authinfo->host);
    if(ssh_connect(connector->m_ssh)!=SSH_OK){
        ssh_free(connector->m_ssh);
    return -1;
    }
    if(ssh_userauth_password(connector->m_ssh,authinfo->username,authinfo->password)!=SSH_AUTH_SUCCESS){
    ssh_free(connector->m_ssh);
    return -1;
    }
    connector->m_sftp = sftp_new(connector->m_ssh);
    if(sftp_init(connector->m_sftp)!=SSH_OK){
    sftp_free(connector->m_sftp);
        ssh_free(connector->m_ssh);
    return -1;
    }
    return 0;
}

/* connReaddirはpathを受け取り、AttributeのList構造体を領域確保ともにポインタを返却*/
List* connReaddir(const char* path)
{
    Connector* connector = getConnector(NULL);
    List* list = newList();
    Attribute* attr;
    sftp_attributes sfstat;
    sftp_dir dir;
    int pathlen;

    //open direcotry
    if(connector==NULL)
    {
        printf("Connector is down.\n");
        return list;
    }
    pthread_mutex_lock(&(connector->mutex));
    dir = sftp_opendir(connector->m_sftp, path);
    if(dir==NULL)
    {
        printf("error open dir.\n");
        pthread_mutex_unlock(&(connector->mutex));
        return list;
    }
    sfstat = sftp_readdir(connector->m_sftp, dir);
    while(sfstat!=NULL)
    {
        pathlen = strlen(sfstat->name) + 1;
        attr = malloc(sizeof(Attribute));
        strncpy(attr->path, sfstat->name, pathlen);
        attr->st.st_size = sfstat->size;
        attr->st.st_atime = sfstat->atime;
        attr->st.st_mtime = sfstat->mtime;
        attr->st.st_nlink = 1;
        push_front(list, &attr, sizeof(Attribute));
        sfstat = sftp_readdir(connector->m_sftp, dir);
    }
    pthread_mutex_unlock(&(connector->mutex));
    return list;
}

/* connStatはAttributeのポインタを受け取りその領域を予約して取得した属性をコピーする。 */
Attribute* connStat(const char* path)
{
    //printf("connStat %s\n",path);
    Attribute* attr = NULL;
    sftp_attributes sfstat;
    Connector* connector = getConnector(NULL);
    if(connector == NULL)
    {
        return NULL;
    }
    pthread_mutex_lock(&(connector->mutex));
    sfstat = sftp_stat(connector->m_sftp, path);
    pthread_mutex_unlock(&(connector->mutex));
    if(sfstat != NULL)
    {
        attr = newAttr(strlen(path) + 1);
        strncpy(attr->path, path, strlen(path) + 1);
        attr->st.st_size  = sfstat->size;
        attr->st.st_atime = sfstat->atime;
        attr->st.st_mtime = sfstat->mtime;
        attr->st.st_ctime = sfstat->createtime;
        if(sfstat->type==1){
            attr->st.st_mode = S_IFREG | 0755;
            attr->st.st_nlink = 1;
        }else if(sfstat->type==2){
            attr->st.st_mode = S_IFDIR | 0755;
        attr->st.st_nlink = 2;
        }
    }
    return attr;
}

FileSession* connOpen(const char* path,int flag)
{
    FileSession* rf;
    sftp_file file;
    int path_size;
    Connector* connector = getConnector(NULL);

    pthread_mutex_lock(&(connector->mutex));
    file = sftp_open(connector->m_sftp, path, O_RDWR, 0);
    pthread_mutex_unlock(&(connector->mutex));
    if(file == NULL)
    {
        printf("error %d\n",sftp_get_error(connector->m_sftp));
        return NULL;
    }

    path_size = strlen(path);
    rf = malloc(sizeof(FileSession));
    rf->path = malloc(sizeof(char)*(path_size + 1));
    strncpy(rf->path, path, path_size);
    rf->path[path_size] = '\0';
    rf->fh = file;
    return rf;
}

int connRead(FileSession* file,off_t offset, void* buffer, int size)
{
    /* charを予約して、sftp_readしてコピーする。 */
    int read_sum = 0;
    int read_size = 0;
    int nbytes = 1;
    Connector* connector = getConnector(NULL);

    if(file == NULL){
        printf("Remotefile* file not exist\n");
        return -1;
    }

    //読み込み
    pthread_mutex_lock(&(connector->mutex));
    if(sftp_seek(file->fh, offset) < 0){
        pthread_mutex_unlock(&(connector->mutex));
        return -1;
    }
    pthread_mutex_unlock(&(connector->mutex));

    for(; (nbytes != 0) & (size > 0) ;){
        if( size > CHUNK_SIZE ){
            read_size = CHUNK_SIZE;
        }
        else{
            read_size = size;
        }

        pthread_mutex_lock(&(connector->mutex));
        nbytes = sftp_read(file->fh, buffer, read_size);
        pthread_mutex_unlock(&(connector->mutex));

        if(nbytes < 0){
            return -1;
        }
        //次のreadのためにbufferのオフセットを更新して読み込み分サイズを減らす。
        buffer += nbytes;
        size -= nbytes;
        //総読み込みサイズの計算
        read_sum += nbytes;
    }
    return read_sum;
}

int connWrite(FileSession* file,off_t offset, void* buffer, int size)
{
    /* charを予約して、sftp_writeしてコピーする。 */
    int write_sum = 0;
    int write_size = 0;
    int nbytes = 1;
    Connector* connector = getConnector(NULL);
    
    if(file == NULL)
    {
        printf("Remotefile* file not exist\n");
        return -1;
    }

    //書き込み
    pthread_mutex_lock(&(connector->mutex));
    if(sftp_seek(file->fh, offset) < 0)
    {
        pthread_mutex_unlock(&(connector->mutex));
        return -1;
    }
    pthread_mutex_unlock(&(connector->mutex));

    for(; (nbytes != 0) & (size > 0) ;)
    {
        if( size > CHUNK_SIZE )
        {
            write_size = CHUNK_SIZE;
        }
        else
        {
            write_size = size;
        }

        pthread_mutex_lock(&(connector->mutex));
        nbytes = sftp_write(file->fh, buffer, write_size);
        pthread_mutex_unlock(&(connector->mutex));

        if(nbytes < 0)
        {
            return -1;
        }
        //次のreadのためにbufferのオフセットを更新して読み込み分サイズを減らす。
        buffer += nbytes;
        size -= nbytes;
        //総読み込みサイズの計算
        write_sum += nbytes;
    }
    return write_sum;
}

int connClose(FileSession* file)
{
    Connector* connector = getConnector(NULL);
    if(file == NULL)
    {
        printf("Remotefile* file not exist\n");
        return -1;
    }

    pthread_mutex_lock(&(connector->mutex));
    if(sftp_close(file->fh) == SSH_ERROR)
    {
        printf("sftp_close error\n");
        pthread_mutex_unlock(&(connector->mutex));
        return -1;
    }
    pthread_mutex_unlock(&(connector->mutex));

    free(file->path);
    free(file);
    file = NULL;
    return 0;
}

#endif

#ifdef RAW
//flock read write lock
//sessionをマルチスレッド
int loadConnOption(char* path, Authinfo* authinfo)
{
    char ch;
    char port[6];
    FILE* file;
    int count = 0;
    file = fopen(path, "r");
    if(file == NULL)
    {
        return -1;
    }
    ch = getc(file);
    while((ch != EOF) && (ch != '\n'))
    {
        authinfo->host[count] = ch;    
        ch = getc(file);
        count ++;
    }
    authinfo->host[count] = '\0';

    ch = getc(file);
    count = 0;
    while((ch != EOF) && (ch != '\n'))
    {
        port[count] = ch;    
        ch = getc(file);
        count ++;
    }
    port[count] = '\0';

    authinfo->port = atoi(port);

    return 0;
}

Connector* getConnector(char* configpath)
{
    static Connector* connector = NULL;
    static Authinfo* authinfo = NULL;
    if(connector==NULL)
    {
        connector = (Connector*)malloc(sizeof(Connector));
        authinfo = (Authinfo*)malloc(sizeof(Authinfo));

        //init mutex
        pthread_mutex_init(&(connector->mutex),NULL);
        pthread_mutex_lock(&(connector->mutex));

        if(loadConnOption(configpath, authinfo) < 0)
        {
            printf("loadConnOption failed.\n");
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }
        if(connInit(connector, authinfo) < 0)
        {
            printf("connection cannot be established\n");
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }
        pthread_mutex_unlock(&(connector->mutex));
    }
    if(requestHealth(connector->sockfd) == 0){
        return connector;
    }else{
        pthread_mutex_lock(&(connector->mutex));
        if(connInit(connector, authinfo) < 0){
            printf("connection cannot be established\n");
            pthread_mutex_unlock(&(connector->mutex));
            return NULL;
        }else{
            pthread_mutex_unlock(&(connector->mutex));
            return connector;
        }
    }
}

int connInit(Connector* connector,Authinfo* authinfo)
{
    connector->sockfd = getClientSock(authinfo->host, authinfo->port);
    if(connector->sockfd < 0){
        return -1;
    }

    return 0;
}

/* connReaddirはpathを受け取り、AttributeのList構造体を領域確保ともにポインタを返却*/
List* connReaddir(const char* path)
{
    Connector* connector = getConnector(NULL);
    List* stats;
    Attribute* attr;
    int pathlen;

    //open direcotry
    if(connector == NULL){ return NULL;}
    pthread_mutex_lock(&(connector->mutex));
    stats = requestReaddir(connector->sockfd, path); 
    pthread_mutex_unlock(&(connector->mutex));
    return stats;
}

/* connStatはAttributeのポインタを受け取りその領域を予約して取得した属性をコピーする。 */
Attribute* connStat(const char* path)
{
    int rc;
    //printf("connStat %s\n",path);
    Attribute* attr = NULL;
    struct stat stbuf;
    Connector* connector = getConnector(NULL);
    if(connector == NULL){ return NULL;}
    pthread_mutex_lock(&(connector->mutex));
    rc = requestStat(connector->sockfd, path, &stbuf);
    pthread_mutex_unlock(&(connector->mutex));
    if(rc < 0){ return NULL;}
    attr = malloc(sizeof(Attribute));
    strncpy(attr->path, path, strlen(path) + 1);
    attr->st = stbuf;

    return attr;
}

FileSession* connOpen(const char* path,int flag)
{
    FileSession* file;
    int path_size, fd;
    Connector* connector = getConnector(NULL);

    pthread_mutex_lock(&(connector->mutex));
    fd = requestOpen(connector->sockfd, path, flag);
    pthread_mutex_unlock(&(connector->mutex));
    if(fd < 0){
        return NULL;
    }

    file = malloc(sizeof(FileSession));
    bzero(file, sizeof(FileSession));

    strcpy(file->path, path);
    file->fh = fd;

    return file;
}

int connRead(FileSession* file, off_t offset, void* buffer, int size)
{
    /* charを予約して、sftp_readしてコピーする。 */
    int read_sum = 0;
    int read_size = 0;
    int nbytes = 1;
    int rc;
    Connector* connector = getConnector(NULL);

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

        pthread_mutex_lock(&(connector->mutex));
        nbytes = requestRead(connector->sockfd, file->fh, buffer, offset, read_size);
        pthread_mutex_unlock(&(connector->mutex));

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

int connWrite(FileSession* file, off_t offset, void* buffer, int size)
{
    /* charを予約して、sftp_writeしてコピーする。 */
    int write_sum = 0;
    int write_size = 0;
    int nbytes = 1;
    Connector* connector = getConnector(NULL);
    
    if(file == NULL)
    {
        printf("Remotefile* file not exist\n");
        return -1;
    }

    //書き込み
    for(; (nbytes != 0) & (size > 0) ;){
        if( size > CHUNK_SIZE ){
            write_size = CHUNK_SIZE;
        }else{
            write_size = size;
        }

        pthread_mutex_lock(&(connector->mutex));
        nbytes = requestWrite(connector->sockfd, file->fh, buffer, offset, write_size);
        pthread_mutex_unlock(&(connector->mutex));

        if(nbytes < 0){
            return -1;
        }
        //次のreadのためにbufferのオフセットを更新して読み込み分サイズを減らす。
        buffer += nbytes;
        offset += nbytes;
        size -= nbytes;
        //総読み込みサイズの計算
        write_sum += nbytes;
    }
    return write_sum;
}

int connClose(FileSession* file)
{
    Connector* connector = getConnector(NULL);
    int rc;

    if(file == NULL)
    {
        printf("Remotefile* file not exist\n");
        return -1;
    }

    pthread_mutex_lock(&(connector->mutex));
    rc = requestClose(connector->sockfd, file->fh); 
    pthread_mutex_unlock(&(connector->mutex));

    if(rc < 0){ return -1; }
    free(file);
    return 0;
}

#endif
