#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "config.h"
#include "cache.h"

#include <postgresql/libpq-fe.h>

//str系util
char* pathcat(const char* dirpath, const char* child){
    int dirlen = 0;
    int chilen = 0;
    char* output = NULL;
    char* buf1 = NULL;
    char* buf2 = NULL;
    char* buf3 = NULL;

    if((dirpath == NULL) | (child == NULL)){ return NULL; }

    dirlen = strlen(dirpath);
    chilen = strlen(child);

    buf1 = strdup(dirpath);
    if(buf1 == NULL){ return NULL; }
    buf2 = strdup(child);
    if(buf2 == NULL){ free(buf1); return NULL; }
   
    if(buf1[dirlen - 1] == '/'){ buf1[dirlen - 1] = '\0'; }
    buf3 = buf2;
    if(buf3[0] == '/'){ buf3++; }

    output = malloc(dirlen + chilen + 3);
    if(output == NULL){
        free(buf1);
        free(buf2);
        return NULL;
    }
    bzero(output, dirlen + chilen + 3);

    strcpy(output, buf1);
    strcat(output, "/");
    strcat(output, buf3); 
    
    free(buf1);
    free(buf2);
    return output;
}

char* lastchild(const char* path){
    char* buf1 = NULL;
    char* buf2 = NULL;
    int len = 0;
    int offset = 0;

    if(path == NULL){return NULL;}

    buf1 = strdup(path);
    len = strlen(path);

    if(strcmp(path, "/") == 0){
        free(buf1);
        return NULL;
    }

    if(buf1[len - 1] == '/'){
        free(buf1);
        return NULL;
    }

    offset = len - 1;
    while(buf1[offset] != '/'){
        if(offset < 1){
            offset = -1;
            break;
        }
        offset--;
    }

    buf2 = strdup(buf1 + offset + 1);
    free(buf1);
    return buf2; 
}

CacheConfig* loadCacheConfig(char* configpath){
    char* dbname;
    FILE* file;
    int count = 0;

    file = fopen(configpath, "r");
    if(file == NULL){
        return NULL;
    }

    CacheConfig* config = malloc(sizeof(CacheConfig));
    bzero(config, sizeof(CacheConfig));
    dbname = searchOptionKey(file, "DBNAME");
    strncpy(config->dbname, dbname, strlen(dbname) + 1);
    return config;
}

//DB接続初期化
int initCacheSession(Cache* cache){
    char config[512] = {0};

    if(cache == NULL){return -1;}
    if(cache->config == NULL){return -1;}

    sprintf(config, "dbname=%s", cache->config->dbname);
    cache->session = PQconnectdb(config);
    if(cache->session == NULL){
        puts("connect db error");
        return -1;
    }
    return 0;
}

Cache* newCache(CacheConfig* config){
    int rc;
    Cache* cache;

    if(config == NULL){ return NULL;}

    cache = malloc(sizeof(Cache));
    if(cache == NULL){
        return NULL;
    }

    //init mutex
    cache->mutex = malloc(sizeof(pthread_mutex_t));
    if(cache->mutex == NULL){
        puts("cache mutex is not allocated");
        return NULL;
    }
    pthread_mutex_init(cache->mutex, NULL);

    cache->config = config;
    rc = initCacheSession(cache);
    if(rc < 0){
        return NULL;
    }

    rc = createCacheTable(cache);
    if(rc < 0){
        return NULL;
    }

    resetCache(cache->session);

    return cache;
}

/*DBのリセット*/
void resetCache(PGconn* session){
    int rc;

    if(session == NULL){ return; }
    //exec sql
    PGresult* res;
    //reset
    res = PQexec(session, "TRUNCATE Cache;");
    PQclear(res);
    res = PQexec(session, "select setval ('Cache_id_seq',1, false);");
    PQclear(res);
    res = PQexec(session, "TRUNCATE Dcache;");
    PQclear(res);
}

/*DB接続を終了*/
void closeCacheSession(Cache* cache){
    if(cache == NULL){return;}
    PQfinish(cache->session);
}

void freeCache(Cache* cache){
    if(cache == NULL){return;}
    closeCacheSession(cache);
    free(cache);
}

