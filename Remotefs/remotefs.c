#include "remotefs.h"
#include "conn.h"
#include "list.h"
#include "map.h"
#include "attr.h"
#include "mirror.h"
#include "record.h"
#include <fuse3/fuse.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct FileHandler 
{
    off_t offset;
    FileSession* session;
    MirrorFile* mfile;
} FileHandler;

typedef struct FsData
{
    IntMap* FhMap;
    char* RemoteRoot;
    Mirror* mirror;
    Record* record;
} FsData;

typedef struct Args
{
    char* RemoteRoot;
    char* Connfig;
} Args;

Args* getArgs(char* RemoteRoot, char* SshConfig)
{
    static Args* args = NULL;
    int len;
    if(args == NULL)
    {
        args = malloc(sizeof(Args));
        if(RemoteRoot != NULL)
        {
            len = strlen(RemoteRoot);
            args->RemoteRoot = malloc(sizeof(char) * (len + 1)); 
            strncpy(args->RemoteRoot, RemoteRoot, len);
            args->RemoteRoot[len] = '\0';
        }
        if(SshConfig != NULL)
        {
            len = strlen(SshConfig);
            args->Connfig = malloc(sizeof(char) * (len + 1)); 
            strncpy(args->Connfig, SshConfig, len);
            args->Connfig[len] = '\0';
        }
    }
    return args;
}

FsData* getFsData()
{
    int rc;

    static FsData* fs = NULL;
    if(fs == NULL)
    {
        int len;
        Args* args;
        Connector* connecotor;
        char dbpath[256];
        char mirrorpath[256];

        args = getArgs(NULL,NULL);
        if(args == NULL)
        {
            printf("no args\n");
            exit(EXIT_FAILURE);
        }

        fs = malloc(sizeof(FsData));
        fs->FhMap = newIntMap();
        //SSH接続とSFTPセッションの確立 
        connecotor = getConnector(args->Connfig);
        if(connecotor == NULL)
        {
            printf("SSH session is not established\n");
            exit(EXIT_FAILURE);
        }

        //FsDataにリモートサーバーのルートパスを設定
        len = strlen(args->RemoteRoot);
        fs->RemoteRoot = malloc(sizeof(char) * (len + 1)); 
        strncpy(fs->RemoteRoot, args->RemoteRoot, len);
        fs->RemoteRoot[len] = '\0';

        //FsDataにMirrorを設定
        realpath("./db/mirror.db", dbpath);
        realpath("./mirrors", mirrorpath);
        fs->mirror = constructMirror(dbpath, mirrorpath, args->Connfig);
        resetMirrorDB(fs->mirror->dbsession);
        rc = startMirroring(fs->mirror);
        if(rc < 0){
            printf("startMirroring fail\n");
            exit(EXIT_FAILURE);
        }

        //Recordの設定
        fs->record = newRecord(); 
        
        printf("Init %s\n", fs->RemoteRoot);
    }
    return fs;
}

Mirror* getMirror(){
    FsData* fs = getFsData();
    return fs->mirror;
}


IntMap* getFhMap()
{
    FsData* fs = getFsData();
    return fs->FhMap;
}

char* getRoot()
{
    FsData* fs = getFsData();
    return fs->RemoteRoot;
}

char* patheditor(const char* path)
{
    int len1, len2;
    char* root,* buffer;
   
    root = getRoot(); 
    len1 = strlen(root);
    len2 = strlen(path);
    buffer = malloc(sizeof(char) * (len1 + len2 + 1));
    strncpy(buffer, root, len1);
    buffer[len1] = '\0';
    strncat(buffer, path, len2);
    buffer[len1 + len2] = '\0';
    return buffer;
}

int fuseGetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    //printf("getattr %s\n",path);
    Attribute* attr;
    char* RemotePath;

    RemotePath = patheditor(path);
    attr = connStat(RemotePath);
    if(attr == NULL)
    {
        printf("connStat error\n");
        free(RemotePath);
        return -ECANCELED;
    }
    *stbuf = attr->st;
    free(attr);
    free(RemotePath);
    return 0;
}

