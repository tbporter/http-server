#include "json.h"
#include "debug.h"
#include <stdio.h>
#include <unistd.h>
#include "events.h"
#define PROC_BUFFER_SIZE 512
int loadavg(struct buffer* buf) {
	FILE *f = fopen("/proc/loadavg", "r");
	size_t size = 0;
	char* line = 0;

	loadavg_t avg;
	if (!feof (f)){
   		ssize_t lsz = getline (&line, &size, f);
   		DEBUG_PRINT("loadavg: %s lsz: %d\n", line, (int)lsz);
   		sscanf(line, "%f %f %f %d/%d %d\n", &avg.wait1,&avg.wait5,&avg.wait15,&avg.runable_entities,&avg.entities,&avg.last_pid );
	}
	print_to_buffer(buf, "{\"total_threads\" : \"%d\", \"loadavg\" : [\"%f\", \"%f\", \"%f\"], \"running_threads\" : \"%d\"}", avg.entities, avg.wait1,avg.wait5,avg.wait15, avg.runable_entities);
	//DEBUG_PRINT("loadavg: %s lsz: %d", line, lsz);
	fclose (f);
    return 0;
}
int meminfo(struct buffer* buf) {
	
    return 0;
}
