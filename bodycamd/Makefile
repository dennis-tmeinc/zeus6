#
# Makefile for ZEUS 6 DVR
#

include ../config

LDFLAGS+= -lstdc++ -lpthread -lcrypt 
EXTRASOURCES = \
	$(PROJDIR)/lib/cjson/cjson.cpp \
	$(PROJDIR)/lib/net/net.cpp \
	$(PROJDIR)/dvrsvr/config.cpp
EXTRAHEADERS = \
	$(PROJDIR)/lib/cjson/cjson.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/dvrsvr/cfg.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/ioprocess/diomap.h 

SOURCES=bodycamd.cpp bodycam.cpp
HEADERS=bodycam.h

all: bodycamd

bodycamd: $(SOURCES) $(EXTRASOURCES)  $(HEADERS) $(EXTRAHEADERS)
	$(CC) $(CFLAGS) -O2 -o $@  $(SOURCES) $(EXTRASOURCES) $(LDFLAGS)

clean :
	rm   -f bodycamd
	rm -f *.o
