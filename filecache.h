#include <sqlite3.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <vector>

#include "common.h"

class filecache {
    public:
        sqlite3 *db;
        std::string fcache_root;
    public:
        filecache();
        ~filecache();
        int init(std::string dbname);
        int reserve_(int size);
        int register_(std::string path, struct stat stbuf);
        int lookup_(std::string path);
    private:
        int validate_(std::string path, time_t mtime);
        int delete_(std::string path);
};

