#include "mfs.h"
#include "common.h"
#include "fuse-system.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cstddef>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <string>
#include <cstring>
#include <utility>

MirrorFileSystem::MirrorFileSystem(): FileSystem() {
}

MirrorFileSystem::~MirrorFileSystem(){
    mirror_thread.join();
    close_pipes();
}

int MirrorFileSystem::init_(std::string config_path){
    MapEditor me;
    me.load(config_path);
    mount_point = me.maps.at("mount_point").front();
    mirror_root = me.maps.at("mirror_root").front();
    record_db = me.maps.at("record_db").front();
    recorder.init(record_db);
    drfs.init_(config_path);
    open_pipes();
    mirror_thread = std::thread(&MirrorFileSystem::pipe_reader, this); 
    return 0;
}

int MirrorFileSystem::getattr_(std::string path, struct stat& stbuf){
    if(stat_mirror(path, stbuf) == 0)
        return 0;
    return drfs.getattr_(path, stbuf);
}

int MirrorFileSystem::readdir_(std::string path, 
        std::vector<struct direntstat>& dirents){
    int rc = drfs.readdir_(path, dirents);
    if(rc == -ENETDOWN){
        dirents.clear();
        if(readdir_mirror(path, dirents) < 0)
            return rc;
    }
    return 0;
}

//drfsとmfsのfd競合を防ぐ
int MirrorFileSystem::open_(std::string path, int flags){
    struct stat stbuf;
    mfs_fh fh;
    fh.path = path;
    int fd = new_map_index(mfs_fh_map);
    if(stat_mirror(path, stbuf) == 0){
        std::string mpath = get_mirror_path(path);
        fh.fd = open(mpath.c_str(), O_RDWR);
        if(fh.fd < 0)
            return fh.fd;
        fh.is_drfs = false;
    }else{
        fh.fd = drfs.open_(path, flags);
        if(fh.fd < 0)
            return fh.fd;
        fh.is_drfs = true;
    }
    mfs_fh_map.emplace(std::make_pair(fd, fh));
    recorder.add_history(path, "open", get_op_pid());
    return fd;
}

int MirrorFileSystem::close_(int fd){
    int rc{0};
    if(mfs_fh_map.find(fd) != mfs_fh_map.end()){
        mfs_fh& fh = mfs_fh_map.at(fd);
        if(fh.is_drfs){
            rc = drfs.close_(fd);
        }else{
            rc = close(fh.fd);
            if(rc < 0)
                return -errno;
        }
        mfs_fh_map.erase(fd);
        recorder.add_history(fh.path, "close", get_op_pid());
        return 0;
    }else{
        return -EBADF;
    }
    return rc;
}

int MirrorFileSystem::read_(int fd, char* buffer, 
        int offset, int size){
    if(mfs_fh_map.find(fd) != mfs_fh_map.end()){
        mfs_fh& fh = mfs_fh_map.at(fd);
        recorder.add_history(fh.path, "read", get_op_pid());
        if(fh.is_drfs){
            return drfs.read_(fd, buffer, offset, size);
        }else{
            lseek(fh.fd, offset, SEEK_SET);
            int rc = read(fh.fd, buffer, size);
            if(rc < 0)
                return -errno;
            return rc;
        }
    }else{
        return -EBADF;
    }
}

int MirrorFileSystem::write_(int fd, const char* buffer, 
        int offset, int size){
    if(mfs_fh_map.find(fd) != mfs_fh_map.end()){
        mfs_fh& fh = mfs_fh_map.at(fd);
        recorder.add_history(fh.path, "write", get_op_pid());
        if(fh.is_drfs){
            return drfs.write_(fd, buffer, offset, size);
        }else{
            lseek(fh.fd, offset, SEEK_SET);
            int rc = write(fh.fd, buffer, size);
            if(rc < 0)
                return -errno;
            return rc;
        }
    }else{
        return -EBADF;
    }
}

const char* pipe_dir = "./pipes";
const char* add_pipe_path = "./pipes/add";
const char* delete_pipe_path = "./pipes/delete";
const char* pull_pipe_path = "./pipes/pull";
const char* push_pipe_path = "./pipes/push";
char working_dir[256];

