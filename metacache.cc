#include "metacache.h"
#include "common.h"

#include <bits/types/time_t.h>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <unistd.h>

metacache::~metacache(){
    sqlite3_close(db);
}

int create_metacache_table(sqlite3 *db){
    const char* stat_query = "CREATE TABLE IF NOT EXISTS stats ( path TEXT PRIMARY KEY, stat BLOB, time INTEGER);";
    const char* dirent_query = "CREATE TABLE IF NOT EXISTS dirents ( dir TEXT, dname TEXT, entry BLOB, time INTEGER, PRIMARY KEY(dir, dname));";
    if(sqlite3_exec(db, stat_query, nullptr, nullptr, nullptr) != SQLITE_OK)
        return -1;
    if(sqlite3_exec(db, dirent_query, nullptr, nullptr, nullptr) != SQLITE_OK)
        return -1;
    return 0;
}

int metacache::init(std::string dbname){
    int rc = sqlite3_open(dbname.c_str(), &db);
    if(rc != SQLITE_OK)
        return -1;
    return create_metacache_table(db);
}

int metacache::delete_stat(std::string path){
    //stmtの構築
    const std::string query = "DELETE FROM stats WHERE path = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int metacache::getattr_(std::string path, struct stat& stbuf){
    //stmtの構築
    const std::string query = "SELECT * FROM stats WHERE path = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    //クエリの実行
    if(sqlite3_step(stmt) != SQLITE_ROW){
        sqlite3_finalize(stmt);
        return -1;
    }
    //キャッシュ取得時より1分経過しているか
    time_t time = sqlite3_column_int(stmt, 2);
    time_t now = std::time(nullptr);
    if((now - time) > METACACHE_TIME){
        sqlite3_finalize(stmt);
        //キャッシュの廃棄
        delete_stat(path);
        return -1;
    }
    //stat構造体をロードしてコピー
    const struct stat* pstat = reinterpret_cast<const struct stat*>(sqlite3_column_blob(stmt, 1));
    std::memcpy(&stbuf, pstat, sizeof(stbuf));
    sqlite3_finalize(stmt);
    return 0;
}

int metacache::delete_dirent(std::string path){
    //stmtの構築
    const std::string query = "DELETE FROM dirents WHERE dir = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int metacache::readdir_(std::string path, std::vector<dirent>& entries){
    //stmtの構築
    const std::string query = "SELECT * FROM dirents WHERE dir = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    //クエリの実行
    entries.clear();
    if(sqlite3_step(stmt) == SQLITE_ROW){
        //キャッシュ取得時より1分経過しているか
        time_t time = sqlite3_column_int(stmt, 3);
        time_t now = std::time(nullptr);
        if((now - time) > METACACHE_TIME){
            sqlite3_finalize(stmt);
            //キャッシュの廃棄
            delete_dirent(path);
            return -1;
        }
        const dirent* entry = reinterpret_cast<const dirent*>(sqlite3_column_blob(stmt, 2));
        entries.push_back(*entry);
    }else{
        sqlite3_finalize(stmt);
        return -1;
    }
    //残りのentryを取得
    for(;;){
        int rc = sqlite3_step(stmt);
        if(rc == SQLITE_ROW){
            const dirent* entry = reinterpret_cast<const dirent*>(sqlite3_column_blob(stmt, 2));
            entries.push_back(*entry);
        }else if(rc == SQLITE_DONE){
            break;
        }else{
            sqlite3_finalize(stmt);
            return -1;
        }
    }
    sqlite3_finalize(stmt);
    return 0;
}

int metacache::ls_(std::string path, std::vector<direntstat>& entries){
    std::vector<dirent> dirents;
    if(readdir_(path, dirents) < 0)
        return -1;
    entries.clear();
    for(auto& de : dirents){
        direntstat ds;
        std::string entry_path = to_real_path(de.d_name, path);
        ds.entry = de;
        if(getattr_(entry_path, ds.st) < 0){
            entries.clear();
            return -1;
        }
        entries.push_back(ds);
    }
    return 0;
}

int metacache::update_stat(std::string path, const struct stat& stbuf){
    //stmtの構築
    const std::string query = "INSERT INTO stats (path, stat, time) VALUES (?, ?, ?) \
        ON CONFLICT (path) DO UPDATE SET stat = ?, time = ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
    sqlite3_bind_blob(stmt, 2, &stbuf, sizeof(stbuf), nullptr);
    time_t now = std::time(nullptr);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_blob(stmt, 4, &stbuf, sizeof(stbuf), nullptr);
    sqlite3_bind_int64(stmt, 5, now);
    //クエリの実行
    if(sqlite3_step(stmt) != SQLITE_DONE){
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int metacache::update_dirent(std::string path, const std::vector<dirent>& entries){
    const std::string query = "INSERT INTO dirents (dir, dname, entry, time) VALUES (?, ?, ?, ?) \
        ON CONFLICT (dir, dname) DO UPDATE SET entry = ?, time = ?;";
    sqlite3_stmt *stmt;
    time_t now = std::time(nullptr);
    for(auto& entry : entries){
        sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, nullptr);
        sqlite3_bind_text(stmt, 2, entry.d_name, -1, nullptr);
        sqlite3_bind_blob(stmt, 3, &entry, sizeof(entry), nullptr);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_bind_blob(stmt, 5, &entry, sizeof(entry), nullptr);
        sqlite3_bind_int64(stmt, 6, now);
        //クエリの実行
        if(sqlite3_step(stmt) != SQLITE_DONE){
            sqlite3_finalize(stmt);
            return -1;
        }
        sqlite3_finalize(stmt);
    }
    return 0;
}

int metacache::update_ls(std::string path, std::vector<direntstat>& entries){
    std::vector<dirent> dirents;
    for(auto& en : entries){
        std::string entry_path = to_real_path(en.entry.d_name, path);
        update_stat(entry_path, en.st);
        dirents.push_back(en.entry);
    }
    if(update_dirent(path, dirents) < 0)
        return -1;
    return 0;
}


#ifdef TESTMETACACHE
int main(){
    metacache cache;
    if(cache.init("./sampledb") < 0)
        return 0;
    struct stat stbuf;
    dirent d1, d2;
    strncpy(d1.d_name, "file1", 6);
    strncpy(d2.d_name, "file2", 6);
    std::vector<dirent> entries = {d1, d2};
    lstat("./sampledb", &stbuf);
    if(cache.update_stat("dir1/file1", stbuf) < 0)
        return 0;
    if(cache.update_stat("dir1/file2", stbuf) < 0)
        return 0;
    if(cache.update_dirent("dir1", entries) < 0)
        return 0;
    struct stat stbuf_cache;
    std::vector<direntstat> entries_cache;
    if(cache.getattr_("dir1/file1", stbuf_cache) < 0)
        return 0;
    if(cache.ls_("dir1", entries_cache) < 0)
        return 0;
    printf("file1 size %ld\n", stbuf_cache.st_size);

    for(auto& ds : entries_cache){
        printf("dir entries %s\n", ds.entry.d_name);
    }

    sleep(10);

    struct stat stbuf_recache;
    std::vector<dirent> entries_recache;
    if(cache.getattr_("file1", stbuf_recache) == 0)
        puts("not deleted stat");
    if(cache.readdir_("dir1", entries_recache) == 0)
        puts("not deleted dirent");
    return 0;
}
#endif
