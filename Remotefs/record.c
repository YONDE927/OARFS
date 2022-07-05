#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "record.h"

RecordConfig* loadrecordconfig(){
    char ch;
    char port[6];
    FILE* file;
    int count = 0;

    file = fopen("config/db.config", "r");
    if(file == NULL)
    {
        return NULL;
    }

    RecordConfig* config = malloc(sizeof(RecordConfig));
    ch = getc(file);
    while((ch != EOF) && (ch != '\n'))
    {
        config->dbname[count] = ch;    
        ch = getc(file);
        count ++;
    }
    config->dbname[count] = '\0';
    return config;
}

//DB接続初期化
int initRecordSession(Record* record){
    const char** dbkey;
    const char** dbval;

    if(record == NULL){return -1;}
    dbkey = malloc(1 * sizeof(char*));
    dbval = malloc(1 * sizeof(char*));
    dbkey[0] = "dbname"; dbval[0] = record->config->dbname; 

    record->session = PQconnectdbParams(dbkey, dbval, 1);
    if(record->session == NULL){
        puts("connect db error");
        exit(0);
    }
    return 0;
}

Record* newRecord(){
    int rc;
    Record* record;
    RecordConfig* config;

    config = loadrecordconfig();
    if(config == NULL){ return NULL;}

    record = malloc(sizeof(Record));
    record->config = config;
    rc = initRecordSession(record);
    if(rc < 0){
        return NULL;
    }

    createRecordTable(record);

    resetRecord(record->session);

    return record;
}

/*DBのリセット*/
void resetRecord(PGconn* session){
    int rc;

    //exec sql
    PGresult* res;
    //reset
    res = PQexec(session, "TRUNCATE Record;");
    PQclear(res);
    res = PQexec(session, "select setval ('Record_id_seq',1, false);");
    PQclear(res);
}

/*DB接続を終了*/
void closeRecordSession(Record* record){
    if(record == NULL){return;}
    PQfinish(record->session);
}

void freeRecord(Record* record){
    closeRecordSession(record);
    free(record);
}

/*DBテーブルを作成*/
int createRecordTable(Record* record){
    PGresult* res;

    if(record->session == NULL){
        printf("createRecordTable failed\n");
        return -1;
    }
    
    //sql text
    res = PQexec(record->session, "CREATE TABLE IF NOT EXISTS Record(id SERIAL PRIMARY KEY, PATH TEXT, OPCODE TEXT, TIME INTEGER);");

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    };
    PQclear(res);
    return 0;
}

char* opname[] = {"OPEN","CLOSE","READ","WRITE","GETATTR","READDIR"};

//基本形
int recordOperation(Record* record, const char* path, op_t op){
    int rc;
    time_t now;
    PGresult* res;

    //time
    now = time(NULL);

    //sql text
    int paramlength[] = {0, 0, sizeof(int)};
    int paramformat[] = {0, 0, 1};
    const char* vals[] = {path, opname[op], (char*)&now};

    res = PQexecParams(record->session,
                       "REPLACE INTO Record VALUES($1,$2,$3);",
                       3,           /* パラメータは1つ。 */
                       NULL,        /* バックエンドにパラメータの型を推測させる。 */
                       vals,
                       paramlength,        /* テキストのため、パラメータ長は不要。 */
                       paramformat,        /* デフォルトで全てのパラメータはテキスト。 */
                       0);          /* バイナリ結果を要求。 */

    if(PQresultStatus(res) != PGRES_COMMAND_OK){
        PQclear(res);
        return -1;
    };
    PQclear(res);
    return 0;
}
