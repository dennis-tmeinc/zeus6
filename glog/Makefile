
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

HEADERS =../dvrsvr/config.h ../dvrsvr/genclass.h ../ioprocess/diomap.h ../ioprocess/serial.h \
		$(PROJDIR)/lib/util/string.h\
		$(PROJDIR)/lib/util/array.h

SOURCES =  glog.cpp ../dvrsvr/config.cpp ../dvrsvr/string.cpp ../ioprocess/serial.cpp ../ioprocess/diomap.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

CFLAGS += -I../dvrsvr
#-L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib

glog : $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
#	cp glog /home/jianpeng/eagle34/dvrtest/

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f glog
