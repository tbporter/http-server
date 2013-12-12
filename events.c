#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "parse.h"
#include "events.h"
#include "debug.h"
#define BUF_SIZE 256

void* read_conn(void* data){
	
	struct http_socket* socket = (struct http_socket*) data;

	int count = 0;
	char temp_buffer[BUF_SIZE];
	do{
		count = read(socket->fd, temp_buffer, BUF_SIZE);
		if(count >= 0){
			socket->read_buffer = realloc(socket->read_buffer, socket->read_buffer_size + count); //TODO: make sure realloc works
			memcpy(socket->read_buffer + socket->read_buffer_size, temp_buffer, count);
			socket->read_buffer_size += count;
			DEBUG_PRINT("Read in %d bytes to buffer", count);
		}
	}while(count);

  	//Save room on the stack
	char m[BUF_SIZE],u[BUF_SIZE],v[BUF_SIZE];
	struct http_request req = {m, u, v};

	if(parse_header(socket->read_buffer, socket->read_buffer_size, &req)){
		DEBUG_PRINT("HTTP request- method: %s, uri: %s, ver: %s", req.method, req.uri, req.ver);
		return NULL;
	}
	else{
		DEBUG_PRINT("no http request read");
		return NULL;
	}
}