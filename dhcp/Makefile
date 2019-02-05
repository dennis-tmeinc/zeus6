# Makefile for ZEUS6 DVR server app
#

include ../config

CFLAGS += -mthumb

default: all

src:
	tar xvjf udhcp_0.9.8.tar.bz2
	echo 1 > src

udhcpc: src
	$(MAKE) -j -C udhcp udhcpc
	cp udhcp/udhcpc .

udhcpd: src
	$(MAKE) -j -C udhcp udhcpd
	cp udhcp/udhcpd .

all: src
	$(MAKE) -j -C udhcp
	cp udhcp/udhcpd .

clean:
	rm -f -r udhcp
	rm -f udhcpc udhcpd src
