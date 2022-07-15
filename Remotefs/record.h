#pragma once

#include <postgresql/libpq-fe.h>

typedef struct{
    char dbname[256];
} RecordConfig;

typedef struct{
    PGconn* session;
    RecordConfig* config;
} Record;

RecordConfig* loadRecordConfig(char* configpath);
Record* newRecord(RecordConfig* config);
void freeRecord(Record* record);
void resetRecord(PGconn* session);

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
