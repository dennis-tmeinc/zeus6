
include ../config

SOURCES=dosfsckcheck.cpp

HEADERS=

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

dosfsckcheck : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)  $(LDFLAGS)

%.o : %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f dosfsckcheck
