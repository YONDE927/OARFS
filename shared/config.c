#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char* searchOptionKey(FILE* fi, const char* keyword){
    char* line = NULL;
    char* output = NULL;
    char* saveptr;
    char* key, *val;
    size_t n;
    int len, errno;

    //if(key == NULL){return -1;}
    if(fi == NULL){ return NULL;}
    fseek(fi, 0, SEEK_SET);
    while((len = getline(&line, &n, fi)) >= 0){
        line[len - 1] = '\0';
        //printf("%s\n", line);
        //キーとバリューしかないので二回strtokを呼ぶ
        key = strtok_r(line, " ", &saveptr);
        //printf("key: %s\n", key);
        if(strcmp(key, keyword) == 0){
            val = strtok_r(NULL, " ", &saveptr);
            output = strdup(val);
        }
        free(line);
        line = NULL;
    }
    if(errno != 0){
        if(output != NULL){free(output);}
        return NULL;
    }
    return output;
}

