#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "record.h"

Record* newRecord(){
    int rc;
    Record* record;

    record = malloc(sizeof(Record));
    
    rc = initRecordSession("./db/record.db", &record->session);
    if(rc < 0){
        return NULL;
    }

    createRecordTable(record);

    resetRecord(record->session);

    return record;
}

void freeRecord(Record* record){
    int rc;

    rc = closeRecordSession(record);
    free(record);
}

//DB接続初期化
int initRecordSession(const char *filename, sqlite3 **ppDb){
    int rc;

    //sqlite3のスレッド対応
    rc = sqlite3_initialize();
    rc = sqlite3_config(SQLITE_CONFIG_SERIALIZED);

    rc = sqlite3_open(filename, ppDb);
    if(rc){
        sqlite3_close(*ppDb);
        return -1;
    }
    return 0;
}

/*DBのリセット*/
void resetRecord(sqlite3* dbsession){
    int rc;
    char* errmsg = 0;

    rc = sqlite3_exec(dbsession, "DELETE FROM Record;", NULL, 0, &errmsg);
    if(rc != SQLITE_OK){
        printf("customQuery failed.\n");
    }
}

/*DB接続を終了*/
int closeRecordSession(Record* record){
    int rc;
    sqlite3_close(record->session);
    return 0;
}

/*DBテーブルを作成*/
int createRecordTable(Record* record){
    int rc;
    sqlite3_stmt *stmt;

    if(record->session == NULL){
        printf("createRecordTable failed\n");
        return -1;
    }
    
    //sql text
    rc = sqlite3_prepare_v2(record->session, 
            "CREATE TABLE IF NOT EXISTS Record"
            "(path TEXT, operation TEXT, time INTEGER);",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        return -1;
    }

    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        printf("createRecordTable failed\n");
        sqlite3_finalize(stmt);
    }

    //end
    sqlite3_finalize(stmt);
    return 0;
}

char* opname[] = {"OPEN","CLOSE","READ","WRITE","GETATTR","READDIR"};

//基本形
int recordOperation(Record* record, const char* path, op_t op){
    int rc;
    sqlite3_stmt* stmt;
    time_t now;

    //time
    now = time(NULL);
    //sql text
    rc = sqlite3_prepare_v2(record->session, 
            "REPLACE INTO Record VALUES "
            "( ?, ?, ?);",
            -1, &stmt, 0);
    if(rc != SQLITE_OK){
        printf("invalid sql\n");
        return -1;
    }else{
        sqlite3_bind_text(stmt, 1, path, -1, 0);
        sqlite3_bind_text(stmt, 2, opname[op], -1, 0);
        sqlite3_bind_int(stmt, 3, (int)now);
    }

    //exectute
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE){
        printf("insert record fail.\n");
        return -1;
    }

    //end
    sqlite3_finalize(stmt);
    return 0;
}
