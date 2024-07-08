#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

#define MAXLINE 1024

int main() {
    int listenfd, connfd;
    sockaddr_in servaddr;
    char buff[MAXLINE + 1];
    time_t ticks;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) {
        throw std::runtime_error("create socket error");
    }

    std::memset(&buff, 0, sizeof(char) * (MAXLINE + 1));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(1048);

    int binded = bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr));
    if(binded < 0) {
        std::cout << errno << '\n';
        throw std::runtime_error("bind socket error");
    }

    int listend = listen(listenfd, MAXLINE);
    if(listend < 0) {
        std::cout << errno << '\n';
        throw std::runtime_error("listen error");
    }

    while(true) {
        connfd = accept(listenfd, (sockaddr*)NULL, NULL);
        std::cout << errno << '\n';
        ticks = time(0);
        snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
        write(connfd, buff, strlen(buff));

        close(connfd);
    }
}
