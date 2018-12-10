# Makefile for Eagle32 DVR ioprocess app

include ../config

SOURCES=cfg.cpp

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h
EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS+= -lstdc++
CFLAGS+= -I../dvrsvr -g -Wall

all : cfg cfgpc

cfg : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)

cfgpc : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	gcc -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :
	rm -f *.o
	rm -f cfg cfgpc







