#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

#include "conn.h"
#include "attr.h"
#include "cache.h"
#include "config.h"
#include "fileoperation.h"
#include "mirror.h"

#define BLOCK_SIZE 4048 //4KB

int initDbSession(Mirror* mirror);
int closeDbSession(Mirror* mirror);
int createMirrorTable(Mirror* mirror);
int getMirrorUsedStorage(Mirror* mirror);

//ConfigはMirrorが後で参照する可能性があるので、解放しない。
MirrorConfig* loadMirrorConfig(char* path){
    char* host, *port, *dbname, *root, *mirrorroot, *mirrorreq;
    FILE* file;
    MirrorConfig* config;
    int count = 0;

    file = fopen(path, "r");
    if(file == NULL){ return NULL; }

    config = malloc(sizeof(MirrorConfig));
    if(config == NULL){
        fclose(file);
        return NULL;
    }
    bzero(config, sizeof(MirrorConfig));

    config->connectconfig = loadConnConfig(path);
    if(config->connectconfig == NULL){ 
        fclose(file);
        free(config);
    }

    strncpy(config->path, path, strlen(path) + 1);

    if((dbname = searchOptionKey(file, "DBNAME")) != NULL){
        strncpy(config->dbname, dbname, strlen(dbname) + 1);
        free(dbname);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    if((mirrorroot = searchOptionKey(file, "MIRRORROOT")) != NULL){
        strncpy(config->mirrorroot, mirrorroot, strlen(mirrorroot) + 1);
        free(mirrorroot);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    if((mirrorreq = searchOptionKey(file, "MIRRORREQ")) != NULL){
        strncpy(config->mirrorreq, mirrorreq, strlen(mirrorreq) + 1);
        free(mirrorreq);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    fclose(file);
    return config;
}

void freeMirrorConfig(MirrorConfig* config){
    if(config != NULL){
        freeConnectConfig(config->connectconfig);
        free(config);
    }
}

void mirror_signal_handler(int signum){
    killpg(0, SIGKILL);
    exit(0);
}

Mirror* constructMirror(MirrorConfig* config){
    int rc;
    Mirror* mirror;

    //子プロセス終了の予約
    /* シグナルハンドラの登録 */
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = mirror_signal_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    if(config == NULL){ return NULL; }

    mirror = malloc(sizeof(Mirror));
    if(mirror == NULL){ return NULL; }
    bzero(mirror, sizeof(Mirror));

    mirror->config = config;

    rc = initDbSession(mirror);
    if(rc < 0){
        return NULL; 
    }

    rc = createMirrorTable(mirror);
    if(rc < 0){
        return NULL; 
    }

    mirror->connector = getConnector(config->connectconfig);
    if(mirror->connector == NULL){
        return NULL;
    }

    mirror->killswitch = 0;
    mirror->taskpid = 0;

    mirror->task_rwlock = malloc(sizeof(pthread_rwlock_t));
    rc = pthread_rwlock_init(mirror->task_rwlock, NULL);
    if(rc < 0){
        return NULL;
    }

    mirror->list_lock = malloc(sizeof(pthread_mutex_t));
    rc = pthread_mutex_init(mirror->list_lock, NULL);
    if(rc < 0){
        return NULL;
    }

    mirror->list_cond = malloc(sizeof(pthread_cond_t));
    rc = pthread_cond_init(mirror->list_cond, NULL);
    if(rc < 0){
        return NULL;
    }

    mirror->tasklist = newList();

    return mirror;
}

void freeMirror(Mirror* mirror){
    mirror->killswitch = 1;
    pthread_cond_signal(mirror->list_cond);
    pthread_join(mirror->taskthread, NULL);
    pthread_rwlock_destroy(mirror->task_rwlock);
    pthread_mutex_destroy(mirror->list_lock);
    pthread_cond_destroy(mirror->list_cond);
    closeDbSession(mirror);
    freeList(mirror->tasklist, freeMirrorTask);
    freeMirrorConfig(mirror->config);
}

MirrorFile* constructMirrorFile(const char* path, struct stat st){
    MirrorFile* file;
    
    file = malloc(sizeof(MirrorFile));
    strncpy(file->path, path, strlen(path) + 1);
    file->mtime = st.st_mtime;
    file->atime = st.st_atime;
    file->size = st.st_size;
    file->ref_cnt = 0;
    return file;
}

void showMirrorFile(MirrorFile* file){
    if(file != NULL){
        printf("{path = %s, size = %d, mtime = %d, atime = %d, ref_cnt = %d}\n",
               file->path, file->size, file->mtime, file->atime, file->ref_cnt);
    }
}

void freeMirrorFile(MirrorFile* file){
    if(file != NULL){
        free(file);
    }
}

/******************************/
/*共通のコンストラクタここまで*/
/******************************/

/************/
/*文字列処理*/
/************/

/*"/"を"%"に変える*/
char* convertFileName(const char* path){
    char* name;
    int size;

    name = strdup(path);
    size = strlen(name);
    for(int i = 0; i < size; i++){
        if(name[i] == '/'){
            name[i] = '%';
        }
    }
    return name;
}

/*実際のミラーファイルのパスを生成*/
char* getMirrorPath(Mirror* mirror, const char* path){
    int root_size, path_size;
    char* mirrorpath, *convertpath;

    convertpath = convertFileName(path);

    root_size = strlen(mirror->config->mirrorroot);
    path_size = strlen(convertpath);

    mirrorpath = malloc(root_size + path_size + 2);
    strncpy(mirrorpath, mirror->config->mirrorroot, root_size + 1);
    strcat(mirrorpath, "/");
    strcat(mirrorpath, convertpath);

    free(convertpath);

    return mirrorpath;
} 

/********************/
/*文字列処理ここまで*/
/********************/

/**********************/
/* DBに関するコード群 */
/**********************/
/*DBセッションを開始*/
int initDbSession(Mirror* mirror){
    char config[512] = {0};
    //const char** dbkey;
    //const char** dbval;

    //if(mirror == NULL){return -1;}
    //dbkey = malloc(1 * sizeof(char*));
    //dbval = malloc(1 * sizeof(char*));
    //dbkey[0] = "dbname"; dbval[0] = mirror->config->dbname; 
    //mirror->dbsession = PQconnectdbParams(dbkey, dbval, 1);

    sprintf(config, "dbname=%s", mirror->config->dbname);
    mirror->dbsession = PQconnectdb(config);
    if(mirror->dbsession == NULL){
        puts("connect db error");
        exit(0);
    }
    return 0;
}

/*DBセッションを終了*/
int closeDbSession(Mirror* mirror){
    if(mirror == NULL){return -1;}
    PQfinish(mirror->dbsession);
    return 0;
}

/*ミラー用のDBテーブルを作成*/
int createMirrorTable(Mirror* mirror){
    int rc;

    if(mirror == NULL){return -1;}

    if(mirror->dbsession == NULL){
        printf("createMirrorTable failed\n");
        return -1;
    }

    PGresult* res;
    
    //sql text
    res = PQexec(mirror->dbsession, "CREATE TABLE IF NOT EXISTS Mirror(path TEXT PRIMARY KEY, size INTEGER, mtime INTEGER, atime INTEGER, ref_cnt INTEGER);");

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    };

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    };
    PQclear(res);
    
    return 0;
}

/*ミラーファイルの挿入*/
int insertMirrorFileToDB(Mirror* mirror, MirrorFile* file){
    PGresult* res;

    if(mirror == NULL){return -1;}
    //sql text
    //int paramlength[] = {0, sizeof(int), sizeof(int), sizeof(int), sizeof(int)};
    //int paramformat[] = {0, 1, 1, 1, 1};
    //int size, mtime, atime, ref_cnt;

    //size = htonl(file->size);
    //mtime = htonl(file->mtime);
    //atime = htonl(file->atime);
    //ref_cnt = htonl(file->ref_cnt);

    //const char* vals[] = { file->path, (char*)&size, (char*)&mtime, (char*)&atime, (char*)&ref_cnt};

    //res = PQexecParams(mirror->dbsession,
    //                   "REPLACE INTO Mirror VALUES ($1::text, $2::integer, $3::integer, $4::integer, $5::integer);",
    //                   5,           /* パラメータは1つ。 */
    //                   NULL,        /* バックエンドにパラメータの型を推測させる。 */
    //                   vals,
    //                   paramlength,        /* テキストのため、パラメータ長は不要。 */
    //                   paramformat,        /* デフォルトで全てのパラメータはテキスト。 */
    //                   0);          /* バイナリ結果を要求。 */

    char query[512];
    sprintf(query, "INSERT INTO Mirror VALUES ('%s', %d, %d, %d, %d) ON CONFLICT (path) DO UPDATE SET size = %d, mtime = %d, atime = %d, ref_cnt = %d;",
            file->path, file->size, file->mtime, file->atime, file->ref_cnt, file->size, file->mtime, file->atime, file->ref_cnt);
    printf("query: %s\n", query);
    res = PQexec(mirror->dbsession, query);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        printf("%s\n", PQresultErrorMessage(res));
        PQclear(res);
        puts("insertMirrorFileToDB error");
        return -1;
    };
    PQclear(res);
    return 0;
}

