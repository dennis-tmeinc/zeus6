
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

SRCS = volumeid.c md5check.c md5.c

OBJS = $(patsubst %.c, %.o, $(SRCS))

CFLAGS += -I../dvrsvr

all: volumeid md5check

volumeid : volumeid.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

md5check :  md5check.o md5.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f volumeid md5check


