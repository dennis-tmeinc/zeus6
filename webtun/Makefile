
# Makefile for Eagle32 DVR server app
#

include ../config

HEADERS=$(PROJDIR)/dvrsvr/config.h $(PROJDIR)/dvrsvr/genclass.h 

SRCS = webtun.cpp

EXTSRCS = $(PROJDIR)/dvrsvr/config.cpp 

OBJS = $(patsubst %.cpp, %.o, $(SRCS))
OBJS += $(patsubst %.cpp, %.o, $(EXTSRCS))

CFLAGS += -I$(PROJDIR)/lib -I../rconn
LDLIBS += -lcurl -lpthread -lfixc++

# CFLAGS += -DRCMAIN 
# HEADERS += ../rconn/channel.h  ../rconn/net.h
# SRCS += ../rconn/rconn.cpp ../rconn/channel.cpp ../rconn/net.cpp

webtun : $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

testcurl : testcurl.cpp $(EXTSOURCES)  $(HEADERS)
	$(CC) $(CFLAGS) -o $@  testcurl.cpp $(EXTSOURCES) $(LDFLAGS)
	
%.o : %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f *.o
	rm -f $(OBJS)
	rm -f webtun testcurl
