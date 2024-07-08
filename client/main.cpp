#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

#define MAXLINE 1000

int main(int argc, char** argv) {
    int sockfd, n;

    char recvline[MAXLINE + 1];
    sockaddr_in servaddr;

    if(argc != 2)
        throw std::runtime_error("no argument provided");

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw std::runtime_error("socket error");

    std::memset(&recvline, 0, sizeof(char) * MAXLINE + 1);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1048);

    if(inet_pton(AF_INET, argv[1], &servaddr.sin_addr) < 0)
        throw std::runtime_error("inet pton error");

    auto error = connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr));

    if(error < 0) {
        std::cout << errno << '\n';
        throw std::runtime_error("connect error");
    }

    while((n = read(sockfd, recvline, MAXLINE)) > 0) {
        recvline[n] = 0;
        std::cout << recvline << '\n';
    }

    if(n < 0)
        throw std::runtime_error("connect error");

    return 0;
}

