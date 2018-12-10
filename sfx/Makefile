
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

LDFLAGS+= -lstdc++ 

EXTRASOURCES = \
	$(PROJDIR)/lib/lzmasdk/dec.cpp \
	$(PROJDIR)/lib/lzmasdk/dec.cpp \
	$(PROJDIR)/dvrsvr/config.cpp
EXTRAHEADERS = \
	$(PROJDIR)/lib/cjson/cjson.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/dvrsvr/cfg.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/ioprocess/diomap.h 

SOURCES=bodycamd.cpp bodycam.cpp
HEADERS=

DECSOURCES = \
	$(PROJDIR)/lib/lzmasdk/dec.cpp \
	$(PROJDIR)/lib/lzmasdk/LzmaDec.c 

ENCSOURCES = \
	$(PROJDIR)/lib/lzmasdk/enc.cpp \
	$(PROJDIR)/lib/lzmasdk/LzmaEnc.c \
	$(PROJDIR)/lib/lzmasdk/LzFind.c

sfxfiles: sfx mksfx

all: sfx mksfx sfx86 extractsfx

sfx : sfx.cpp extract.cpp $(DECSOURCES)
	$(CC) $(CFLAGS) -Os -o $@ sfx.cpp extract.cpp $(DECSOURCES) $(LDFLAGS)
	$(STRIP) sfx

sfx86 : sfx.cpp extract.cpp $(DECSOURCES)
	gcc $(CFLAGS) -o $@ sfx.cpp extract.cpp $(DECSOURCES)
	strip sfx86

mksfx : mksfx.cpp $(ENCSOURCES)
	gcc $(CFLAGS) -o $@ mksfx.cpp $(ENCSOURCES) -lstdc++

extractsfx : extractsfx.cpp extract.cpp $(DECSOURCES)
	gcc $(CFLAGS) -o $@ -g extractsfx.cpp extract.cpp $(DECSOURCES) -lstdc++

clean :
	rm -f mksfx
	rm -f sfx
	rm -f sfx86
	rm -f extractsfx
	rm -f *.o
