#include <signal.h>
#include "remotefs.h"
#include "connection.h"

FsData fsdata = {0, NULL, NULL, NULL, NULL, NULL};
char configrealpath[256];

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
    bzero(config, sizeof(FsConfig));

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

    mountpoint = searchOptionKey(file, "DBNAME");

    fclose(file);
    
    return config;
}

int initFsData(char* configpath){
    int rc, len;
    ConnectConfig* connectconfig;
    MirrorConfig* mirrorconfig;
    RecordConfig* recordconfig;
    CacheConfig* cacheconfig;

    if(fsdata.initiated == 0){
        //Recordの設定
        recordconfig = loadRecordConfig(configpath);
        if(recordconfig == NULL){
            puts("record config error");
            exit(EXIT_FAILURE);
        }

        fsdata.record = newRecord(recordconfig); 
        if(fsdata.record == NULL){
            puts("fsdata.record error");
            exit(EXIT_FAILURE);
        }

        //Cacheの設定
        cacheconfig = loadCacheConfig(configpath);
        if(cacheconfig == NULL){
            puts("cache config error");
            exit(EXIT_FAILURE);
        }

        fsdata.cache = newCache(cacheconfig);
        if(fsdata.cache == NULL){
            puts("fsdata.cache error");
            exit(EXIT_FAILURE);
        }

        fsdata.config = loadFsConfig(configpath);
        if(fsdata.config == NULL){
            puts("fsdata.config error");
            exit(EXIT_FAILURE);
        }

        connectconfig = loadConnConfig(configpath);
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
        mirrorconfig = loadMirrorConfig(configpath);
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
        //mirrorProcessRun(configpath);

        fsdata.initiated = 1;
        
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
    int rc = 0;
    int online = 0;
    Attribute* attr;
    char* RemotePath;

    RemotePath = patheditor(path);
    if(RemotePath == NULL){ return -ECANCELED; }

    //オンラインチェック
    online = connStatus(fsdata.connector);
    if(online < 0){
        attr = lookupCache(fsdata.cache, RemotePath);
        if(attr == NULL){
            free(RemotePath);
            return -ENOENT;
        }
    }else{
        attr = connStat(fsdata.connector, RemotePath);
        if(attr == NULL){
            printf("connStat error\n");
            //connStat中のオフライン
            attr = lookupCache(fsdata.cache, RemotePath);
            if(attr == NULL){
                free(RemotePath);
                return -ENOENT;
            }
            free(RemotePath);
            return 0;
        }

        rc = registerCache(fsdata.cache, attr);
        if(rc < 0){
            printf("registerCache error\n");
            free(RemotePath);
            return -ENOENT;
        }
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
    char* RemotePath = NULL;
    FileHandler file = {0};
    FileSession* session = NULL;

    //ファイルハンドラマップを取得
    fh = newhandler(fsdata.FhMap);

    RemotePath = patheditor(path);

    //ミラーファイルを参照
    file.mfile = search_mirror(fsdata.mirror, RemotePath);
    if(file.mfile != NULL){
        puts("open MirrorFile");
        rc = openMirrorFile(fsdata.mirror, file.mfile); 
        if(rc < 0){
            free(RemotePath);
            return -ECANCELED;
        }
        /* ファイルハンドラの管理用マップ構造体へ登録 */
        insIntMap(fsdata.FhMap, fh, &file, sizeof(FileHandler)); 
        fi->fh = fh;
        //レコード
        recordOperation(fsdata.record, RemotePath, oOPEN);
        free(RemotePath);
        return 0;
    }
    
    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション
     * リモートファイルのオープン */
    session = connOpen(fsdata.connector, RemotePath, fi->flags);
    if(session == NULL){
        free(RemotePath);
        return -ENETDOWN;
    }
    file.session = session;
    /* リモートを参照ここまで*/

    //ミラーファイルをオーダーする
    write_mirror_request(fsdata.mirror, RemotePath);

    /* ファイルハンドラの管理用マップ構造体へ登録 */
    insIntMap(fsdata.FhMap, fh, &file, sizeof(FileHandler)); 
    fi->fh = fh;

    //レコード
    recordOperation(fsdata.record, RemotePath, oOPEN);
    free(RemotePath);

    return 0;  
}

int fuseRead(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    FileHandler* fh = NULL;
    int rc = 0;
    char* RemotePath = NULL;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }

    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        puts("read MirrorFile");
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
        puts("write to MirrorFile");
        rc = writeMirrorFile(fsdata.mirror, fh->mfile, offset, size, buffer);
        if(rc < 0){
            puts("writeMirrorFile error");
            return -EBADFD;
        }
        RemotePath = patheditor(path);
        recordOperation(fsdata.record, RemotePath, oWRITE);
        free(RemotePath);
        //通信ができる場合はリモートにも送りたいので、終了しない。けど、ミラーファイルの場合は通信セッションをハンドラが持たないので無理。
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connWrite(fsdata.connector, fh->session, offset, (void*)buffer, size);
    if(rc < 0){
        return -ENETDOWN;
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
        puts("close MirrorFile");
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
        return -ENETDOWN;
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
    int rc = 0;
    int online = 0;
    List* attrs = NULL;
    char* RemotePath = NULL;
    Attribute* attr = NULL;
    
    RemotePath = patheditor(path);
    if(RemotePath == NULL){ return -1; }

    online = connStatus(fsdata.connector);
    if(online < 0){
        attrs = lookupDirCache(fsdata.cache, RemotePath);
        if(attrs == NULL){
            free(RemotePath);
            return -ENOENT;
        }
    }else{
        attrs = connReaddir(fsdata.connector, RemotePath); 
        if(attrs == NULL){
            attrs = lookupDirCache(fsdata.cache, RemotePath);
            if(attrs == NULL){
                free(RemotePath);
                return -ENOENT;
            }
        }else{
            rc = registerDirCache(fsdata.cache, RemotePath, attrs);
            if(rc < 0){
                free(RemotePath);
                freeList(attrs, NULL);
                return -ENOENT;
            }
        }
    }

    for(Node* node = attrs->head; node != NULL; node = node->next){
        attr = node->data;
        //printStat(attr);
        filler(buf, attr->path, &(attr->st), 0, FUSE_FILL_DIR_PLUS);
    }
    freeList(attrs, NULL);
    free(RemotePath);

    return 0;    
}

void* fuseInit(struct fuse_conn_info *conn, struct fuse_config *cfg){
    printf("configpath: %s\n", configrealpath);
    initFsData(configrealpath);
    return NULL;
}

void fuseDestory(void *private_data){
    freeIntMap(fsdata.FhMap);
}

off_t fuseLseek(const char *path, off_t offset, int whence, struct fuse_file_info *fi){
    FileHandler* fh;
    int rc;

    fh = getIntMap(fsdata.FhMap, fi->fh);
    if(fh == NULL){
        return -EBADFD;
    }

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
    char* configpath = "./config/config.txt";

    struct sigaction act, oact;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, &oact);

    realpath(configpath, configrealpath);
    printf("%s\n", configrealpath);
    return fuse_main(argc, argv, &fuseOper, NULL);
}
