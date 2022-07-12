#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "list.h"
#include "map.h"
#include "attr.h"
#include "conn.h"
#include "mirror.h"
#include "record.h"

typedef struct FileHandler {
    off_t offset;
    FileSession* session;
    MirrorFile* mfile;
} FileHandler;

typedef struct FsConfig{
    char path[256];
    char RemoteRoot[256];
    char MountPoint[256];
} FsConfig;

typedef struct FsData{
    int initiated;
    IntMap* FhMap;
    Mirror* mirror;
    Record* record;
    Connector* connector;
    FsConfig* config;
} FsData;

FsData fsdata = {0, NULL, NULL, NULL, NULL, NULL};

FsConfig* loadFsConfig(char* path);

int initFsData(char* configpath);
int fuseGetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
int fuseOpen(const char* path, struct fuse_file_info* fi);
int fuseRead(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* fi);
int fuseWrite(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* fi);
int fuseStatfs(const char* fs, struct statvfs* stbuf);
int fuseRelease(const char* path, struct fuse_file_info* fi);
int fuseReaddir (const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,struct fuse_file_info* fi, enum fuse_readdir_flags flags);
void* fuseInit(struct fuse_conn_info* conn, struct fuse_config* cfg);
void fuseDestory(void* private_data);
off_t fuseLseek(const char* path, off_t offset, int whence, struct fuse_file_info* fi);


FsConfig* loadFsConfig(char* path){
    char* remoteroot;
    char* mountpoint;
    FILE* file;
    FsConfig* config;

    if(path == NULL){ return NULL; }

    file = fopen(path, "r");
    if(file == NULL){ return NULL; }

    config = malloc(sizeof(FsConfig));
    if(config == NULL){
        fclose(file);
        return NULL;
    }

    strncpy(config->path, path, strlen(path) + 1);

    if((remoteroot = searchOptionKey(file, "REMOTEROOT")) != NULL){
        strncpy(config->RemoteRoot, remoteroot, strlen(remoteroot) + 1);
        free(remoteroot);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    if((mountpoint = searchOptionKey(file, "MOUNTPOINT")) != NULL){
        strncpy(config->MountPoint , mountpoint, strlen(mountpoint) + 1);
        free(mountpoint);
    }else{
        fclose(file);
        free(config);
        return NULL;
    }

    fclose(file);
    
    return config;
}

int initFsData(char* configpath){
    int rc, len;
    ConnectConfig* connectconfig;
    MirrorConfig* mirrorconfig;

    if(fsdata.initiated == 0){

        fsdata.config = loadFsConfig(configpath);
        if(fsdata.config == NULL){
            puts("fsdata.config error");
            exit(EXIT_FAILURE);
        }

        connectconfig = loadConnConfig(fsdata.config->path);
        if(connectconfig == NULL){
            puts("connect config error");
            exit(EXIT_FAILURE);
        }

        fsdata.connector = getConnector(connectconfig);
        if(fsdata.connector == NULL){
            puts("fsdata.connector error");
            exit(EXIT_FAILURE);
        }

        fsdata.FhMap = newIntMap();

        //FsDataにMirrorを設定
        mirrorconfig = loadMirrorConfig(fsdata.config->path);
        if(mirrorconfig == NULL){
            puts("mirror config error");
            exit(EXIT_FAILURE);
        }

        fsdata.mirror = constructMirror(mirrorconfig);
        if(fsdata.mirror == NULL){
            puts("fsdata.mirror error");
            exit(EXIT_FAILURE);
        }

        //mirrorのリセット。テスト用
        resetMirrorDB(fsdata.mirror);

        //mirrorringのプロセスを開始する。
        //子プロセスの生成と終了条件について今後整理したい。
        mirrorProcessRun(fsdata.config->path);

        //Recordの設定
        fsdata.record = newRecord(); 
        if(fsdata.record == NULL){
            puts("fsdata.record error");
            exit(EXIT_FAILURE);
        }
        
        printf("Init %s\n", fsdata.config->RemoteRoot);
    }
    
    return 0;
}

char* patheditor(const char* path){
    int len1, len2;
    char* root,* buffer;
   
    root = fsdata.config->RemoteRoot;
    len1 = strlen(root);
    len2 = strlen(path);
    buffer = malloc(sizeof(char) * (len1 + len2 + 1));
    strncpy(buffer, root, len1);
    buffer[len1] = '\0';
    strncat(buffer, path, len2);
    buffer[len1 + len2] = '\0';
    return buffer;
}

int fuseGetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
    //printf("getattr %s\n",path);
    Attribute* attr;
    char* RemotePath;

    RemotePath = patheditor(path);
    attr = connStat(fsdata.connector, RemotePath);
    if(attr == NULL){
        printf("connStat error\n");
        free(RemotePath);
        return -ECANCELED;
    }
    *stbuf = attr->st;
    free(attr);
    free(RemotePath);
    return 0;
}

int pair_max(int a,int b){
    if(a > b){
        return a;
    }else{
        return b;
    }
}

int max(int tmp, int* li, int size){
    if(size <= 1){
        return pair_max(tmp, li[0]);
    }else{
        tmp = pair_max(tmp, li[0]);
        size--;
        return max(tmp, li + 1, size);
    }
}

int newhandler(IntMap* map){
    int map_size,ind,fh,top;
    int* fhs;

    if(map == NULL){
        return -1;
    }

    map_size = lenIntMap(map);
    fhs = malloc(sizeof(int) * map_size);
    ind = 0;
    for(IntMapNode* node = map->head; node != NULL; node = node->next){
        fh = node->key;
        fhs[ind] = fh;
        ind++;
    }
    //search new file handler
    top = max(fhs[0], fhs, map_size) + 1;
    free(fhs);

    return top;
}

