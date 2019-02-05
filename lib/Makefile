# Makefile for Eagle32 DVR 

include ../config

# how to build libcppmem.a
#    $(CC) -Os -c cppmem.cpp
#    $(CROSS_COMPILE_PREFIX)ar -cvq libcppmem.a cppmem.o

SRC=fixcpp/*
libfixc++.a : $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)
	$(CROSS_COMPILE_PREFIX)ar -cvq libfixc++.a *.o

clean :
	rm -f *.o
	rm -f libfixc++.a
