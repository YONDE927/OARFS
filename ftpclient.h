#pragma once
#include "connection.h"
#include "ftp-type.h"
#include "common.h"

#include <netinet/in.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <sys/stat.h>
#include <dirent.h>

class FtpClient{
    protected:
        Client client_;
        std::mutex mtx_;
    public:
        FtpClient(std::string ip, short port);
        FtpClient();
        virtual ~FtpClient();
        virtual int init(std::string ip, short port);
        int echoback_(std::string msg);
        int getattr_(std::string path, struct stat& stbuf);
        int readdir_(std::string path, std::vector<dirent>& dirents);
        int ls_(std::string path, std::vector<direntstat>& entries);
        int open_(std::string path);
        int close_(int fd);
        int read_(int fd, int offset, int size,
                char* buffer);
        int write_(int fd, int offset, int size,
                const char* buffer);
        int create_(std::string path);
};

class TryFtpClient: public FtpClient{
    private:
        std::vector<std::shared_ptr<ftp::baseReq>> unsend_reqs;
        std::condition_variable cond_;
        std::thread sending_thread;
        bool term{false};
        struct trywriteReq: public ftp::writeReq{
            std::shared_ptr<char> buffer;
        };
    private:
        int switch_req(std::shared_ptr<ftp::baseReq> req);
        void sending_task();
    public:
        TryFtpClient(std::string ip, short port);
        TryFtpClient();
        ~TryFtpClient();
        int init(std::string ip, short port)override;
        int run(std::string ip, short port);
        int eopen_(std::string path, bool do_resend);
        int eclose_(int fd, bool do_resend);
        int ewrite_(int fd, int offset, int size,
                const char* buffer, bool do_resend);
        int ecreate_(std::string path, bool do_resend);
        void print_unsend_reqs();
};
