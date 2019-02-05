#
# Makefile for ZEUS 6 DVR
#

include ../config

CFLAGS += -fno-rtti
LDFLAGS += -lpthread -lcrypt -lm

SOURCES = bodycam.cpp bodycamd.cpp \
	$(PROJDIR)/lib/cjson/cjson.cpp \
	$(PROJDIR)/lib/net/net.cpp \
	$(PROJDIR)/dvrsvr/config.cpp
HEADERS = bodycam.h \
	$(PROJDIR)/lib/cjson/cjson.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/dvrsvr/cfg.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/ioprocess/diomap.h 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

all: bodycamd

bodycamd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

#.cpp.o:
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm   -f bodycamd
	rm -f $(OBJS)
	rm -f *.o