int fuseOpen(const char *path, struct fuse_file_info *fi){
    int map_size;
    int rc, ind, fh;
    char* RemotePath;
    FileHandler file = {0};
    FileSession* session;

    //ファイルハンドラマップを取得
    fh = newhandler(fsdata.FhMap);

    RemotePath = patheditor(path);

    //ミラーファイルを参照
    file.mfile = search_mirror(fsdata.mirror, RemotePath);
    if(file.mfile != NULL){
        rc = openMirrorFile(fsdata.mirror, file.mfile); 
        if(rc < 0){
            free(RemotePath);
            return -ECANCELED;
        }
        /* ファイルハンドラの管理用マップ構造体へ登録 */
        insIntMap(fsdata.FhMap, fh, &file, sizeof(FileHandler)); 
        fi->fh = fh;
        free(RemotePath);
        return 0;
    }
    
    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション
     * リモートファイルのオープン */
    session = connOpen(fsdata.connector, RemotePath, fi->flags);
    if(session == NULL){
        free(RemotePath);
        return -ECANCELED;
    }
    file.session = session;
    /* リモートを参照ここまで*/

    //ミラーファイルをオーダーする
    write_mirror_request(fsdata.mirror, RemotePath);
    free(RemotePath);

    /* ファイルハンドラの管理用マップ構造体へ登録 */
    insIntMap(fsdata.FhMap, fh, &file, sizeof(FileHandler)); 
    fi->fh = fh;

    //レコード
    RemotePath = patheditor(path);
    recordOperation(fsdata.record, RemotePath, oOPEN);
    free(RemotePath);

    return 0;  
}

int fuseRead(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    FileHandler* fh;
    int rc;
    char* RemotePath;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }

    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = readMirrorFile(fsdata.mirror, fh->mfile, offset, size, buffer);
        if(rc < 0){
            printf("readMirrorFile fail\n");
            return -EBADFD;
        }
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connRead(fsdata.connector, fh->session, offset, buffer, size);
    if(rc < 0){
        return -ENETDOWN;
    }
    /* リモートを参照ここまで*/

    //オフセットの設定 
    fh->offset += rc;

    //レコード
    RemotePath = patheditor(path);
    recordOperation(fsdata.record, RemotePath, oREAD);
    free(RemotePath);

    return rc;
}

int fuseWrite(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    FileHandler* fh;
    char* RemotePath;
    int rc;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }
   
    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = writeMirrorFile(fsdata.mirror, fh->mfile, offset, size, buffer);
        if(rc < 0){
            return -EBADFD;
        }
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connWrite(fsdata.connector, fh->session, offset, (void*)buffer, size);
    if(rc < 0){
        return -ECANCELED;
    }
    /* リモートを参照ここまで*/

    //オフセットの設定 
    fh->offset += rc;

    //レコード
    RemotePath = patheditor(path);
    recordOperation(fsdata.record, RemotePath, oWRITE);
    free(RemotePath);

    return rc;    
}

int fuseRelease(const char *path, struct fuse_file_info *fi){
    FileHandler* fh;
    int rc;
    char* RemotePath;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }
   
    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = closeMirrorFile(fsdata.mirror, fh->mfile);
        if(rc < 0){
            return -EBADFD;
        }
        //対象ファイルのFileHandlerをfhMapから削除して解放
        delIntMap(fsdata.FhMap, fi->fh);
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connClose(fsdata.connector, fh->session);
    if(rc < 0){
        return -ECANCELED;
    }
    /* リモートを参照ここまで*/

    //対象ファイルのFileHandlerをfhMapから削除して解放
    delIntMap(fsdata.FhMap, fi->fh);

    //レコード
    RemotePath = patheditor(path);
    recordOperation(fsdata.record, RemotePath, oCLOSE);
    free(RemotePath);
   
    return 0;    
}

int fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
    List* attrs;
    char* RemotePath;
    Attribute* attr;
    
    RemotePath = patheditor(path);
    attrs = connReaddir(fsdata.connector, RemotePath); 
    if(attrs == NULL){
        free(RemotePath);
        return -ECANCELED;
    }
    for(Node* node = attrs->head; node != NULL; node = node->next){
        attr = node->data;
        filler(buf, attr->path, &(attr->st), 0, FUSE_FILL_DIR_PLUS);
    }
    freeList(attrs, NULL);
    free(RemotePath);

    //レコード
    //recordOperation(fs->record, path, OPEN);

    return 0;    
}

void* fuseInit(struct fuse_conn_info *conn, struct fuse_config *cfg){
    char* configpath;

    configpath = fuse_get_context()->private_data;
    initFsData(configpath);
    return NULL;
}

void fuseDestory(void *private_data){
    FsData* fs;

    fs = private_data;
    freeIntMap(fs->FhMap);
    free(fs);
}

off_t fuseLseek(const char *path, off_t offset, int whence, struct fuse_file_info *fi){
    FileHandler* fh;
    int rc;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
   
    fh->offset = offset;
    return fh->offset;
}

struct fuse_operations fuseOper = {
    .getattr = fuseGetattr,
    .open = fuseOpen,
    .read = fuseRead,
    .write = fuseWrite,
    //.statfs = fuseStatfs,
    .release = fuseRelease,
    .readdir = fuseReaddir,
    .init = fuseInit,
    .destroy = fuseDestory,
    .lseek = fuseLseek
};

int main(int argc, char* argv[]){

    if(argc < 2){
        puts("need config path");
    }
    return fuse_main(argc, argv, &fuseOper, argv[1]);
}
