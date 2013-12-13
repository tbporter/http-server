#include <stdio.h>
#include <string.h>
#include "parse.h"
#include "debug.h"
#include <ctype.h>
#define BUF_LEN 1024

int parse_is_header_finished(char* buf, int buf_len){
    int i;
    for(i=0; i<buf_len-1; i++){
        if(!strncmp(buf+i,"\r\n",2)){
            return 1;
        }
    }
    return 0;
}

int parse_header(char* buf, int buf_len, struct http_request* req){
	if(!parse_is_header_finished(buf,buf_len)){
		return 0;
	}
	char tmp[BUF_LEN];
	char* str_s = buf;
	int str_s_len;


	str_s_len = parse_string(str_s, buf_len);
	if(str_s_len<0){
		DEBUG_PRINT("no string!\n");
		return 0;
	}

	strncpy(tmp, str_s, str_s_len);

	sscanf(tmp, "%s %s %s\n", req->method, req->uri, req->ver);
	parse_uri_callback(req);
	return 1;
}

 void parse_uri_callback(struct http_request* r){
	int i = 0;

	r->cb[0] = '\0';
	while(r->uri[i] != '\0' && i<BUF_LEN){
		if(r->uri[i] == '?'){
			strcpy(r->cb, r->uri + i + 1);
			r->uri[i] = '\0';
			break;
		}
		i++;
	}

	if(strncmp ("callback=", r->cb, 9) != 0){
		DEBUG_PRINT("no callback");
		r->cb[0] = '\0';
		return;
	}
	strcpy(r->cb, r->cb + 9);
	i = 10;
	while(r->cb[i] != '\0' && i<BUF_LEN){
		if(!(isalnum(r->cb[i]) || r->cb[i] == '_' || r->cb[i] == '.')){
			r->cb[i] = '\0';
			break;
		}
		i++;
	}
}

int parse_string(char* start, int buf_len){
	int i;
	for(i=0; i<buf_len;i++){
		if(start[i] == '\n')
			return i;
	}

	return -1;
}