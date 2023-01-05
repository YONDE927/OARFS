#include "copy.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

copyfs cfs;

int open_repl(){
    std::string path;
    std::cout << "path: ";
    std::cin >> path;
    int fd = cfs.open_(path, std::to_string(getpid()));
    if(fd >= 0){
        std::cout << "fd of " << path << " is " << fd << std::endl;
    }else{
        std::cout << fd << std::endl;
    }
    return 0;
}

int close_repl(){
    int fd;
    std::cout << "fd: ";
    std::cin >> fd;
    int rc = cfs.close_(fd);
    if(rc >= 0){
        std::cout << fd << " was closed" << std::endl;
    }else{
        std::cout << rc << std::endl;
    }
    return 0;
}

int input_int(){
    int i{0};
    std::string size_s;
    std::cin >> size_s;
    try{
        i = stoi(size_s); 
    }catch(const std::exception& e){
        std::cout << "invalid input" << std::endl;
        i = -1;
    }
    return i;
}

int read_repl(){
    int fd;
    std::cout << "fd: ";
    std::cin >> fd;
    int offset;
    std::cout << "offset: ";
    offset = input_int();
    if(offset < 0){
        return -1;
    }
    int size;
    std::cout << "size: ";
    size = input_int();
    if(size < 0){
        return -1;
    }
    std::shared_ptr<char> buffer(new char[size]);
    memset(buffer.get(), 0, size);
    int rc = cfs.read_(fd, buffer.get(), offset, size);
    if(rc >= 0){
        std::cout << buffer << std::endl;
    }else{
        std::cout << rc << std::endl;
    }
    return 0;
}

std::shared_ptr<char> dup_shared(std::string str){
    std::shared_ptr<char> buffer(new char[str.size() + 1]);
    memcpy(buffer.get(), str.c_str(), str.size() + 1);
    return buffer;
}

int write_repl(){
    int fd;
    std::cout << "fd: ";
    std::cin >> fd;
    int offset;
    std::cout << "offset: ";
    offset = input_int();
    if(offset < 0){
        return 0;
    }

    std::string str;
    std::cout << "buffer: ";
    std::cin >> str;
    
    int rc = cfs.write_(fd, str.c_str(), offset, str.size() + 1);
    if(rc >= 0){
        std::cout << "[write success]" << std::endl;
    }else{
        std::cout << strerror(-rc) << std::endl;
    }
    return 0;
}

int repl_switch(std::string oper){
    if(oper == "open"){
        open_repl();
    }else if(oper == "close"){
        close_repl();
    }else if(oper == "read"){
        read_repl();
    }else if(oper == "write"){
        write_repl();
    }
    return 0;
}

void repl(){
    for(;;){
        std::string oper;
        std::cout << ">> ";
        std::cin >> oper;
        if(!std::cin){
            break;
        }
        if(repl_switch(oper) < 0){
            break;
        }
    }
}

int main(int argc, char** argv){
    repl();
    return 0;
}
