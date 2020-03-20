
# Makefile for Eagle32 DVR ioprocess app
#

include ../config

HEADERS += \
	$(PROJDIR)/lib/cjson/cjson.h \
	$(PROJDIR)/lib/net/net.h \
	$(PROJDIR)/dvrsvr/config.h \
	$(PROJDIR)/dvrsvr/genclass.h \
	$(PROJDIR)/ioprocess/diomap.h 

DECSRCS = \
	$(PROJDIR)/lib/lzmasdk/dec.cpp \
	$(PROJDIR)/lib/lzmasdk/LzmaDec.c 

ENCSRCS = \
	$(PROJDIR)/lib/lzmasdk/enc.cpp \
	$(PROJDIR)/lib/lzmasdk/LzmaEnc.c \
	$(PROJDIR)/lib/lzmasdk/LzFind.c

sfxfiles: sfx mksfx

all: sfx mksfx sfx86 extractsfx

sfx : sfx.o extract.o $(DECSRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mksfx : mksfx.cpp $(ENCSRCS) $(HEADERS)
	gcc $(CFLAGS_PC) -o $@ mksfx.cpp $(ENCSRCS) -lstdc++

sfx86 : sfx.cpp extract.cpp $(DECSRCS) $(HEADERS)
	gcc $(CFLAGS_PC) -o $@ sfx.cpp extract.cpp $(DECSRCS) -lstdc++

extractsfx : extractsfx.cpp extract.cpp $(DECSRCS) $(HEADERS)
	gcc $(CFLAGS_PC) -o $@ extractsfx.cpp extract.cpp $(DECSRCS) -lstdc++

%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f sfx mksfx sfx86 extractsfx
	rm -f *.o

