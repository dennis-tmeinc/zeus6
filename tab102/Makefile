# try 'make -d' to debug make rules

# $@: The filename of a rule's target
# $<: The name of the first dependency in a rule
# $^: Space-delimited list of all the dependencies in a rule
# $?: same as $^ but only those newer than the target

include ../config

LDFLAGS += -lpthread

SRCS = tab102.cpp
SRCS += ../dvrsvr/string.cpp ../dvrsvr/config.cpp

HEADERS = ../dvrsvr/genclass.h ../dvrsvr/cfg.h ../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SRCS))

tab102 : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean

clean:
	rm -f $(OUT) *.o *~
	rm -f $(OBJS)
