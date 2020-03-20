#
# Makefile for ZEUS 6 DVR
#

include ../config

LDFLAGS += -lpthread -lcrypt -lm -lrt

SOURCES = bodycam.cpp bodycamd.cpp \
	$(PROJDIR)/lib/json/json.cpp \
	$(PROJDIR)/lib/net/net.cpp \
	$(PROJDIR)/ioprocess/diomap.cpp \
	$(PROJDIR)/dvrsvr/config.cpp
HEADERS = bodycam.h \
	$(PROJDIR)/lib/json/json.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/dvrsvr/config.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/ioprocess/diomap.h 

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

all: bodycamd

bodycamd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

#.cpp.o:
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm   -f bodycamd
	rm -f $(OBJS)
	rm -f *.o
