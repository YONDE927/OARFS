#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct JsonNode {
    char* key;
    char* value;
    struct JsonNode* next;
    struct JsonNode* child;
};

char* readFirstKey(char* jsontext){
    int length, keylength;
    int begin, end;
    char* key;

    length = strlen(jsontext);

    //キーの最初を見つける
    begin = -1; end = -1;
    for(int i = 0; i < length; i++){
        if(end > 0){
            break;
        }
        if(jsontext[i] == '\"'){
            if(begin < 0){
                begin = i;
            }else{
                end = i;
            }
        }
    }

    if((begin < 0) | (end < 0)){
        return NULL;
    }

    keylength = end - begin;
    key = malloc(keylength);
    bzero(key, keylength);
    strncpy(key, jsontext + begin + 1, keylength);
    
    return key;
}

int main(){
    char* samplejson = "{\n\"glossary\": \"hello\"}";
    char* key;

    key = readFirstKey(samplejson);
    printf("key: %s\n", key);
    return 0;
}
