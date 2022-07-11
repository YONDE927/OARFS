#pragma once
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

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


