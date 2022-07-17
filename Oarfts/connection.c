#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "connection.h"

/*Server*/
int getServerSock(short port){
    int rc, tmp;
    int listenfd;
    struct sockaddr_in servaddr;
    int yes = 1;

    //ソケット生成
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        printf("create sock fail\n");
        return -1;
    }

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0){
        return -1;
    }

    //アドレス生成
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    //ポート接続
    rc = bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(rc < 0){
        printf("bind fail\n");
        return -1;
    }

    //接続待ち
    rc = listen(listenfd, 5);
    if(rc < 0){
        printf("listen fail\n");
        return -1;
    }
    return listenfd;
}

int acceptSock(int listenfd){
    struct sockaddr_in cliaddr;
    int rc, tmp, clientfd;
    //クライアントソケット生成
    tmp = sizeof(cliaddr);
    clientfd = accept(listenfd, (struct sockaddr*)&cliaddr, (socklen_t*)&tmp);
    if(clientfd < 0){
        printf("accept fail\n");
        return -1;
    }

    //ソケット設定
    int yes;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if(setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0){
        return -1;
    }

    char str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &cliaddr.sin_addr, str, INET_ADDRSTRLEN);
    printf("accept ip %s\naccept port %d\n", str, cliaddr.sin_port);

    return clientfd;
}

/*client*/
int getClientSock(char* ip, short port){
    int rc = 0;
    int yes = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct sockaddr_in cliaddr;
    
    //ソケット生成
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        printf("create sock fail\n");
        printf("%s\n", strerror(errno));
        return -1;
    }

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0){
        return -1;
    }

    //アドレス生成
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);

    //接続
    rc = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(rc < 0){
        printf("connect fail\n");
        return -1;
    }

    return sockfd;
}

void sigpipeHandler(int sig){
    printf("SIGPIPE\n");
    return;
}
