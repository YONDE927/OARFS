#include "connection.h"

#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <thread>
#include <system_error>

#include <unistd.h>
#include <sys/socket.h>

void avoid_sigpipe(){
    signal(SIGPIPE, SIG_IGN);
}

void set_timeout(int socket, int time){
    struct timeval timeout;      
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
}

SocketTask::~SocketTask(){
}

const int SocketTask::run(int sd){
    int i = 1;
    int j = 0;
    send(sd, &i, sizeof(i), 0);
    recv(sd, &j, sizeof(j), 0);
    close(sd);
    std::cout << "recv: " << j << std::endl;
    return 0;
}

Server::Server(std::string ip, short port){
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_.sin_port = htons(port);
    avoid_sigpipe();
}

Server::~Server(){
}

void Server::run(SocketTask& task){
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_socket < 0){
        std::cout << __FUNCTION__ << " "  << strerror(errno) << std::endl;
        return;
    }
    //socket config
    int yes{1};
    if(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
                (const char *)&yes, sizeof(yes)) < 0){return;}
    //bind socket
    if(bind(listen_socket, (struct sockaddr*)&addr_, sizeof(addr_)) < 0){
        std::cout << __FUNCTION__ << " " << strerror(errno) << std::endl;
        return;
    }

    if(listen(listen_socket, 5) < 0){
        std::cout << __FUNCTION__ << strerror(errno) << std::endl;
        return;
    }

    socklen_t addr_len = sizeof(sockaddr_in);
    for(;;){
        sockaddr_in client_addr;
        int client_socket = accept(listen_socket,
                (struct sockaddr*)&client_addr, &addr_len);
        if(client_socket < 0){
            std::error_code ec(errno, std::generic_category());
            std::cout << "accept error happen: " << ec.message() << std::endl; 
            continue;
        }

        auto client_thread = std::thread([&](){task.run(client_socket);});

        client_thread.detach();
    }
}

Client::Client(std::string ip, short port){
    init(ip, port);
}

Client::Client(){
}

Client::~Client(){
}

int Client::init(std::string ip, short port){
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_.sin_port = htons(port);
    avoid_sigpipe();
    return 0;
}

int Client::run(SocketTask& task){
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0){return -1;}

    if(connect(sd, (struct sockaddr*)&addr_, sizeof(addr_)) < 0){
        std::error_code ec(errno, std::generic_category());
        std::cout << "connect error happen: " << ec.message() << std::endl; 
        return -1; 
    }

    return task.run(sd);
}

int Client::conn(){
    if(sd < 0){
        sd = socket(AF_INET, SOCK_STREAM, 0);
        if(sd < 0){return -1;}

        //set timeout
        set_timeout(sd,3);

        if(connect(sd, (struct sockaddr*)&addr_, sizeof(addr_)) < 0){
            return -errno; 
        }
    }
    return sd;
}

int Client::close_socket(){
    if(sd >= 0){
        close(sd);
        sd = -1;
    }
    return 0;
}
