include ../config

SRCS	= fw_env.c  fw_env_main.c
HEADERS	= fw_env.h

CFLAGS += -DUSE_HOSTCC

all:	fw_printenv

fw_printenv: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f fw_printenv *.o

