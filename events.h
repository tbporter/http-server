#ifndef _EVENTS_H
#define _EVENTS_H
#include "parse.h"
#include <stdbool.h>
void* read_conn(void* data);
void* write_conn(void* data);
bool is_write_buffer_finished(struct http_socket*);
bool is_data_buffer_finished(struct http_socket*);

void handle_request(struct http_socket*, struct http_request* req);
void handle_static_request(struct http_socket*, struct http_request* req);
void handle_dynamic_request(struct http_socket*, struct http_request* req);
void print_to_buffer(struct http_socket* socket, char* str, ...);
int file_exist(char* filename);
void write_error(int error);

/* Run a loop for 15 seconds */
int allocanon(void);
int freeanon(void);
int run_loop(void);
void* spin(void* data);
/* Return 1 for DNE, 2 for permission denied */
int file_load(struct http_socket* http, char* filename);
void finish_read(struct http_socket* socket);


#endif