/*ミラーファイルの検索*/
MirrorFile* lookupMirrorFileFromDB(Mirror* mirror, const char* path){
    int rc;
    MirrorFile* file;

    //sql text
    PGresult* res;

    if(mirror == NULL){return NULL;}
    //sql text
    int paramlength[] = {0};
    int paramformat[] = {0};
    const char* vals[] = {path};

    res = PQexecParams(mirror->dbsession,
                       "SELECT * FROM Mirror where path = $1", 
                       1,           /* パラメータは1つ。 */
                       NULL,        /* バックエンドにパラメータの型を推測させる。 */
                       vals,
                       paramlength,        /* テキストのため、パラメータ長は不要。 */
                       paramformat,        /* デフォルトで全てのパラメータはテキスト。 */
                       0);          /* バイナリ結果を要求。 */

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
        puts("lookupMirrorFileFromDB error");
        return NULL;
    }

    //問い合わせの取り出し
    if(PQntuples(res) < 1){
        PQclear(res);
        puts("lookupMirrorFileFromDB file not found");
        return NULL;
    }
    
    file = malloc(sizeof(MirrorFile)); 
    if(file == NULL){ 
        PQclear(res);
        return NULL;
    }
    strncpy(file->path, path, strlen(path) + 1);
    file->size = atoi(PQgetvalue(res, 0, 1));
    file->mtime = atoi(PQgetvalue(res, 0, 2));
    file->atime = atoi(PQgetvalue(res, 0, 3));
    file->ref_cnt = atoi(PQgetvalue(res, 0, 4));
    file->fp = NULL;

    PQclear(res);
    return file;
}

