#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#include "server.h"
#include "parse.h"
#include "events.h"
#include "debug.h"
#include "json.h"

#define BUF_SIZE 256
#define OUT_BUF_ALLOC_SIZE 512

#define ANON_SIZE 67108864

static pthread_mutex_t anon_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static void** anon_list = NULL;
static int anon_list_n;
static int anon_list_size;

static char* file_path;

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
	char m[BUF_SIZE],u[BUF_SIZE],v[BUF_SIZE], cb[BUF_SIZE];
	struct http_request req = {m, u, v, cb};

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
		
		if(!is_buffer_finished(socket->write)){
			count = write(socket->fd,socket->write.data + socket->write.pos, socket->write.last - socket->write.pos);
			socket->write.pos += count;
		}
		if(is_buffer_finished(socket->write) && !is_buffer_finished(socket->data)){
			count = write(socket->fd,socket->data.data + socket->data.pos, socket->data.size - socket->data.pos);
			socket->data.pos += count;
		}
		DEBUG_PRINT("NOT FINISHED");
	} while(!is_buffer_finished(socket->write) && !is_buffer_finished(socket->data) && count != 0);
	
	if(is_buffer_finished(socket->write) && is_buffer_finished(socket->data)){
		DEBUG_PRINT("FINISHED WRITING YO\n");
		DEBUG_PRINT("size: %d\n", socket->data.size);
		watch_read(socket);
	}else{
		watch_write(socket);
	}
	return NULL;
}

bool is_buffer_finished(struct buffer b){
	return b.pos >= b.last; 
}

void handle_request(struct http_socket* socket, struct http_request* req){

    if (!strcasecmp(req->method, "GET")) {

    	if(strstr(req->uri, "/loadavg")){
			DEBUG_PRINT("/roadavg\n");

			if(req->cb[0] != '\0'){
				print_to_buffer(&socket->data, "%s(", req->cb);
				loadavg(&socket->data);
				print_to_buffer(&socket->data, ")");	
			} else 
				loadavg(&socket->data);

			send_json(socket, 0);
    	}
		else if(strstr(req->uri, "/meminfo")){
			DEBUG_PRINT("/meminfo\n");

			if(req->cb[0] != '\0'){
				print_to_buffer(&socket->data, "%s(", req->cb);	
				meminfo(&socket->data);	
				print_to_buffer(&socket->data, ")");
			} else
				meminfo(&socket->data);	
	
			send_json(socket,0);    		
		}
		else if(strstr(req->uri, "/runloop")){
			DEBUG_PRINT("/runloop\n");
			run_loop();
			print_to_buffer(&socket->data, "Started loop");
			send_plain_text(socket,0);
		}
		else if(strstr(req->uri, "/allocanon")){
			DEBUG_PRINT("/allocannon\n");
			if(allocanon() == 0)
				print_to_buffer(&socket->data, "allocanon success");
			else
				print_to_buffer(&socket->data, "allocanon didn't work");

			send_plain_text(socket,0);

		}
		else if(strstr(req->uri, "/freeanon")){
			DEBUG_PRINT("/freeanon\n");
			if(freeanon() == 0)
				print_to_buffer(&socket->data, "freeanon success");
			else
				print_to_buffer(&socket->data, "freeanon didn't free any blocks");

			send_plain_text(socket,0);
		}
		else if (!strstr(req->uri, "cgi-bin")){
			handle_static_request(socket, req);
		}
		else
			handle_dynamic_request(socket, req);

    }
    else{
    	write_error(socket,501);
    	return;
    }
}

