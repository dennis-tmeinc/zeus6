# Makefile for ZEUS6 DVR server app
#

include ../config

default: hostapd

src:
	tar xvzf hostapd-2.6.tar.gz
	echo 1 > src

hostapd: src
	$(MAKE) -C hostapd-2.6/hostapd
	cp hostapd-2.6/hostapd/hostapd hostapd
	$(STRIP) hostapd

clean:
	rm -f hostapd src
