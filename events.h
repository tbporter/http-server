#ifndef _EVENTS_H
#define _EVENTS_H
#include "parse.h"

void* read_conn(void* data);
void handle_request(struct http_socket*, struct http_request* req);
void handle_static_request(struct http_socket*, struct http_request* req);
void handle_dynamic_request(struct http_socket*, struct http_request* req);
void print_to_buffer(struct http_socket* socket, char* str, ...);
int file_exist(char* filename);
void write_error(int error);

/* Run a loop for 15 seconds */
int run_loop(void);
void* spin(void* data);
int file_load(struct http_socket* http, char* filename);
#endif
