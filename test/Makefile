#
# Makefile for DVR server app
#

include ../config

SRCS = test.cpp ../lib/net/net.cpp

OBJS = $(patsubst %.cpp, %.o, $(SRCS))

CFLAGS += -g -I../lib/net
LDLIBS += -lpthread

test : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f test *.o *~
	rm -f $(OBJS)
