#pragma once

#include "list.h"
#include "map.h"
#include "conn.h"
#include "attr.h"

#include <sqlite3.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>

#define BLOCK_SIZE 4048 //4KB
                        //
typedef struct Mirror {
    pthread_rwlock_t* task_rwlock;
    pthread_mutex_t* list_lock;
    pthread_cond_t* list_cond;
    int killswitch;
    sqlite3* dbsession;
    char* root;
    List* tasklist; //List of MirrorTask
    pthread_t taskthread;
} Mirror;

typedef struct MirrorFile {
    char* path;
    int fd;
    int size;
    int mtime;
    int atime;
    int ref_cnt;
} MirrorFile;

//コンストラクタはcreateTask
typedef struct MirrorTask {
    MirrorFile* file;
    char* path;
    int block_num;
    int iterator;
    struct stat st;
} MirrorTask;

int initDbSession(const char *filename, sqlite3 **ppDb);

int closeDbSession(sqlite3* pDb);

int createMirrorTable(sqlite3* dbsession);

int getMirrorUsedStorage(sqlite3* dbsession);

void resetMirrorDB(sqlite3* dbsession);

MirrorTask* createTask(const char* path);

void freeMirrorTask(void* task);

Mirror* constructMirror(char* dbname, char* root, char* connfig);

void freeMirror(Mirror* mirror);

void showMirrorFile(MirrorFile* file);

void freeMirrorFile(MirrorFile* file);

Connector* getMirrorConnector(char* configpath);

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


