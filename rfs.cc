#include "rfs.h"
#include "common.h"
#include "filesystem.h"
#include "fuse-system.h"

#include <dirent.h>
#include <string>

RemoteFileSystem::RemoteFileSystem(): FileSystem() {
}

RemoteFileSystem::~RemoteFileSystem(){
}

int RemoteFileSystem::init_(std::string config_path){
    MapEditor me;
    me.load(config_path);
    mount_point = me.maps.at("mount_point").front();
    remote_root = me.maps.at("remote_root").front();
    std::string ip = me.maps.at("server_ip").front(); 
    short port = std::stoi(me.maps.at("server_port").front());
    cache.init(me.maps.at("metacache").front());
    ftp.init(ip, port);
    return 0;
}

int RemoteFileSystem::getattr_(std::string path, struct stat& stbuf){
    path = to_real_path(path, remote_root);
    //printf("%s\n", path.c_str());
    if(cache.getattr_(path, stbuf) < 0){
        int rc = ftp.getattr_(path, stbuf);
        if(rc < 0)
            return rc;
        cache.update_stat(path, stbuf);
    }else{
        //printf("%s cache exists\n", path.c_str());
    }
    return 0;
}

int RemoteFileSystem::readdir_(std::string path, 
        std::vector<struct direntstat>& dirents){
    path = to_real_path(path, remote_root);
    if(cache.ls_(path, dirents) < 0){
        dirents.clear();
        int rc = ftp.ls_(path, dirents);
        if(rc < 0)
            return rc;
        cache.update_ls(path, dirents);
    }else{
        printf("%s cache exists\n", path.c_str());
    }
    return 0;
}

int RemoteFileSystem::open_(std::string path, int flags){
    path = to_real_path(path, remote_root);
    return ftp.open_(path);
}

int RemoteFileSystem::close_(int fd){
    return ftp.close_(fd);
}

int RemoteFileSystem::read_(int fd, char* buffer, 
        int offset, int size){
    return ftp.read_(fd, offset, size, buffer);
}

int RemoteFileSystem::write_(int fd, const char* buffer, 
        int offset, int size){
    return ftp.write_(fd, offset, size, buffer);
}

#ifdef RFS
int main(int argc, char** argv){
    RemoteFileSystem fs;
    return fusemain(fs, argc, argv);
}
#endif
