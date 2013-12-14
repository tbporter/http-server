#ifndef _PARSE_H
#define _PARSE_H

#include <stdbool.h>
#include "server.h"
struct buffer;
struct http_request {
    char* method;
    char* uri;
    char* ver;
    char* cb;
    bool keep_alive;
};

int parse_is_header_finished(char* buf, int buf_length);
int parse_header(char* buf, int buf_length, struct http_request* req);
int parse_string(char* start, int buf_len);
void parse_uri_callback(struct http_request* r);
#endif