#pragma once

#include "filesystem.h"
#include "rfs.h"
#include "delaytask.h"

#include <memory>

class drfs_write_task: public DelayTask {
    public:
        RemoteFileSystem* rfs;
        int fd;
        int offset;
        int size;
        std::shared_ptr<char> buffer;
    public:
        drfs_write_task(RemoteFileSystem* rfs_,
                int fd_, int offset_, int size_,
                std::shared_ptr<char> buffer_);
        int exec_() override;
};

class drfs_close_task: public DelayTask {
    public:
        RemoteFileSystem* rfs;
        int fd;
    public:
        drfs_close_task(RemoteFileSystem* rfs_, int fd_);
        int exec_() override;
};

class DelayRemoteFileSystem : public FileSystem {
    public:
        RemoteFileSystem rfs;
        DelayTaskManager dtm;
    public:
        DelayRemoteFileSystem();
        ~DelayRemoteFileSystem();
        int init_(std::string config_path) override;
        int getattr_(std::string path, struct stat& stbuf) override;
        int readdir_(std::string path, 
                std::vector<struct direntstat>& dirents) override;
        int open_(std::string path, int flags) override;
        int close_(int fd) override;
        int read_(int fd, char* buffer, int offest, int size) override;
        int write_(int fd, const char* buffer, 
                int offest, int size) override;
};
