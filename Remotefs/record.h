#pragma once

#include <sqlite3.h>

typedef struct Record {
    sqlite3* session;
} Record;

Record* newRecord();
void freeRecord(Record* record);
void resetRecord(sqlite3* dbsession);

//DB接続初期化
int initRecordSession(const char *filename, sqlite3 **ppDb);

/*DB接続を終了*/
int closeRecordSession(Record* record);

/*DBテーブルを作成*/
int createRecordTable(Record* record);

typedef enum op_t {
    oOPEN,
    oCLOSE,
    oREAD,
    oWRITE,
    oGETATTR,
    oREADDIR
} op_t;

//基本形
int recordOperation(Record* record, const char* path, op_t op);
