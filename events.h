#ifndef _EVENTS_H
#define _EVENTS_H
#include "parse.h"
#include <stdbool.h>
void* read_conn(void* data);
void* write_conn(void* data);
bool is_buffer_finished(struct buffer);

void set_path(char* path);

void handle_request(struct http_socket*, struct http_request* req);
void handle_static_request(struct http_socket*, struct http_request* req);
void handle_dynamic_request(struct http_socket*, struct http_request* req);
void print_to_buffer(struct buffer*, char* str, ...);
int file_exist(char* filename);
void write_error(struct http_socket*,int error);

void send_plain_text(struct http_socket*, int error);
void send_json(struct http_socket* s, int err);
/* Run a loop for 15 seconds */
int allocanon(void);
/* Returns 1 if none are available to munmap, 0 on success and -1 on error */
int freeanon(void);
int run_loop(void);
void* spin(void* data);
/* Return 1 for DNE, 2 for permission denied */
int file_load(struct http_socket* http, char* filename);
void finish_read(struct http_socket* socket);

#endif
