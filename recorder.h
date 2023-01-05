#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <vector>

#include "common.h"

class Recorder{
    public:
        sqlite3 *db;
    public:
        ~Recorder();
        int init(std::string dbname);
        int add_history(std::string path, std::string op);
        int add_history(std::string path, std::string op, pid_t pid);
    private:
        int delete_old();
};

