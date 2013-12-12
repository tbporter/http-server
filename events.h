#include "parse.h"

void* read_conn(void* data);
void handle_request(struct http_socket*, struct http_request* req);
void handle_static_request(struct http_socket*, struct http_request* req);
void handle_dynamic_request(struct http_socket*, struct http_request* req);
void print_to_buffer(struct http_socket* socket, char* str, ...);
int file_exist(char* filename);
void write_error(int error);