
CC=g++

CPLUSPLUS_FLAGS= 


SOURCES=bodycameratest.cpp jsoncpp.cpp

		
all: bodycameratest

.$(cpp).$(o):
	$(CC) -c $(CPLUSPLUS_FLAGS) $<


OBJS=jsoncpp.o bodycameratest.o	


bodycameratest: $(OBJS)
	$(CC) -o bodycameratest $(OBJS) -lpthread
	
clean:
	rm -f *.o bodycameratest
	

