
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=rconn.cpp adbclient.cpp net.cpp

HEADERS=

EXTHEADERS=../dvrsvr/config.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -DANDROID_CLIENT
LDFLAGS += -lpthread

rconn : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) -g $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f rconn
