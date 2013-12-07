#
# A simple Makefile to build 'esh'
#
LDFLAGS=
LDLIBS=-lpthread
# The use of -Wall, -Werror, and -Wmissing-prototypes is mandatory 
# for this assignment
CFLAGS=-Wall -Werror -Wmissing-prototypes
#YFLAGS=-v

OBJECTS=server.o
HEADERS=server.h
PLUGINDIR=plugins

default: http-server

debug: CFLAGS += -DDEBUG -g
	HEADERS += debug.h
debug: http-server

$(OBJECTS) : $(HEADERS)

http-server: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJECTS) $(LDLIBS)

clean: 
	rm -f $(OBJECTS) $(LIB_OBJECTS) http-server \
