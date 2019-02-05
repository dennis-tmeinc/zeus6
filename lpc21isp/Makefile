
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config

SOURCES=adprog.c lpc21isp.c lpcprog.c lpcterm.c
HEADERS=$(patsubst %.c, %.h, $(SOURCES))
OBJS = $(patsubst %.c, %.o, $(SOURCES))

CFLAGS += -I../dvrsvr

lpc21isp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f lpc21isp
