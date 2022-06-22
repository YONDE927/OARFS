#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_SIZE   13

typedef struct StrMapNode{
    struct StrMapNode* next;
    char* key;
    void* value;
} StrMapNode;

typedef struct StrMap{
    StrMapNode* hash[HASH_SIZE];
    int size;
} StrMap;

StrMap* newStrMap();
void* getStrMap(StrMap* map,char* key);
void insStrMap(StrMap* map, char* key, void* value, int size);
void delStrMap(StrMap* map, char* key, void(*fptr)(void*));
int lenStrMap(StrMap* map);
void freeStrMap(StrMap* map);
void printStrMap(StrMap* map);
void mapStrMap(StrMap* map, void* buf, void(*func)(void*,void*));

typedef struct IntMapNode{
    struct IntMapNode* next;
    int key;
    void* value;
} IntMapNode;

typedef struct IntMap{
    IntMapNode* head;
    int size;
} IntMap;

IntMap* newIntMap();
void* getIntMap(IntMap* map, int key);
void insIntMap(IntMap* map, int key, void* value, int size);
void delIntMap(IntMap* map, int key);
int lenIntMap(IntMap* map);
void freeIntMap(IntMap* map);
void printIntMap(IntMap* map);

