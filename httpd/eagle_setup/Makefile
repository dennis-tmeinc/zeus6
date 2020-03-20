# Makefile for Eagle32 DVR web base setup app
#

include ../../config

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=login.cpp \
	getquery.cpp \
	loginx.cpp \
	pagevalue.cpp \
	cfgupload.cpp \
	firmwareupd.cpp \
	mcufwupd.cpp \
	mfid.cpp \
	gangle.cpp \
	getjpeg.cpp \
	eagle_setup.cpp \
	$(PROJDIR)/lib/json/json.cpp \
	$(PROJDIR)/dvrsvr/config.cpp \
	$(PROJDIR)/dvrsvr/string.cpp \
	$(PROJDIR)/dvrsvr/md5.cpp \
	$(PROJDIR)/dvrsvr/crypt.cpp \
	$(PROJDIR)/ioprocess/diomap.cpp \
	$(PROJDIR)/lib/net/net.cpp

HEADERS = $(PROJDIR)/dvrsvr/config.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/dvrsvr/dvr.h $(PROJDIR)/ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

TARGET= login \
	loginx\
	pagevalue\
	cfgupload\
	firmwareupd\
	mcufwupd\
	mfid

LDLIBS += -lcrypt

all: $(TARGET)

login : login.o getquery.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

loginx : loginx.o getquery.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

pagevalue : pagevalue.o getquery.o $(PROJDIR)/lib/json/json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

cfgupload : cfgupload.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

firmwareupd : firmwareupd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

mcufwupd : mcufwupd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

mfid : mfid.o getquery.o $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/md5.o $(PROJDIR)/dvrsvr/crypt.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

gangle.cgi : gangle.o getquery.o $(PROJDIR)/ioprocess/diomap.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

getjpeg.cgi : getjpeg.o getquery.o $(PROJDIR)/lib/net/net.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

eagle_setup : eagle_setup.o $(PROJDIR)/dvrsvr/md5.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS) 

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f $(TARGET)
	rm -f eagle_setup *.cgi
	rm -f $(OBJS)
	rm -f *.o