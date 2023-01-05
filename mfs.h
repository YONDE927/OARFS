//mirrorファイルいつ同期するか問題
//それ以外はまずまず動いている

#pragma once

#include <queue>
#include <map>
#include <thread>
#include <sys/epoll.h>
#include "common.h"
#include "filesystem.h"
#include "drfs.h"
#include "recorder.h"

#define DL_CHUNK_SIZE 640000

class MirrorFileSystem : public FileSystem {
    public:
        std::string mirror_root;
        std::string record_db;
        DelayRemoteFileSystem drfs;
        Recorder recorder;
    public:
        MirrorFileSystem();
        ~MirrorFileSystem();
        int init_(std::string config_path) override;
        int getattr_(std::string path, struct stat& stbuf) override;
        int readdir_(std::string path, 
                std::vector<struct direntstat>& dirents) override;
        int open_(std::string path, int flags) override;
        int close_(int fd) override;
        int read_(int fd, char* buffer, int offset, int size) override;
        int write_(int fd, const char* buffer, 
                int offset, int size) override;
    //mirror ops
    private:
        struct mfs_fh{
            int fd;
            bool is_drfs;
            std::string path;
        };
        std::map<int, mfs_fh> mfs_fh_map;
        int pipe_fds[4];
        struct epoll_event events[10];
        std::thread mirror_thread;
    public:
        std::string get_mirror_path(std::string path);
        int open_pipes();
        int close_pipes();
        int pipe_switch(int fd);
        int pipe_switch_test(int fd);
        int pipe_reader();
        int stat_mirror(std::string path, struct stat& stbuf);
        int readdir_mirror(std::string path, 
                std::vector<struct direntstat>& dirents);
        int pull_(std::string path);
        int push_(std::string path);
        int add_(std::string path);
        int delete_(std::string path);
        void trypull_(std::string path);
        void trypush_(std::string path);
        void tryadd_(std::string path);
        void trydelete_(std::string path);
};
