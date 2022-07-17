#include "attr.h"
#include "list.h"

#include <postgresql/libpq-fe.h>

typedef struct{
    char dbname[256];
} CacheConfig;

typedef struct{
    PGconn* session;
    pthread_mutex_t* mutex;
    CacheConfig* config;
} Cache;

char* pathcat(const char* dirpath, const char* child);

CacheConfig* loadCacheConfig(char* configpath);
Cache* newCache(CacheConfig* config);
void freeCache(Cache* cache);
void resetCache(PGconn* session);

int createCacheTable(Cache* cache);

int _registerCache(PGconn* session, Attribute* attr);
int registerCache(Cache* cache, Attribute* attr);
int registerDirCache(Cache* cache, char* path, List* attrs);
Attribute* lookupCache(Cache* cache, char* path);
List* lookupDirCache(Cache* cache, char* path);


