#include <stdio.h>
#include <string.h>
#include "parse.h"

#define BUF_LEN 512

int parse_is_header_finished(char* buf, int buf_len){
    int i;
    for(i=0; i<buf_len-1; i++){
        if(!strncmp(buf+i,"\r\n",2)){
            return 1;
        }
    }
    return 0;
}

void parse_header(char* buf, int buf_len, struct http_header* header){
	if(!parse_is_header_finished(buf,buf_len)){
		return;
	}
	char tmp[BUF_LEN];
	char* str_s = buf;
	int str_s_len;


	str_s_len = parse_string(str_s, buf_len);
	if(str_s_len<0)
		return; //TODO: error!

	strncpy(tmp, str_s, str_s_len);
	sscanf(tmp, "%s %s %s\n", header->method, header->uri, header->ver);
}

int parse_string(char* start, int buf_len){
	int i;
	for(i=0; i<buf_len;i++){
		if(start[i] == '\0')
			return i;
	}

	return -1;
}