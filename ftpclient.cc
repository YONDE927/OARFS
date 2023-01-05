#include "ftpclient.h"
#include "common.h"
#include "connection.h"
#include "ftp-type.h"

#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <sys/socket.h>

FtpClient::FtpClient(std::string ip, short port):client_(ip, port){
}

FtpClient::FtpClient():client_(){
}

FtpClient::~FtpClient(){}

int FtpClient::init(std::string ip, short port){
    return client_.init(ip, port);
}

int FtpClient::echoback_(std::string msg){
    std::lock_guard<std::mutex> lock(mtx_);
    if(msg.size() > MSG_LEN){
        std::cout << "msg length is too big" << std::endl;
        client_.close_socket();
        return -ENETDOWN;
    }
    ftp::echobackDatagram dgm;
    std::memcpy(&dgm.msg, msg.c_str(), msg.size() + 1);
    //接続を確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        return -ENETDOWN;
    }
    //echoback
    auto rtype = ftp::echoback;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        return -ENETDOWN;
    }

    if(send(sd, &dgm, sizeof(dgm), 0) != sizeof(dgm)){
        client_.close_socket();
        return -ENETDOWN;
    }

    ftp::echobackDatagram dgm_r;
    if(recv(sd, &dgm_r, sizeof(dgm_r), MSG_WAITALL) != sizeof(dgm_r)){
        client_.close_socket();
        return -ENETDOWN;
    }
    std::cout << "msg: " << dgm_r.msg << std::endl;
    return 0;
}

int FtpClient::getattr_(std::string path, struct stat& stbuf){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        std::cout << "conn socket" << std::endl;
        return -ENETDOWN;
    }
    //getattrリクエストの提示
    auto rtype = ftp::getattr;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        std::cout << "send reqtype" << std::endl;
        return -ENETDOWN;
    }
    //getattrリクエストの送信
    ftp::getattrReq req;
    std::memset(&req, 0, sizeof(req));
    std::memcpy(&req.path, path.c_str(), path.size() + 1);
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        std::cout << "send req" << std::endl;
        return -ENETDOWN;
    }
    //getattrレスポンスの受信
    ftp::getattrRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        std::cout << "recv res" << std::endl;
        return -ENETDOWN;
    }
    std::cout << "received getattr err_code = " << res.errno_ << std::endl;
    //stat構造体の受信
    if(res.errno_ != 0){
        return -res.errno_;
    }else{
        if(recv(sd, &stbuf, sizeof(stbuf), MSG_WAITALL) != sizeof(stbuf)){
            client_.close_socket();
            std::cout << "recv stbuf" << std::endl;
            return -ENETDOWN;
        }
        return 0;
    }
}

int FtpClient::readdir_(std::string path, std::vector<dirent>& dirents){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        std::cout << "conn socket" << std::endl;
        return -ENETDOWN;
    }
    //readdirリクエストの提示
    auto rtype = ftp::readdir;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        std::cout << "send req type" << std::endl;
        return -ENETDOWN;
    }
    //readdirリクエストの送信
    ftp::readdirReq req;
    std::memcpy(&req.path, path.c_str(), path.size() + 1);
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        std::cout << "send req" << std::endl;
        return -ENETDOWN;
    }
    //readdirレスポンスの受信
    ftp::readdirRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        std::cout << "recv res" << std::endl;
        return -ENETDOWN;
    }
    //dirent構造体の配列を受信
    if(res.errno_ != 0){
        return -res.errno_;
    }else{
        struct dirent de;
        for(int i=0; i<res.ndirent; i++){
            if(recv(sd, &de, sizeof(de), MSG_WAITALL) != sizeof(de)){
                dirents.clear();
                client_.close_socket();
                std::cout << "recv dirent type" << std::endl;
                return -ENETDOWN;
            }
            dirents.push_back(de);
        }
        return 0;
    }
}

