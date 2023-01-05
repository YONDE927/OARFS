#pragma once

#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <memory>
#define MSG_LEN 32
#define PATH_LEN 256

namespace ftp{
    enum requestType{
        echoback,
        getattr,
        setattr,
        readdir,
        ls,
        open,
        close,
        read,
        write,
        create,
        download,
        upload
    };

    struct echobackDatagram{
        char msg[MSG_LEN];
    };

    struct baseReq{
        requestType reqtype;
    };

    struct getattrReq: public baseReq{
        char path[PATH_LEN];
        getattrReq(): baseReq{getattr}{};
    };

    struct getattrRes{
        int errno_;
    };

    struct readdirReq: public baseReq{
        char path[PATH_LEN];
        readdirReq(): baseReq{readdir}{};
    };

    struct readdirRes{
        int errno_;
        int ndirent;
    };

    struct lsReq: public baseReq{
        char path[PATH_LEN];
        lsReq(): baseReq{ls}{};
    };

    struct lsRes{
        int errno_;
        int nentry;
    };

    struct openReq: public baseReq{
        char path[PATH_LEN];
        openReq(): baseReq{open}{};
    };

    struct openRes{
        int errno_;
        int fd;
    };

    struct closeReq: public baseReq{
        int fd;
        closeReq(): baseReq{close}{};
    };

    struct closeRes{
        int errno_;
    };

    struct writeReq: public baseReq{
        int fd;
        int offset;
        int size;
        writeReq(): baseReq{write}{};
    };

    struct writeRes{
        int errno_;
    };

    struct readReq: public baseReq{
        int fd;
        int offset;
        int size;
        readReq(): baseReq{read}{};
    };

    struct readRes{
        int errno_;
        int size;
    };

    struct createReq: public baseReq{
        char path[PATH_LEN];
        createReq(): baseReq{create}{};
    };

    struct createRes{
        int errno_;
    };

    struct downloadReq: public baseReq{
        char from[PATH_LEN];
        char to[PATH_LEN];
        downloadReq(): baseReq{download}{};
    };

    struct downloadRes{
        int errno_;
    };

    struct uploadReq: public baseReq{
        char from[PATH_LEN];
        char to[PATH_LEN];
        uploadReq(): baseReq{upload}{};
    };

    struct uploadRes{
        int errno_;
    };
}
