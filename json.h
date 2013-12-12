#ifndef _JSON_H
#define _JSON_H

#include "server.h"
#include <sys/types.h>
typedef struct {
    float wait1;
    float wait5;
    float wait15;
    int runable_entities;
    int entities;
    pid_t last_pid;
} loadavg_t;

int loadavg(struct buffer* buf);
int meminfo(struct buffer* buf);

#endif
