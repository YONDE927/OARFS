#include "mirror.h"
#include "fileoperation.h"

#include <sqlite3.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define BLOCK_SIZE 4048 //4KB

int initDbSession(const char *filename, sqlite3 **ppDb);
int closeDbSession(sqlite3* pDb);
int createMirrorTable(sqlite3* dbsession);
int getMirrorUsedStorage(sqlite3* dbsession);
MirrorTask* createTask(const char* path);
void freeMirrorTask(void* task);

Mirror* constructMirror(char* dbname, char* root, char* connfig){
    int rc;
    Mirror* mirror;
    Connector* connector;

    mirror = malloc(sizeof(Mirror));
    rc = initDbSession(dbname, &(mirror->dbsession));
    if(rc < 0){
        return NULL;
    }

    rc = createMirrorTable(mirror->dbsession);
    if(rc < 0){
        return NULL;
    }

    connector = getMirrorConnector(connfig);

    mirror->root = strdup(root);

    mirror->killswitch = 0;

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
    free(mirror->root);
    pthread_rwlock_destroy(mirror->task_rwlock);
    pthread_mutex_destroy(mirror->list_lock);
    pthread_cond_destroy(mirror->list_cond);
    closeDbSession(mirror->dbsession);
    freeList(mirror->tasklist, freeMirrorTask);
}

MirrorFile* constructMirrorFile(const char* path, struct stat st){
    MirrorFile* file;
    
    file = malloc(sizeof(MirrorFile));
    file->path = strdup(path);
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
        free(file->path);
        free(file);
    }
}

/******************************/
/*共通のコンストラクタここまで*/
/******************************/

/************/
/*文字列処理*/
/************/

/*文字列コピー*/
char* strdup2(const char* str){
    int n;
    char* dest = 0;

    n = strlen(str) + 1;
    dest = malloc(n + 1);
    strncpy(dest, str, n);

    return dest;
}

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

    root_size = strlen(mirror->root);
    path_size = strlen(convertpath);

    mirrorpath = malloc(root_size + path_size + 2);
    strncpy(mirrorpath, mirror->root, root_size + 1);
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
int initDbSession(const char *filename, sqlite3 **ppDb){
    int rc;

    //sqlite3のスレッド対応
    rc = sqlite3_initialize();
    rc = sqlite3_config(SQLITE_CONFIG_SERIALIZED);

    rc = sqlite3_open(filename, ppDb);
    if(rc){
        sqlite3_close(*ppDb);
        return -1;
    }
    return 0;
}

/*DBセッションを終了*/
int closeDbSession(sqlite3* pDb){
    int rc;
    sqlite3_close(pDb);
    return 0;
}

/*DBの状態を確認。現在はただバージョンを返す。*/
int getDbStatus(sqlite3* pdb){
    int rc;
    sqlite3_stmt *res;

    rc = sqlite3_prepare_v2(pdb, "SELECT SQLITE_VERSION()", -1, &res, 0);
    if(rc != SQLITE_OK){
        printf("db stmt failed.\n");
        sqlite3_finalize(res);
        return -1;
    }
    
    rc = sqlite3_step(res);
    if(rc == SQLITE_ROW){
        printf("%s\n", sqlite3_column_text(res, 0));
    }

    sqlite3_finalize(res);
    return 0;
}

/*ミラー用のDBテーブルを作成*/
int createMirrorTable(sqlite3* dbsession){
    int rc;
    sqlite3_stmt *stmt;

    if(dbsession == NULL){
        printf("createMirrorTable failed\n");
        return -1;
    }
    
    //sql text
    rc = sqlite3_prepare_v2(dbsession, 
            "CREATE TABLE IF NOT EXISTS Mirrors"
            "(path TEXT PRIMARY KEY, size INTEGER, mtime INTEGER"
            ", atime INTEGER, ref_cnt INTEGER);",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        sqlite3_finalize(stmt);
        return -1;
    }

    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        printf("createMirrorTable failed\n");
        sqlite3_finalize(stmt);
    }

    //end
    sqlite3_finalize(stmt);
    return 0;
}

