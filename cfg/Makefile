# Makefile for Eagle32 DVR ioprocess app

include ../config

SOURCES=cfg.cpp

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h
EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp

HEADERS=

OBJS  = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

CFLAGS+= -I../dvrsvr

all : cfg cfgpc

cfg : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cfgpc :$(SOURCES) $(EXTSOURCES)
	gcc -o $@ $^ -lstdc++

#.cpp.o:
%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f cfg cfgpc