std::string MirrorFileSystem::get_mirror_path(std::string path){
    return to_real_path(path, mirror_root);
}

int MirrorFileSystem::open_pipes(){
    mkdir(pipe_dir, 0700);
    mkfifo(add_pipe_path, 0600);
    mkfifo(delete_pipe_path, 0600);
    mkfifo(pull_pipe_path, 0600);
    mkfifo(push_pipe_path, 0600);
    pipe_fds[0] = open(pull_pipe_path, O_RDWR); 
    if(pipe_fds[0] < 0)
        printf("%s\n", strerror(errno));
    pipe_fds[1] = open(push_pipe_path, O_RDWR);
    if(pipe_fds[1] < 0)
        printf("%s\n", strerror(errno));
    pipe_fds[2] = open(add_pipe_path, O_RDWR); 
    if(pipe_fds[2] < 0)
        printf("%s\n", strerror(errno));
    pipe_fds[3] = open(delete_pipe_path, O_RDWR); 
    if(pipe_fds[3] < 0)
        printf("%s\n", strerror(errno));
    return 0;
}

int MirrorFileSystem::close_pipes(){
    for(int i = 0; i < 4; i++){
        close(pipe_fds[i]);
    }
    return 0;
}

std::string read_string(int fd){
    char word;
    std::string output;
    for(;;){
        if(read(fd, &word, 1) != 1)
            break;
        if(word == '\0' | word == '\n')
            break;
        output.push_back(word);
    }
    return output;
}

int MirrorFileSystem::pipe_switch(int fd){
    std::string path = read_string(fd);
    if(fd == pipe_fds[0]){
        printf("pull %s\n", path.c_str());
        int rc = pull_(path);
        if(rc < 0)
            printf("mirror task error [pull]: %s\n", strerror(errno));
    }
    if(fd == pipe_fds[1]){
        printf("push %s\n", path.c_str());
        int rc = push_(path);
        if(rc < 0)
            printf("mirror task error [push]: %s\n", strerror(errno));
    }
    if(fd == pipe_fds[2]){
        printf("add %s\n", path.c_str());
        int rc = add_(path);
        if(rc < 0)
            printf("mirror task error [add]: %s\n", strerror(errno));
    }
    if(fd == pipe_fds[3]){
        printf("delete %s\n", path.c_str());
        int rc = delete_(path);
        if(rc < 0)
            printf("mirror task error [delete]: %s\n", strerror(errno));
    }
    return 0;
}

int MirrorFileSystem::pipe_switch_test(int fd){
    if(fd == pipe_fds[0])
        printf("pull %s\n", read_string(fd).c_str());
    if(fd == pipe_fds[1])
        printf("push %s\n", read_string(fd).c_str());
    if(fd == pipe_fds[2])
        printf("add %s\n", read_string(fd).c_str());
    if(fd == pipe_fds[3])
        printf("delete %s\n", read_string(fd).c_str());
    return 0;
}

int MirrorFileSystem::pipe_reader(){
    struct epoll_event ev;
    int epollfd = epoll_create1(0);
    for(int i = 0; i < 4; i++){
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = pipe_fds[i];
        epoll_ctl(epollfd, EPOLL_CTL_ADD, pipe_fds[i], &ev);
    }
    for(;;){
        //read pipe
        int nfds = epoll_wait(epollfd, events, 10, -1);
        for(int i=0; i < nfds; i++){
            int fd = events[i].data.fd;
            pipe_switch(fd);
        }
    }
    return 0;
}

int MirrorFileSystem::stat_mirror(std::string path, 
        struct stat& stbuf){
    path = get_mirror_path(path);
    if(lstat(path.c_str(), &stbuf) < 0)
        return -1;
    return 0;
}

int MirrorFileSystem::readdir_mirror(std::string path, 
        std::vector<struct direntstat>& dirents){
    DIR* dir = opendir(path.c_str());
    if(dir == nullptr)
        return -errno;
    struct dirent* entry;
    struct direntstat attr;
    for(;;){
        entry = readdir(dir);
        if(entry == nullptr)
            break;
        attr.entry = *entry;
        std::string child = to_real_path(entry->d_name, path);
        if(lstat(child.c_str(), &attr.st) < 0)
            break;
        dirents.push_back(attr);
    }
    return 0;
}