/*ミラーファイルの挿入*/
int insertMirrorFileToDB(sqlite3* dbsession, MirrorFile* file){
    int rc;
    sqlite3_stmt* stmt;

    //sql text
    rc = sqlite3_prepare_v2(dbsession, 
            "REPLACE INTO Mirrors VALUES "
            "( ?, ?, ?, ?, ?);",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        sqlite3_finalize(stmt);
        return -1;
    }else{
        sqlite3_bind_text(stmt, 1, file->path, -1, 0);
        sqlite3_bind_int(stmt, 2, file->size);
        sqlite3_bind_int(stmt, 3, file->mtime);
        sqlite3_bind_int(stmt, 4, file->atime);
        sqlite3_bind_int(stmt, 5, file->ref_cnt);
    }

    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        printf("insertMirrorFile fail.\n");
        sqlite3_finalize(stmt);
        return -1;
    }

    //end
    sqlite3_finalize(stmt);
    return 0;
}

/*ミラーファイルの検索*/
MirrorFile* lookupMirrorFileFromDB(sqlite3* dbsession, const char* path){
    int rc;
    sqlite3_stmt* stmt;
    MirrorFile* file;

    //sql text
    rc = sqlite3_prepare_v2(dbsession, 
            "SELECT * FROM Mirrors WHERE "
            "path = ?;",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        sqlite3_finalize(stmt);
        return NULL;
    }else{
        sqlite3_bind_text(stmt, 1, path, -1, 0);
    }
    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW){
        printf("lookupMirrorFileFromDB %s fail.\n", path);
        file = NULL;
    }else{
        file = malloc(sizeof(MirrorFile));
        file->path = strdup((const char*)sqlite3_column_text(stmt, 0));
        file->size = sqlite3_column_int(stmt, 2);
        file->mtime = sqlite3_column_int(stmt, 3);
        file->atime = sqlite3_column_int(stmt, 4);
        file->ref_cnt = sqlite3_column_int(stmt, 5);
        file->fd = -1;
    }

    sqlite3_finalize(stmt);
    return file;
}

/*ミラーファイルの削除*/
int deleteMirrorFileFromDB(sqlite3* dbsession, MirrorFile* file){
    int rc;
    sqlite3_stmt* stmt;

    if(file == NULL){
        return -1;
    }

    //sql text
    rc = sqlite3_prepare_v2(dbsession, 
            "DELETE FROM Mirrors WHERE "
            "path = ?;",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        sqlite3_finalize(stmt);
        return -1;
    }else{
        sqlite3_bind_text(stmt, 1, file->path, -1, 0);
    }
    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        printf("delete MirrorFile fail.\n");
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

/*結果表示用関数*/
int showCallback(void* Notused, int argc, char** argv, char **azColName){
    Notused = 0;
    for(int i = 0; i < argc; i++){
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : NULL);
    }
    printf("\n");
    return 0;
}

/*自由な検索*/
int customQuery(sqlite3* dbsession, char* query){
    int rc;
    char* errmsg = 0;

    rc = sqlite3_exec(dbsession, query, showCallback, 0, &errmsg);
    if(rc != SQLITE_OK){
        printf("customQuery failed.\n");
        return -1;
    }
    return 0;
}

/*ミラーファイルの数*/
int getMirrorFileNum(sqlite3* dbsession){
    int rc;
    
    rc = customQuery(dbsession, "SELECT COUNT(*) FROM Mirrors;");
    return rc;
}

/*ミラーストレージの使用量*/
int getMirrorUsedStorage(sqlite3* dbsession){
    int rc;
    sqlite3_stmt* stmt;

    //sql text
    rc = sqlite3_prepare_v2(dbsession, 
            "SELECT SUM(SIZE) FROM Mirrors;",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        return -1;
    }
    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW){
        printf("getMirrorUsedStorage fail.\n");
        return -1;
    }else{
        rc = sqlite3_column_int(stmt, 0);
    }
    return rc;
}

/*DBのリセット*/
void resetMirrorDB(sqlite3* dbsession){
    customQuery(dbsession, "DELETE FROM mirrors;");
}

/****************************/
/*DBに関するコード群ここまで*/
/****************************/

/**********************/
/*通信に関するコード群*/
/**********************/

Connector* getMirrorConnector(char* configpath)
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

/*タスクの作成*/
MirrorTask* createTask(const char* path){
    MirrorTask* task;
    Attribute* attribute;

    //コネクタの取得
    Connector* connector = getMirrorConnector(NULL);
    if(connector == NULL){
        return NULL;
    }
   
    //Attributeの取得
    attribute = connStat(path);
    if(attribute == NULL){
        return NULL;
    }

    //MirrorTaskの作成
    task = malloc(sizeof(MirrorTask));
    task->file = constructMirrorFile(path, attribute->st);
    task->st = attribute->st;
    task->path = strdup(path);
    task->block_num = attribute->st.st_size / BLOCK_SIZE + 1;
    task->iterator = 0;

    free(attribute);

    return task;
}

