#include "../../include/protocol.h"

#include <asm-generic/errno-base.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>



int send_all(int sockfd, const void *buffer, size_t len) {
    size_t total_sent = 0;
    ssize_t bytes_sent;

    while (total_sent < len) {
        
        bytes_sent = send(sockfd, (const char*)buffer + total_sent, len - total_sent, 0);

        if (bytes_sent == -1) {
            
            if (errno == EINTR) continue; // Прервано сигналом, пробуем снова
            perror("send_all");

            return -1;

        }

        total_sent += bytes_sent;
    }
    return 0;
}

/*
    Возвращает:
    > 0 - кол-во прочитанных байт
    -1 - ошибка
*/

int recv_all(int sockfd, void *buffer, size_t len) {
    ssize_t total_received = 0;
    ssize_t bytes_received;

    while(total_received < len) {

        bytes_received = recv(sockfd, (char*)buffer + total_received, len - total_received, MSG_WAITALL);

        if (bytes_received == -1) {

            if(errno == EINTR) continue; // прервано сигналом - повторяем
            
            perror("recv all");
            return -1;

        }

        if(bytes_received == 0) {

            fprintf(stderr, "connection close by peer\n");
            return -1;
            
        }

        total_received += bytes_received;

    }

    return (ssize_t)total_received; // успешно прочитано 'len' байт
}

