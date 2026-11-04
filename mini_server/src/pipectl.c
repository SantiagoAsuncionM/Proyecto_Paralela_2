//
// Created by Sleyter Angulo on 9/17/26.
// Actualizado por Fabricio Arias y Santiago Asuncion
//

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define RESP_MAX 1024
#define CMD_MAX  512

static int send_all(int fd, const char* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int send_request(const struct sockaddr_in* server, const char* command,
        char* resp, size_t cap) {
    int file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (file_descriptor < 0)
        return -1;

    if (connect(file_descriptor, (const struct sockaddr* )server, sizeof* server) < 0) {
        close(file_descriptor);
        return -1;
    }

    // printf("Sending command: %s\n", command);
    if (send_all(file_descriptor, command, strlen(command)) < 0) {
        close(file_descriptor);
        return -1;
    }

    size_t len = 0;
    while (len < cap - 1) {
        ssize_t n = recv(file_descriptor, resp + len, cap - 1 - len, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            close(file_descriptor);
            return -1;
        }
        if (n == 0)
            break;
        len += (size_t)n;
        if (memchr(resp, '\n', len) != NULL)
            break;
    }
    resp[len] = '\0';
    close(file_descriptor);
    return len > 0 ? 0 : -1;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr,
                "usage: %s <host> <port> <command>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server.sin_addr) != 1) {
        fprintf(stderr, "invalid host '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }
    char command[CMD_MAX];
    strcpy(command, argv[3]);
    strcat(command, "\n");
    
    char resp[RESP_MAX];
    if (send_request(&server, command, resp, sizeof resp) < 0)
        return EXIT_FAILURE;

    printf("%s\n", resp);

    if (strncmp(resp, "ERROR", 5) == 0 || strncmp(resp, "BUSY", 4) == 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
