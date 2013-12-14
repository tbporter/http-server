#ifndef _SERVER_H
#define _SERVER_H
#include <time.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include "list.h"
#include "threadpool.h"
#include "parse.h"

struct http_socket;
struct http_req_buffer;

int open_listenfd(int port);
void* check_connections(void* data);
int watch_read(struct http_socket* http);
int watch_write(struct http_socket* http);
int destroy_socket(struct http_socket* http);
int conn_relay(char* relay_server);
int parse_args(int, char**);
void clear_buffers(struct http_socket* http);

struct buffer {
    char* data;
    int size;
    int pos;
    int last;
};

struct http_socket {
    int fd;
    /*struct pollfd* poll_fd;*/
    struct epoll_event event;
    char* read_buffer;
    int read_buffer_size;

    struct buffer write;
    struct buffer data;
    bool mmaped;
    time_t last_access;
    struct list_elem elem;

    bool keep_alive;
};

struct future_elem {
    struct future* future;
    struct list_elem elem;
};
#endif

