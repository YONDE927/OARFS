#pragma once

#include <asm-generic/errno-base.h>
#include <string>
#include <vector>
#include <map>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <dirent.h>

struct direntstat {
    dirent entry;
    struct stat st;
};

class MapEditor{
    public:
        std::map<std::string, std::vector<std::string>> maps;
        MapEditor();
        MapEditor(std::string path);
        void load(std::string path);
        void save(std::string path);
        void print();
};

int open_cine(std::string path);
int create_cine(std::string path);
int copy_cine(std::string from, std::string to);
bool file_exist(std::string path);

std::string getdirpath(std::string path);
std::string getfilename(std::string path);
int filesize(std::string path);
int min(int x, int y);
int max(int x, int y);
std::string to_real_path(std::string path, std::string root);
std::string filename(std::string path);
std::string dirpath(std::string path);
void makedir(std::string path);

template<class T>
int new_map_index(std::map<int, T>& mp){
    int i = 0;
    for(;;){
        if(mp.find(i) == mp.end())
            return i;
        i++; 
    }
}

std::string get_process_name(pid_t pid);
