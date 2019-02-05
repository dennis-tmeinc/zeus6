
# Makefile for ZEUS 6 DVR
#

include ../../config

CFLAGS += -I$(PROJDIR) 
LDFLAGS+= -lcrypt

# eaglehttpd: eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp eaglehttpd.h
#	$(CC) $(CFLAGS)  -DMOD_SUPPORT -o $@ eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp $(LDFLAGS)

eaglehttpd_src=eaglehttpd.cpp smallssi.cpp var.cpp access.cpp mod.cpp 
eaglehttpd_h=eaglehttpd.h

eaglehttpd: $(eaglehttpd_src) $(eaglehttpd_h) 
	$(CC) $(CFLAGS) -I .  -DMOD_SUPPORT -o $@  $(eaglehttpd_src) $(LDFLAGS)
	cp eaglehttpd /home/dennis/nfsroot/

clean :
	rm   -f eaglehttpd
	rm  -f /home/dennis/nfsroot/eaglehttpd



