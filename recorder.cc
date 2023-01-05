#include "recorder.h"
#include "common.h"

#include <bits/types/time_t.h>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_RECORD 1000

Recorder::~Recorder(){
    sqlite3_close(db);
}

int create_recorder_table(sqlite3 *db){
    const char* query = "CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, op TEXT, program TEXT, time INTEGER);";
    if(sqlite3_exec(db, query, nullptr, nullptr, nullptr) != SQLITE_OK)
        return -1;
    return 0;
}

int Recorder::init(std::string dbname){
    int rc = sqlite3_open(dbname.c_str(), &db);
    if(rc != SQLITE_OK)
        return -1;
    return create_recorder_table(db);
}

int count_history_record(sqlite3 *db){
    const std::string query = "SELECT COUNT(*) FROM history;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    //クエリの実行
    if(sqlite3_step(stmt) != SQLITE_ROW){
        sqlite3_finalize(stmt);
        return 0;
    }
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int Recorder::delete_old(){
    int cnt = count_history_record(db);
    if(cnt <= MAX_RECORD)
        return 0;
    int num_delete = cnt - MAX_RECORD;
    const std::string query = "DELETE FROM history ORDER BY id LIMIT ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, num_delete);
    if(sqlite3_step(stmt) != SQLITE_DONE){
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int Recorder::add_history(std::string path, std::string op){
    //stmtの構築
    const std::string query = "INSERT INTO history (path, op, time) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    sqlite3_bind_text(stmt, 2, op.c_str(), -1, nullptr);
    int now = time(nullptr);
    sqlite3_bind_int64(stmt, 3, now);
    //クエリの実行
    if(sqlite3_step(stmt) != SQLITE_DONE){
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    delete_old();
    return 0;
}

int Recorder::add_history(std::string path, std::string op, pid_t pid){
    //stmtの構築
    const std::string query = "INSERT INTO history (path, op, program, time) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    sqlite3_bind_text(stmt, 2, op.c_str(), -1, nullptr);
    std::string process = get_process_name(pid);
    sqlite3_bind_text(stmt, 3, process.c_str(), -1, nullptr);
    int now = time(nullptr);
    sqlite3_bind_int64(stmt, 4, now);

    //printf("query: %s\n", sqlite3_expanded_sql(stmt));
    //クエリの実行
    if(sqlite3_step(stmt) != SQLITE_DONE){
        sqlite3_finalize(stmt);
        printf("query fail\n");
        return -1;
    }
    sqlite3_finalize(stmt);
    delete_old();
    return 0;
}

#ifdef RECORDER 
int main(){
    int cnt{0};
    Recorder record;
    if(record.init("./sampledb") < 0)
        return 0;
    if(record.add_history("file", "open", getpid()) < 0){
        printf("error 0");
        return 0;
    }
    cnt = count_history_record(record.db);
    printf("%d\n", cnt);
    if(record.add_history("file", "read", getpid()) < 0){
        printf("error 1");
        return 0;
    }
    cnt = count_history_record(record.db);
    printf("%d\n", cnt);
    if(record.add_history("file", "read", getpid()) < 0){
        printf("error 1");
        return 0;
    }
    cnt = count_history_record(record.db);
    printf("%d\n", cnt);
    if(record.add_history("file", "close", getpid()) < 0){
        printf("error 1");
        return 0;
    }
    cnt = count_history_record(record.db);
    printf("%d\n", cnt);
    if(record.add_history("file2", "open", getpid()) < 0){
        printf("error 1");
        return 0;
    }
    cnt = count_history_record(record.db);
    printf("%d\n", cnt);
    return 0;
}
#endif
