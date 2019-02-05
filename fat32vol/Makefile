
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

all: fat32vol

fat32vol : fat32vol.c 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -f *.o
	rm -f fat32vol


