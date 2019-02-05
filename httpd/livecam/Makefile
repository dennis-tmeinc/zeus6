
# Makefile for Eagle32 DVR web base setup app
#

include ../../config

#SOURCES=eagle_setup.cpp md5.cpp
SOURCES=livecam.cpp $(PROJDIR)/lib/net/net.cpp
HEADERS=

EXTSOURCES=$(PROJDIR)/dvrsvr/config.cpp $(PROJDIR)/dvrsvr/string.cpp $(PROJDIR)/dvrsvr/crypt.cpp
EXTHEADERS=$(PROJDIR)/dvrsvr/cfg.h $(PROJDIR)/dvrsvr/genclass.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))
OBJS += $(patsubst %.cpp, %.o, $(EXTSOURCES))

LDFLAGS += -lcrypt

mp4live : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.cpp $(HEADERS) $(EXTHEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f mp4live
	rm -f $(OBJS)
	rm -f *.o