/*DBテーブルを作成*/
int createCacheTable(Cache* cache){
    PGresult* res;

    if(cache == NULL){return -1;}
    if(cache->session == NULL){
        printf("createCacheTable failed\n");
        return -1;
    }
    
    //sql text
    res = PQexec(cache->session, "CREATE TABLE IF NOT EXISTS Cache(path TEXT PRIMARY KEY, size INTEGER, mode INTEGER, uid INTEGER, gid INTEGER, "
                                 "blksize BIGINT, blocks BIGINT, ino BIGINT, dev BIGINT, rdev BIGINT, nlink BIGINT, mtime BIGINT, "
                                 "atime BIGINT, ctime BIGINT);");
    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        printf("createCacheTable failed\n");
        PQclear(res);
        return -1;
    };
    PQclear(res);

    //sql text
    res = PQexec(cache->session, "CREATE TABLE IF NOT EXISTS Dcache(dir TEXT , child TEXT, PRIMARY KEY (dir, child));");
    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    };
    PQclear(res);

    return 0;
}

int _registerCache(PGconn* session, Attribute* attr){
    int rc;
    PGresult* res;

    if(session == NULL){ return -1;}

    char query[1024];
    sprintf(query, "INSERT INTO Cache VALUES('%s',%ld,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld) "
                   "ON CONFLICT (path) DO UPDATE SET size = %ld, mode = %d, uid= %d, gid = %d, "
                   "blksize = %ld, blocks = %ld, ino = %ld, dev = %ld, rdev = %ld, nlink = %ld, "
                   "mtime = %ld, atime = %ld, ctime = %ld;",
            attr->path,attr->st.st_size,attr->st.st_mode,attr->st.st_uid,attr->st.st_gid,attr->st.st_blksize,attr->st.st_blocks,
            attr->st.st_ino,attr->st.st_dev,attr->st.st_rdev,attr->st.st_nlink,attr->st.st_mtime,attr->st.st_atime,attr->st.st_ctime,
            attr->st.st_size,attr->st.st_mode,attr->st.st_uid,attr->st.st_gid,attr->st.st_blksize,attr->st.st_blocks,
            attr->st.st_ino,attr->st.st_dev,attr->st.st_rdev,attr->st.st_nlink,attr->st.st_mtime,attr->st.st_atime,attr->st.st_ctime);

    res = PQexec(session, query);
    printf("query: %s\n", query);
    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        puts("insert cache error");
        return -1;
    };
    PQclear(res);
    return 0;
}

int registerCache(Cache* cache, Attribute* attr){
    int rc;
    PGresult* res;

    if(cache == NULL){return -1;}
    if(cache->session == NULL){ return -1;}

    char query[1024];
    sprintf(query, "INSERT INTO Cache VALUES('%s',%ld,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld) "
                   "ON CONFLICT (path) DO UPDATE SET size = %ld, mode = %d, uid= %d, gid = %d, "
                   "blksize = %ld, blocks = %ld, ino = %ld, dev = %ld, rdev = %ld, nlink = %ld, "
                   "mtime = %ld, atime = %ld, ctime = %ld;",
            attr->path,attr->st.st_size,attr->st.st_mode,attr->st.st_uid,attr->st.st_gid,attr->st.st_blksize,attr->st.st_blocks,
            attr->st.st_ino,attr->st.st_dev,attr->st.st_rdev,attr->st.st_nlink,attr->st.st_mtime,attr->st.st_atime,attr->st.st_ctime,
            attr->st.st_size,attr->st.st_mode,attr->st.st_uid,attr->st.st_gid,attr->st.st_blksize,attr->st.st_blocks,
            attr->st.st_ino,attr->st.st_dev,attr->st.st_rdev,attr->st.st_nlink,attr->st.st_mtime,attr->st.st_atime,attr->st.st_ctime);

    pthread_mutex_lock(cache->mutex);
    res = PQexec(cache->session, query);
    pthread_mutex_unlock(cache->mutex);
    printf("query: %s\n", query);
    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        puts("insert cache error");
        return -1;
    };
    PQclear(res);
    return 0;
}