void handle_static_request(struct http_socket* socket, struct http_request* req){
	char filename[BUF_SIZE], filetype[BUF_SIZE];

	strcpy(filename, file_path);
	strcat(filename, req->uri);
	if (req->uri[strlen(req->uri)-1] == '/') 
		strcat(filename, "files/index.html");

	DEBUG_PRINT("filename: %s\n", filename);

	if(!file_exist(filename)){
		write_error(socket,404);	
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
	print_to_buffer(&socket->write, "HTTP/1.1 200 OK\n");
	print_to_buffer(&socket->write, "Server: Best Server\n");
	print_to_buffer(&socket->write, "Content-length: %d\n", socket->data.size);
	print_to_buffer(&socket->write, "Content-type: %s\n", filetype);
	print_to_buffer(&socket->write, "\r\n");

	int i;
	for(i=0; i < socket->write.pos; i++){
		DEBUG_PRINT("%c", socket->write.data[i]);
	}

	finish_read(socket);
}

void print_to_buffer(struct buffer* b, char* str, ...){
	va_list arg_ptr;
	int n;
	int size_left;
	do{
		size_left = b->size - b->pos;
		va_start(arg_ptr, str);
		n = vsnprintf(b->data + b->pos, size_left, str, arg_ptr);
		va_end(arg_ptr);
	
		if(n >= size_left){
			b->data = realloc(b->data, b->size + OUT_BUF_ALLOC_SIZE);
			b->size += OUT_BUF_ALLOC_SIZE;
		}else {
			b->pos += n;
		}
	}
	while(n >= size_left);
}

void send_plain_text(struct http_socket* s, int err){
	if(err)
		print_to_buffer(&s->write, "HTTP/1.1 %d\n", err);
	else
		print_to_buffer(&s->write, "HTTP/1.1 200 OK\n");
	print_to_buffer(&s->write, "Server: Best Server\n");
	print_to_buffer(&s->write, "Content-length: %d\n", s->data.pos);
	print_to_buffer(&s->write, "Content-type: text/plain\n");
	print_to_buffer(&s->write, "\r\n");
	finish_read(s);
}

void send_json(struct http_socket* s, int err){
	if(err)
		print_to_buffer(&s->write, "HTTP/1.1 %d\n", err);
	else
		print_to_buffer(&s->write, "HTTP/1.1 200 OK\n");
	print_to_buffer(&s->write, "Server: Best Server\n");
	print_to_buffer(&s->write, "Content-length: %d\n", s->data.pos);
	print_to_buffer(&s->write, "Content-type: application/json\n");
	print_to_buffer(&s->write, "\r\n");
	finish_read(s);
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

void write_error(struct http_socket* s,int error){
	DEBUG_PRINT("Http error: %d\n", error);
	print_to_buffer(&s->data,"HTTP Error %d\n", error);
	send_plain_text(s,error);
}

int allocanon() {
    void* anon_block;
    pthread_mutex_lock(&anon_list_mutex);
    /* Handle initialization case */
    if (anon_list == NULL) {
        if ((anon_list = calloc(10, sizeof(void*))) == NULL) {
            perror("Error allocating anon_list");
            pthread_mutex_unlock(&anon_list_mutex);
            return -1;
        }
        anon_list_n = 0;
        anon_list_size = 10;
    }
    /* anon_list_n will be anon_list_size + 1 when a expansion is needed */
    else if (anon_list_n >= anon_list_size) {
        DEBUG_PRINT("Extending anon_list from %d\n", anon_list_size);
        void* new_list;
        if ((new_list = realloc(anon_list, sizeof(void*) * (anon_list_size + 10))) == NULL) {
            perror("Error realloc anon_list");
            pthread_mutex_unlock(&anon_list_mutex);
            return -1;
        }
        DEBUG_PRINT("Changing anon_list from %p to %p\n", anon_list, new_list);
        anon_list = new_list;
        anon_list_size += 10;
    }
    /* Otherwise we have space available */

    /* Now generic wagon horse code */
    anon_block = mmap(NULL, ANON_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon_block == MAP_FAILED) {
        perror("Allocating anonymous block");
        pthread_mutex_unlock(&anon_list_mutex);
        return -1;
    }
    DEBUG_PRINT("Setting anon_list[%d] to %p\n", anon_list_n, anon_block);
    anon_list[anon_list_n++] = anon_block;
    pthread_mutex_unlock(&anon_list_mutex);
    return 0;
}

/* Returns 1 if none are available to munmap, 0 on success and -1 on error */
int freeanon() {
    DEBUG_PRINT("Freeing an anonymous block\n");
    pthread_mutex_lock(&anon_list_mutex);
    if (anon_list_n <= 0) {
        pthread_mutex_unlock(&anon_list_mutex);
        DEBUG_PRINT("No anonymouse blocks to free\n");
        return 1;
    }
    DEBUG_PRINT("Freeing anon_list[%d] %p\n", anon_list_n - 1, anon_list[anon_list_n - 1]);
    if (munmap(anon_list[--anon_list_n], ANON_SIZE)) {
        perror("munmap anonymous block");
        anon_list_n++;
        pthread_mutex_unlock(&anon_list_mutex);
        return -1;
    }
    pthread_mutex_unlock(&anon_list_mutex);
    return 0;
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
        if (fd == EEXIST || fd == EFAULT || fd == ENAMETOOLONG || fd == ETXTBSY)
            return 1;
        if (fd == EACCES || fd == EPERM)
            return 2;
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
    http->data.data = mapped_file;
    http->data.size = stat_block.st_size;
    http->data.pos = http->data.size;
    return 0;
}

void finish_read(struct http_socket* socket){
	socket->write.last = socket->write.pos;
	socket->data.last = socket->data.pos;
	socket->write.pos = 0;
	socket->data.pos = 0;
	watch_write(socket);
}

void set_path(char* path) {
    file_path = path;
}
