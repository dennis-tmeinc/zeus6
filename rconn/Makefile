
# Makefile for Eagle32 DVR server app
#

include ../config

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/lib/net/net.h 
EXTSOURCES= $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/lib/net/net.cpp 

SOURCES=rconn.cpp adbclient.cpp channel.cpp
HEADERS=rconn.h adbclient.h channel.h

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
