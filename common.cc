#include "common.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>

#include <unistd.h>
#include <cstdio>
#include <fcntl.h>

MapEditor::MapEditor(){

}

MapEditor::MapEditor(std::string path){
    load(path);
}

void MapEditor::load(std::string path){
    std::ifstream fs(path);
    std::string line;
    while(std::getline(fs, line)){
        char* str = strdup(line.c_str());

        std::string key(strtok(str, " "));
        maps.try_emplace(key, std::vector<std::string>());
        for(;;){
            char* tp =  strtok(nullptr, " ");
            if(tp != nullptr){
                std::string val(tp);
                maps.at(key).push_back(val);
            }else{
                break;
            }
        }
        free(str);
    }
}

void MapEditor::save(std::string path){
    int fd = open(path.c_str(), O_RDWR | O_TRUNC | O_CREAT, 0600);
    if(fd < 0){ return; }

    for(auto pair : maps){
        write(fd, pair.first.c_str(), pair.first.size());
        write(fd, " ", 1);
        for(auto val : pair.second){
            write(fd, val.c_str(), val.size());
            write(fd, " ", 1);
        }
        write(fd, "\n", 1);
    }
}

void MapEditor::print(){
    for(const auto& pair : maps){
        std::cout << "[" << pair.first << "]: ";
        for(const auto& v : pair.second){
            std::cout << v << ", ";
        }
        std::cout << std::endl;
    }
}

int test_config(){
    std::string config = "ftpconfig";
    MapEditor reader;
    reader.load(config);
    reader.print();
    return 0;
}

bool file_exist(std::string path){
    struct stat stbuf;
    int rc = lstat(path.c_str(), &stbuf);
    if(rc < 0){
        return false;
    }
    return true;
}

int open_cine(std::string path){
    if(file_exist(path)){
        return open(path.c_str(), O_RDWR);
    }else{
        return creat(path.c_str(), 0600);
    }
}

int create_cine(std::string path){
    int fd = open_cine(path);
    if(fd < 0){
        return -errno;
    }else{
        close(fd);
    }
    return 0;
}

int copy_cine(std::string from, std::string to){
    if(file_exist(to)){
        return 0;
    }else{
        try{
            std::filesystem::copy(from, to);
            return 1;
        }catch(std::exception& e){
            std::cout << e.what() << std::endl;
            return -1;
        }
    }
}

std::string getdirpath(std::string path){
    if(path == "/"){
        return ".";
    }
    int ind = path.find_last_of("/");
    if(ind == path.npos){
        return "";
    }
    return path.substr(0, ind);
}

void makedir(std::string path){
    if(path == "/")
        return;
    if(path == ".")
        return;
    if(path == "")
        return;
    std::string dir = getdirpath(path);
    makedir(dir);
    char ab_path[256];
    realpath(path.c_str(), ab_path);
    if(mkdir(ab_path, 0777) < 0)
        printf("%s\n", strerror(errno));
}

std::string getfilename(std::string path){
    std::filesystem::path p(path);
    return p.filename();
}

int filesize(std::string path){
    struct stat stbuf;
    if(lstat(path.c_str(), &stbuf) < 0){
        return 0;
    }else{
        return stbuf.st_size;
    }
}

int min(int x, int y){
    if(x >= y){return y;}else{return x;}
}

int max(int x, int y){
    if(x >= y){return x;}else{return y;}
}

std::string to_real_path(std::string path, std::string root){
    if(path == "/"){
        return root;
    }
    if(root.back() == '/')
        root.pop_back();
    if(path.front() == '/')
        return root + path;
    return root + "/" + path;
}

std::string filename(std::string path){
    int pos = path.find_last_of('/');
    if(pos == path.npos)
        return path;
    else
        return path.substr(pos + 1);
}

std::string dirpath(std::string path){
    int pos = path.find_last_of('/');
    if(pos == path.npos)
        return path;
    else
        return path.substr(0, pos);
}

std::string get_process_name(pid_t pid){
    char buffer[256];
    sprintf (buffer, "/proc/%d/exe", pid);
    if(readlink(buffer, buffer, 256) < 0)
        return "";
    return buffer;
}
