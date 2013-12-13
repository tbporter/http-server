#ifndef _PARSE_H
#define _PARSE_H
struct http_request {
    char* method;
    char* uri;
    char* ver;
    char* cb;
};

int parse_is_header_finished(char* buf, int buf_length);
int parse_header(char* buf, int buf_length, struct http_request* req);
int parse_string(char* start, int buf_len);
void parse_uri_callback(struct http_request* r);
#endif