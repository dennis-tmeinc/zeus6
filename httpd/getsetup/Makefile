
# Makefile for Eagle32 DVR dvrsetup app
#

include ../../config

SOURCES=

EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h $(PROJDIR)/lib/json/json.h
EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/lib/json/json.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

LDFLAGS= -lstdc++ -lcrypt

CFLAGS+= -Wall

all: getsetup applysetup

getsetup: getsetup.cpp cfgtable.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ getsetup.cpp cfgtable.cpp $(EXTSOURCES) $(LDFLAGS)

applysetup: applysetup.cpp cfgtable.cpp $(EXTSOURCES) $(EXTHEADERS)
	$(CC) $(CFLAGS) -o $@ applysetup.cpp cfgtable.cpp $(EXTSOURCES) ../../dvrsvr/crypt.cpp $(LDFLAGS)

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f applysetup
	rm -f getsetup
	rm -f *.o


