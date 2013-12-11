#ifndef _SERVER_H
#define _SERVER_H
#include <time.h>
#include "list.h"
int open_listenfd(int port);
void* check_connections(void* data);

struct http_socket {
    int fd;
    struct pollfd* poll_fd;
    char* read_buffer;
    int buf_size;
    time_t last_access;
    struct list_elem elem;
};
#endif

