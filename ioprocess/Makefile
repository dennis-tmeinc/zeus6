
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

HEADERS=diomap.h mcu.h serial.h
EXTHEADERS=../dvrsvr/cfg.h ../dvrsvr/genclass.h
EXTSOURCES=../dvrsvr/config.cpp ../dvrsvr/string.cpp

IOSOURCES=ioprocess.cpp mcu.cpp mcucmd.cpp tab102.cpp serial.cpp

IOOBJS = $(patsubst %.cpp, %.o, $(IOSOURCES))
EXTOBJS = $(patsubst %.cpp, %.o, $(EXTSOURCES))

#LDFLAGS = -L../dvrsvr/eagle32 -lsdk -ldadsp ../dvrsvr/eagle32/dsplink.lib
LDFLAGS += -lpthread

TARGET = ioprocess panelled devicepower buzzer dvrlog dumpio
default: $(TARGET)

ioprocess : $(IOOBJS) $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dumpio : dumpio.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

panelled : panelled.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

devicepower : devicepower.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

buzzer : buzzer.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dvrlog : dvrlog.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(IOOBJS) $(EXTOBJS)
	rm -f $(TARGET)
