#pragma once

#include <netinet/in.h>
#include <string>
#include <csignal>

#include <arpa/inet.h>

void avoid_sigpipe();

enum resType{
    OK,
    ERROR
};

class SocketTask{
    public:
        virtual ~SocketTask();
        virtual const int run(int sd);
};

class Server{
    private:
        sockaddr_in addr_;
    public:
        Server(std::string ip, short port);
        ~Server();
        void run(SocketTask& task);
};

class Client{
    private:
        sockaddr_in addr_;
        int sd{-1};
    public:
        Client(std::string ip, short port);
        Client();
        ~Client();
        int init(std::string ip, short port);
        int run(SocketTask& task);
        int conn();
        int close_socket();
};