/*タスクの解放*/
void freeMirrorTask(void* pointer){
    MirrorTask* task = pointer;
    free(task->path);
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
    FileSession* filesession;
    Attribute* attribute;
    MirrorFile* file, *tmp;
    FILE* fp; 
    char* mirrorpath;
    char buffer[4048];

    const char* path = task->path;
    file = task->file;

    printf("execTask %s\n", path);

    //すでに持っているか
    tmp = lookupMirrorFileFromDB(mirror->dbsession, task->path);
    if(tmp != NULL){
        //持っている
        printf("execTask have %s\n", path);
        return -1;
    }

    Connector* connector = getMirrorConnector(NULL);
    if(connector == NULL){
        printf("execTask getMirrorConnector %s fail\n", path);
        return -1;
    }
   
    filesession = connOpen(path, 0);
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
        return -1;
    }

    //ブロックごとのダウンロードループ
    for(task->iterator = 0; task->iterator < task->block_num; task->iterator++){
        pthread_rwlock_wrlock(mirror->task_rwlock);
        offset = task->iterator * BLOCK_SIZE;
        rc = connRead(filesession, (off_t)(offset), buffer, 4048);
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
    rc = connClose(filesession);
    if(rc < 0){
        puts("connClose on execTask fail");
        return -1;
    }

    //DBへ登録
    rc = insertMirrorFileToDB(mirror->dbsession, file);
    if(rc < 0){
        free(mirrorpath);
        return -1;
    }

    //メモリ解放
    free(mirrorpath);
    return 0;
}

/*タスクの追加*/
int appendTask(Mirror* mirror, const char* path){
    MirrorTask* task;

    task = createTask(path);
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
                return NULL;
            }
            pthread_cond_wait(mirror->list_cond, mirror->list_lock);
            node = get_front(mirror->tasklist);
        }
        task = node->data;
        rc = execTask(mirror, task);
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
void request_mirror(Mirror* mirror, const char* path){
    int rc;

    printf("request_mirror %s\n", path);
    rc = appendTask(mirror, path);
    pthread_cond_signal(mirror->list_cond);
}

MirrorFile* search_mirror(Mirror* mirror, const char* path){
    return lookupMirrorFileFromDB(mirror->dbsession, path);
}


void check_mirror(Mirror* mirror, const char* path){
    int rc;
    
    rc = getMirrorFileNum(mirror->dbsession);
}

/*MirrorFileをオープンし、ファイルディスクリプタを返す*/
int openMirrorFile(MirrorFile* file){
    int fd;

    if(file == NULL){
        return -1;
    }
    fd = open(file->path, O_RDWR);
    if(fd < 0){
        printf("open mirrorfile fail\n");
        file->fd = -1;
        return -1;
    } 
    file->fd = fd;
    return 0;
}

/*MirrorFileのファイルディスクリプタに対してreadを発行する*/
int readMirrorFile(MirrorFile* file, off_t offset, size_t size, char* buf){
    int rc;

    if(file == NULL){
        printf("MirrorFile* is NULL\n");
        return -1;
    }
    if(file->fd == -1){
        printf("MirrorFile is not open\n");
        return -1;
    }
    rc = lseek(file->fd, offset, SEEK_SET);
    if(rc < 0){
        printf("readMirrorFile seek fail\n");
        return -1;
    }
    rc = read(file->fd, buf, size);
    if(rc < 0){
        printf("readMirrorFile read fail\n");
        return -1;
    }
    return rc;
}
/*MirrorFileのファイルディスクリプタに対してwriteを発行する*/
int writeMirrorFile(MirrorFile* file, off_t offset, size_t size, const char* buf){
    int rc;

    if(file == NULL){
        printf("MirrorFile* is NULL\n");
        return -1;
    }
    rc = lseek(file->fd, offset, SEEK_SET);
    if(rc < 0){
        printf("readMirrorFile seek fail\n");
        return -1;
    }
    rc = write(file->fd, buf, size);
    if(rc < 0){
        printf("readMirrorFile fail\n");
    }
    return rc;
}

/*MirrorFileをクローズする*/
int closeMirrorFile(MirrorFile* file){
    int rc;

    if(file == NULL){
        return -1;
    }
    rc = close(file->fd);
    if(rc < 0){
        printf("close mirrorfile fail\n");
        return -1;
    }
    file->fd = -1;
    return 0;
}

/****************************************/
/*インターフェースに関するコードここまで*/
/****************************************/

