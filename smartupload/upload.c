#include "upload.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>

#define BLOCK_SIZE (1024 * 1024)

/* 
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static const unsigned int crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};
/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */

static unsigned int 
crc32(unsigned int crc, const unsigned char* buf, unsigned int len)
{
    if (buf == 0) 
	return 0L;

    crc = crc ^ 0xffffffffL;
    while (len >= 8)
    {
      DO8(buf);
      len -= 8;
    }
    if (len) do {
      DO1(buf);
    } while (--len);
    return crc ^ 0xffffffffL;
}

static void writeDebug(char *fmt, ...)
{
  va_list vl;
  char str[256];

  va_start(vl, fmt);
  vsprintf(str, fmt, vl);
  va_end(vl);

  fprintf(stderr, "%s\n", str);

#ifdef DBGOUT
  char ts[64];
  time_t t = time(NULL);
  ctime_r(&t, ts);
  ts[strlen(ts) - 1] = '\0';

  FILE *fp;
  fp = fopen(dbgfile, "a");
  if (fp != NULL) {
    fprintf(fp, "%s : %s\n", ts, str);
    fclose(fp);
  }
#endif
}

/*
 * return  1: parameter error
 *        -1: disk error
 *         0: success
 */
int write_resume_info(struct fileInfo *fi, off_t offset,
		       char *dirname, char *servername)
{
  FILE *fp;
  char path[256];

  if (!fi || !dirname || !servername)
    return 1;

  snprintf(path, sizeof(path),
	   "%s/_%s_/resumeinfo",
	   dirname, servername);
  fp = fopen(path, "w");
  if (fp) {
    fprintf(fp,
	    "%02d,%04d,%02d,%02d,"
	    "%02d,%02d,%02d,%03d,"
	    "%u,%u,%c,%c,%d,%ld,%s\n",
	    fi->ch, fi->year, fi->month, fi->day, 
	    fi->hour, fi->min, fi->sec, fi->millisec,
	    fi->len, fi->prelock, fi->type, fi->ext,
	    fi->size, offset, servername);
    fclose(fp);
    return 0;
  }

  return -1;
}

#if 0
void write_resume_info2(struct fileInfo *fi, off_t offset,
		      char *resumeinfo_filename, char *servername)
{
  char *ptr, *dir_path;

  if (!resumeinfo_filename) return;

  dir_path = strdup(resumeinfo_filename);
  if (!dir_path) return; 

  /* get path to resume_info file */
  ptr = strstr(dir_path, "/_");
  if (!ptr) {
    free(dir_path);
    return;
  }

  *ptr = 0;
  write_resume_info(fi, offset, dir_path, servername);

  free(dir_path);
}
#endif

static int connectTcp(char *addr, short port)
{
  int sfd;
  struct sockaddr_in destAddr;

  //fprintf(stderr, "connectTcp(%s,%d)\n", addr,port);
  sfd = socket(PF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    return -1;

  destAddr.sin_family = AF_INET;
  destAddr.sin_port = htons(port);
  destAddr.sin_addr.s_addr = inet_addr(addr);
  memset(&(destAddr.sin_zero), '\0', 8);

  if (connect(sfd, (struct sockaddr *)&destAddr,
	      sizeof(struct sockaddr)) == -1) {
    close(sfd);
    return -1;
  }

  return sfd;
}

static int sendall(int s, unsigned char *buf, int *len)
{
  int total = 0;
  int bytesleft = *len;
  int n = 0;

  while (total < *len) {
    //writeDebug("send(%d)", bytesleft);
    n = send(s, buf + total, bytesleft, MSG_NOSIGNAL);
    //writeDebug("send return (%d)", n);
    if (n == -1) { break; }
    total += n;
  }

  *len = total;

  return n == -1 ? -1 : 0;
}

static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret = 0;

  do {
    clearerr(stream);
    ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
  } while (ret < nmemb && ferror(stream) && errno == EINTR);

  return ret;
}

static int blockUntilReadable(int socket, struct timeval* timeout) {
  int result = -1;
  do {
    fd_set rd_set;
    FD_ZERO(&rd_set);
    if (socket < 0) break;
    FD_SET((unsigned) socket, &rd_set);
    const unsigned numFds = socket+1;

    result = select(numFds, &rd_set, NULL, NULL, timeout);
    if (timeout != NULL && result == 0) {
      break; // this is OK - timeout occurred
    } else if (result <= 0) {
#if defined(__WIN32__) || defined(_WIN32)
#else
      if (errno == EINTR || errno == EAGAIN) continue;
#endif
      fprintf(stderr, "select() error: \n");
      break;
    }

    if (!FD_ISSET(socket, &rd_set)) {
      fprintf(stderr, "select() error - !FD_ISSET\n");
      break;
    }
  } while (0);

  return result;
}

