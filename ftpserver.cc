#include "ftpserver.h"

#include "common.h"
#include "ftp-type.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

in_addr_t FtpServer::get_in_addr(int sd){
    sockaddr_in addr;
    socklen_t socklen = sizeof(addr);
    if(getpeername(sd, (sockaddr*)&addr, &socklen) < 0){
        return -1;
    }else{
        return addr.sin_addr.s_addr;
    }
}

bool FtpServer::auth_host(int sd){
    in_addr_t addr = get_in_addr(sd);
    if(addr < 0){
        return false;
    }else{
        for(auto host_addr : host_list){
            if(addr == host_addr){
                return true;
            }
        }
    }
    return false;
}

int FtpServer::echoback_(int sd){
    //echobackReqの受取
    ftp::echobackDatagram dg;
    int dg_size = sizeof(dg);
    if(recv(sd, &dg, dg_size, MSG_WAITALL) != dg_size)
        return -1;
    //echobackの送信
    if(send(sd, &dg, dg_size, 0) != dg_size)
        return -1;
    return 0;
}

int FtpServer::getattr_(int sd){
    //getattrReqの受取
    ftp::getattrReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req))
        return -1;
    //lstatの実行
    struct stat stbuf{0};
    int err_code{0};
    if(lstat(req.path, &stbuf) < 0)
        err_code = errno;
    //getattrResの送信
    //printf("errcode = %d\n", err_code);
    ftp::getattrRes res{err_code};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    //statの送信
    if(err_code == 0){
        int stat_size = sizeof(stbuf);
        if(send(sd, &stbuf, stat_size, 0) != stat_size)
            return -1;
    }
    return 0;
}

int FtpServer::readdir_(int sd){
    //readdirReqの受取
    ftp::readdirReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req))
        return -1;
    //readdirの実行
    int err_code{0};
    DIR* dirp;
    std::vector<dirent> dirents;
    //opendirの実行
    dirp = opendir(req.path);
    if(dirp == nullptr){
        err_code = errno;
    }else{
        //readdirの実行
        dirent* dep;
        for(;;){
            dep = readdir(dirp);
            if(dep == nullptr){
                break;
            }else{
                dirents.push_back(*dep);
            }
        }
        //closedirの実行
        closedir(dirp);
    }
    //readdirResの送信
    ftp::readdirRes res{err_code, static_cast<int>(dirents.size())};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res)){
        return -1;
    }
    //dirent配列の送信
    if(dirents.size() > 0){
        int dirents_size = dirents.size() * sizeof(dirent);
        if(send(sd, dirents.data(), dirents_size, 0) != dirents_size){
            return -1;
        }
    }
    return 0;
}

int FtpServer::ls_(int sd){
    //lsReqの受取
    ftp::lsReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //readdirの実行
    int err_code{0};
    std::vector<direntstat> entries;
    DIR* dirp;
    dirp = opendir(req.path);
    if(dirp == nullptr){
        err_code = errno;
    }else{
        dirent* dep;
        for(;;){
            direntstat attr;
            dep = readdir(dirp);
            if(dep == nullptr){
                break;
            }else{
                attr.entry = *dep;
                std::string entry_path = static_cast<std::string>(req.path) + "/" + static_cast<std::string>(dep->d_name);
                lstat(entry_path.c_str(), &attr.st);
                entries.push_back(attr);
            }
        }
        closedir(dirp);
    }
    //readdirResの送信
    ftp::lsRes res{err_code, static_cast<int>(entries.size())};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    //entry配列の送信
    if(err_code == 0){
        if(entries.size() > 0){
            int entries_size = entries.size() * sizeof(direntstat);
            if(send(sd, entries.data(), entries_size, 0) != entries_size)
                return -1;
        }
    }
    return 0;
}

std::string get_ip_from_socket(int sd){
    sockaddr_in addr;
    socklen_t sock_size = sizeof(addr);
    if(getsockname(sd, (sockaddr*)&addr, &sock_size) < 0){
        return std::string();
    }
    return inet_ntoa(addr.sin_addr);
}

int FtpServer::open_(int sd){
    //openReqの受取
    ftp::openReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //openの実行
    std::string ip = get_ip_from_socket(sd);
    int err_code{0};
    int fd = open(req.path, O_RDWR | O_CREAT, 0700);
    if(fd < 0){ err_code = errno; } 
    //openResの送信
    ftp::openRes res{err_code, fd};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    return 0;
}

int FtpServer::close_(int sd){
    //closeReqの受取
    ftp::closeReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //closeの実行
    int err_code = close(req.fd);
    if(err_code < 0){ err_code = errno; }
    //closeResの送信
    ftp::closeRes res{err_code};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    return 0;
}