/*ミラーファイルの削除*/
int deleteMirrorFileFromDB(Mirror* mirror, MirrorFile* file){
    if(file == NULL){
        return -1;
    }

    //sql text
    PGresult* res;

    if(mirror == NULL){return -1;}
    //sql text
    int paramlength[] = {0};
    int paramformat[] = {0};
    const char* vals[] = {};

    res = PQexecParams(mirror->dbsession,
                       "DELETE FROM Mirror where path = $1", 
                       1,           /* パラメータは1つ。 */
                       NULL,        /* バックエンドにパラメータの型を推測させる。 */
                       vals,
                       paramlength,        /* テキストのため、パラメータ長は不要。 */
                       paramformat,        /* デフォルトで全てのパラメータはテキスト。 */
                       0);          /* バイナリ結果を要求。 */

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    }
    PQclear(res);
    return 0;
}

/*自由な検索*/
int customQuery(Mirror* mirror, char* query){
    PGresult* res;
    //sql text
    res = PQexec(mirror->dbsession, query);

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    }
    PQclear(res);
    return 0;
}

/*ミラーファイルの数*/
int getMirrorFileNum(Mirror* mirror){
    PGresult* res;
    int num;
    
    res = PQexec(mirror->dbsession, "SELECT COUNT(*) FROM Mirror;");

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
        return -1;
    }
    num = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return num;
}

/*ミラーストレージの使用量*/
int getMirrorUsedStorage(Mirror* mirror){
    int sum;
    //sql text
    PGresult* res;
    res = PQexec(mirror->dbsession, "SELECT SUM(SIZE) FROM Mirror;");

    if(PQresultStatus(res) != PGRES_TUPLES_OK){
        PQclear(res);
        return -1;
    }
    sum = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return sum;
}