static int remoteOpenFile(int s, const char* filename)
{
  int size;
  struct controlopenfile cof;
  struct controlresponse cr;
  
  cof.cmd = CONTROL_OPEN_FILE;
  strncpy(cof.filename, filename, MAX_PATH);
  
  size = sizeof(cof);
  if(sendall(s, (char*)&cof, &size)){
    writeDebug("open file send error.");
    return -1;
  }

  struct timeval timeout = {10, 0};
  if (blockUntilReadable(s, &timeout) <= 0) {
    writeDebug("remoteopen timeout");
    return -1;
  }

  if (recv(s, (char*)&cr, sizeof(cr), 0) != sizeof(cr)) {
    writeDebug("open file recv error.");
    return -1;
  }
  if (cr.cmd == CONTROL_SUCCESS) {
    return 1;
  }

  writeDebug("open file other fail %d", cr.cmd);

  return -1;
}

static int remoteClose(int s)
{
  struct controlresponse cr;
  struct controlclose cc;
  int size;

  cc.cmd = CONTROL_CLOSE_FILE;
  size = sizeof(cc);
  if(sendall(s, (char*)&cc, &size)){
    writeDebug("remote close send error.");
    return 0;
  }
  
  struct timeval timeout = {10, 0};
  if (blockUntilReadable(s, &timeout) <= 0) {
    writeDebug("remoteclose timeout");
    return 0;
  }

  if (recv(s, (char*)&cr, sizeof(cr), 0)!= sizeof(cr)) {
    writeDebug("close error recv");
    return 0;
  }

  if (cr.cmd == CONTROL_SUCCESS) {
    return 1;
  }

  writeDebug("close error CONTROL_FAIL");

  return 0;
}

static int remoteWrite(int s, char* buff, off_t offset, int bufflen)
{
  struct controlwrite cw;
  struct controlresponse cr;
  int size;

  //writeDebug("remoteWrite");

  cw.cmd = CONTROL_WRITE;
  cw.offset = offset;
  cw.length = bufflen;
  cw.crc = crc32(0, buff, bufflen);

  size = sizeof(cw);
  if(sendall(s, (char*)&cw, &size)){
    writeDebug("remote write send error.");
    return 0;
  }
  //writeDebug("control write sent");

  int fixbufsize = 256*1024;
  int sendbufsize = 0;
  int remaindatalen = bufflen;
  while (remaindatalen > 0) {
    //writeDebug("%d, %d", remaindatalen, fixbufsize);
    if (remaindatalen > fixbufsize){
      sendbufsize = fixbufsize;
    }else{
      sendbufsize = remaindatalen;
    }
    size = sendbufsize;
    //writeDebug("sending %d", size);
    if(sendall(s, buff+(bufflen-remaindatalen), &size)){
      writeDebug("remote write send error2.");
      return 0;
    }else{
      //writeDebug("remote write sent %d bytes", size);
      remaindatalen-= sendbufsize;
    }
  }

  //writeDebug("waiting for reply");
  struct timeval timeout = {10, 0};
  if (blockUntilReadable(s, &timeout) <= 0) {
    writeDebug("remotewrite timeout");
    return 0;
  }

  if (recv(s, (char*)&cr, sizeof(cr), 0)!= sizeof(cr)) {
    writeDebug("write error recv");
    return 0;
  }
  if (cr.cmd == CONTROL_SUCCESS) {
    //writeDebug("write success offset %ld", offset);
    return 1;
  }
  writeDebug("write error CONTROL_FAIL");

  return 0;
}

int oldversion;
static long long remoteFileSize(int s)
{
  int size;
  struct controlfilesize cfs;
  struct controlresponsesize crs;
  
  cfs.cmd = CONTROL_FILE_SIZE;
  
  size = sizeof(cfs);
  if(sendall(s, (char*)&cfs, &size)){
    writeDebug("file size query error.");
    return -1LL;
  }

  struct timeval timeout = {3, 0};
  if (blockUntilReadable(s, &timeout) <= 0) {
    writeDebug("file size query timeout");
    oldversion = 1;
    return -1LL;
  }

  if (recv(s, (char*)&crs, sizeof(crs), 0) != sizeof(crs)) {
    writeDebug("file size query recv error.");
    return -1LL;
  }
  if (crs.cmd == CONTROL_SUCCESS) {
    return crs.size;
  }

  writeDebug("file size query other fail %d", crs.cmd);

  return -1LL;
}