int registerDirCache(Cache* cache, char* path, List* attrs){
    int rc;
    char query[1024];
    char* abspath = NULL;
    PGresult* res;
    Attribute* attr = NULL;
    Attribute _attr;

    if(cache == NULL){return -1;}
    if(cache->session == NULL){ return -1;}

    if(attrs == NULL){
        return -1;
    }

    for(Node* node = attrs->head; node != NULL; node = node->next){
        _attr = *(Attribute*)node->data;

        abspath = pathcat(path, _attr.path);
        if(abspath == NULL){ return -1; }

        bzero(_attr.path, 256);
        strcpy(_attr.path, abspath);

        free(abspath);

        sprintf(query, "INSERT INTO Dcache VALUES('%s','%s') ON CONFLICT (dir, child) DO NOTHING;", path, _attr.path);

        pthread_mutex_lock(cache->mutex);
        res = PQexec(cache->session, query);
        pthread_mutex_unlock(cache->mutex);
        //printf("query: %s\n", query);
        if(PQresultStatus(res) != PGRES_COMMAND_OK){
            PQclear(res);
            puts("insert cache error");
            return -1;
        };
        PQclear(res);

        rc = registerCache(cache, &_attr);
        if(rc < 0){ return -1; }
    }
    return 0;
}

Attribute* lookupCache(Cache* cache, char* path){
    int rc;
    char query[512] = {0};
    Attribute* attr = NULL;
    PGresult* res;

    if(cache == NULL){return NULL;}
    if(path == NULL){return NULL;}
    sprintf(query, "SELECT * FROM Cache WHERE path = '%s';", path);

    pthread_mutex_lock(cache->mutex);
    res = PQexec(cache->session, query);
    pthread_mutex_unlock(cache->mutex);
    printf("query: %s\n", query);
    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
        puts("lookupCache error");
        return NULL;
    }

    //問い合わせの取り出し
    if(PQntuples(res) < 1){
        PQclear(res);
        puts("lookupCache attr not found");
        return NULL;
    }

    puts("query has some result");

    attr = malloc(sizeof(Attribute));
    if(attr == NULL){
        PQclear(res);
        return NULL;
    }
    bzero(attr, sizeof(Attribute));
    
    strncpy(attr->path, path, strlen(path) + 1);
    attr->st.st_size = atol(PQgetvalue(res, 0, 1));
    attr->st.st_mode = atoi(PQgetvalue(res, 0, 2));
    attr->st.st_uid = atoi(PQgetvalue(res, 0, 3));
    attr->st.st_gid = atoi(PQgetvalue(res, 0, 4));
    attr->st.st_blksize = atol(PQgetvalue(res, 0, 5));
    attr->st.st_blocks = atol(PQgetvalue(res, 0, 6));
    attr->st.st_ino = atol(PQgetvalue(res, 0, 7));
    attr->st.st_dev = atol(PQgetvalue(res, 0, 8));
    attr->st.st_rdev = atol(PQgetvalue(res, 0, 9));
    attr->st.st_nlink = atol(PQgetvalue(res, 0, 10));
    attr->st.st_mtime = atol(PQgetvalue(res, 0, 11));
    attr->st.st_atime = atol(PQgetvalue(res, 0, 12));
    attr->st.st_ctime = atol(PQgetvalue(res, 0, 13));

    PQclear(res);

    printAttr(attr);

    return attr;
}

List* lookupDirCache(Cache* cache, char* path){
    int rc = 0;
    int nrow = 0;
    char query[512];
    char* child = NULL;
    char* childname = NULL;
    List* list = NULL;
    PGresult* res = NULL;

    sprintf(query, "SELECT * FROM Dcache WHERE dir = '%s';", path);

    pthread_mutex_lock(cache->mutex);
    res = PQexec(cache->session, query);
    pthread_mutex_unlock(cache->mutex);
    //printf("query: %s\n", query);
    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
        puts("lookupDirCache error");
        return NULL;
    }

    //問い合わせの取り出し
    nrow = PQntuples(res);
    if(nrow < 1){
        PQclear(res);
        puts("lookupDirCache dir not found");
        return NULL;
    }

    puts("query has some result");

    list = newList();
    if(list == NULL){
        PQclear(res);
        return NULL;
    }

    for(int i = 0; i < nrow; i++){
        Attribute* attr = NULL;
        child = PQgetvalue(res, i, 1);
        attr = lookupCache(cache, child);
        childname = lastchild(attr->path);
        if(childname == NULL){continue;}
        bzero(attr->path, 256);
        strncpy(attr->path, childname, strlen(childname) + 1);
        free(childname);
        if(attr != NULL){
            push_front(list, attr, sizeof(Attribute));
        }
    }
        
    PQclear(res);

    return list;
}








