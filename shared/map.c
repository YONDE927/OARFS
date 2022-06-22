#include <stdio.h>
#include "map.h"

StrMap* newStrMap()
{
    StrMap* map;
    map = malloc(sizeof(StrMap));
    memset(map,0,sizeof(StrMap));
    return map;
}

int StrMapKey(char* str)
{
    unsigned int ret = 0;
    for(;(*str)!='\0';str++)
    {
    ret += (int)*str;
    }
    //printf("%s chain is %d\n",str,ret % HASH_SIZE);
    return ret % HASH_SIZE;
}

void* getStrMap(StrMap* map,char* key)
{
    StrMapNode* pNode = map->hash[StrMapKey(key)];
    for(;pNode!=NULL;pNode=pNode->next)
    {
    if(strcmp(pNode->key, key) == 0)
    {
        return pNode->value;
    }
    }
    return NULL;
}

void insStrMap(StrMap* map, char* key, void* value, int size)
{
    int mapchain = StrMapKey(key);
    StrMapNode* pNode = map->hash[mapchain];
    //新しいノードを予約し、値を埋める。
    int keysize = strlen(key) + 1;
    StrMapNode* newNode = malloc(sizeof(StrMapNode));
    //keyのメモリを予約
    newNode->key = (char*)malloc(keysize);
    //keyに文字列をコピー
    strncpy(newNode->key, key, keysize);
    //valueのメモリを予約
    newNode->value = malloc(size);
    //valueにデータをコピー
    memcpy(newNode->value, value, size);
    //valueのnextにリンクリストの先頭アドレスを登録
    newNode->next = pNode;
    //ハッシュチェーンの先頭に登録
    map->hash[mapchain] = newNode;
    //printf("newNode->key = %s, newNode->value = %s\n",newNode->key,(char*)newNode->value);
}

void delStrMap(StrMap* map, char* key, void(*fptr)(void*))
{
    int mapchain = StrMapKey(key);
    StrMapNode* pNode = map->hash[mapchain];
    StrMapNode* prev = NULL;
    for(;pNode!=NULL;pNode=pNode->next)
    {
    printf("search map %s\n",key);
    if(strcmp(pNode->key, key) == 0)
    {
        //前のノードのnextに次のノードのアドレスを譲渡
        if(prev!=NULL)
        {
        if(pNode->next!=NULL)
        {
            prev->next = pNode->next;
        }
        }
        else
        {
        //最初のノードの場合
        map->hash[mapchain] = pNode->next;
        }
        //ノードのメモリを解放
        free(pNode->key);
        free(pNode->value);
        free(pNode);
        break;
    }
    else
    {
        prev = pNode;
    }
    }
}

int lenStrMap(StrMap* map)
{
    int i,size = 0;
    StrMapNode* pNode;
    for ( i = 0 ; i < HASH_SIZE ; i ++ )
    {
    pNode = map->hash[i];
    while(pNode)
    {
        size++;
        pNode = pNode->next;
    }
    }
    return size;
}

void mapStrMap(StrMap* map, void* buf, void(*func)(void*,void*))
{
    int i,size = 0;
    StrMapNode* pNode;
    for ( i = 0 ; i < HASH_SIZE ; i ++ )
    {
    pNode = map->hash[i];
    while(pNode)
    {
        func(pNode->value, buf);
        pNode = pNode->next;
    }
    }
}

void freeStrMap(StrMap* map)
{
    int i;
    StrMapNode* pNode;
    StrMapNode* pNext;
    for ( i = 0 ; i < HASH_SIZE ; i ++ )
    {
    pNode = map->hash[i];
    while(pNode)
    {
        pNext = pNode->next;
        free(pNode->key);
        free(pNode->value);
        free(pNode);
        pNode = pNext;
    }        
    }
}

void printStrMap(StrMap* map)
{
    StrMapNode* pNode;
    int i;
    printf("map contents\n");
    for ( i = 0 ; i < HASH_SIZE ; i ++ )
    {
    pNode = map->hash[i];
    while(pNode)
    {
        printf("key<%s> = value<%s>\n",pNode->key,(char*)pNode->value);
        pNode = pNode->next;
    }
    }
    printf("map size is %d\n",lenStrMap(map));
}

IntMap* newIntMap()
{
    IntMap* map;
    map = malloc(sizeof(IntMap));
    memset(map,0,sizeof(IntMap));
    return map;
}

void* getIntMap(IntMap* map,int key)
{
    IntMapNode* pNode = map->head;
    for(;pNode!=NULL;pNode=pNode->next)
    {
    if(pNode->key == key)
    {
        return pNode->value;
    }
    }
    return NULL;
}

void insIntMap(IntMap* map, int key, void* value, int size)
{
    IntMapNode* pNode = map->head;
    //新しいノードを予約し、値を埋める。
    IntMapNode* newNode = malloc(sizeof(IntMapNode));
    memset(newNode, 0, sizeof(IntMapNode));
    //keyのメモリを代入
    newNode->key = key;
    //valueのメモリを予約
    newNode->value = malloc(size);
    //valueにデータをコピー
    memcpy(newNode->value, value, size);
    //valueのnextにリンクリストの先頭アドレスを登録
    newNode->next = pNode;
    //ハッシュチェーンの先頭に登録
    map->head = newNode;
    //printf("newNode->key = %s, newNode->value = %s\n",newNode->key,(char*)newNode->value);
}

void delIntMap(IntMap* map, int key)
{
    IntMapNode* pNode = map->head;
    IntMapNode* prev = NULL;
    for(;pNode!=NULL;pNode=pNode->next)
    {
    if(pNode->key == key)
    {
        //前のノードのnextに次のノードのアドレスを譲渡
        if(prev!=NULL)
        {
        prev->next = pNode->next;
        }
        else
        {
        //最初のノードの場合
        map->head = pNode->next;
        }
        //ノードのメモリを解放
        free(pNode->value);
        free(pNode);
        break;
    }
    else
    {
        prev = pNode;
    }
    }
}

int lenIntMap(IntMap* map)
{
    int i,size = 0;
    IntMapNode* pNode;
    pNode = map->head;
    while(pNode)
    {
    size++;
    pNode = pNode->next;
    }
    return size;
}

void freeIntMap(IntMap* map)
{
    int i;
    IntMapNode* pNode;
    IntMapNode* pNext;
    pNode = map->head;
    while(pNode)
    {
    pNext = pNode->next;
    free(pNode->value);
    free(pNode);
    pNode = pNext;
    }        
}


void printIntMap(IntMap* map)
{
    IntMapNode* pNode;
    int i;
    printf("map contents\n");
    pNode = map->head;
    while(pNode)
    {
    printf("key<%d> = value<%s>\n",pNode->key,(char*)pNode->value);
    pNode = pNode->next;
    }
    printf("map size is %d\n",lenIntMap(map));
}