int pair_max(int a,int b)
{
    if(a > b){
        return a;
    }else{
        return b;
    }
}

int max(int tmp, int* li, int size)
{
    if(size <= 1){
        return pair_max(tmp, li[0]);
    }else{
        tmp = pair_max(tmp, li[0]);
        size--;
        return max(tmp, li + 1, size);
    }
}

int newhandler(IntMap* map)
{
    int map_size,ind,fh,top;
    int* fhs;
    if(map == NULL)
    {
        return -1;
    }

    map_size = lenIntMap(map);
    fhs = malloc(sizeof(int) * map_size);
    ind = 0;
    for(IntMapNode* node = map->head; node != NULL; node = node->next)
    {
        fh = node->key;
        fhs[ind] = fh;
        ind++;
    }
    //search new file handler
    top = max(fhs[0], fhs, map_size) + 1;
    free(fhs);
    return top;
}

int fuseOpen(const char *path, struct fuse_file_info *fi)
{
    int map_size;
    int rc,ind,fh;
    char* RemotePath;
    IntMap* FhMap;
    FsData* fs;
    FileHandler file = {0};
    FileSession* session;

    //ファイルハンドラマップを取得
    fs = getFsData();
    FhMap = fs->FhMap;
    fh = newhandler(FhMap);

    RemotePath = patheditor(path);

    //ミラーファイルを参照
    file.mfile = search_mirror(getMirror(), RemotePath);
    if(file.mfile != NULL){
        rc = openMirrorFile(file.mfile); 
        if(rc < 0){
            free(RemotePath);
            return -ECANCELED;
        }
        /* ファイルハンドラの管理用マップ構造体へ登録 */
        insIntMap(FhMap, fh, &file, sizeof(FileHandler)); 
        fi->fh = fh;
        free(RemotePath);
        return 0;
    }
    
    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション
     * リモートファイルのオープン */
    session = connOpen(RemotePath, fi->flags);
    if(session == NULL)
    {
        free(RemotePath);
        return -ECANCELED;
    }
    file.session = session;
    /* リモートを参照ここまで*/

    //ミラーファイルをオーダーする
    request_mirror(getMirror(), RemotePath);
    free(RemotePath);

    /* ファイルハンドラの管理用マップ構造体へ登録 */
    insIntMap(FhMap, fh, &file, sizeof(FileHandler)); 
    fi->fh = fh;

    //レコード
    recordOperation(fs->record, path, oOPEN);

    return 0;  
}

int fuseRead(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    FsData* fs;
    IntMap* FhMap; 
    FileHandler* fh;
    int rc;
    char* RemotePath;

    //ファイルハンドラマップを取得
    fs = getFsData();
    FhMap = fs->FhMap;

    fh = getIntMap(FhMap, fi->fh);
    if(fh == NULL)
    {
        return -EBADFD;
    }

    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = readMirrorFile(fh->mfile, offset, size, buffer);
        if(rc < 0){
            printf("readMirrorFile fail\n");
            return -EBADFD;
        }
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connRead(fh->session, offset, buffer, size);
    if(rc < 0)
    {
        return -ENETDOWN;
    }
    /* リモートを参照ここまで*/

    //ミラーファイルをオーダーする
    RemotePath = patheditor(path);
    request_mirror(getMirror(), RemotePath);
    free(RemotePath);
    //オフセットの設定 
    fh->offset += rc;

    //レコード
    recordOperation(fs->record, path, oREAD);

    return rc;
}

