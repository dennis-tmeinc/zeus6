
# Makefile for Eagle32 DVR server app
#

include ../config

SRCS = zdaemon.cpp
HEADERS = 

OBJS = $(patsubst %.cpp, %.o, $(SRCS))

zdaemon : zdaemon.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.cpp $(HEADERS)  
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f zdaemon
