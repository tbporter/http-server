struct http_header {
    char* method;
    char* uri;
    char* ver;
};

int parse_is_header_finished(char* buf, int buf_length);
void parse_header(char* buf, int buf_length, struct http_header* header);
int parse_string(char* start, int buf_len);