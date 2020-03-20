# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

include ../config

LDLIBS += -lpthread -lrt

SRCS = tab102.cpp
SRCS += ../dvrsvr/string.cpp ../dvrsvr/config.cpp ../ioprocess/diomap.cpp ../ioprocess/serial.cpp

HEADERS = ../dvrsvr/genclass.h ../dvrsvr/config.h ../ioprocess/diomap.h ../ioprocess/serial.h

OBJS = $(patsubst %.cpp, %.o, $(SRCS))

tab102 : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -r tab102
	rm -f $(OUT) *.o *~
	rm -f $(OBJS)