int FtpServer::read_(int sd){
    int err_code{0};
    std::shared_ptr<char> buffer;
    //readReqの受取
    ftp::readReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //read
    int nread{0};
    buffer = std::shared_ptr<char>(new char[req.size]);
    if(lseek(req.fd, req.offset, SEEK_SET) < 0){
        err_code = errno;
    }else{
        nread = read(req.fd, buffer.get(), req.size);
        if(nread < 0){ err_code = -errno; }
    }
    //readResの送信
    ftp::readRes res{err_code, nread};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    //dataの送信
    if(err_code == 0){
        if(send(sd, buffer.get(), nread, 0) != nread)
            return -1;
    }
    return 0;
}

int FtpServer::write_(int sd){
    int err_code{0};
    //writeReqの受取
    ftp::writeReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //writeResの送信
    ftp::writeRes res{err_code};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res)){
        return -1;
    }
    //エラー終了
    if(err_code != 0 || req.size == 0){
        return 0;
    }
    //dataの受信
    std::shared_ptr<char> buffer(new char[req.size]);
    if(recv(sd, buffer.get(), req.size, MSG_WAITALL) != req.size){
        return -1;
    }
    //dataの書き込み
    if(lseek(req.fd, req.offset, SEEK_SET) < 0)
        return 0;
    write(req.fd, buffer.get(), req.size);
    return 0;
}

int FtpServer::create_(int sd){
    //createReqの受取
    ftp::createReq req;
    if(recv(sd, &req, sizeof(req), MSG_WAITALL) != sizeof(req)){
        return -1;
    }
    //createの実行
    int err_code{0};
    std::string ip = get_ip_from_socket(sd);
    int fd = creat(req.path, 0700);
    if(fd < 0)
        err_code = -1;
    close(fd);
    //createResの送信
    ftp::createRes res{err_code};
    if(send(sd, &res, sizeof(res), 0) != sizeof(res))
        return -1;
    return 0;
}

FtpServer::~FtpServer(){
    //do nothing now
};

const int FtpServer::run(int socket){
    int rc{0};
    //check ip
    if(!auth_host(socket)){
        std::cout << "invalid host" << std::endl;
        close(socket);
        return -1;
    }
    while(socket > 0){
        //recv request
        enum ftp::requestType rtype;
        if(recv(socket, &rtype, sizeof(rtype), MSG_WAITALL) 
                != sizeof(rtype)){
            close(socket);
            return -1;
        };

        //switch request
        switch(rtype){
            case ftp::echoback:
                std::cout << "echoback" << std::endl;
                rc = echoback_(socket);
                break;
            case ftp::getattr:
                std::cout << "getattr" << std::endl;
                rc = getattr_(socket);
                break;
            case ftp::readdir:
                std::cout << "readdir" << std::endl;
                rc = readdir_(socket);
                break;
            case ftp::ls:
                std::cout << "ls" << std::endl;
                rc = ls_(socket);
                break;
            case ftp::open:
                std::cout << "open" << std::endl;
                rc = open_(socket);
                break;
            case ftp::close:
                std::cout << "close" << std::endl;
                rc = close_(socket);
                break;
            case ftp::read:
                std::cout << "read" << std::endl;
                rc = read_(socket);
                break;
            case ftp::write:
                std::cout << "write" << std::endl;
                rc = write_(socket);
                break;
            case ftp::create:
                std::cout << "create" << std::endl;
                rc = create_(socket);
                break;
            default:
                rc = -1;
                break;
        };
        if(rc < 0){
            close(socket);
            socket = -1;
        }
    }
    return 0;
};

#ifdef SERVER
std::list<in_addr_t> convert_hostlist(const std::vector<std::string>& ip_list){
    std::list<in_addr_t> hostlist;
    for(auto& ip : ip_list){
        hostlist.push_back(inet_addr(ip.c_str()));
    }
    return hostlist;
}

int server_session(std::string config_path){
    MapEditor config(config_path);
    std::string ip, port_s;
    std::vector<std::string> peer_ips;
    short port{0};
    try{
        ip = config.maps.at("ip").front();
        port_s = config.maps.at("port").front();
        peer_ips = config.maps.at("peer");
        port = std::stoi(port_s);
    }catch(const std::out_of_range& e){
        std::cout << "config error" << std::endl;
        return -1;
    }
    Server server(ip, port);
    FtpServer fserver(convert_hostlist(peer_ips));
    server.run(fserver);
    return 0;
}

int main(int argc, char** argv){
    if(argc < 2)
        return -1;
    return server_session(argv[1]); 
}
#endif
