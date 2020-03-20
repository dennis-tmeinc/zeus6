
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config

LDLIBS += -lrt

SOURCES=cfgdown.cpp \
	cfgreport.cpp\
	dvrstatus.cpp\
	gcal.cpp \
	gcrash.cpp

HEADERS=

EXTSOURCES=\
	$(PROJDIR)/dvrsvr/config.cpp \
	$(PROJDIR)/dvrsvr/string.cpp \
	$(PROJDIR)/lib/net/net.cpp \
	$(PROJDIR)/lib/json/json.cpp \
	$(PROJDIR)/ioprocess/diomap.cpp

EXTHEADERS=$(PROJDIR)/dvrsvr/config.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/lib/json/json.h \
	$(PROJDIR)/ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
EXTOBJS = $(patsubst %.cpp, %.o, $(EXTSOURCES))

OBJS += $(EXTOBJS)

TARGET=cfgdown.cgi dvrstatus.cgi cfgreport gcal.cgi gcrash.cgi

all: $(TARGET)

cfgdown.cgi: cfgdown.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

gcal.cgi : gcal.o $(PROJDIR)/ioprocess/diomap.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

gcrash.cgi : gcrash.o $(PROJDIR)/ioprocess/diomap.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

dvrstatus.cgi : dvrstatus.o ../eagle_setup/getquery.o $(PROJDIR)/ioprocess/diomap.o $(PROJDIR)/lib/net/net.o $(PROJDIR)/lib/json/json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

cfgreport : cfgreport.o $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/ioprocess/diomap.o $(PROJDIR)/lib/net/net.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -f $(OBJS)
	rm -f *.o


