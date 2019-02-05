
# Makefile for Eagle32 DVR ioprocess app 
#

include ../config

SOURCES=tmefile.c

OBJS = $(patsubst %.c, %.o, $(SOURCES))

all: tmefile

tmefile : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f tmefile
