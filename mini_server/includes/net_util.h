//
// Created by Sleyter Angulo on 9/14/26.
//

#ifndef MINI_SERVER_NET_UTIL_H
#define MINI_SERVER_NET_UTIL_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    int file_descriptor;
    unsigned long connection_id;
} connection_t;

int nu_listen(unsigned short port, int backlog);

int nu_write_all(int file_descriptor, const void *buffer, size_t size);

int nu_read_all(int fd, char* buf, size_t n);

ssize_t nu_drain_request(int file_descriptor);

int nu_send_response(int file_descriptor, unsigned long connection_id);
ssize_t nu_read_line(int fd, char *buf, size_t cap);

#endif //MINI_SERVER_NET_UTIL_H