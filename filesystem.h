#pragma once

#include <sys/types.h>
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>

#include <linux/limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <map>

#include "common.h"

class FileSystem {
    public:
        std::string mount_point;
        std::string local_root;
        std::map<int, std::string> fd_map;
    public:
        virtual ~FileSystem();
        virtual int init_(std::string config_path);
        virtual int getattr_(std::string path, struct stat& stbuf);
        virtual int readdir_(std::string path, std::vector<struct direntstat>& dirents);
        virtual int open_(std::string path, int flags);
        virtual int close_(int fd);
        virtual int read_(int fd, char* buffer, int offest, int size);
        virtual int write_(int fd, const char* buffer, int offest, int size);
    public:
        pid_t get_op_pid();
};
