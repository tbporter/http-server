#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <error.h>
#include <pthread.h>

#include "server.h"
#include "debug.h"

#define SA struct sockaddr

#define BACKLOG_QUEUE 50
#define LISTEN_PORT 9000

static pthread_mutex_t connections_mutex = PTHREAD_MUTEX_INITIALIZER;
static fd_set connections;
static int connections_max = 0;

static pthread_mutex_t socket_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct list socket_list;

int main(int argv, char** argc) {
    int listening_sock;
    struct sockaddr_in clientaddr;
    /* Initialize the listening socket */
    if ((listening_sock = open_listenfd(LISTEN_PORT)) < 0)
        return -1;
    
    /* Take care of the socket_list */
    list_init(&socket_list);

    /* Setup the fdset for select */
    FD_ZERO(&connections);

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
        DEBUG_PRINT("Add connection %d to fdset\n", connection);
        FD_SET(connection, &connections);
        if (connections_max < connection)
            connections_max = connection;
        pthread_mutex_unlock(&connections_mutex);
    }

    return 0;
}

void* check_connections(void* data) {
    struct timeval timeout;
    /* TODO: All the error handling */
    while (true) {
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        pthread_mutex_lock(&connections_mutex);
        int ready = select(connections_max + 1, &connections, NULL, NULL, &timeout);
        DEBUG_PRINT("Selecting with %d ready and %d max\n", ready, connections_max);
        int handled = 0;
        pthread_mutex_unlock(&connections_mutex);
        /* short circuit locking */
        if (ready == 0) {
            sched_yield();
            continue;
        }
        DEBUG_PRINT("Readable connections exist!\n");
        /* Check readable connections */
        struct list_elem* current;
        /* We need to lock this down while grepping it */
        pthread_mutex_lock(&socket_list_mutex);
        for (current = list_front(&socket_list); 
                handled < ready; 
                current = list_next(current)) {
            /* Get the current socket from the list */
            struct http_socket* current_socket = list_entry(current, struct
                    http_socket, elem);
            /* Check if it is ready */
            if (FD_ISSET(current_socket->fd, &connections)) {
                DEBUG_PRINT("Ready socket %d, removing from list\n", current_socket->fd);
                /* TODO: Let other function add it back in when it's ready */
                FD_CLR(current_socket->fd, &connections);
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
