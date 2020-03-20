
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config

SOURCES = getsetup.cpp applysetup.cpp cfgtable.cpp \
	$(PROJDIR)/dvrsvr/crypt.cpp

HEADERS=$(PROJDIR)/dvrsvr/config.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/lib/json/json.h

EXTSRCS = $(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/lib/json/json.cpp

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

EXTOBJS = $(patsubst %.cpp, %.o, $(EXTSRCS))
OBJS += $(EXTOBJS)

LDLIBS += -lcrypt

all: getsetup applysetup

getsetup: getsetup.o cfgtable.o $(EXTOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

applysetup: applysetup.o cfgtable.o $(EXTOBJS) $(PROJDIR)/dvrsvr/crypt.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f applysetup getsetup
	rm -f $(OBJS)
	rm -f *.o


