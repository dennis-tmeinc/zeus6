
# Makefile for Eagle32 DVR ioprocess app
#

include ../config


SOURCES = glog.cpp
HEADERS =

HEADERS +=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
SOURCES +=../dvrsvr/config.cpp ../dvrsvr/string.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS += -I../dvrsvr
LDFLAGS += -lpthread 
#-L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib

glog : $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
#	cp glog /home/jianpeng/eagle34/dvrtest/

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f glog
