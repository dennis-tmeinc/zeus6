
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

HEADERS=diomap.h mcu.h serial.h \
		$(PROJDIR)/lib/util/string.h\
		$(PROJDIR)/lib/util/array.h

EXTHEADERS=../dvrsvr/config.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp diomap.cpp

IOSOURCES=ioprocess.cpp mcu.cpp mcucmd.cpp serial.cpp

IOOBJS = $(patsubst %.cpp, %.o, $(IOSOURCES))
EXTOBJS = $(patsubst %.cpp, %.o, $(EXTSOURCES))

#LDFLAGS = -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib
LDLIBS += -lpthread -lrt

TARGET = ioprocess panelled devicepower buzzer dvrlog dumpio
default: $(TARGET)

ioprocess : $(IOOBJS) $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

dumpio : dumpio.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

panelled : panelled.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

devicepower : devicepower.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

buzzer : buzzer.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

dvrlog : dvrlog.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(IOOBJS) $(EXTOBJS)
	rm -f $(TARGET)