int FtpClient::ls_(std::string path, std::vector<direntstat>& entries){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        std::cout << "conn socket" << std::endl;
        return -ENETDOWN;
    }
    //lsリクエストの提示
    auto rtype = ftp::ls;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        std::cout << "send req type" << std::endl;
        return -ENETDOWN;
    }
    //lsリクエストの送信
    ftp::lsReq req;
    std::memcpy(&req.path, path.c_str(), path.size() + 1);
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        std::cout << "send req" << std::endl;
        return -ENETDOWN;
    }
    //lsレスポンスの受信
    ftp::lsRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        std::cout << "recv res" << std::endl;
        return -ENETDOWN;
    }
    //dirent&stat構造体の配列を受信
    entries.clear();
    if(res.errno_ != 0){
        return -res.errno_;
    }else{
        struct direntstat attr;
        for(int i=0; i<res.nentry; i++){
            if(recv(sd, &attr, sizeof(attr), MSG_WAITALL) != sizeof(attr)){
                entries.clear();
                client_.close_socket();
                std::cout << "recv dirent type" << std::endl;
                return -ENETDOWN;
            }
            entries.push_back(attr);
        }
        return 0;
    }
}

int FtpClient::open_(std::string path){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        std::cout << "conn socket" << std::endl;
        return -ENETDOWN;
    }
    //openリクエストの提示
    auto rtype = ftp::open;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        std::cout << "send reqtype" << std::endl;
        return -ENETDOWN;
    }
    //openリクエストの送信
    ftp::openReq req;
    std::memcpy(&req.path, path.c_str(), path.size() + 1);
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        std::cout << "send req" << std::endl;
        return -ENETDOWN;
    }
    //openレスポンスの受信
    ftp::openRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        std::cout << "recv res" << std::endl;
        return -ENETDOWN;
    }
    if(res.errno_ > 0){
        return -res.errno_;
    }
    return res.fd;
}

int FtpClient::close_(int fd){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        std::cout << "conn socket" << std::endl;
        return -ENETDOWN;
    }
    //closeリクエストの提示
    auto rtype = ftp::close;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        std::cout << "send reqtype" << std::endl;
        return -ENETDOWN;
    }
    //closeリクエストの送信
    ftp::closeReq req;
    req.fd = fd;
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        std::cout << "send req" << std::endl;
        return -ENETDOWN;
    }
    //closeレスポンスの受信
    ftp::closeRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        std::cout << "recv res" << std::endl;
        return -ENETDOWN;
    }
    if(res.errno_ > 0){
        return -res.errno_;
    }
    return 0;
}

int FtpClient::read_(int fd, int offset, int size, char* buffer){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        return -ENETDOWN;
    }
    //readリクエストの提示
    auto rtype = ftp::read;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //readリクエストの送信
    ftp::readReq req;
    req.offset = offset;
    req.size = size;
    req.fd = fd;
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //readレスポンスの受信
    ftp::readRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //データの送信
    if(res.errno_ > 0){
        return -res.errno_;
    }else{
        if(recv(sd, buffer, res.size, MSG_WAITALL) != res.size){
            client_.close_socket();
            return -ENETDOWN;
        }
        return res.size;
    }
}

int FtpClient::write_(int fd, int offset, int size,
        const char* buffer){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        return -ENETDOWN;
    }
    //writeリクエストの提示
    auto rtype = ftp::write;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //writeリクエストの送信
    ftp::writeReq req;
    req.offset = offset;
    req.size = size;
    req.fd = fd;
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //writeレスポンスの受信
    ftp::writeRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //データの受信
    if(res.errno_ > 0){
        return -res.errno_;
    }else{
        if(send(sd, buffer, size, 0) != size){
            client_.close_socket();
            return -ENETDOWN;
        }
        return size;
    }
}

