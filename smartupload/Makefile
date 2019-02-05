
include ../config

target: smartftp

SRCS = smartftp.c upload.c parseconfig.c gforce_peak.c hikfile.c
OBJS = $(patsubst %.c, %.o, $(SRCS))
HEADERS = $(patsubst %.c, %.h, $(SRCS))

#EXTRA_CFLAGS = -I./curl-7.20.1/include
CFLAGS += -I$(PROJDIR)/lib/curl
LDFLAGS += -lcurl -lrt -lpthread

smartftp: smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o
	$(CC) $(CFLAGS) -o smartftp smartftp.o upload.o parseconfig.o gforce_peak.o hikfile.o $(LDFLAGS) -Wl,-rpath=.

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

strip:
	$(STRIP) smartftp
	
clean:
	rm -f *.o smartftp
