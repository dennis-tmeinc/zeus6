
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config

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
	$(PROJDIR)/ioprocess/diomap.cpp

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
EXTOBJS = $(patsubst %.cpp, %.o, $(EXTSOURCES))

OBJS += $(EXTOBJS)

TARGET=cfgdown.cgi dvrstatus.cgi cfgreport gcal.cgi gcrash.cgi

all: $(TARGET)

cfgdown.cgi: cfgdown.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

gcal.cgi : gcal.o $(PROJDIR)/ioprocess/diomap.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

gcrash.cgi : gcrash.o $(PROJDIR)/ioprocess/diomap.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

dvrstatus.cgi : dvrstatus.o ../eagle_setup/getquery.o $(PROJDIR)/ioprocess/diomap.o $(PROJDIR)/lib/net/net.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cfgreport : cfgreport.o $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/ioprocess/diomap.o $(PROJDIR)/lib/net/net.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -f $(OBJS)
	rm -f *.o