/*DBのリセット*/
void resetMirrorDB(Mirror* mirror){
    customQuery(mirror, "DELETE FROM mirror;");
}

/****************************/
/*DBに関するコード群ここまで*/
/****************************/

/**********************/
/*通信に関するコード群*/
/**********************/

/*タスクの作成*/
MirrorTask* createTask(Mirror* mirror, const char* path){
    MirrorTask* task;
    Attribute* attribute;

    //コネクタの取得
    if(mirror == NULL){ return NULL; }
  
    //Attributeの取得
    attribute = connStat(mirror->connector, path);
    if(attribute == NULL){ return NULL; }

    //MirrorTaskの作成
    task = malloc(sizeof(MirrorTask));
    if(task == NULL){
        free(attribute);
        return NULL;
    }
    bzero(task, sizeof(MirrorTask));
    task->file = constructMirrorFile(path, attribute->st);
    task->st = attribute->st;
    strncpy(task->path, path, strlen(path) + 1);
    task->block_num = attribute->st.st_size / BLOCK_SIZE + 1;
    task->iterator = 0;

    free(attribute);

    return task;
}

/*タスクの解放*/
void freeMirrorTask(void* pointer){
    MirrorTask* task = pointer;
    freeMirrorFile(task->file);
    free(task);
}

/*タスクの出力*/
void printMirrorTask(void* pointer){
    MirrorTask* task = pointer;

    printf("[task]\n%s\n", task->file->path);
    printf("%d\n", task->file->size);
    printf("%d\n", task->block_num);
}

/*タスクの実行*/
int execTask(Mirror* mirror, MirrorTask* task){
    int rc, block_num, offset, size, errno;
    FILE* fp; 
    const char* path;
    char* mirrorpath = NULL;
    char buffer[4048];
    Connector* connector = NULL; 
    FileSession* filesession = NULL;
    Attribute* attr = NULL;
    MirrorFile* file, *tmp;

    path = task->path;
    file = task->file;

    printf("execTask %s\n", path);

    //すでに持っているか
    tmp = lookupMirrorFileFromDB(mirror, task->path);
    if(tmp != NULL){
        //持っている
        printf("execTask have %s\n", path);
        freeMirrorFile(tmp);
        tmp = NULL;
        return 0;
    }

    connector = mirror->connector;
    if(connector == NULL){
        return -1;
    }

    filesession = connOpen(connector, path, 0);
    if(filesession == NULL){
        printf("execTask connOpen %s fail\n", path);
        return -1;
    }
    printf("execTask filesession create %s\n", path);

    //ストレージ上のファイル作成とオープン
    mirrorpath = getMirrorPath(mirror, path);
    fp = fopen(mirrorpath, "w+");
    if(fp == NULL){
        printf("execTask fopen %s error %d fail\n", mirrorpath, errno);
        free(mirrorpath);
        free(filesession);
        mirrorpath = NULL;
        filesession = NULL;
        return -1;
    }

    //ブロックごとのダウンロードループ
    for(task->iterator = 0; task->iterator < task->block_num; task->iterator++){
        pthread_rwlock_wrlock(mirror->task_rwlock);
        offset = task->iterator * BLOCK_SIZE;
        rc = connRead(connector, filesession, (off_t)(offset), buffer, 4048);
        if(rc < 0){
            break;
        }
        pthread_rwlock_unlock(mirror->task_rwlock);
        buffer[4047] = '\0';
        fseek(fp, offset, SEEK_SET);
        rc = fwrite(buffer, 1, rc, fp);
    }
    fclose(fp);

    //リモートファイルのクローズ
    rc = connClose(connector, filesession);
    if(rc < 0){
        puts("connClose on execTask fail");
        free(mirrorpath);
        free(filesession);
        mirrorpath = NULL;
        filesession = NULL;
        return -1;
    }

    //DBへ登録
    rc = insertMirrorFileToDB(mirror, file);
    if(rc < 0){
        free(mirrorpath);
        mirrorpath = NULL;
        filesession = NULL;
        return -1;
    }

    attr = connStat(connector, path);
    if(attr == NULL){
        printf("execTask connStat %s fail\n", path);
        free(mirrorpath);
        mirrorpath = NULL;
        filesession = NULL;
        return -1;
    }

    file->attr = *attr;

    rc = _registerCache(mirror->dbsession, &file->attr);
    if(rc < 0){
        free(mirrorpath);
        free(attr);
        mirrorpath = NULL;
        filesession = NULL;
        attr = NULL;
        return -1;
    }

    //メモリ解放
    free(mirrorpath);
    free(attr);
    mirrorpath = NULL;
    filesession = NULL;
    attr = NULL;
    return 0;
}

