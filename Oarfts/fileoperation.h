#pragma once

#include "attr.h"
#include "transfer.h"
#include "list.h"
#include <sys/stat.h>

#define PATH_MAZ 256
#define DGRAM_SIZE 4048

typedef enum {
    NONE,// 0
    OPEN,// 1
    CLOSE,// 2
    READ,// 3
    WRITE,// 4
    STAT,// 5
    READDIR,// 6
    HEALTH// 7
} req_t;

typedef enum {
    DENY,
    YES,
    NO,
    DISCONNECT,
    BUSY,
    WAIT
} res_t;

int requestOpen(int sockfd, const char* path, int mode);

int responseOpen(int sockfd, struct Payload request);

int requestClose(int sockfd, int fd);

int responseClose(int sockfd, struct Payload request);

int requestRead(int sockfd, int fd, char* buf, int offset, int size);

int responseRead(int sockfd, struct Payload request);

int requestWrite(int sockfd, int fd, char* buf, int offset, int size);

int responseWrite(int sockfd, struct Payload request);

void swapStat(struct stat* stbuf);

int requestStat(int sockfd, const char* path, struct stat* stbuf);

int responseStat(int sockfd, struct Payload request);

void printdirstat(void* dstat);

List* requestReaddir(int sockfd, const char* path);

int responseReaddir(int sockfd, struct Payload request);

int requestHealth(int sockfd);

int responseHealth(int sockfd, struct Payload request);
