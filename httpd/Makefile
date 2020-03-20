
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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

eagle_setup- :
	$(MAKE) -C eagle_setup

livecam- :
	$(MAKE) -C livecam

getsetup- :
	$(MAKE) -C getsetup

cfgdown- :
	$(MAKE) -C cfgdown

clean :
	rm -f eaglehttpd
	rm -f *.o
	rm -f $(OBJS)
	$(MAKE) -C eagle_setup clean
	$(MAKE) -C getsetup clean
	$(MAKE) -C livecam clean
	$(MAKE) -C cfgdown clean



