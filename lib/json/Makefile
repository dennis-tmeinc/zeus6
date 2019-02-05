# Makefile for Eagle32 DVR ioprocess app

include ../../config

jtest : json.cpp jsontest.cpp json.h
	$(CC) $(CFLAGS) -o $@ json.cpp jsontest.cpp $(LDFLAGS)

clean :
	rm -f *.o
	rm -f jtest
