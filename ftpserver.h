#pragma once
#include "connection.h"
#include "ftp-type.h"

#include <list>
#include <map>
#include <netinet/in.h>
#include <string>
#include <mutex>

class FtpServer: public SocketTask{
    private:
        std::list<in_addr_t> host_list;
        std::mutex mtx_;
    private:
        in_addr_t get_in_addr(int sd);
        bool auth_host(int sd);
        int echoback_(int sd);
        int getattr_(int sd);
        int readdir_(int sd);
        int ls_(int sd);
        int open_(int sd);
        int close_(int sd);
        int read_(int sd);
        int write_(int sd);
        int create_(int sd);
    public:
        FtpServer(std::list<in_addr_t> host_list_):host_list{host_list_}{}
        ~FtpServer() override;
        const int run(int socket) override;
};