int FtpClient::create_(std::string path){
    std::lock_guard<std::mutex> lock(mtx_);
    //接続の確認
    int sd = client_.conn();
    if(sd < 0){
        client_.close_socket();
        return -ENETDOWN;
    }
    //createリクエストの提示 
    auto rtype = ftp::create;
    if(send(sd, &rtype, sizeof(rtype), 0) != sizeof(rtype)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //createリクエストの送信
    ftp::createReq req;
    std::memcpy(&req.path, path.c_str(), path.size() + 1);
    if(send(sd, &req, sizeof(req), 0) != sizeof(req)){
        client_.close_socket();
        return -ENETDOWN;
    }
    //createレスポンスの受信
    ftp::createRes res;
    if(recv(sd, &res, sizeof(res), MSG_WAITALL) != sizeof(res)){
        client_.close_socket();
        return -ENETDOWN;
    }

    return -res.errno_;
}

//TryFtpClient
int TryFtpClient::switch_req(std::shared_ptr<ftp::baseReq> req){
    if(req == nullptr){
        return -1;
    }
    switch(req->reqtype){
        case ftp::close:
        {
            auto creq = static_pointer_cast<ftp::closeReq>(req);
            return eclose_(creq->fd, false);
        }
        case ftp::write:
        {
            auto wreq = static_pointer_cast<trywriteReq>(req);
            return ewrite_(wreq->fd, wreq->offset, wreq->size,
                    wreq->buffer.get(), false);
        }
        default:
        {
            return 0;
        }
    }
}

void TryFtpClient::sending_task(){
    int sleep_interval{0};
    const int max_interval = 3;
    for(;;){
        if(term){
            break;
        }
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this](){return unsend_reqs.size() > 0;});
        lock.unlock();
        if(unsend_reqs.begin() != unsend_reqs.end()){
            auto req_ptr = unsend_reqs.front();
            int rc = switch_req(req_ptr);
            if(rc != -ENETDOWN){
                unsend_reqs.erase(unsend_reqs.begin());
                sleep_interval = 0;
            }else{
                std::cout << "background try fail" << std::endl;
                sleep(sleep_interval);
                if(sleep_interval < max_interval){
                    sleep_interval += 1;
                }
            }
        }
    }
}

TryFtpClient::TryFtpClient(std::string ip, short port): FtpClient(ip, port){
    sending_thread = std::thread(&TryFtpClient::sending_task, this); 
}

TryFtpClient::TryFtpClient(): FtpClient(){
}

TryFtpClient::~TryFtpClient(){
    std::lock_guard<std::mutex> lock(mtx_);
    term = true;
    cond_.notify_one();
    sending_thread.join();
}

int TryFtpClient::init(std::string ip, short port){
    client_.init(ip, port);
    sending_thread = std::thread(&TryFtpClient::sending_task, this); 
    return 0;
}

int TryFtpClient::eopen_(std::string path, bool do_resend){
    int fd = open_(path);
    if(fd == -ENETDOWN){
        if(do_resend){
            std::lock_guard<std::mutex> lock(mtx_);
            std::shared_ptr<ftp::openReq> preq(new ftp::openReq);
            preq->reqtype = ftp::open;
            std::memcpy(preq->path, path.c_str(), path.size());
            unsend_reqs.push_back(preq); 
            cond_.notify_one();
        }
    }
    return fd;
}

int TryFtpClient::eclose_(int fd, bool do_resend){
    int rc = close_(fd);
    if(rc == -ENETDOWN){
        if(do_resend){
            std::lock_guard<std::mutex> lock(mtx_);
            std::shared_ptr<ftp::closeReq> preq(new ftp::closeReq);
            preq->reqtype = ftp::close;
            preq->fd = fd;
            unsend_reqs.push_back(preq); 
            cond_.notify_one();
        }
    }
    return rc;
}

int TryFtpClient::ewrite_(int fd, int offset, int size,
        const char* buffer, bool do_resend){
    int rc = write_(fd, offset, size, buffer);
    if(rc == -ENETDOWN){
        if(do_resend){
            std::lock_guard<std::mutex> lock(mtx_);
            std::shared_ptr<trywriteReq> preq(new trywriteReq);
            preq->reqtype = ftp::write;
            preq->fd = fd;
            preq->offset = offset;
            preq->size = size;
            preq->buffer = std::shared_ptr<char>(new char[size]);
            std::memcpy(preq->buffer.get(), buffer, size);
            unsend_reqs.push_back(preq); 
            cond_.notify_one();
        }
    }
    return rc;
}

int TryFtpClient::ecreate_(std::string path, bool do_resend){
    int rc = create_(path);
    if(rc == -ENETDOWN){
        if(do_resend){
            std::lock_guard<std::mutex> lock(mtx_);
            std::shared_ptr<ftp::createReq> preq(new ftp::createReq);
            preq->reqtype = ftp::create;
            std::memcpy(&preq->path, path.c_str(), path.size() + 1);
            unsend_reqs.push_back(preq); 
            cond_.notify_one();
        }
    }
    return rc;
}

const char* requestType_str[] = {
    "echoback",
    "getattr",
    "readdir",
    "ls",
    "open",
    "close",
    "read",
    "write",
    "create"
};

void TryFtpClient::print_unsend_reqs(){
    for(const auto& req : unsend_reqs){
        std::cout << "type: " << requestType_str[req->reqtype];
    }
}
