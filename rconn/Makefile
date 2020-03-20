# Makefile for Eagle32 DVR server app
#
include ../config

EXTHEADERS=$(PROJDIR)/dvrsvr/config.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/lib/net/net.h 
EXTSOURCES= $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/lib/net/net.cpp 

SOURCES=rconn.cpp adbclient.cpp channel.cpp
HEADERS=rconn.h adbclient.h channel.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

CFLAGS += -DANDROID_CLIENT
LDLIBS += -lpthread 

rconn : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	
#rconn for DVR ()
rconn_dvr : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS) 
	$(CC) $(CFLAGS) -DDVRMONITOR  -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS) $(LDLIBS)

usbrst: usbrst.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	
%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	echo $(CFLAGS)
	rm -f *.o
	rm -f $(OBJS)
	rm -f rconn
