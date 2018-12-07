#
# Makefile for ZEUS 6 DVR
#

include ../config

CFLAGS += -I$(PROJDIR) 
LDFLAGS+= -lcrypt -lstdc++ -lpthread

SOURCES=bodycamd.cpp bodycam.cpp cjson.cpp ../net/net.cpp ../dvrsvr/config.cpp
HEADERS=bodycam.h cjson.h ../net/net.h ../ioprocess/diomap.h ../dvrsvr/genclass.h ../dvrsvr/cfg.h

all: bodycamd

bodycamd: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -O2 -o $@  $(SOURCES) $(LDFLAGS)

clean :
	rm   -f bodycamd
	rm -f *.o
