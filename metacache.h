#include <sqlite3.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <vector>

#include "common.h"

#define METACACHE_TIME 60

class metacache {
    public:
        sqlite3 *db;
    public:
        ~metacache();
        int init(std::string dbname);
        int getattr_(std::string path, struct stat& stbuf);
        int readdir_(std::string path, std::vector<dirent>& entries);
        int ls_(std::string path, std::vector<direntstat>& entries);
        int update_stat(std::string path, const struct stat& stbuf);
        int update_dirent(std::string path, const std::vector<dirent>& entries);
        int update_ls(std::string path, std::vector<direntstat>& entries);
    private:
        int delete_stat(std::string path);
        int delete_dirent(std::string path);
};

