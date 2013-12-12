#ifndef _SERVER_H
#define _SERVER_H
#include <time.h>
#include <sys/epoll.h>
#include "list.h"
#include "threadpool.h"
int open_listenfd(int port);
void* check_connections(void* data);

struct http_socket {
    int fd;
    /*struct pollfd* poll_fd;*/
    struct epoll_event event;
    char* read_buffer;
    int read_buffer_size;
    time_t last_access;
    struct list_elem elem;
};

struct future_elem {
    struct future* future;
    struct list_elem elem;
};
#endif