/*タスクの追加*/
int appendTask(Mirror* mirror, const char* path){
    MirrorTask* task;

    task = createTask(mirror, path);
    if(task == NULL){
        return -1;
    }

    push_back(mirror->tasklist, task, sizeof(MirrorTask));
    return 0;
}

/*タスクの削除*/
void deleteTask(Mirror* mirror, const char* path){
    Node* tmpnode, *node, *prenode;
    MirrorTask* task;

    node = mirror->tasklist->head;
    prenode = NULL;
    for(;node != NULL;){
        tmpnode = node;
        task = (MirrorTask*)(node->data);

        if(strcmp(task->path, path) == 0){
            //リストの先頭の時
            if(prenode == NULL){
                mirror->tasklist->head = node->next;
            }else{
                prenode->next = node->next;
            }
            //先にノードを進めておく
            node = node->next;
            //nodeの解放
            freeMirrorTask(task);
            free(tmpnode);
        }else{
            node = node->next;
            prenode = node;
        }
    }
}

/*総合タスク管理*/
/*ミラー通信のロック方針はミラー側はブロック単位でファイルシステム側はオペレーション単位でロックする。*/
void* loopTask(void* pmirror){
    int rc;
    Node* node;
    MirrorTask* task;
    Mirror* mirror;

    mirror = pmirror;
    while(1){
        node = get_front(mirror->tasklist);

        pthread_mutex_lock(mirror->list_lock);

        while(node == NULL){
            if(mirror->killswitch == 1){
                pthread_mutex_unlock(mirror->list_lock);
                return NULL;
            }
            pthread_cond_wait(mirror->list_cond, mirror->list_lock);
            node = get_front(mirror->tasklist);
        }
        task = node->data;
        rc = execTask(mirror, task);
        if(rc < 0){
            pthread_mutex_unlock(mirror->list_lock);
            continue;
        }
        pop_front(mirror->tasklist, freeMirrorTask);

        pthread_mutex_unlock(mirror->list_lock);
    }
    return NULL;
}

/*ミラーリングのスレッドを開始*/
int startMirroring(Mirror* mirror){
    int rc;

    mirror->killswitch = 0;
    rc = pthread_create(&(mirror->taskthread), NULL, loopTask, mirror);
    if(rc != 0){
        printf("create thread fail\n");
        return -1;
    }
    return 0;
}
/******************************/
/*通信に関するコード群ここまで*/
/******************************/

/**********************/
/*ミラーファイルの管理*/
/**********************/

/******************************/
/*ミラーファイルの管理ここまで*/
/******************************/

/********************************/
/*インターフェースに関するコード*/
/********************************/
void write_mirror_request(Mirror* mirror, const char* path){
    FILE* file;

    file = fopen(mirror->config->mirrorreq, "a");
    if(file == NULL){ return; }
    flock(fileno(file), LOCK_EX);
    fwrite("\n", 1, 1, file);
    fwrite(path, 1, strlen(path), file);
    fwrite("\n", 1, 1, file);
    fsync(fileno(file));
    flock(fileno(file), LOCK_UN);
    fclose(file);
}

void request_mirror(Mirror* mirror, const char* path){
    printf("request_mirror %s\n", path);
    appendTask(mirror, path);
    pthread_cond_signal(mirror->list_cond);
}

MirrorFile* search_mirror(Mirror* mirror, const char* path){
    return lookupMirrorFileFromDB(mirror, path);
}

void check_mirror(Mirror* mirror, const char* path){
    int rc;
    rc = getMirrorFileNum(mirror);
}

