#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
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

#define POOL_THREADS 6

/* Properly set reading events */
#define EPOLL_READ EPOLLIN | EPOLLRDHUP | EPOLLONESHOT
#define EPOLL_WRITE EPOLLOUT | EPOLLRDHUP | EPOLLONESHOT
#define EVENT_QUEUE 8

static int port_n = 10394;

static int epoll_fd;

static struct thread_pool* tpool;

static char* relay_server = NULL;
#define RELAY_PREFIX_LENGTH 10
static const char* relay_prefix = "group394\r\n";


int main(int argc, char** argv) {
    int listening_sock;
    struct sockaddr_in clientaddr;
    /* We need this for accept */
    socklen_t clientlen = sizeof(clientaddr);
    
    if (parse_args(argc, argv)) {
        printf("Couldn't parse arguments\n");
        return -1;
    }
         
    /* Initialize the listening socket */
    if ((listening_sock = open_listenfd(port_n)) < 0)
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

    /* If we are connecting to a relay server */
    if (relay_server) {
        if (conn_relay(relay_server)) {
            printf("Failed to setup relay server, double check your argument\n");
            return -1;
        }
    }

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
        if (ready < 0) {
            perror("epoll_wait");
            continue;
        }
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
    struct sockaddr_in6 serveraddr;
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
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
    serveraddr.sin6_family = AF_INET6;
    serveraddr.sin6_addr = in6addr_any;
    serveraddr.sin6_port = htons((unsigned short)port);
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

int conn_relay(char* relay_server) {
    struct addrinfo hints, *server;
    int fd;
    char* port;
    char* hostname = NULL;

    /* First parse out the relay_server */
    int i;
    for (i = 0; relay_server[i] != '\0'; i++) {
        if (relay_server[i] == ':') {
            if ((hostname = malloc(i + 1)) == NULL) {
                perror("Failed to malloc relay server hostname");
                return -1;
            }
            memcpy(hostname, relay_server, i);
            hostname[i] = '\0';
            port = relay_server + i + 1;
            break;
        }
    }
    if (hostname == NULL || port == 0) {
        fprintf(stderr, "Invalid relay server\n");
        return -1;
    }
    DEBUG_PRINT("Detected host: %s, port: %s\n", hostname, port);

    memset(&hints, 0, sizeof hints);
    /* Configure new socket */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port, &hints, &server)) {
        perror("Getting relay server addr info");
        return -1;
    }

    struct addrinfo* c_addr;
    for (c_addr = server; c_addr != NULL; c_addr = c_addr->ai_next) {
        if ((fd = socket(c_addr->ai_family, c_addr->ai_socktype,
                        c_addr->ai_protocol)) == -1) {
            perror("Creating relay server socket");
            continue;
        }
        if (connect(fd, c_addr->ai_addr, c_addr->ai_addrlen) == -1) {
            close(fd);
            perror("Connecting to relay server");
            continue;
        }
        break;
    }
    /* now check if one was successful */
    if (c_addr == NULL) {
        fprintf(stderr, "Unable to connect to relay server\n");
        return -1;
    }
    freeaddrinfo(server);
    free(hostname);
    /* Good we were successful now get it in epoll */
    /* Make socket non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    /* Setup socket struct */
    struct http_socket* http = calloc(1, sizeof(struct http_socket));
    if (http == NULL) {
        perror("Allocating relay server http_socket");
        return -1;
    }
    http->fd = fd;
    http->event.events = EPOLL_READ;
    http->event.data.ptr = http;
    write(fd, relay_prefix, 10);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &http->event) < 0) {
        perror("Adding relay server connection to epoll");
        return -1;
    }
    return 0;
}

int parse_args(int argc, char** argv) {
    while(true) {
        char current_opt = getopt(argc, argv, "r:p:R:");
        if (current_opt == 'r') {
            /* Iterate optind and set relay server */
            DEBUG_PRINT("Setting relay server to %s\n", optarg);
            relay_server = optarg;
        }
        else if (current_opt == 'R') {
            DEBUG_PRINT("Setting server root to %s\n", optarg);
            set_path(optarg);
        }
        else if (current_opt == 'p') {
            DEBUG_PRINT("Setting port to %s\n", optarg);
            port_n = atoi(optarg);
        }
        else if (current_opt == -1) {
            return 0;
        }
        else {
            printf("Unrecognized option -%c\n", current_opt);
            return -1;
        }
    }
}

int watch_read(struct http_socket* http) {
    http->event.events = EPOLL_READ;
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, http->fd, &http->event);
}

void clear_buffers(struct http_socket* http){
    if (http->write.data) {
        free(http->write.data);
        http->write.data = NULL;
        http->write.size = 0;
        http->write.pos = 0;
        http->write.last = 0;
    }
    if (http->mmaped) {
        DEBUG_PRINT("munmap the file because we're done writing\n");
        munmap(http->data.data, http->data.size);
        http->data.data = NULL;
        http->data.size = 0;
        http->data.pos = 0;
        http->data.last = 0;
    }
    if (http->data.data) {
        free(http->write.data);
        http->data.data = NULL;
        http->data.size = 0;
        http->data.pos = 0;
        http->data.last = 0;
    }
    if (http->read_buffer) {
        free(http->read_buffer);
        http->read_buffer = NULL;
        http->read_buffer_size = 0;
    }
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
    if (http->write.data)
        free(http->write.data);
    if (http->data.data) {
        if (http->mmaped) {
            if (munmap(http->data.data, http->data.size)) {
                perror("munmap data buffer in destroy socket");
            }
        }
        else {
            DEBUG_PRINT("%p", http->data.data);
            free(http->data.data);
        }
    }
    free(http);
    return 0;
}
