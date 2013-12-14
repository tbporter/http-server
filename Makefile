#
# A simple Makefile to build 'esh'
#
LDFLAGS=
LDLIBS=-lpthread
# The use of -Wall, -Werror, and -Wmissing-prototypes is mandatory 
# for this assignment
CFLAGS=-Wall -Werror -Wmissing-prototypes
#YFLAGS=-v

OBJECTS=server.o list.o threadpool.o parse.o events.o json.o
HEADERS=server.h list.h threadpool.h parse.h events.h json.h
PLUGINDIR=plugins

default: sysstatd

debug: CFLAGS += -DDEBUG -g
	HEADERS += debug.h
debug: sysstatd

$(OBJECTS) : $(HEADERS)

sysstatd: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS)

clean: 
	rm -f $(OBJECTS) $(LIB_OBJECTS) sysstatd \

all: sysstatd