int fuseWrite(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    FsData* fs;
    IntMap* FhMap;
    FileHandler* fh;
    char* RemotePath;
    int rc;

    //ファイルハンドラマップを取得
    fs = getFsData();
    FhMap = fs->FhMap;

    fh = getIntMap(FhMap, fi->fh);
    if(fh == NULL)
    {
        return -EBADFD;
    }
   
    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = writeMirrorFile(fh->mfile, offset, size, buffer);
        if(rc < 0){
            return -EBADFD;
        }
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connWrite(fh->session, offset, (void*)buffer, size);
    if(rc < 0)
    {
        return -ECANCELED;
    }
    /* リモートを参照ここまで*/

    //ミラーファイルをオーダーする
    RemotePath = patheditor(path);
    request_mirror(getMirror(), path);
    free(RemotePath);

    //オフセットの設定 
    fh->offset += rc;

    //レコード
    recordOperation(fs->record, path, oWRITE);

    return rc;    
}

int fuseRelease(const char *path, struct fuse_file_info *fi)
{
    FsData* fs;
    IntMap* FhMap;
    FileHandler* fh;
    int rc;

    //ファイルハンドラマップを取得
    fs = getFsData();
    FhMap = getFhMap();

    fh = getIntMap(FhMap, fi->fh);
    if(fh == NULL)
    {
        return -EBADFD;
    }
   
    //ミラーファイルが参照できる
    if(fh->mfile != NULL){
        rc = closeMirrorFile(fh->mfile);
        if(rc < 0){
            return -EBADFD;
        }
        //対象ファイルのFileHandlerをfhMapから削除して解放
        delIntMap(FhMap, fi->fh);
        return rc;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
    //通信呼び出し
    rc = connClose(fh->session);
    if(rc < 0){
        return -ECANCELED;
    }
    /* リモートを参照ここまで*/

    //対象ファイルのFileHandlerをfhMapから削除して解放
    delIntMap(FhMap, fi->fh);

    //レコード
    recordOperation(fs->record, path, oCLOSE);
   
    return 0;    
}

int fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    List* attrs;
    char* RemotePath;
    Attribute* attr;
    
    RemotePath = patheditor(path);
    attrs = connReaddir(RemotePath); 
    if(attrs == NULL)
    {
        free(RemotePath);
        return -ECANCELED;
    }
    for(Node* node = attrs->head; node != NULL; node = node->next)
    {
        attr = node->data;
        filler(buf, attr->path, &(attr->st), 0, FUSE_FILL_DIR_PLUS);
    }
    freeList(attrs, NULL);
    free(RemotePath);

    //レコード
    //recordOperation(fs->record, path, OPEN);

    return 0;    
}

void* fuseInit(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    return NULL;
}

void fuseDestory(void *private_data)
{
    FsData* fs;

    fs = private_data;
    freeIntMap(fs->FhMap);
    free(fs);
}

off_t fuseLseek(const char *path, off_t offset, int whence, struct fuse_file_info *fi)
{
    IntMap* FhMap;
    FileHandler* fh;
    int rc;

    //ファイルハンドラマップを取得
    FhMap = getFhMap();

    fh = getIntMap(FhMap, fi->fh);
    if(fh == NULL)
    {
        return -EBADFD;
    }

    /* キャッシュ・ローカルには見つからなくてリモートを参照するセクション*/
   
    fh->offset = offset;
    return fh->offset;
}

static struct fuse_operations fuseOper = 
{
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

int main(int argc, char* argv[])
{
    Args* args;
    FsData* fs;
    int i, new_argc;
    char* new_argv[10];

    if(argc < 4)
    {
        printf("RemoteFs [mountpoint] [remoteroot] [ssh.config]\n");
        exit(EXIT_FAILURE);
    }

    //Argsの取得
    args = getArgs(argv[2], argv[3]);

    //FsDataの初期化
    fs = getFsData();

    for (i=0, new_argc=0; (i<argc) && (new_argc<10); i++)
    {
        if((i != 2) & (i != 3))
        {
            printf("%s\n",argv[i]);
            new_argv[new_argc++] = argv[i];
        }
    }
    
    return fuse_main(new_argc, new_argv, &fuseOper, NULL);
}
