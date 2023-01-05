#define FUSE_USE_VERSION 31

#include "fuse-system.h"

#include "filesystem.h"
#include <fuse3/fuse.h>
#include <sys/stat.h>
#include <cstring>

#include <iostream>


FileSystem* fs;

int fs_getattr(const char *path,struct stat *stbuf, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return fs->getattr_(path, *stbuf);
}

int fs_open(const char *path, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    int fd = fs->open_(path, fi->flags);
    if(fd < 0)
        return fd;
    fi->fh = fd;
    return 0;
}

int fs_read(const char *path, char *buf, size_t size,
        off_t offset, fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return fs->read_(fi->fh, buf, offset, size);
}

int fs_write(const char *path, const char *buf, size_t size,
        off_t offset, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return fs->write_(fi->fh, buf, offset, size);
}

int fs_release(const char *path, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return fs->close_(fi->fh);
}

int fs_opendir(const char *path, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return 0;
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t off, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags){
    std::cout << __FUNCTION__ << std::endl;
    std::vector<direntstat> dirents;
    int rc = fs->readdir_(path, dirents);
    if(rc == 0){
        for(auto& di : dirents){
            filler(buf, di.entry.d_name, &di.st, 0, FUSE_FILL_DIR_PLUS); 
        }
        return 0;
    }else{
        return rc;
    }
}

int fs_releasedir(const char *path, struct fuse_file_info *fi){
    std::cout << __FUNCTION__ << std::endl;
    return 0;
}

void *fs_init(struct fuse_conn_info *conn, struct fuse_config *conf){
    std::cout << __FUNCTION__ << std::endl;
    return nullptr;
}

static struct fuse_operations fs_ops = {
    .getattr = fs_getattr,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .release = fs_release,
    .opendir = fs_opendir,
    .readdir = fs_readdir,
    .releasedir = fs_releasedir,
    .init = fs_init,
};

int fusemain(FileSystem& fs_ref, int argc, char *argv[]){
    int new_argc{0};
    char* new_args[10];

    fs = &fs_ref;

    for(int i=0;i<argc && new_argc<10;i++){
        if(static_cast<std::string>(argv[i]) == "--config"){
            i++;
            fs->init_(argv[i]);
        }else{
            new_args[new_argc] = argv[i];
            new_argc++;
        }
    }

    new_args[new_argc] = strdup(fs->mount_point.c_str());
    new_argc++;

    return fuse_main(new_argc, new_args, &fs_ops, NULL);
}