/*
 * fdest: e.g. \\192.168.3.20\data_d\_BUS001\20090721\CH00\...
 */

off_t sockfilecopy(FILE *fp_src, 
		   char *fdest, off_t offset,
		   int *finished, int *write_error, int *file_exist,
		   struct fileInfo *fi, char *dirname, char *servername)
{
  int sstream;
  char readbuf[BLOCK_SIZE];
  int bytes;
  size_t totalcopied = 0;
  off_t cur_offset;
  long long cur_filesize; 

  if (finished) *finished = 0;
  if (write_error) *write_error = 0;
  if (file_exist) *file_exist = 0;

  //writeDebug("sockfilecopy(%ld)", offset);

  char server_ip[128];
  memset(server_ip, 0, sizeof(server_ip));
  char *p1 = strchr(fdest+2, '\\');
  if (!p1) {
    return offset;
  }
   
  strncpy(server_ip, fdest+2, (p1-fdest-2));

  //writeDebug("connecting to: %s", server_ip);
  sstream = connectTcp(server_ip, STREAM_PORT);
  if (sstream == -1) {
    return offset;
  }

  //writeDebug("opening : %s", fdest);
  if (remoteOpenFile(sstream, fdest) == -1) {
    writeDebug("remoteopenfile fail");
    close(sstream);
    return offset;
  }

  if (fi && !oldversion) {
    cur_filesize = remoteFileSize(sstream);
    writeDebug("remotefilesize:%lld(%u)", cur_filesize, fi->size);
    /* already uploaded the same file? */ 
    if (cur_filesize == (long long)fi->size) {
      if (finished) *finished = 1;
      if (file_exist) *file_exist = 1;
      writeDebug("same file on the server already!");
      remoteClose(sstream);
      close(sstream);
      return offset;
    }
  }

  //writeDebug("seeking to: %ld", offset);
  if (fseek(fp_src, offset, SEEK_SET) == -1) {
    writeDebug("seek error:%s", strerror(errno));
    remoteClose(sstream);
    close(sstream);
    return offset;
  }

  //writeDebug("reading file %p", fp_src);
  while (1) {
    cur_offset = ftell(fp_src);

    bytes = safe_fread(readbuf, 1, BLOCK_SIZE, fp_src);
    //writeDebug("read:%d", bytes);
    if (ferror(fp_src)) {
      writeDebug("readfile fail");
      remoteClose(sstream);
      close(sstream);
      return offset+totalcopied;
    }

    if (remoteWrite(sstream, readbuf, cur_offset, bytes)) {
      totalcopied += bytes;
      /* save resume info only when 1 full block is written */
      if (bytes == BLOCK_SIZE) {
	if (-1 == write_resume_info(fi,
				    offset + totalcopied,
				    dirname, servername)) {
	  if (write_error) *write_error = 1;
	}
      }
    }else{
      writeDebug("remotewrite file fail");
      remoteClose(sstream);
      close(sstream);
      return offset+totalcopied;
    }

    if (feof(fp_src)) {
      //writeDebug("EOF");
      if (finished) *finished = 1;
      break;
    }
  }
  
  remoteClose(sstream);
  close(sstream);

  return offset+totalcopied;
}

off_t remotefilecopy(FILE *fp_src, char *fdest, off_t offset,
		     struct fileInfo *fi, char *dirname, char *servername,
		     int *write_error, int *file_exist)
{
  off_t offsetin, offsetout;
  int finished = 0;
  int i;
  
  offsetin = offset;
  for (i = 0; i < 2; i++) {
    writeDebug("sockfilecopy:%s offset %ld", fdest, offsetin);
    offsetout = sockfilecopy(fp_src, fdest, offsetin, &finished, write_error,
			     file_exist, fi, dirname, servername);
    //writeDebug("sockfilecopy return: %ld", offsetout);
    if (!finished) {
      offsetin = offsetout;
    }else{
      break;
    }
  }
  writeDebug("write result: %ld", offsetout);
  
  return offsetout;
}
