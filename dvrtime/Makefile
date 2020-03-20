
# Makefile for Eagle32 DVR server app
#

include ../config

LDLIBS += -lrt

SOURCES=dvrtime.cpp

HEADERS=

EXTHEADERS=../dvrsvr/config.h ../dvrsvr/genclass.h ../ioprocess/diomap.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp ../ioprocess/diomap.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

dvrtime : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

dtime : $(SOURCES) $(EXTSOURCES) $(EXTHEADERS)
	gcc $(CFLAGS_PC) -o $@ $(SOURCES) $(EXTSOURCES) -lstdc++

%.o : %.cpp $(HEADERS)  $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f dvrtime dtime
