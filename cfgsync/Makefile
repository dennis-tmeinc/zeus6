
# Makefile for Eagle32 DVR ioprocess app 
# Changes:
# 08/14/2009 Harrison
#    1. copy cfgpc to /deploy/MDVR5100
#

include ../config

SOURCES=cfgsync.cpp

EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h 
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

CFLAGS += -I../dvrsvr

all : cfgsync cfgsyncpc

cfgsync : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cfgsyncpc : $(SOURCES) $(EXTSOURCES) $(HEADERS) $(EXTHEADERS)
	gcc $(CFLAGS) -o $@ $(SOURCES) $(EXTSOURCES) $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) -g -Wall -c $< -o $@

clean :  
	rm -f *.o
	rm -f cfgsync cfgsyncpc







