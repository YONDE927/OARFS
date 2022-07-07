#pragma once

#include "list.h"
#include "map.h"
#include "conn.h"
#include "attr.h"

#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <postgresql/libpq-fe.h>

#define BLOCK_SIZE 4048 //4KB

typedef struct Mirror {
    pthread_rwlock_t* task_rwlock;
    pthread_mutex_t* list_lock;
    pthread_cond_t* list_cond;
    int killswitch;
    PGconn* dbsession;
    char dbname[256];
    char root[256];
    List* tasklist; //List of MirrorTask
    pthread_t taskthread;
} Mirror;

typedef struct MirrorFile {
    char path[256];
    int fd;
    int size;
    int mtime;
    int atime;
    int ref_cnt;
} MirrorFile;

//コンストラクタはcreateTask
typedef struct MirrorTask {
    MirrorFile* file;
    char path[256];
    int block_num;
    int iterator;
    struct stat st;
} MirrorTask;


MirrorTask* createTask(const char* path);

void freeMirrorTask(void* task);

Mirror* constructMirror(char* root, char* config);

void freeMirror(Mirror* mirror);

void showMirrorFile(MirrorFile* file);

void freeMirrorFile(MirrorFile* file);

Connector* getMirrorConnector(char* configpath);

void resetMirrorDB(Mirror* mirror);

/*ミラーリングのスレッドを開始*/
int startMirroring(Mirror* mirror);

void request_mirror(Mirror* mirror, const char* path);

MirrorFile* search_mirror(Mirror* mirror, const char* path);

void check_mirror(Mirror* mirror, const char* path);

/*MirrorFileをオープンし、ファイルディスクリプタを返す*/
int openMirrorFile(MirrorFile* file);

/*MirrorFileのファイルディスクリプタに対してreadを発行する*/
int readMirrorFile(MirrorFile* file, off_t offset, size_t size, char* buf);
/*MirrorFileのファイルディスクリプタに対してwriteを発行する*/
int writeMirrorFile(MirrorFile* file, off_t offset, size_t size, const char* buf);

/*MirrorFileをクローズする*/
int closeMirrorFile(MirrorFile* file);

void* mirrorProcess();

