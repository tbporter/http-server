#include "server.h"
#include "parse.h"
#include "events.h"

#define BUF_SIZE 256

void* read_conn(void* data){
	
	struct http_socket* socket = (struct http_socket*) data;
	//Save room on the stack
	char m[BUF_SIZE],u[BUF_SIZE],v[BUF_SIZE];
	struct http_request req = {m, u, v};

	while(!parse_header(socket->read_buffer, socket->buf_size, &req)){
		//do nothing until we get a good header
	}
	return NULL;
}