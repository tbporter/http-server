#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "server.h"
#include "parse.h"
#include "events.h"
#include "debug.h"

#define BUF_SIZE 256
#define OUT_BUF_ALLOC_SIZE 1024

void* read_conn(void* data){
	
	struct http_socket* socket = (struct http_socket*) data;

	int count = 0;
	char temp_buffer[BUF_SIZE];
	do{
		count = read(socket->fd, temp_buffer, BUF_SIZE);
		if(count > 0){
			socket->read_buffer = realloc(socket->read_buffer, socket->read_buffer_size + count); //TODO: make sure realloc works
			memcpy(socket->read_buffer + socket->read_buffer_size, temp_buffer, count);
			socket->read_buffer_size += count;
			DEBUG_PRINT("Read in %d bytes to buffer\n", count);
		}
	}while(count>0);
  	//Save room on the stack
	char m[BUF_SIZE],u[BUF_SIZE],v[BUF_SIZE];
	struct http_request req = {m, u, v};

	if(parse_header(socket->read_buffer, socket->read_buffer_size, &req)){
		DEBUG_PRINT("HTTP request- method: %s, uri: %s, ver: %s\n", req.method, req.uri, req.ver);
		//handle_request(socket,&req);
		return NULL;
	}
	else{
		DEBUG_PRINT("no http request read\n");
		return NULL;
	}
}

void handle_request(struct http_socket* socket, struct http_request* req){

    if (!strcasecmp(req->method, "GET")) {

		if (!strstr(req->uri, "cgi-bin"))
			handle_static_request(socket, req);
		else
			handle_dynamic_request(socket, req);

    }
    else
    	write_error(501);

}

void handle_static_request(struct http_socket* socket, struct http_request* req){
	char filename[BUF_SIZE], filetype[BUF_SIZE];

	struct stat sbuf;

	strcpy(filename, ".");
	strcat(filename, req->uri);
	if (req->uri[strlen(req->uri)-1] == '/') 
		strcat(filename, "index.html");

	if(!file_exist(filename)){
		write_error(404);
		return;
	}

	stat(filename, &sbuf);
	
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpg");
	else 
		strcpy(filetype, "text/plain");

    
	print_to_buffer(socket, "HTTP/1.1 200 OK\n");
	print_to_buffer(socket, "Server: Tiny Web Server\n");
	print_to_buffer(socket, "Content-length: %d\n", (int)sbuf.st_size);
	print_to_buffer(socket, "Content-type: %s\n", filetype);
	print_to_buffer(socket, "\r\n");

}

void print_to_buffer(struct http_socket* socket, char* str, ...){
	va_list arg_ptr;
	int n;

	do{

		va_start(arg_ptr, str);
		n = vsnprintf(socket->write_buffer + socket->write_buffer_pos, socket->write_buffer_size, str, arg_ptr);
		va_end(arg_ptr);
	
		if(n){
			socket->write_buffer = realloc(socket->write_buffer, socket->write_buffer_size + OUT_BUF_ALLOC_SIZE);
			socket->write_buffer_size += OUT_BUF_ALLOC_SIZE;
		}
		else{
			socket->write_buffer_pos += strlen(str);
		}
	}
	while(n != 0);
}

void handle_dynamic_request(struct http_socket* socket, struct http_request* req){
	//char cgi_args[BUF_SIZE], filename[BUF_SIZE];


}


int file_exist(char* filename){
	struct stat sbuf;
    if (stat(filename, &sbuf) < 0)
    	return 0;
    return 1;
}

void write_error(int error){
	switch(error){
		case 501:

		break;
		case 404:
		break;
	}
}

int file_load(struct http_socket* http, char* filename) {
    int fd;
    struct stat stat_block;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Opening file for mmaping");
        return -1;
    }
    if (fstat(fd, &stat_block) != 1) {
        perror("Statting file for mmaping");
        close(fd);
        return -1;
    }
    void* mapped_file = mmap(NULL, stat_block.st_size, PROT_READ, MAP_PRIVATE, fd, 0);    
    if (mapped_file == MAP_FAILED) {
        perror("mmaping a file");
        close(fd);
        return -1;
    }
    http->mmaped = true;
    http->write_buffer = mapped_file;
    http->write_buffer_size = stat_block.st_size;
    return 0;
}






