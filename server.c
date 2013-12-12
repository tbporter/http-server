#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <error.h>
#include <pthread.h>
#include <poll.h>
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

static pthread_mutex_t connections_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct pollfd* connections;
static int connections_n = 0;

static pthread_mutex_t socket_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list socket_list;

static struct thread_pool* tpool;

static pthread_mutex_t future_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list future_list;

int main(int argv, char** argc) {
    int listening_sock;
    struct sockaddr_in clientaddr;
    /* Initialize the listening socket */
    if ((listening_sock = open_listenfd(LISTEN_PORT)) < 0)
        return -1;
    
    /* Take care of the lists */
    list_init(&socket_list);
    list_init(&future_list);
    /* Thread pool initialization */
    tpool = thread_pool_new(POOL_THREADS);

    /* Setup the pollfd* for poll */
    if ((connections = calloc(1024, sizeof(struct pollfd))) == NULL) {
        perror("Allocating pollfd array");
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
        /* Lets set this connection correctly */
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
        /* Lock it down while we access it */
        /* TODO: All the error checks */
        pthread_mutex_lock(&socket_list_mutex);
        DEBUG_PRINT("Add new socket fd:%d to socket_list\n", connection);
        list_push_front(&socket_list, &new_socket->elem);
        pthread_mutex_unlock(&socket_list_mutex);
        /* Now set up the fdset */
        pthread_mutex_lock(&connections_mutex);
        DEBUG_PRINT("Add connection %d to pollfd\n", connection);
        connections[connections_n].fd = connection;
        connections[connections_n].events = POLLIN;
        ++connections_n;
        /* Get the pointer right */
        new_socket->poll_fd = connections + connections_n - 1;
        pthread_mutex_unlock(&connections_mutex);
    }

    return 0;
}

void* check_connections(void* data) {
    /* TODO: All the error handling */
    while (true) {
        int ready = poll(connections, connections_n, 2000);
        DEBUG_PRINT("Polling with %d ready and %d number\n", ready, connections_n);
        int handled = 0;
        /* short circuit locking */
        if (ready == 0) {
            sched_yield();
            continue;
        }
        DEBUG_PRINT("%d readable connections exist!\n", ready);
        /* Check readable connections */
        struct list_elem* current;
        /* We need to lock this down while grepping it */
        pthread_mutex_lock(&socket_list_mutex);
        int i = 0;
        for (; i < connections_n; i++) {
            DEBUG_PRINT("%d - %hd\n", connections[i].fd, connections[i].revents);
        }
        for (current = list_front(&socket_list); 
                handled < ready; 
                current = list_next(current)) {
            /* Get the current socket from the list */
            struct http_socket* current_socket = list_entry(current, struct
                    http_socket, elem);
            struct pollfd* poll_fd = current_socket->poll_fd;
            /* Check if it is ready */
            DEBUG_PRINT("%p - %x\n", poll_fd, poll_fd->revents);
            if (poll_fd->fd > 0 && poll_fd->revents & POLLIN) {
                poll_fd->fd = -1;
                DEBUG_PRINT("Ready socket %d, removing from list\n",
                        current_socket->fd);
                /* TODO: Let other function add it back in when it's ready */
                struct future_elem* current_future = calloc(1, sizeof(struct future_elem));
                if (current_future == NULL) {
                    perror("Allocating future_elem after a poll\n");
                    exit(EXIT_FAILURE);
                }
                current_future->future = thread_pool_submit(tpool, read_conn,
                        (void*) current_socket);
                /* Add future to list */
                pthread_mutex_lock(&future_mutex);
                list_push_back(&future_list, &current_future->elem);
                pthread_mutex_unlock(&future_mutex);
                /* TODO: call appropiate function */
                ++handled;
            }
        }
        pthread_mutex_unlock(&socket_list_mutex);
        sched_yield();
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
