#ifndef _SERVER_H
#define _SERVER_H
#include <time.h>
#include <sys/epoll.h>
#include "list.h"
#include "threadpool.h"
struct http_socket;

int open_listenfd(int port);
void* check_connections(void* data);
int watch_read(struct http_socket* http);
int watch_write(struct http_socket* http);
int destroy_socket(struct http_socket* http);

struct http_socket {
    int fd;
    /*struct pollfd* poll_fd;*/
    struct epoll_event event;
    char* read_buffer;
    int read_buffer_size;
    char* write_buffer;
    int write_buffer_size;
    int write_buffer_pos;
    time_t last_access;
    struct list_elem elem;
};

struct future_elem {
    struct future* future;
    struct list_elem elem;
};
#endif

