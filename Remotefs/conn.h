#pragma once

#include <pthread.h>
#include "attr.h"
#include "list.h"

#define PATH_LEN 256
#define IP_LEN 64

typedef struct {
    char path[PATH_LEN];
    char host[IP_LEN];
    short port;
} ConnectConfig;

typedef struct {
    int sockfd;
    pthread_mutex_t* mutex;
    ConnectConfig* config;
} Connector;

typedef struct {
    char path[PATH_LEN];
    int fh;
} FileSession;

ConnectConfig* loadConnConfig(char* path);
void freeConnectConfig(ConnectConfig* config);
Connector* getConnector(ConnectConfig* config);
int checkConnector(Connector* connector);
int reConnector(Connector* connector);
void freeConnector(Connector* connector);

List* connReaddir(Connector* connector, const char* path);
Attribute* connStat(Connector* connector, const char* path);
FileSession* connOpen(Connector* connector, const char* path, int flag);
int connRead(Connector* connector, FileSession* file, off_t offset, void* buffer, int size);
int connWrite(Connector* connector, FileSession* file, off_t offset, void* buffer, int size);
int connClose(Connector* connector, FileSession* file);
int connStatus(Connector* connector);

