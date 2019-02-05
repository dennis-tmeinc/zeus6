
include ../config

LDFLAGS += -lpthread

SOURCES=filert.c parseconfig.c

OBJS = $(patsubst %.c, %.o, $(SOURCES))

all: filert

filert: $(OBJS)
	$(CC) $(CFLAGS) -o filert $^ $(LDFLAGS)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

strip:
	$(STRIP) filert

clean:
	rm -f *.o filert
