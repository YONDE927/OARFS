#pragma once

#include <pthread.h>
#include <sys/types.h>
#include <postgresql/libpq-fe.h>

#include "list.h"
#include "conn.h"

#define BLOCK_SIZE 4048 //4KB

typedef struct {
    char path[IP_LEN];
    char dbname[PATH_LEN];
    char mirrorroot[PATH_LEN];
    char mirrorreq[PATH_LEN];
    ConnectConfig* connectconfig;
} MirrorConfig;

typedef struct {
    pthread_rwlock_t* task_rwlock;
    pthread_mutex_t* list_lock;
    pthread_cond_t* list_cond;
    int killswitch;
    FILE* request;
    Connector* connector;
    PGconn* dbsession;
    MirrorConfig* config;
    int taskpid;
    List* tasklist; //List of MirrorTask
    pthread_t taskthread;
} Mirror;

typedef struct MirrorFile {
    char path[256];
    FILE* fp;
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

MirrorConfig* loadMirrorConfig(char* path);
void freeMirrorConfig(MirrorConfig* config);

Mirror* constructMirror(MirrorConfig* config);
void freeMirror(Mirror* mirror);

MirrorTask* createTask(Mirror* mirror, const char* path);
void freeMirrorTask(void* task);

void showMirrorFile(MirrorFile* file);
void freeMirrorFile(MirrorFile* file);

void resetMirrorDB(Mirror* mirror);

/*ミラーリングのスレッドを開始*/
int startMirroring(Mirror* mirror);

void request_mirror(Mirror* mirror, const char* path);
MirrorFile* search_mirror(Mirror* mirror, const char* path);
void check_mirror(Mirror* mirror, const char* path);

/*MirrorReqFileに要求するファイルを書き込む*/
void write_mirror_request(Mirror* mirror, const char* path);
/*MirrorFileをオープンし、ファイルディスクリプタを返す*/
int openMirrorFile(Mirror* mirror, MirrorFile* file);
/*MirrorFileのファイルディスクリプタに対してreadを発行する*/
int readMirrorFile(Mirror* mirror, MirrorFile* file, off_t offset, size_t size, char* buf);
/*MirrorFileのファイルディスクリプタに対してwriteを発行する*/
int writeMirrorFile(Mirror* mirror, MirrorFile* file, off_t offset, size_t size, const char* buf);
/*MirrorFileをクローズする*/
int closeMirrorFile(Mirror* mirror, MirrorFile* file);

void* mirrorProcess();
int mirrorProcessRun(char* configpath);