std::string get_download_path(std::string path){
    const char* download_flag = ".download.";
    std::string dir = getdirpath(path);
    std::string file = filename(path);
    return to_real_path(download_flag + file, dir);
}

char buffer[DL_CHUNK_SIZE];

int MirrorFileSystem::pull_(std::string path){
    printf("%s\n", __FUNCTION__);
    std::string dl_path = get_download_path(path);
    dl_path = get_mirror_path(dl_path);
    std::string mirror_path = get_mirror_path(path);
    struct stat stbuf;
    //check mirror file
    int rc = drfs.rfs.getattr_(path, stbuf);
    if(rc < 0)
        return rc;
    if(!S_ISREG(stbuf.st_mode))
        return -EINVAL;
    //open mirror file
    std::string dir = getdirpath(dl_path);
    makedir(dir);
    size_t filesize = stbuf.st_size;
    int fd = open(dl_path.c_str(), O_WRONLY|O_CREAT, 0600);
    if(fd < 0)
        return -errno;
    //open remote file
    int rfd = drfs.rfs.open_(path, O_RDONLY);
    if(rfd < 0){
        close(fd);
        remove(dl_path.c_str());
        return rfd;
    }
    //read loop
    int offset = 0;
    while(filesize > 0){
        int read_size = min(filesize, DL_CHUNK_SIZE);
        memset(buffer, 0, DL_CHUNK_SIZE);
        //download
        int dsize = drfs.rfs.read_(rfd, buffer, offset, read_size);
        if(dsize < 0){
            close(fd);
            remove(dl_path.c_str());
            drfs.rfs.close_(rfd);
            return dsize;
        }
        //write
        if(write(fd, buffer, dsize) < 0){
            close(fd);
            remove(dl_path.c_str());
            drfs.rfs.close_(rfd);
            return -errno;
        }
        offset += dsize;
        filesize -= dsize;
    }
    close(fd);
    drfs.rfs.close_(rfd);
    rename(dl_path.c_str(), mirror_path.c_str());
    return 0; 
}

int MirrorFileSystem::push_(std::string path){
    printf("%s\n", __FUNCTION__);
    struct stat stbuf;
    if(stat_mirror(path, stbuf) < 0)
        return -ENOENT;
    //open mirror file
    std::string mirror_path = get_mirror_path(path);
    size_t filesize = stbuf.st_size;
    int fd = open(mirror_path.c_str(), O_RDONLY);
    if(fd < 0)
        return -errno;
    //open remote file
    int rfd = drfs.rfs.open_(path, O_WRONLY|O_CREAT);
    if(rfd < 0){
        close(fd);
        return rfd;
    }
    //write loop
    int offset = 0;
    char buffer[DL_CHUNK_SIZE];
    while(filesize > 0){
        int write_size = min(filesize, DL_CHUNK_SIZE);
        memset(buffer, 0, DL_CHUNK_SIZE);
        //read
        if(read(fd, buffer, write_size) < 0){
            close(fd);
            drfs.rfs.close_(rfd);
            return -errno;
        }
        //upload
        int usize = drfs.rfs.write_(rfd, buffer, offset, write_size);
        if(usize < 0){
            close(fd);
            drfs.rfs.close_(rfd);
            return usize;
        }
        offset += write_size;
        filesize -= write_size;
    }
    close(fd);
    drfs.rfs.close_(rfd);
    return 0; 
}

int MirrorFileSystem::add_(std::string path){
    printf("%s\n", __FUNCTION__);
    struct stat stbuf;
    if(stat_mirror(path, stbuf) < 0){
        return pull_(path);
    }
    return 0;
}

int MirrorFileSystem::delete_(std::string path){
    printf("%s\n", __FUNCTION__);
    std::string mirror_path = get_mirror_path(path);
    remove(mirror_path.c_str());
    return 0;
}

//#define MFS
#ifdef MFS
int main(int argc, char** argv){
    int nargc = 5;
    char* nargv[] = {
        "./mfs",
        "--config",
        "../mfsconfig",
        "-s",
        "-f"
    };
    MirrorFileSystem fs;

    return fusemain(fs, nargc, nargv);
}
#endif