/*MirrorFileをオープンし、ファイルディスクリプタを返す*/
int openMirrorFile(Mirror* mirror, MirrorFile* file){
    if(file == NULL){
        return -1;
    }
    file->fp = fopen(file->path, "r+");
    if(file->fp == NULL){
        printf("open mirrorfile fail\n");
        return -1;
    } 
    return 0;
}

/*MirrorFileのファイルディスクリプタに対してreadを発行する*/
int readMirrorFile(Mirror* mirror, MirrorFile* file, off_t offset, size_t size, char* buf){
    int rc;

    if(file == NULL){
        printf("MirrorFile* is NULL\n");
        return -1;
    }

    if(file->fp == NULL){
        printf("MirrorFile is not open\n");
        return -1;
    }

    rc = fseek(file->fp, offset, SEEK_SET);
    if(rc < 0){
        printf("readMirrorFile seek fail\n");
        return -1;
    }

    rc = fread(buf, 1, size, file->fp);
    if(rc < 0){
        printf("readMirrorFile read fail\n");
        return -1;
    }

    return rc;
}

/*MirrorFileのファイルディスクリプタに対してwriteを発行する*/
int writeMirrorFile(Mirror* mirror, MirrorFile* file, off_t offset, size_t size, const char* buf){
    int rc;

    if(file == NULL){
        printf("MirrorFile* is NULL\n");
        return -1;
    }

    rc = fseek(file->fp, offset, SEEK_SET);
    if(rc < 0){
        printf("readMirrorFile seek fail\n");
        return -1;
    }

    rc = fwrite(buf, 1, size, file->fp);
    if(rc < 0){
        printf("readMirrorFile fail\n");
    }

    return rc;
}

/*MirrorFileをクローズする*/
int closeMirrorFile(Mirror* mirror, MirrorFile* file){
    int rc;

    if(file == NULL){
        return -1;
    }

    rc = fclose(file->fp);
    if(rc < 0){
        printf("close mirrorfile fail\n");
        return -1;
    }

    file->fp = NULL;
    
    return 0;
}

/****************************************/
/*インターフェースに関するコードここまで*/
/****************************************/

/**********************************************/
/*バッググラウンドミラープロセスに関するコード*/
/**********************************************/

//ミラー要求ファイル監視関数
void readMirrorRequest(Mirror* mirror){
    int nread;
    size_t len;
    char* line = NULL;
    char* linebuf = NULL;
    FILE* fi;

    if(mirror == NULL){return;}

    while(mirror->killswitch == 0){
        line = NULL;
        fi = fopen(mirror->config->mirrorreq, "r+");
        if(fi == NULL){ return; }

        flock(fileno(fi), LOCK_EX);
        fseek(fi, SEEK_SET, 0);
        while((nread = getline(&line, &len, fi)) != -1){
            if(nread > 0){
                linebuf = strdup(line);
                linebuf[nread - 1] = '\0';
                printf("requesting mirror: %s\n", linebuf);
                request_mirror(mirror, linebuf);
                free(linebuf);
            }
        }
        if(line != NULL){ free(line); }
        ftruncate(fileno(fi), 0);
        flock(fileno(fi), LOCK_UN);
        fclose(fi);
        sleep(5);
    }
}

//ミラープロセス用関数
//プロセス内で新しいミラー構造体を生成する。ループで要求を受け付けてタスクに追加
void* mirrorProcess(char* configpath){
    int rc;
    MirrorConfig* config;
    Mirror* mirror;
    char mirrorpath[256];
    FILE* fi;

    config = loadMirrorConfig(configpath);
    if(config == NULL){ return NULL; }

    mirror = constructMirror(config);
    if(mirror == NULL){
        freeMirrorConfig(config);
        return NULL; 
    }

    rc = startMirroring(mirror);
    if(rc < 0){
        freeMirror(mirror);
        return NULL;
    }

    fi = fopen(mirror->config->mirrorreq,"r+");
    if(fi == NULL){
        freeMirror(mirror);
    }
    printf("open mirror.req in %s\n", mirror->config->mirrorreq);
    readMirrorRequest(mirror);

    return NULL;
}

int mirrorProcessRun(char* configpath){
    int pid;

    pid = fork();
    if(pid == -1){
        puts("fork mirror process fail");
    }else if(pid == 0){
        mirrorProcess(configpath);
    }

    return pid;
}








