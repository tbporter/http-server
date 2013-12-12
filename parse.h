#ifndef _PARSE_H
#define _PARSE_H
struct http_request {
    char* method;
    char* uri;
    char* ver;
};

int parse_is_header_finished(char* buf, int buf_length);
int parse_header(char* buf, int buf_length, struct http_request* req);
int parse_string(char* start, int buf_len);

#endif