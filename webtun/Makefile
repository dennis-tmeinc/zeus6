
# Makefile for Eagle32 DVR server app
#

include ../config

CFLAGS += -I$(PROJDIR)/lib

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h 
EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp 

HEADERS=
SOURCES=webtun.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
CFLAGS+= -I../rconn 
LDFLAGS+= -lcurl -lstdc++ -lpthread

# CFLAGS+= -DRCMAIN 
# EXTHEADERS+= ../rconn/channel.h  ../rconn/net.h
# EXTSOURCES+= ../rconn/rconn.cpp ../rconn/channel.cpp ../rconn/net.cpp

webtun : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)
	$(STRIP) $@

testcurl : testcurl.cpp $(EXTSOURCES) $(EXTHEADERS)
	gcc -o $@ testcurl.cpp $(EXTSOURCES) -I. -lcurl -lstdc++
	
%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f webtun
