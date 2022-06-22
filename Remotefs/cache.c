#include <stdio.h>
#include <string.h>
#include "list.h"
#include "map.h"
#include "conn.h"

#define BLOCK_SIZE 10240 // 10KB
#define MAX_BLOCK_NUM 100 // 10KB
#define MAX_FILE_SIZE 1024000 //1MB
#define MAX_CACHE_SIZE 104857600 //100MB


//ファイルごとにキャッシュ＆統合もできる
typedef struct CacheBlock
{
    char* buffer;
    int index;
} CacheBlock;

typedef struct Cache
{
    CacheBlock* blocks[MAX_BLOCK_NUM]; //CacheBlock
    int opened;
    int size;
    int mtime;
    int dirty;
} Cache;

typedef struct MemCache
{
    List* cache_queue; //Cache
    StrMap* cache_map;
} MemCache;

//シングルトンMemCacheを取得する。
MemCache* getMemCache()
{
    static MemCache* memcache = NULL;
    if(memcache == NULL)
    {
    memcache = malloc(sizeof(MemCache));
    memcache->cache_queue = newList();
    memcache->cache_map = newStrMap();
    }
    return memcache;
}

/*総キャッシュサイズを調べるためのカウンタ*/
void measureCache(void* cache, void* size)
{
    *(int*)size += ((Cache*)cache)->size;
}

//総キャッシュサイズを調べる
int sizeofMemCache(MemCache* memcache)
{
    int size;
    mapStrMap(memcache->cache_map, &size, measureCache);
    return size;
}

//MemCacheからキャッシュを検索する。
Cache* lookupCache(char* path)
{
    Cache* cache;
    MemCache* memcache = getMemCache();

    if(memcache == NULL)
    {
    return NULL;
    }
    cache = getStrMap(memcache->cache_map, path);
    return cache;
}

CacheBlock* newBlock(int index)
{
    CacheBlock* block;

    block = malloc(sizeof(CacheBlock));
    block->buffer = NULL;
    block->index = index;
    return block;
}

Cache* newCache(int size)
{
    Cache* cache;
    CacheBlock* block;

    cache = malloc(sizeof(Cache));
    memset(cache, 0, sizeof(Cache));
    return cache;
}

int slimCache(MemCache* memcache, int size);
//MemCacheにcacheを登録する。キャッシュサイズが大きければ古いキャッシュを削除する。
int registerCache(char* path, Cache* cache)
{
    MemCache* memcache = getMemCache();

    if(memcache == NULL)
    {
    return -1;
    }
    slimCache(memcache, cache->size);
    insStrMap(memcache->cache_map, path, cache, sizeof(Cache)); 
    push_back(memcache->cache_queue, path, strlen(path) + 1);
    return 0;
}

//キャッシュが最新の状態か修正日時を用いてチェックする。
int validateCache(Cache* cache, int mtime)
{
    if(cache == NULL)
    {
    return -1;
    }
    if(cache->mtime > mtime)
    {
    return 1;
    }
    return 0;
}

void freeBlock(void* block)
{
    CacheBlock* pblock = block;

    if(pblock->buffer != NULL)
    {
    free(pblock->buffer);
    }
    free(pblock);
}

void freeCache(void* cache)
{
    Cache* pcache = cache;
    for(int i = 0; i < MAX_BLOCK_NUM; i++)
    {
    if(pcache->blocks[i] != NULL)
    {
        if(pcache->blocks[i]->buffer != NULL)
        {
        free(pcache->blocks[i]->buffer);
        }
        free(pcache->blocks[i]);
    }
    }
    free(pcache);
}

//キャッシュを削除
void delCache(MemCache* memcache, char* path)
{
    delStrMap(memcache->cache_map, path, freeCache);
}

//古いキャッシュをサイズ分削除
int slimCache(MemCache* memcache, int size)
{
    int cache_size, room;
    char* path;
    List* queue;
    Cache* cache;
    
    queue = memcache->cache_queue;
    cache_size = sizeofMemCache(memcache);
    room = MAX_CACHE_SIZE - cache_size;
    //余地があれば終了
    if(room > size)
    {
    return 0;
    }
    //余地がサイズ分を超えるまでキューの前から削除
    while(room < size)
    {
    path = queue->head->data;
    cache = lookupCache(path);
    room += cache->size;
    delCache(memcache, path);
    pop_front(queue, free);
    }
    return 0;
}

int writeBlock(CacheBlock* block, char* buf, int offset, int size)
{
    if(block->buffer == NULL)
    {
    block->buffer = malloc(BLOCK_SIZE);
    }
    if((offset > BLOCK_SIZE) | (size > BLOCK_SIZE))
    {
    return -1;
    }
    memcpy(block->buffer + offset, buf, size);
    return size;
}

int writeCache(Cache* cache, char* buf, int offset, int size);

//キャッシュブロックからバッファへコピー
int readBlock(CacheBlock* block, char* buf, int offset, int size)
{
    if((offset > BLOCK_SIZE) | (size > BLOCK_SIZE))
    {
    return -1;
    }
    memcpy(buf, block->buffer + offset, size);
    return size;
}

//キャッシュがあれば該当部分をコピー、なければ相当部分を外部へリクエスト
int readCache(Cache* cache,FileSession* session, char* buf, int offset, int size)
{
    int start, end, nread,read_size, read_offset, rc;
    CacheBlock* block;
    
    nread = size;
    start = offset / BLOCK_SIZE;
    end = (offset + size) / BLOCK_SIZE;
    for(int i = start; i <= end; i++)
    {
        if(size > BLOCK_SIZE)
        {
            read_size = BLOCK_SIZE;
        }
        else
        {
            read_size = size;
        }

        if(offset > (i * BLOCK_SIZE))
        {
            read_offset = offset - (i * BLOCK_SIZE);
        }
        else
        {
            read_offset = 0;
        }
        //キャッシュ読み込み
        if(cache->blocks[i] != NULL)
        {
            rc = readBlock(cache->blocks[i], buf, read_offset, read_size);
            if(rc < 0)
            {
                return -1;
            }
            buf += rc;
            }
        else
        {
            //外部リクエスト
            rc = connRead(session, read_offset, buf, read_size);
            if(rc < 0)
            {
                return -1;
            }
            buf += rc;
            //キャッシュ追加
            size -= read_size;
        }
    }
    return nread;
}

//バッファをキャッシュに書き込む。書き込み場所によっては既存キャッシュを統合・上書きする。

int main()
{
    MemCache* memcache = getMemCache();
    printf("%d files are cached.\n", lenStrMap(memcache->cache_map));
    return 0;
}
