#include "filesystem.h"
#include "fuse-system.h"

#include "common.h"
#include <cerrno>
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

FileSystem::~FileSystem(){}

int FileSystem::init_(std::string config_path){
    MapEditor me;
    me.load(config_path);
    mount_point = me.maps.at("mount_point").front();
    local_root = me.maps.at("local_root").front();
    return 0;
}

int FileSystem::getattr_(std::string path, struct stat& stbuf){
    path = to_real_path(path, local_root);
    if(lstat(path.c_str(), &stbuf) < 0)
        return -errno;
    return 0;
}

int FileSystem::readdir_(std::string path, 
        std::vector<struct direntstat>& dirents){
    path = to_real_path(path, local_root);
    DIR* dirp;
    dirp = opendir(path.c_str());
    if(dirp == nullptr)
        return -errno;
    dirent* dep;
    for(;;){
        direntstat ds;
        dep = readdir(dirp);
        if(dep == nullptr){
            break;
        }else{
            ds.entry = *dep;
            lstat(dep->d_name, &ds.st);
            dirents.push_back(ds);
        }
    }
    closedir(dirp);
    return 0;
}

int FileSystem::open_(std::string path, int flags){
    path = to_real_path(path, local_root);
    int fd = open(path.c_str(), flags);
    if(fd < 0)
        return -errno;
    return fd;
}

int FileSystem::close_(int fd){
    if(close(fd) < 0)
        return -errno;
    return 0;
}

int FileSystem::read_(int fd, char* buffer, 
        int offest, int size){
    if(lseek(fd, offest, SEEK_SET) < 0)
        return -errno;
    int rc = read(fd, buffer, size);
    if(rc < 0)
        return -errno;
    return rc;
}

int FileSystem::write_(int fd, const char* buffer, 
        int offest, int size){
    if(lseek(fd, offest, SEEK_SET) < 0)
        return -errno;
    int rc = write(fd, buffer, size);
    if(rc < 0)
        return -errno;
    return rc;
}

pid_t FileSystem::get_op_pid(){
    struct fuse_context* context = fuse_get_context();
    if(context == nullptr)
        return -1;
    return context->pid;
}

#ifdef PASSTHROUGH
int main(int argc, char** argv){
    FileSystem fs;
    return fusemain(fs, argc, argv);
}
#endif
