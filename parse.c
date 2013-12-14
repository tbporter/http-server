#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "parse.h"
#include "debug.h"
#include <ctype.h>
#define BUF_LEN 4096

int parse_is_header_finished(char* buf, int buf_len){
    int i;  	
    for(i=0; i<buf_len - 3; i++){

    	if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
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
	if(str_s_len>BUF_LEN)
		str_s_len = BUF_LEN;
	strncpy(tmp, str_s, str_s_len);

	sscanf(tmp, "%s %s %s\n", req->method, req->uri, req->ver);
	parse_uri_callback(req);

	int i;
	DEBUG_PRINT("START OF BUFFER READ\n");
	for(i=0; i<buf_len; i++){
		DEBUG_PRINT("%c", buf[i]);
	}
	DEBUG_PRINT("\nEND OF BUFFER READ\n");
	return 1;
}

 void parse_uri_callback(struct http_request* r){
	int i;

	r->cb[0] = '\0';

	//Find the ?
	for(i=0; r->uri[i] != '\0' && i< BUF_LEN; i++){
		if(r->uri[i] == '?'){
			break;
		}
	}
	//If it's not a ?, return
	if( i>= BUF_LEN || r->uri[i] != '?'){
		DEBUG_PRINT("no callback last char: %c\n", r->uri[i]);
		return;
	}
	r->uri[i] = '\0';
	i++; //increment past ?


	for(; i!= '\0' && i<BUF_LEN; i++){
		
		if(strncmp("callback=", r->uri +i ,9) == 0){
			int j;
			i = i + 9;
			for(j=0; j<BUF_LEN; j++){
				if(!(isalnum(r->uri[i]) || r->uri[i] == '_' || r->uri[i] == '.')){
					r->cb[j] = '\0';
					DEBUG_PRINT("CALLBACK = %s\n", r->cb);
					return;
				}
				r->cb[j] = r->uri[i];
				i++;
			}		
		}
		
		while(i<BUF_LEN && r->uri[i] != '&' && r->uri[i] != '\0'){
			i++;
		}
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