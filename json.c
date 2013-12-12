#include "json.h"
#include "debug.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "events.h"
#include <string.h>
#define PROC_BUFFER_SIZE 512
int loadavg(struct buffer* buf) {
	FILE *f = fopen("/proc/loadavg", "r");
	size_t size = 0;
	char* line = 0;

	loadavg_t avg;
	if (!feof (f)){
   		ssize_t lsz = getline (&line, &size, f);
   		if(lsz >0){
   			sscanf(line, "%f %f %f %d/%d %d\n", &avg.wait1,&avg.wait5,&avg.wait15,&avg.runable_entities,&avg.entities,&avg.last_pid );
			print_to_buffer(buf, "{\"total_threads\": \"%d\", \"loadavg\": [\"%f\", \"%f\", \"%f\"], \"running_threads\": \"%d\"}", avg.entities, avg.wait1,avg.wait5,avg.wait15, avg.runable_entities);
		}	
	}
	fclose (f);
    return 0;
}
int meminfo(struct buffer* buf) {
	FILE *f = fopen("/proc/meminfo", "r");
	size_t size = 0;
	char* line = 0;
	char mem_name[256];
	int mem_bytes;
	bool add_comma = false;
	print_to_buffer(buf, "{");
	while (!feof (f)){
   		ssize_t lsz = getline (&line, &size, f);
   		if(lsz > 0){
   			if(add_comma)
				print_to_buffer(buf, ", ");
			else
				add_comma = true;
   			sscanf(line, "%s %d \n",mem_name, &mem_bytes );
   			mem_name[strlen(mem_name)-1] = '\0'; //get rid of the colon
   			print_to_buffer(buf, "\"%s\": \"%d\"", mem_name, mem_bytes);
   		}
		
	}
	print_to_buffer(buf, "}");
	fclose (f);	
    return 0;
}
