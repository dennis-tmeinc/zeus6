# Makefile for Eagle32 DVR server app
#
include ../config

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/lib/net/net.h 
EXTSOURCES= $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/lib/net/net.cpp 

SOURCES=rconn.cpp adbclient.cpp channel.cpp
HEADERS=rconn.h adbclient.h channel.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

CFLAGS += -DANDROID_CLIENT -fno-rtti
LDFLAGS += -lpthread

rconn : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	
usbrst: usbrst.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	
%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	echo $(CFLAGS)
	rm -f *.o
	rm -f $(OBJS)
	rm -f rconn
