#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <error.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "server.h"
#include "events.h"
#include "threadpool.h"
#include "debug.h"

#define SA struct sockaddr

#define BACKLOG_QUEUE 50
#define LISTEN_PORT 9000

#define POOL_THREADS 6

/* Properly set reading events */
#define EPOLL_READ EPOLLIN | EPOLLRDHUP | EPOLLONESHOT
#define EPOLL_WRITE EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT
#define EVENT_QUEUE 8

static int epoll_fd;

static struct thread_pool* tpool;

int main(int argv, char** argc) {
    int listening_sock;
    struct sockaddr_in clientaddr;
    /* Initialize the listening socket */
    if ((listening_sock = open_listenfd(LISTEN_PORT)) < 0)
        return -1;
    
    /* Thread pool initialization */
    tpool = thread_pool_new(POOL_THREADS);

    /* Set up the read epoll */
    if ((epoll_fd = epoll_create(1)) < 0) {
        perror("Creating read epoll\n");
        return -1;
    }

    /* Start the polling thread */
    pthread_t polling;
    pthread_create(&polling, NULL, check_connections, NULL);

    /* We need this for accept */
    socklen_t clientlen = sizeof(clientaddr);
    /* Accept a connection */
    while (true) {
        int connection;
        DEBUG_PRINT("Waiting for new connections\n");
        if ((connection = accept(listening_sock, (SA *)&clientaddr,
                        &clientlen)) < 0) {
            perror("Waiting on a connection");
            return -1;
        }
        DEBUG_PRINT("Got ahold of new connection\n");

        /* Make socket non-blocking */
        int flags = fcntl(connection, F_GETFL, 0);
        fcntl(connection, F_SETFL, flags | O_NONBLOCK);

        /* Handle our structures first in case there is an intermediate read */
        struct http_socket* new_socket;
        if ((new_socket = calloc(1, sizeof(struct http_socket))) == NULL) {
            perror("Allocating a new http_socket");
            return -1;
        }
        new_socket->fd = connection;
        /* TODO: Set this to current time */
        new_socket->last_access = 0;
        /* TODO: All the error checks */
        new_socket->event.events = EPOLL_READ;
        new_socket->event.data.ptr = new_socket;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connection, &new_socket->event) < 0) {
            perror("Adding new connection to epoll");
            return -1;
        }
    }
    return 0;
}

void* check_connections(void* data) {
    struct epoll_event ready_events[EVENT_QUEUE];
    /* TODO: All the error handling */
    while (true) {
        int ready = epoll_wait(epoll_fd, ready_events, 10, -1);
        DEBUG_PRINT("epoll with %d ready\n", ready);
        int i;
        for (i = 0; i < ready; i++) {
            if (ready_events[i].events & EPOLLRDHUP || ready_events[i].events & EPOLLERR || ready_events[i].events & EPOLLHUP) {
                /* TODO: remove/close fd */
                DEBUG_PRINT("Found a socket that should die\n");
                destroy_socket((struct http_socket*) ready_events[i].data.ptr);
            }
            else if (ready_events[i].events & EPOLLIN) {
                DEBUG_PRINT("Found a read event for %d\n",
                        ((struct http_socket*) ready_events[i].data.ptr)->fd);
                /* TODO: memory leak from allocated future */
                thread_pool_submit(tpool, read_conn, (void*)
                        ready_events[i].data.ptr);
            }
            else if (ready_events[i].events & EPOLLOUT) {
                DEBUG_PRINT("Found a write availability for %d\n",
                        ((struct http_socket*) ready_events[i].data.ptr)->fd);
                thread_pool_submit(tpool, write_conn, (void*) ready_events[i].data.ptr);
            }
        }
    }
}

int open_listenfd(int port) {
    /* From the textbook */
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("On initial socket opening");
        return -1;
    }

    /* Eliminates "Address already in use" error from bind */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
            (const void *)&optval , sizeof(int)) < 0) {
        perror("On setting socket properties");
        return -1;
    }

    /* Listenfd will be an end point for all requests to port
    on any IP address for this host */
    /* TODO error crap */
    memset((void *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("Binding socking");
        return -1;
    }

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, BACKLOG_QUEUE) < 0) {
        perror("Setting socket to listen");
        return -1;
    }

    return listenfd;
}

int watch_read(struct http_socket* http) {
    http->event.events = EPOLL_READ;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, http->fd, &http->event);
}
int watch_write(struct http_socket* http) {
    http->event.events = EPOLL_WRITE;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, http->fd, &http->event);
}
int destroy_socket(struct http_socket* http){
    DEBUG_PRINT("Deallocating socket\n");
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, http->fd, &http->event)) {
        perror("Deleting socket from epoll");
    }
    if (close(http->fd)) {
        perror("Failed to close http_socket fd -- note this may be valid");
    }
    if (http->read_buffer)
        free(http->read_buffer);
    if (http->write_buffer)
        free(http->write_buffer);
    if (http->data_buffer) {
        if (http->mmaped) {
            if (munmap(http->data_buffer, http->data_buffer_size)) {
                perror("munmap data buffer in destroy socket");
            }
        }
        else {
            free(http->data_buffer);
        }
    }
    free(http);
    return 0;
}
