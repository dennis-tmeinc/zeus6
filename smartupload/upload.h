#ifndef _UPLOAD_H_
#define _UPLOAD_H_

#include <stdio.h>
#include <asm/types.h>
#include <sys/types.h>
#include <unistd.h>


#define CONTROL_PORT 49953
#define STREAM_PORT 49954

#define CONTROL_OPEN_FILE       1
#define CONTROL_CLOSE_FILE      2
#define CONTROL_SEEK		3
#define CONTROL_CHECKSUM	4
#define CONTROL_CHECKSUM_ASYN   5
#define CONTROL_WRITE		6
#define CONTROL_FILE_SIZE	7
#define CONTROL_SUCCESS		10000
#define CONTROL_FAIL		10001

#define CRC_OK		1
#define CRC_FAIL	2

#define WM_RECEIVE_DATA WM_APP+1
#define WM_RECEIVE_CRC_MSG WM_APP+2

#define MAX_PATH 260


struct controlresponse{
	__u32 cmd;
};

struct controlresponseseek{
	__u32 cmd;
	__s64 offset;
} __attribute__ ((__packed__));

struct controlresponsesize{
	__u32 cmd;
	__s64 size;
} __attribute__ ((__packed__));

struct controlopenfile{
	__u32 cmd;
	__s8 filename[MAX_PATH];
};

struct controlclose{
	__u32 cmd;
};

struct controlseek{
	__u32 cmd;
	__s64 offset;
	__u32 from;
} __attribute__ ((__packed__));

struct controlwrite{
	__u32 cmd;
	__s64 offset;
	__u32 length;
	__u32 crc;
} __attribute__ ((__packed__));

struct controlfilesize{
	__u32 cmd;
};

struct fileInfo {
  char ch, month, day, hour, min, sec, type, ext;
  unsigned short year, millisec;
  unsigned int len;
  unsigned int size; /* file size */
  unsigned int prelock;
  int pathId;
};

off_t remotefilecopy(FILE *fp_src, char *fdest, off_t offset,
		     struct fileInfo *fi, char *dirname, char *servername,
		     int *write_error, int *file_exist);

#endif
