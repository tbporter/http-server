#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
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
		handle_request(socket,&req);
		return NULL;
	}
	else{
		DEBUG_PRINT("no http request read\n");
		watch_read(socket);
		return NULL;
	}
}

void* write_conn(void* data){
	struct http_socket* socket = (struct http_socket*) data;
	int count = 0;


	do{
		
		if(!is_write_buffer_finished(socket)){
			count = write(socket->fd,socket->write_buffer + socket->write_buffer_pos, socket->write_buffer_size - socket->write_buffer_pos);
			socket->write_buffer_pos += count;
		}


		if(is_write_buffer_finished(socket) && !is_data_buffer_finished(socket)){
			count = write(socket->fd,socket->data_buffer + socket->data_buffer_pos, socket->data_buffer_size - socket->data_buffer_pos);
			socket->data_buffer_pos += count;
		}

	} while(is_write_buffer_finished(socket) && is_data_buffer_finished(socket) && count != 0);
	
	if(is_write_buffer_finished(socket) && is_data_buffer_finished(socket)){
		DEBUG_PRINT("FINISHED WRITING YO\n");
		watch_read(socket);
	}else{
		watch_write(socket);
	}
	return NULL;
}
bool is_write_buffer_finished(struct http_socket* s){
	return s->write_buffer_pos >= s->write_buffer_size; 
}

bool is_data_buffer_finished(struct http_socket* s){
	return s->data_buffer_pos >= s->data_buffer_size; 
}

void handle_request(struct http_socket* socket, struct http_request* req){

    if (!strcasecmp(req->method, "GET")) {

    	if(strstr(req->uri, "/loadavg"))
			DEBUG_PRINT("/roadavg\n");
		else if(strstr(req->uri, "/meminfo"))
			DEBUG_PRINT("/meminfo\n");    		
		else if(strstr(req->uri, "/runloop"))
			DEBUG_PRINT("/runloop\n");
		else if(strstr(req->uri, "/allocanon"))
			DEBUG_PRINT("/allocannon\n");
		else if(strstr(req->uri, "/freeanon"))
			DEBUG_PRINT("/freeanon\n");
		else if (!strstr(req->uri, "cgi-bin"))
			handle_static_request(socket, req);
		else
			handle_dynamic_request(socket, req);

    }
    else
    	write_error(501);

}

void handle_static_request(struct http_socket* socket, struct http_request* req){
	char filename[BUF_SIZE], filetype[BUF_SIZE];

	strcpy(filename, ".");
	strcat(filename, req->uri);
	if (req->uri[strlen(req->uri)-1] == '/') 
		strcat(filename, "files/index.html");

	DEBUG_PRINT("filename: %s\n", filename);

	if(!file_exist(filename)){
		write_error(404);
		return;
	}
	
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".js"))
		strcpy(filetype, "application/javascript");
	else if (strstr(filename, ".css"))
		strcpy(filetype, "text/css");
	else 
		strcpy(filetype, "text/plain");

    
	file_load(socket,filename);    
	print_to_buffer(socket, "HTTP/1.1 200 OK\n");
	print_to_buffer(socket, "Server: Best Server\n");
	print_to_buffer(socket, "Content-length: %d\n", socket->data_buffer_size);
	print_to_buffer(socket, "Content-type: %s\n", filetype);
	print_to_buffer(socket, "\r\n");

	int i;
	for(i=0; i < socket->write_buffer_size; i++){
		DEBUG_PRINT("%c", socket->write_buffer[i]);
	}
	finish_read(socket);
}

void print_to_buffer(struct http_socket* socket, char* str, ...){
	va_list arg_ptr;
	int n;
	int size_left;
	do{
		size_left = socket->write_buffer_size - socket->write_buffer_pos;
		va_start(arg_ptr, str);
		n = vsnprintf(socket->write_buffer + socket->write_buffer_pos, size_left, str, arg_ptr);
		va_end(arg_ptr);
	
		if(n >= size_left){
			socket->write_buffer = realloc(socket->write_buffer, socket->write_buffer_size + OUT_BUF_ALLOC_SIZE);
			socket->write_buffer_size += OUT_BUF_ALLOC_SIZE;
		}else {
			socket->write_buffer_pos += n;
		}
	}
	while(n >= size_left);
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
	DEBUG_PRINT("Http error: %d\n", error);
	switch(error){
		case 501:
		break;
		case 404:
		break;
	}
}

int run_loop(){
    pthread_attr_t attrs;
    pthread_t thread;
    if(pthread_attr_init(&attrs)) {
        perror("Initializing runloop pthread_attr_t");
        return -1;
    }
    if (pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED)) {
        perror("Setting detached attribute in run_loop");
        return -1;
    }
    if (pthread_create(&thread, &attrs, &spin, NULL)) {
        perror("Creating spin pthread");
        return -1;
    }
    return 0;
}

void* spin(void* data) {
    time_t THETIMEWASTHEN = time(NULL);
    time_t THETIMEISNOW = time(NULL);
    while(THETIMEWASTHEN + 15 >= THETIMEISNOW)
        THETIMEISNOW = time(NULL);
    return NULL;
}

int file_load(struct http_socket* http, char* filename) {
    DEBUG_PRINT("Loading %s\n", filename);
    int fd;
    struct stat stat_block;
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Opening file for mmaping");
        return -1;
    }
    DEBUG_PRINT("Opened as fd:%d\n", fd);
    if (fstat(fd, &stat_block) != 0) {
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
    http->data_buffer = mapped_file;
    http->data_buffer_size = stat_block.st_size;
    return 0;
}

void finish_read(struct http_socket* socket){
	socket->write_buffer_pos = 0;
	socket->data_buffer_pos = 0;
	watch_write(socket);
}
