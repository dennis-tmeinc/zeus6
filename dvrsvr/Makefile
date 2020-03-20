#
# Makefile for Eagle32 DVR server app
#

include ../config

SOURCES=main.cpp \
		dvrtime.cpp \
		dvrsvr.cpp \
		netsvr.cpp \
		ptz.cpp \
		record.cpp \
		capture.cpp \
		eaglecapture.cpp \
		ipcapture.cpp \
		memory.cpp \
		dvrfile.cpp \
		playback.cpp \
		string.cpp \
		disk.cpp \
		config.cpp \
		event.cpp \
		live.cpp \
		crypt.cpp \
		dio.cpp \
		vri.cpp \
		fifoswap.cpp \
		archive.cpp \
		ipstream.cpp \
		../ioprocess/diomap.cpp

HEADERS=../cfg.h\
		dvr.h \
		genclass.h \
		config.h \
		fbwindow.h \
		crypt.h \
		mpegps.h \
		fifoswap.h \
		block.h  \
		record.h \
		dir.h \
		vri.h \
		archive.h \
		$(PROJDIR)/lib/util/string.h\
		$(PROJDIR)/lib/util/array.h\
		../ioprocess/diomap.h

OBJS = $(patsubst %.cpp, %.o, $(SOURCES))

ifeq ($(EAGLE),zeus8)
  LDFLAGS += -L./libzeus8 
  LDLIBS += -lpthread -lrt -lm -lipcamerasdk -luuid -leagle -lgm  -lsupc++

else
  LDFLAGS += -lstdc++ -lpthread -lm \
	-L./eagle8851 -leagle8851 -lxmlwrapper -lexpat -lmemmgr -lmsgreader \
	-lvideocap -lsharedbuffmgr -lframeratectrl -lfreetype \
	-lmassmemaccess -lh264enc -ljpegenc -lresize \
	-ldeinterlacer -limgbackendprocess -lpbengine -lvideodisp -lROISelector \
	-lipcamerasdk  -luuid -lcrypto

endif

# dvrsvr : $(OBJS) $(LIBRARY)
dvrsvr : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

#.cpp.o:
%.o : %.cpp $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean :
	rm -f *.o
	rm -f dvrsvr
