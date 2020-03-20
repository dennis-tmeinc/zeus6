# Makefile for ZEUS6 DVR server app
#

include ../config

export CROSS_COMPILE 
export CC
export STRIP i
export CFLAGS
export LDFLAGS

default: all

src:
	tar xvjf udhcp_0.9.8.tar.bz2
	echo 1 > src

udhcpc: src
	$(MAKE) -C udhcp udhcpc
	ln -v -f udhcp/udhcpc .

udhcpd: src
	$(MAKE) -C udhcp udhcpd
	ln -v -f udhcp/udhcpd .

all: src
	$(MAKE) -C udhcp
	ln -v -f udhcp/udhcpd .

clean:
	rm -f -r udhcp
	rm -f udhcpc udhcpd src
