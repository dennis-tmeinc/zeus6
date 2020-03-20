# Makefile for Eagle32 DVR ioprocess app

include ../config

SOURCES=cfg.cpp

EXTHEADERS=$(PROJDIR)/dvrsvr/config.h $(PROJDIR)/dvrsvr/genclass.h
EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp

HEADERS=\
		$(PROJDIR)/lib/util/string.h\
		$(PROJDIR)/lib/util/array.h


OBJS  = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

all : cfg cfgpc

cfg : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

cfgpc :$(SOURCES) $(EXTSOURCES)
	gcc $(CFLAGS_PC) -o $@ $^ -lstdc++

#.cpp.o:
%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f cfg cfgpc
