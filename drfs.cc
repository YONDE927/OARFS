#include "drfs.h"
#include "common.h"
#include "fuse-system.h"

#include <asm-generic/errno.h>
#include <dirent.h>
#include <memory>
#include <string>
#include <cstring>

drfs_write_task::drfs_write_task(RemoteFileSystem* rfs_,
                int fd_, int offset_, int size_,
                std::shared_ptr<char> buffer_){
    rfs = rfs_;
    fd = fd_;
    offset = offset_;
    size = size_;
    buffer = buffer_;
}

int drfs_write_task::exec_(){
    printf("%s\n", __FUNCTION__);
    if(rfs == nullptr)
        return 0;
    int rc = rfs->write_(fd, buffer.get(), offset, size);
    if(rc == -ENETDOWN)
        return -1;
    return 0;
}
drfs_close_task::drfs_close_task(RemoteFileSystem* rfs_, int fd_){
    rfs = rfs_;
    fd = fd_;
}

int drfs_close_task::exec_(){
    printf("%s\n", __FUNCTION__);
    if(rfs == nullptr)
        return 0;
    int rc = rfs->close_(fd);
    if(rc == -ENETDOWN)
        return -1;
    return 0;
}

DelayRemoteFileSystem::DelayRemoteFileSystem(): FileSystem() {
}

DelayRemoteFileSystem::~DelayRemoteFileSystem(){
    dtm.stop();
}

int DelayRemoteFileSystem::init_(std::string config_path){
    MapEditor me;
    me.load(config_path);
    mount_point = me.maps.at("mount_point").front();
    rfs.init_(config_path);
    dtm.run();
    return 0;
}

int DelayRemoteFileSystem::getattr_(std::string path, struct stat& stbuf){
    return rfs.getattr_(path, stbuf);
}

int DelayRemoteFileSystem::readdir_(std::string path, 
        std::vector<struct direntstat>& dirents){
    return rfs.readdir_(path, dirents);
}

int DelayRemoteFileSystem::open_(std::string path, int flags){
    return rfs.open_(path, flags);
}

int DelayRemoteFileSystem::close_(int fd){
    int rc{0};
    rc = rfs.close_(fd);
    if(rc == -ENETDOWN){
        std::shared_ptr<drfs_close_task> task =
            std::make_shared<drfs_close_task>(&rfs, fd);
        dtm.add_task(task);
        return 0;
    }
    return rc;
}

int DelayRemoteFileSystem::read_(int fd, char* buffer, 
        int offset, int size){
    return rfs.read_(fd, buffer, offset, size);
}

int DelayRemoteFileSystem::write_(int fd, const char* buffer, 
        int offset, int size){
    int rc = rfs.write_(fd, buffer, offset, size);
    if(rc == -ENETDOWN){
        std::shared_ptr<char> tbuf(new char[size]); 
        std::memcpy(tbuf.get(), buffer, size);
        std::shared_ptr<drfs_write_task> task =
            std::make_shared<drfs_write_task>(&rfs, fd, 
                    offset, size, tbuf);
        dtm.add_task(task);
        return 0;
    }
    return rc;
}

#ifdef DRFS 
int main(int argc, char** argv){
    DelayRemoteFileSystem fs;
    return fusemain(fs, argc, argv);
}
#endif
