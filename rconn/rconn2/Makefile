
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=rconn.cpp adbclient.cpp channel.cpp

HEADERS=rconn.h adbclient.h channel.h

EXTSOURCES=../dvrsvr/config.cpp ../net/net.cpp
EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h ../net/net.h

CFLAGS+= -Os -DANDROID_CLIENT 
LDFLAGS= -lstdc++ -lpthread

rconn : $(SOURCES) $(HEADERS) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) -g $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@
	
usbrst: usbrst.cpp
	$(CC) -g $(CFLAGS) -o $@ usbrst.cpp $(LDFLAGS)
	

clean :
	rm -f *.o
	rm -f rconn
