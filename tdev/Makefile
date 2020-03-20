
# Makefile for Eagle32 DVR server app
#

include ../config

SRCS = tdevd.cpp tdevmount.cpp
HEADERS = ../dvrsvr/dir.h

OBJS = $(patsubst %.cpp, %.o, $(SRCS))

TARGET = tdevd tdevmount

all: $(TARGET)

tdevd : tdevd.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

tdevmount : tdevmount.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

clean :
	rm -f $(TARGET)
	rm -f $(OBJS)
	rm -f *.o
