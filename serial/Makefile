
# Makefile for Eagle32 DVR server app
#

include ../config

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

serial : serial.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean :
	rm -f serial
	rm -f *.o
