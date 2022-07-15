#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char* searchOptionKey(FILE* fi, const char* keyword){
    char* line = NULL;
    char* linebuf = NULL;
    char* output = NULL;
    char* saveptr;
    char* key, *val;
    size_t n;
    int len;

    //if(key == NULL){return -1;}
    if(fi == NULL){ return NULL;}
    fseek(fi, 0, SEEK_SET);
    while((len = getline(&line, &n, fi)) > 0){
        linebuf = strdup(line);
        linebuf[len - 1] = '\0';
        //printf("%s\n", line);
        //キーとバリューしかないので二回strtokを呼ぶ
        key = strtok_r(linebuf, " ", &saveptr);
        //printf("keyword: %s key: %s len: %d\n",keyword, key, len);
        if(len < 2){
            free(linebuf);
            continue; 
        }
        if(strncmp(key, keyword, len) == 0){
            val = strtok_r(NULL, " ", &saveptr);
            output = strdup(val);
            free(linebuf);
            break;
        }
        if(linebuf != NULL){ free(linebuf); }
    }
    if(line != NULL){ free(line); }
    //if(errno != 0){
        //printf("error: %s\n", strerror(errno));
        //if(output != NULL){free(output);}
        //return NULL;
    //}
    return output;
}

