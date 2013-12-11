#include <stdio.h>
#include <stdbool.h>
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
static int connections_n = 0;

int main(int argv, char** argc) {
    int listening_sock;
    struct sockaddr_in clientaddr;
    /* Initialize the listening socket */
    if ((listening_sock = open_listenfd(LISTEN_PORT)) < 0)
        return -1;

    socklen_t clientlen = sizeof(clientaddr);

    FD_ZERO(&connections);

    pthread_t polling;
    pthread_create(&polling, NULL, check_connections, NULL);

    /* Accept a connection */
    while (true) {
        int connection;
        if ((connection = accept(listening_sock, (SA *)&clientaddr,
                        &clientlen)) < 0) {
            perror("Waiting on a connection");
            return -1;
        }
        pthread_mutex_lock(&connections_mutex);
        DEBUG_PRINT("Add connection %d\n", connection);
        FD_SET(connection, &connections);    
        connections_n++;
        pthread_mutex_unlock(&connections_mutex);
        
    }

    return 0;
}

void* check_connections(void* data) {
    struct timeval timeout = {0, 0};
    while (true) {
        pthread_mutex_lock(&connections_mutex);
        select(connections_n, &connections, NULL, NULL, &timeout);
        /* Check readable connections */
        pthread_mutex_unlock(&connections_mutex);
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
