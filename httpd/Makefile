
# Makefile for ZEUS 6 DVR
#

include ../config

CFLAGS += -DMOD_SUPPORT -I ./src_httpd
LDFLAGS += -lcrypt 

all: eaglehttpd eagle_setup- getsetup- cfgdown- livecam-

SOURCES = src_httpd/eaglehttpd.cpp src_httpd/smallssi.cpp src_httpd/var.cpp src_httpd/access.cpp src_httpd/mod.cpp 
HEADERS = src_httpd/eaglehttpd.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

eaglehttpd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

eagle_setup- :
	make -C eagle_setup

livecam- :
	make -C livecam

getsetup- :
	make -C getsetup

cfgdown- :
	make -C cfgdown

clean :
	rm -f eaglehttpd
	rm -f *.o
	rm -f $(OBJS)
	make -C eagle_setup clean
	make -C getsetup clean
	make -C livecam clean
	make -C cfgdown clean



