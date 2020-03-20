/* --- Changes ---
 * 08/11/2009 by Harrison
 *   1. Fix: clear gps_valid flag on any signal
 *
 * 09/14/2009 by Harrison
 *   1. Changed GPS logging interval from 3 to 2 secs.
 *
 * 09/25/2009 by Harrison
 *   1. Make this daemon
 *
 * 09/29/2009 by Harrison
 *   1. Improved GPS data receiving function
 *
 * 10/30/2009 by Harrison
 *   1. Log file always written to current disk (for Multiple disk support)
 *
 * 11/03/2009 by Harrison
 *   1. Fixed OSD GPS coordiates display issue
 *
 * 11/18/2009 by Harrison
 *   1. Fixed GPS buzzer issue
 *
 * 11/20/2009 by Harrison
 *   1. Added semaphore for gps data.
 *
 */

// GPS LOGING

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <memory.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/sem.h>

#include "../cfg.h"

#include "../dvrsvr/config.h"
#include "../dvrsvr/genclass.h"
#include "../ioprocess/diomap.h"

struct baud_table_t {
	speed_t baudv;
	int baudrate;
} baud_table[] = {
	{ B230400, 230400 },
	{ B115200, 115200 },
	{ B57600, 57600 },
	{ B38400, 38400 },
	{ B19200, 19200 },
	{ B9600, 9600 },
	{ B4800, 4800 },
	{ B2400, 2400 },
	{ B1800, 1800 },
	{ B1200, 1200 },
	{ B600, 600 },
	{ B0, 0 }
};

#define SPEEDTABLESIZE (10)

struct speed_table_t {
float speed;
float distance;
} speed_table[SPEEDTABLESIZE];

enum {
	TYPE_CONTINUOUS,
	TYPE_PEAK
};

#define MCUCMD_SETRTC 0x07
#define MCUCMD_BOOTREADY 0x08
#define MCUCMD_ENABLEPEAK 0x0f
#define MCUCMD_UPLOADACK 0x1A
#define MCUCMD_ENABLEDI 0x1B
#define MCUCMD_GET0G 0x18
#define MCUCMD_UPLOAD 0x19
#define MCUCMD_UPLOADPEAK 0x1F
#define MCUCMD_UPLOADPEAKACK 0x20

#define UPLOAD_ACK_SIZE 10
#define ENABLE_DI 0x40
#define DISABLE_DI 0

#define MCULEN 0 /* index of mcu length in the packet */
#define MCUCMD 3 /* index of mcu command in the packet */
#define MAX_MCU_RECV_BUF 13
#define MCU_BUF_SIZE (4 * 1024)

// max distance in degrees
float max_distance = 500;
// converting parameter
float degree1km = 0.012;

struct dio_mmap* p_dio_mmap;

char dvriomap[256] = "/var/dvr/dvriomap";
char gpslogfilename[512];
string dvrcurdisk;
int glog_ok = 0; // logging success.

int gps_port_disable = 0;
int gps_port_handle = -1;
char gps_port_dev[256] = "/dev/ttyS0";
int gps_port_baudrate = 4800; // gps default baud

int gforce_log_enable = 0;
int tab102b_enable = 0;
int tab102b_port_handle = -1;
char tab102b_port_dev[256] = "/dev/ttyUSB1";
int tab102b_port_baudrate = 19200; // tab102b default baud
int tab102b_data_enable = 0;

int inputdebounce = 3;
int gpsvalid;

#ifdef MCU_DEBUG
int mcu_debug_out;
#endif

#define MAXSENSORNUM (32)

string sensorname[MAXSENSORNUM];
int sensor_invert[MAXSENSORNUM];
int sensor_value[MAXSENSORNUM];
double sensorbouncetime[MAXSENSORNUM];
double sensorbouncevalue[MAXSENSORNUM];

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = CFG_FILE;
const char* pidfile = "/var/dvr/glog.pid";

int app_state; // 0: quit, 1: running: 2: restart

unsigned int sigcap = 0;

char disksroot[128] = "/var/dvr/disks";

#define GPSBUFLEN 2048
unsigned char g_gpsBuf[GPSBUFLEN];
int g_gpsBufLen;

unsigned char mcubuf[MCU_BUF_SIZE];
int mcudatasize;
unsigned short g_refX, g_refY, g_refZ, g_peakX;
unsigned short g_peakY, g_peakZ;
unsigned char g_diHi, g_diLo, g_order, r2;

//#define NET_DEBUG
#ifdef NET_DEBUG
#define dprintf(...) writeNetDebug(__VA_ARGS__)
#else
// #define dprintf(...) fprintf(stderr, __VA_ARGS__)
#define dprintf(...)
#endif

struct gps
{
	int gps_connection;
	int gps_valid;
	double gps_speed;	 // knots
	double gps_direction; // degree
	double gps_latitude;  // degree, + for north, - for south
	double gps_longitud;  // degree, + for east,  - for west
	double gps_gpstime;   // seconds from 1970-1-1 12:00:00 (utc)
};

int semid;

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
union semun {
	int val;
	struct semid_ds* buf;
	unsigned short int* array;
	struct seminfo* __buf;
};
#endif

static void prepare_semaphore()
{
	key_t unique_key;
	union semun options;

	unique_key = ftok("/dev/null", 'a');
	semid = semget(unique_key, 1, IPC_CREAT | IPC_EXCL | 0666);

	if (semid == -1) {
		if (errno == EEXIST) {
			semid = semget(unique_key, 1, 0);
			if (semid == -1) {
				fprintf(stderr, "semget error(%s)\n", strerror(errno));
				exit(1);
			}
		} else {
			fprintf(stderr, "semget error(%s)\n", strerror(errno));
			exit(1);
		}
	} else {
		options.val = 1;
		semctl(semid, 0, SETVAL, options);
	}
}

void set_gps_data(struct gps* g)
{
	struct sembuf sb = { 0, -1, 0 };

	if (!p_dio_mmap)
		return;

	sb.sem_op = -1;
	semop(semid, &sb, 1);

	gpsvalid = g->gps_valid; /* global flag */
	p_dio_mmap->gps_connection = g->gps_connection;
	p_dio_mmap->gps_valid = g->gps_valid;
	p_dio_mmap->gps_speed = g->gps_speed;
	p_dio_mmap->gps_direction = g->gps_direction;
	p_dio_mmap->gps_latitude = g->gps_latitude;
	p_dio_mmap->gps_longitud = g->gps_longitud;
	p_dio_mmap->gps_gpstime = g->gps_gpstime;

	sb.sem_op = 1;
	semop(semid, &sb, 1);
}

void clear_gps_data()
{
	struct gps g;
	g.gps_connection = 0;
	g.gps_valid = 0;
	g.gps_speed = 0;
	g.gps_direction = 0;
	g.gps_latitude = 0;
	g.gps_longitud = 0;
	g.gps_gpstime = 0;
	set_gps_data(&g);
}

#ifdef NET_DEBUG
int dfd = -1;

int connectTcp(char* addr, short port)
{
	int sfd;
	struct sockaddr_in destAddr;

	sfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
		return -1;

	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(port);
	destAddr.sin_addr.s_addr = inet_addr(addr);
	memset(&(destAddr.sin_zero), '\0', 8);

	if (connect(sfd, (struct sockaddr*)&destAddr,
			sizeof(struct sockaddr))
		== -1) {
		close(sfd);
		return -1;
	}

	return sfd;
}

void connectDebug()
{
	dfd = connectTcp("192.168.1.220", 11330);
}

void writeNetDebug(char* fmt, ...)
{
	va_list vl;
	char str[256];

	va_start(vl, fmt);
	vsprintf(str, fmt, vl);
	va_end(vl);

	if (dfd != -1) {
		send(dfd, str, strlen(str) + 1, MSG_NOSIGNAL);
	}
}
#endif
#ifdef DBG_A
void writeDebug(char* fmt, ...)
{
	va_list vl;
	char str[256];

	va_start(vl, fmt);
	vsprintf(str, fmt, vl);
	va_end(vl);

	FILE* fp;
	fp = fopen("/var/dvr/glog.txt", "w");
	if (fp) {
		fprintf(fp, "%s", str);
		fclose(fp);
	}
}
#endif

//#define FILE_DEBUG
#ifdef FILE_DEBUG
#define GLOG_SIZE (1024 * 1024) /* 1 MB */
char dbgfile[128];
void setDebugFileName(char* rootdir)
{
	DIR* dir;
	struct dirent* de;
	char path[256];

	dir = opendir(rootdir);
	if (dir) {
		while ((de = readdir(dir)) != NULL) {
			if ((de->d_name[0] == '.') || (de->d_type != DT_DIR))
				continue;

			/* check if smartlog folder exists */
			snprintf(path, sizeof(path), "%s/%s/smartlog", rootdir, de->d_name);

			DIR* dir1;
			dir1 = opendir(path);
			if (dir1) {
				snprintf(dbgfile, sizeof(dbgfile),
					"%s/glog.txt",
					path);
				closedir(dir1);
				break;
			}
		}
		closedir(dir);
	}

	if (dbgfile[0]) {
		struct stat sb;
		if (!stat(dbgfile, &sb)) {
			/* delete file if too big */
			if (sb.st_size > GLOG_SIZE) {
				unlink(dbgfile);
			}
		}
	}
}

void writeDebug(char* fmt, ...)
{
	va_list vl;
	char str[256];

	if (!dbgfile[0]) {
		setDebugFileName("/var/dvr/disks");
		if (!dbgfile[0])
			return;
	}

	va_start(vl, fmt);
	vsprintf(str, fmt, vl);
	va_end(vl);

	char ts[64];
	time_t t = time(NULL);
	ctime_r(&t, ts);
	ts[strlen(ts) - 1] = '\0';

	FILE* fp;
	fp = fopen(dbgfile, "a");
	if (fp != NULL) {
		fprintf(fp, "%s : %s\n", ts, str);
		fclose(fp);
	}
}
#endif

#define WIDTH 16
#define DBUFSIZE 1024
int dump(unsigned char* s, int len)
{
	char buf[DBUFSIZE], lbuf[DBUFSIZE], rbuf[DBUFSIZE];
	unsigned char* p;
	int line, i;

	p = (unsigned char*)s;
	for (line = 1; len > 0; len -= WIDTH, line++) {
		memset(lbuf, '\0', DBUFSIZE);
		memset(rbuf, '\0', DBUFSIZE);
		for (i = 0; i < WIDTH && len > i; i++, p++) {
			sprintf(buf, "%02x ", (unsigned char)*p);
			strcat(lbuf, buf);
			sprintf(buf, "%c", (!iscntrl(*p) && *p <= 0x7f) ? *p : '.');
			strcat(rbuf, buf);
		}
		dprintf("%04x: %-*s    %s\n", line - 1, WIDTH * 3, lbuf, rbuf);
	}
	if (!(len % 16)) {
		dprintf("\n");
	}
	return line;
}

void sig_handler(int signum)
{
	if( signum == SIGTERM || signum == SIGINT ) {
		sigcap |= 0x8000;
	}
	else if (signum == SIGUSR1) {
		sigcap |= 1;
	} else if (signum == SIGUSR2 || signum == SIGCONT) {
		sigcap |= 2;
	}
}

static pthread_mutex_t log_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

void log_lock()
{
	pthread_mutex_lock(&log_mutex);
}

void log_unlock()
{
	pthread_mutex_unlock(&log_mutex);
}

struct gps_data {
	int valid;
	int year;
	int month;
	int day;
	int hour;
	int minute;
	float second;
	float latitude;
	float longitude;
	float speed;
	float direction;
};

/* x: 0 - 99,999 */
int binaryToBCD(int x)
{
	int m, I, b, cc;
	int bcd;

	cc = (x % 100) / 10;
	b = (x % 1000) / 100;
	I = (x % 10000) / 1000;
	m = x / 10000;

	bcd = (m * 9256 + I * 516 + b * 26 + cc) * 6 + x;

	return bcd;
}

static unsigned char getChecksum(unsigned char* buf, int size)
{
	unsigned char cs = 0;
	int i;

	for (i = 0; i < size; i++) {
		cs += buf[i];
	}

	cs = 0xff - cs;
	cs++;

	return cs;
}

static int verifyChecksum(unsigned char* buf, int size)
{
	unsigned char cs = 0;
	int i;

	for (i = 0; i < size; i++) {
		cs += buf[i];
	}

	if (cs)
		return 1;

	return 0;
}

/* return:
 *   -1: select error
 *    0: timeout
 *    1: data available
 *
 */
static int blockUntilReadable(int socket, struct timeval* timeout)
{
	int result = -1;
	do {
		fd_set rd_set;
		FD_ZERO(&rd_set);
		if (socket < 0)
			break;
		FD_SET((unsigned)socket, &rd_set);
		const unsigned numFds = socket + 1;

		/* call select only when no data is available right away */
		unsigned int cbData = 0;
		if (!ioctl(socket, FIONREAD, &cbData) && (cbData > 0)) {
			result = 1;
			break;
		}

		result = select(numFds, &rd_set, NULL, NULL, timeout);
		if (timeout != NULL && result == 0) {
			break; // this is OK - timeout occurred
		} else if (result <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			dprintf("select() error: \n");
			break;
		}

		if (!FD_ISSET(socket, &rd_set)) {
			dprintf("select() error - !FD_ISSET\n");
			break;
		}
	} while (0);

	return result;
}

speed_t get_baudrate(int speed)
{
	speed_t baudrate = 0;
	switch (speed) {
	case 110:
		baudrate = B110;
		break;
	case 300:
		baudrate = B300;
		break;
	case 600:
		baudrate = B600;
		break;
	case 1200:
		baudrate = B1200;
		break;
	case 2400:
		baudrate = B2400;
		break;
	case 4800:
		baudrate = B4800;
		break;
	case 9600:
		baudrate = B9600;
		break;
	case 19200:
		baudrate = B19200;
		break;
	case 38400:
		baudrate = B38400;
		break;
	case 57600:
		baudrate = B57600;
		break;
	case 115200:
		baudrate = B115200;
		break;
	default:
		break;
	}

	return baudrate;
}

int open_serial(char* dev)
{
	int fd;
	fd = open((char*)dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		printf("open usb uart0 failed\n");
		return -1;
	}
	if (fcntl(fd, F_SETFL, 0) < 0) {
		printf("fcntl failed for usb uart\n");
		close(fd);
		return -1;
	}
	return fd;
}

int config_serial(int fd, int baud)
{
	struct termios newtio, oldtio;

	//bzero(&newtio,sizeof(newtio));
	memset(&newtio, 0, sizeof(newtio));
	tcgetattr(fd, &newtio);
	newtio.c_cflag |= CS8 | CLOCAL | CREAD;
	//set no parity
	newtio.c_cflag &= ~PARENB;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;	// non - canonical
	newtio.c_cc[VMIN] = 0; // minimum char 0
	newtio.c_cc[VTIME] = 0;
	//set baud rate
	if (baud == 4800) {
		if (cfsetispeed(&newtio, B4800) < 0) {
			printf("cfsetspeed failed:%d\n", errno);
			return -1;
		}

		if (cfsetospeed(&newtio, B4800) < 0) {
			printf("cfsetospeed failed:%d\n", errno);
			return -1;
		}
	} else if (baud == 9600) {
		if (cfsetispeed(&newtio, B9600) < 0) {
			printf("cfsetspeed failed:%d\n", errno);
			return -1;
		}

		if (cfsetospeed(&newtio, B9600) < 0) {
			printf("cfsetospeed failed:%d\n", errno);
			return -1;
		}
	}
	//set stop bits 1
	newtio.c_cflag &= ~CSTOPB;
	newtio.c_lflag = ICANON;
	tcflush(fd, TCIFLUSH);
	//activate the new setting
	if (tcsetattr(fd, TCSANOW, &newtio) < 0) {
		printf("tcsetattr failed:%d\n", errno);
		return -1;
	}
	tcflush(fd, TCIFLUSH);
	return 0;
}
#if 0
int openCom(char *dev, int speed)
{
  int fd;
  struct termios tio;
  speed_t baudrate;

  baudrate = get_baudrate(speed);
  if (!baudrate) {
	return -1;
  }
  fd=open_serial(dev);
  if(fd<0)
	 return -1;
  if(config_serial(fd,baudrate)<0){
	return -1;
  }
  return fd;
}
#endif
#if 1
int openCom(char* dev, int speed)
{
	int fd;
	struct termios tio;
	speed_t baudrate;

	baudrate = get_baudrate(speed);
	if (!baudrate) {
		return -1;
	}

	fd = open((char*)dev, O_RDWR | O_NOCTTY /* | O_NDELAY*/);
	if (fd == -1) {
		perror(dev);
		return -1;
	}
	dprintf("%s(%d) opened\n", dev, fd);

	tcgetattr(fd, &tio);

	/* set speed */
	cfsetispeed(&tio, baudrate);
	cfsetospeed(&tio, baudrate);

	tio.c_cflag |= (CLOCAL | CREAD); /* minimum setting */
	/* 8N1 setting */
	tio.c_cflag &= ~PARENB; /* No parity */
	tio.c_cflag &= ~CSTOPB; /* 1 stop bit */
	tio.c_cflag &= ~CSIZE;  /* Mask character size bits and */
	tio.c_cflag |= CS8;		/* set the size to 8 bit */

	tio.c_cflag &= ~CRTSCTS;						/* Use CNEW_RTSCTS, No hardware flowcontrol */
	tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* raw input */
	tio.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL); /* no software flowcontrol */
	tio.c_oflag &= ~OPOST;							/* raw output */

	tio.c_cc[VINTR] = 0;
	tio.c_cc[VQUIT] = 0;
	tio.c_cc[VERASE] = 0;
	tio.c_cc[VKILL] = 0;
	tio.c_cc[VEOF] = 4;
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1; /* minimum number of characters to read */
	tio.c_cc[VSWTC] = 0;
	tio.c_cc[VSTART] = 0;
	tio.c_cc[VSTOP] = 0;
	tio.c_cc[VSUSP] = 0;
	tio.c_cc[VEOL] = 0;
	tio.c_cc[VREPRINT] = 0;
	tio.c_cc[VDISCARD] = 0;
	tio.c_cc[VWERASE] = 0;
	tio.c_cc[VLNEXT] = 0;
	tio.c_cc[VEOL2] = 0;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &tio);

	return fd;
}
#endif
/* return:
 *    n: # of bytes received
 *   -1: bad size parameters
 */

int read_nbytes(int fd, unsigned char* buf, int bufsize,
	int rx_size, int timeout_in_secs,
	int showdata, int showprogress)
{
	struct timeval tv;
	int total = 0, bytes;

	if (bufsize < rx_size)
		return -1;

	while (1) {
		bytes = 0;
		tv.tv_sec = timeout_in_secs;
		tv.tv_usec = 0;
		if (blockUntilReadable(fd, &tv) > 0) {
			bytes = read(fd, buf + total, bufsize - total);
			if (bytes > 0) {
				if (showdata)
					dump(buf + total, bytes);
				total += bytes;
				if (showprogress)
					dprintf(".");
			}

			if (total >= rx_size)
				break;
		}

		if (bytes <= 0) { // timeout without receiving any data
			break;
		}
	}

	return total;
}

int got_gps;
char rmc_line[300];
unsigned int handle_gps(int mcufd, unsigned char* buf, int len)
{
	char *token, *str;
	unsigned char* usedUpto = NULL;

	/* leave one byte(-1) for null charater */
	if ((len > 100) || (g_gpsBufLen + len > (int)sizeof(g_gpsBuf) - 1)) {
		// just in case
		// fprintf(stderr, "gps data too big?\n");
		g_gpsBufLen = 0;
		return 1;
	}

	memcpy(g_gpsBuf + g_gpsBufLen, buf, len);
	g_gpsBufLen += len;
	g_gpsBuf[g_gpsBufLen] = '\0';

	str = (char*)g_gpsBuf;
	while (1) {
		token = strstr(str, "\r\n");
		if (!token)
			break;

		token[1] = '\0';

		usedUpto = (unsigned char*)token + 2;
		if (*str == '$') {
			got_gps = 1;
			dprintf("%s\n", str);
			if( str[3] == 'R' && str[4] == 'M' && str[5] == 'C' ) {		// $GPRMC
				memcpy(rmc_line, str, (int)usedUpto - (int)str);
			}
		}

		str = token + 1; /* next statement */
		if (str >= (char*)g_gpsBuf + g_gpsBufLen)
			break;
	}

	if (usedUpto) {
		if (usedUpto < (g_gpsBuf + g_gpsBufLen - 1)) {
			int copyLen = (int)g_gpsBuf + (int)g_gpsBufLen - (int)usedUpto;
			// fprintf(stderr, "moving %d bytes\n", copyLen);
			memmove(g_gpsBuf, usedUpto, copyLen);
			g_gpsBufLen = copyLen;
			// dump(g_gpsBuf, g_gpsBufLen);
		} else {
			g_gpsBufLen = 0; /* no remaining data */
		}
	}

	return 0;
}

/*
 * 0: received nothing
 * 1: received RMC
 * 2: received something else than RMC
 * 3: received partial line
 */
int getgpsdate(char* pchar, struct gps_data* gpsdata)
{
	int mItem = 0;
	int mIndex;

	int lat_deg, long_deg;
	char* pCur = pchar;
	char* ptemp = NULL;
	char fix;
	char lat_dir, long_dir;
	while (mItem < 9) {
#if 1
		// printf("mItem=%d\n",mItem);
		switch (mItem) {
		case 0:
			sscanf(pCur, "%2d%2d%f", &gpsdata->hour, &gpsdata->minute, &gpsdata->second);
			break;
		case 1:
			fix = *pCur;
			break;
		case 2:
			sscanf(pCur, "%2d%f", &lat_deg, &gpsdata->latitude);
			break;
		case 3:
			lat_dir = *pCur;
			break;
		case 4:
			sscanf(pCur, "%3d%f", &long_deg, &gpsdata->longitude);
			break;
		case 5:
			long_dir = *pCur;
			break;
		case 6:
			sscanf(pCur, "%f", &gpsdata->speed);
			break;
		case 7:
			sscanf(pCur, "%f", &gpsdata->direction);
			break;
		case 8:
			sscanf(pCur, "%2d%2d%d", &gpsdata->day, &gpsdata->month, &gpsdata->year);
			break;
		deafult:
			break;
		}
#endif
		ptemp = strchr(pCur, ',');
		pCur = ptemp + 1;
		//  printf("%s\n",pCur);
		++mItem;
	}
#if 0
   printf("year:%d month:%d day:%d hour:%d minute:%d second:%f\n",
	  gpsdata->year,gpsdata->month,gpsdata->day,gpsdata->hour,gpsdata->minute,gpsdata->second);
   printf("fix:%c lat_deg:%d latitude:%f lat_dir:%c long_deg:%d longitude:%f longdir:%c speed:%f direction:%f\n",
	  fix,lat_deg,gpsdata->latitude,lat_dir,long_deg,gpsdata->longitude,long_dir,gpsdata->speed,gpsdata->direction);
#endif
	gpsdata->valid = (fix == 'A');

	gpsdata->latitude = lat_deg + gpsdata->latitude / 60.0;
	if (lat_dir != 'N') {
		gpsdata->latitude = -gpsdata->latitude;
	}

	gpsdata->longitude = long_deg + gpsdata->longitude / 60.0;
	if (long_dir != 'E') {
		gpsdata->longitude = -gpsdata->longitude;
	}

	if (gpsdata->year >= 80) {
		gpsdata->year += 1900;
	} else {
		gpsdata->year += 2000;
	}
	return 0;
}

int gps_readdata2(struct gps_data* gpsdata)
{
	int bytes;
	struct timeval tv;
	unsigned char buf[1024];

	gpsdata->valid = 0;

	bytes = 0;
	tv.tv_sec = 1;
	tv.tv_usec = 200000;
	if (blockUntilReadable(gps_port_handle, &tv) <= 0) {
		return 0;
	}

	bytes = read(gps_port_handle, buf, sizeof(buf));
	if (bytes > 0) {
		got_gps = 0;
		rmc_line[0] = 0;
		handle_gps(gps_port_handle, buf, bytes);
		if (got_gps) {
			if (strlen(rmc_line) > 10) {
				char fix = 0;
				int lat_deg, long_deg;
				char lat_dir, long_dir;
				// end of debug
				//  sprintf(rmc_line,"%s","$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191112,020.3,E*68\n");
				getgpsdate(&rmc_line[7], gpsdata);
#if 0
				sscanf(&rmc_line[7],
					"%2d%2d%f,%c,%2d%f,%c,%3d%f,%c,%f,%f,%2d%2d%d",
					&gpsdata->hour,
					&gpsdata->minute,
					&gpsdata->second,
					&fix,
					&lat_deg, &gpsdata->latitude, &lat_dir,
					&long_deg, &gpsdata->longitude, &long_dir,
					&gpsdata->speed,
					&gpsdata->direction,
					&gpsdata->day,
					&gpsdata->month,
					&gpsdata->year);

				gpsdata->valid = (fix == 'A');

				gpsdata->latitude = lat_deg + gpsdata->latitude / 60.0;
				if (lat_dir != 'N') {
					gpsdata->latitude = -gpsdata->latitude;
				}

				gpsdata->longitude = long_deg + gpsdata->longitude / 60.0;
				if (long_dir != 'E') {
					gpsdata->longitude = -gpsdata->longitude;
				}

				if (gpsdata->year >= 80) {
					gpsdata->year += 1900;
				} else {
					gpsdata->year += 2000;
				}
#endif
				return 1;
			} else {
				return 2;
			}
		}
		return 3;
	} else {
		usleep(100000); /* something wrong in the hardware */
		p_dio_mmap->gps_connection = 0;
	}

	return 0;
}

FILE* gps_logfile;
static int gps_logday;
struct gps_data validgpsdata;

static int gps_close_fine = 0;

void gps_logclose()
{
	if (gps_logfile) {
		fclose(gps_logfile);
		gps_logfile = NULL;
		gps_close_fine = 1;
		/* current disk can change any time */
		gpslogfilename[0] = 0;
	}
}

FILE* gps_logopen(struct tm* t)
{
	char curdisk[256];
	char hostname[128];
	int r;
	int i;

	// check if day changed?
	if (gps_logday != t->tm_mday) {
		gps_logclose();
		gps_logday = t->tm_mday;
		gpslogfilename[0] = 0; // change file
	}

	if (gps_logfile == NULL) {
		if (gpslogfilename[0] == 0) {
			gethostname(hostname, 127);
			struct stat smlogstat;

			FILE* diskfile = fopen(dvrcurdisk.getstring(), "r");
			if (!diskfile)
				return NULL;

			r = fread(curdisk, 1, 255, diskfile);
			fclose(diskfile);
			if (r <= 2)
				return NULL;

			curdisk[r] = 0; // null terminate char

			sprintf(gpslogfilename,
				"%s/smartlog/%s_%04d%02d%02d_N.001",
				curdisk,
				hostname,
				(int)t->tm_year + 1900,
				(int)t->tm_mon + 1,
				(int)t->tm_mday);

			if (stat(gpslogfilename, &smlogstat) == 0) {
				if (S_ISREG(smlogstat.st_mode)) {
					// rename exist "*N.001" to "L.001", so we can reuse same file
					char newname[256];
					strncpy(newname, gpslogfilename, sizeof(newname));
					i = strlen(newname);
					newname[i - 5] = 'L';
					//fprintf(stderr, "rename %s to %s\n", gpslogfilename, newname);
					rename(gpslogfilename, newname);
					strncpy(gpslogfilename, newname, sizeof(gpslogfilename));
				} else {
					gpslogfilename[0] = 0;
				}
			} else {
				i = strlen(gpslogfilename);
				gpslogfilename[i - 5] = 'L';
				if (stat(gpslogfilename, &smlogstat) == 0) {
					if (S_ISREG(smlogstat.st_mode)) {
						// we can use this file.
						//fprintf(stderr, "using %s\n", gpslogfilename);
					} else {
						gpslogfilename[0] = 0;
					}
				} else {
					gpslogfilename[0] = 0;
				}
			}

			if (gpslogfilename[0] == 0) {
				sprintf(gpslogfilename, "%s/smartlog", curdisk);
				mkdir(gpslogfilename, 0777);
				sprintf(gpslogfilename, "%s/smartlog/%s_%04d%02d%02d_L.001",
					curdisk,
					hostname,
					(int)t->tm_year + 1900,
					(int)t->tm_mon + 1,
					(int)t->tm_mday);
			}
		}

		if (gpslogfilename[0] != 0) {
			//fprintf(stderr, "opening %s\n", gpslogfilename);
			gps_logfile = fopen(gpslogfilename, "a");
		}

		if (gps_logfile) {
			fseek(gps_logfile, 0, SEEK_END);
			if (ftell(gps_logfile) < 10) // new file, add file headers
			{
				fseek(gps_logfile, 0, SEEK_SET);
				fprintf(gps_logfile, "%04d-%02d-%02d\n99",
					(int)t->tm_year + 1900,
					(int)t->tm_mon + 1,
					(int)t->tm_mday);

				for (i = 0; i < MAXSENSORNUM && i < p_dio_mmap->inputnum; i++) {
					fprintf(gps_logfile, ",%s", sensorname[i].getstring());
				}
				fprintf(gps_logfile, "\n");
			} else if (gps_close_fine == 0) {
				fprintf(gps_logfile, "\n"); // add a newline, in case power interruption happen and file no flush correctly.
			}
		} else {
			gpslogfilename[0] = 0;
		}
	}

	return gps_logfile;
}

int gps_logprintf1(int logid, const char* fmt, ...)
{
	FILE* logfile;
	va_list ap;

	struct tm t;
	time_t tt;

	tt = time(NULL);
	localtime_r(&tt, &t);

	// no record before 2005
	if (t.tm_year < 110 || t.tm_year > 150) {
		return 0;
	}

	log_lock();
	logfile = gps_logopen(&t);
	if (logfile == NULL) {
		log_unlock();
		glog_ok = 0;
		return 0;
	}

	fprintf(logfile, "15,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f,%04x",
		(int)t.tm_hour,
		(int)t.tm_min,
		(int)t.tm_sec,
		validgpsdata.latitude < 0.0000000 ? -validgpsdata.latitude : validgpsdata.latitude,
		validgpsdata.latitude < 0.0000000 ? 'S' : 'N',
		validgpsdata.longitude < 0.0000000 ? -validgpsdata.longitude : validgpsdata.longitude,
		validgpsdata.longitude < 0.0000000 ? 'W' : 'E',
		(float)(validgpsdata.speed * 1.852), // in km/h.
		(float)validgpsdata.direction, logid);
	va_start(ap, fmt);
	vfprintf(logfile, fmt, ap);
	fprintf(logfile, "\n");
	va_end(ap);

	if (validgpsdata.speed < 1.0) {
		gps_logclose();
	}

	log_unlock();
	glog_ok = 1;
	return 1;
}

int gps_logprintf(int logid, const char* fmt, ...)
{
	FILE* logfile;
	va_list ap;

	struct tm t;
	time_t tt;

	tt = time(NULL);
	localtime_r(&tt, &t);

	// no record before 2010
	if (t.tm_year < 110 || t.tm_year > 150) {
		return 0;
	}

	log_lock();
	logfile = gps_logopen(&t);
	if (logfile == NULL) {
		log_unlock();
		glog_ok = 0;
		return 0;
	}

	fprintf(logfile, "%02d,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f",
		logid,
		(int)t.tm_hour,
		(int)t.tm_min,
		(int)t.tm_sec,
		validgpsdata.latitude < 0.0000000 ? -validgpsdata.latitude : validgpsdata.latitude,
		validgpsdata.latitude < 0.0000000 ? 'S' : 'N',
		validgpsdata.longitude < 0.0000000 ? -validgpsdata.longitude : validgpsdata.longitude,
		validgpsdata.longitude < 0.0000000 ? 'W' : 'E',
		(float)(validgpsdata.speed * 1.852), // in km/h.
		(float)validgpsdata.direction);

	va_start(ap, fmt);
	vfprintf(logfile, fmt, ap);
	fprintf(logfile, "\n");
	va_end(ap);

	if (validgpsdata.speed < 1.0) {
		gps_logclose();
	}

	log_unlock();
	glog_ok = 1;
	return 1;
}

// log gps position
int gps_log()
{
	static float logtime = 0.0;
	//    static float logdirection=0.0 ;
	//    static float logspeed=0.0 ;
	static int stop = 0;
	//    static float disttime = 0 ;
	//    static float dist=0 ;

	float ti;
	float tdiff;
	int log = 0;

	ti = 3600.0 * validgpsdata.hour + 60.0 * validgpsdata.minute + validgpsdata.second;

	//    dist+=gpsdata->speed * (1.852/3.6) * (ti-disttime) ;
	//    disttime=ti ;
	tdiff = ti - logtime;
	if (tdiff < 0.0)
		tdiff = 1000.0;

	if (validgpsdata.speed < 2.0) { //original is 0.2 from harrison
		if (stop == 0 || tdiff > 60.0) {
			stop = 1;
			log = 1;
		}
	} else if (tdiff > 2.0) {
		stop = 0;
		log = 1;
	}

	if (log) {
		if (gps_logprintf(1, "")) {
			logtime = ti;
			//            logdirection = gpsdata->direction ;
			//            logspeed = gpsdata->speed ;
			//            dist=0.0 ;
		}
	}

	return 1;
}

// log sensor event
int sensor_log()
{
	int i;

#ifdef PWII_APP
	static char st_pwii_VRI[128];
	int log_vri = 0;

	//    dio_lock();
	if (p_dio_mmap->pwii_VRI[0] && strcmp(st_pwii_VRI, p_dio_mmap->pwii_VRI) != 0) {
		strncpy(st_pwii_VRI, p_dio_mmap->pwii_VRI, sizeof(st_pwii_VRI) - 1);
		log_vri = 1;
	}
	//    dio_unlock();
	if (log_vri) {
		gps_logprintf(18, ",%s", st_pwii_VRI);
	}

	static unsigned int pwii_buttons = 0;
	unsigned int pwii_xbuttons = p_dio_mmap->pwii_buttons ^ pwii_buttons;
	if (pwii_xbuttons) {
		pwii_buttons ^= pwii_xbuttons;
		if (pwii_xbuttons & 0x100) { // REC
			if (gps_logprintf((pwii_buttons & 0x100) ? 20 : 21, ""))
				;
		}
		if (pwii_xbuttons & 0x200) { // C2
			if (gps_logprintf((pwii_buttons & 0x200) ? 22 : 23, ""))
				;
		}
		if (pwii_xbuttons & 0x400) { // tm
			if (gps_logprintf((pwii_buttons & 0x400) ? 24 : 25, ""))
				;
		}
		if (pwii_xbuttons & 0x800) { // lp
			if (gps_logprintf((pwii_buttons & 0x800) ? 26 : 27, ""))
				;
		}
	}
#endif

	if (validgpsdata.year < 2010)
		return 0;

	// log ignition changes
	static int glog_poweroff = 1; // start from power off
	if (glog_poweroff != p_dio_mmap->poweroff) {
		if (gps_logprintf(2, ",%d", p_dio_mmap->poweroff ? 0 : 1)) {
			glog_poweroff = p_dio_mmap->poweroff;
		} else {
			return 0;
		}
	}

	// log sensor changes
	unsigned int imap = p_dio_mmap->inputmap;

	int sensorid = 3; // start from 03
	int sensorlogenable = 0;
	double btime;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	btime = ((double)(unsigned long)tv.tv_sec) + (double)(tv.tv_usec) / 1000000.0;

	for (i = 0; i < p_dio_mmap->inputnum; i++) {
		int sv;

		// sensor debouncing
		sv = ((imap & (1 << i)) != 0);
		if (sensor_invert[i]) {
			sv = !sv;
		}
		if (i < 6) {
			if (sv != sensor_value[i] && (btime - sensorbouncetime[i]) > 2.5) {
				if (gps_logprintf((sv) ? (sensorid + i * 2) : (sensorid + i * 2 + 1), "")) {
					sensor_value[i] = sv;
				}
			}
		} else {
			if (sv != sensor_value[i] && (btime - sensorbouncetime[i]) > 2.5) {
				sensor_value[i] = sv;
				sensorlogenable = 1;
			}
		}
		if (sv != sensorbouncevalue[i]) { // sensor value bouncing
			sensorbouncetime[i] = btime;
			sensorbouncevalue[i] = sv;
		}
	}

	if (p_dio_mmap->inputnum > 6 && sensorlogenable > 0) {
		int tmap = 0;
		for (i = 6; i < p_dio_mmap->inputnum; ++i) {
			if (sensor_value[i])
				tmap = tmap | (1 << (i - 6));
		}
		gps_logprintf1(tmap, "");
	}

	// log g sensor value
	if (gforce_log_enable) {
		static int gforceserial = 0;
		if( p_dio_mmap->gforce_serial != gforceserial) {
			// gforce value channged!
			gforceserial = p_dio_mmap->gforce_serial;
			if (fabs(p_dio_mmap->gforce_forward_d) > 0.3 || fabs(p_dio_mmap->gforce_right_d) > 0.3 || fabs(p_dio_mmap->gforce_down_d - 1.0) > 1.0) {
				// we record forward/backward, right/left acceleration value, up/down as g-force value
				gps_logprintf(16, ",%.1f,%.1f,%.1f",
					(double)-p_dio_mmap->gforce_forward_d,
					(double)-p_dio_mmap->gforce_right_d,
					(double)p_dio_mmap->gforce_down_d);
			}
		}
	}

	return 1;
}

static int writeCom(int fd, unsigned char* buf, int size)
{
	int ret;

	if (fd < 0)
		return -1;

	ret = write(fd, buf, size);
	if (ret < 0) {
		perror("writeCom");
		return -1;
	}

	dprintf("DM-->MCU(%d)\n", ret);
	dump(buf, ret);

	return ret;
}

int sendUploadRequest(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x06;					// len
	txbuf[bi++] = 0x04;					// dest addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_UPLOAD;		// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendUploadPeakRequest(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x06;					// len
	txbuf[bi++] = 0x04;					// dest addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_UPLOADPEAK;	// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendSetRTC(int fd, struct tm* t)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x0d;											// len
	txbuf[bi++] = 0x04;											// dest addr
	txbuf[bi++] = 0x00;											// my addr
	txbuf[bi++] = MCUCMD_SETRTC;								// cmd
	txbuf[bi++] = 0x02;											// req
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_sec);		// sec
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_min);		// min
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_hour);		// hour
	txbuf[bi++] = t->tm_wday + 1;								// day of week
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_mday);		// day
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_mon + 1);	// month
	txbuf[bi++] = (unsigned char)binaryToBCD(t->tm_year - 100); // year
	txbuf[bi] = getChecksum(txbuf, bi);							// checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int send0GRequest(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x06;					// len
	txbuf[bi++] = 0x04;					// RF module addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_GET0G;			// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendBootReady(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x06;					// len
	txbuf[bi++] = 0x04;					// RF module addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_BOOTREADY;		// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendUploadAck(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x07;				// len
	txbuf[bi++] = 0x04;				// RF module addr
	txbuf[bi++] = 0x00;				// my addr
	txbuf[bi++] = MCUCMD_UPLOADACK; // cmd
	txbuf[bi++] = 0x02;				// req
	txbuf[bi++] = 0x01;
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendEnablePeak(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x06;					// len
	txbuf[bi++] = 0x04;					// RF module addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_ENABLEPEAK;	// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendEnableDI(int fd, unsigned char enable)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x07;					// len
	txbuf[bi++] = 0x04;					// RF module addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_ENABLEDI;		// cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi++] = enable;				// DI enable:0x40, disable:0
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

int sendUploadPeakAck(int fd)
{
	int bi;
	unsigned char txbuf[32];

	bi = 0;
	txbuf[bi++] = 0x07;					// len
	txbuf[bi++] = 0x04;					// RF module addr
	txbuf[bi++] = 0x00;					// my addr
	txbuf[bi++] = MCUCMD_UPLOADPEAKACK; // cmd
	txbuf[bi++] = 0x02;					// req
	txbuf[bi++] = 0x01;
	txbuf[bi] = getChecksum(txbuf, bi); // checksum
	bi++;

	return writeCom(fd, txbuf, bi);
}

/*
 * return:
 *  - 0: success, 1: error
 */
int setTab102RTC(int fd)
{
	int retry = 3;
	struct timeval tv;
	struct tm bt;
	unsigned char buf[1024];

	while (retry > 0) {
		gettimeofday(&tv, NULL);
		localtime_r(&tv.tv_sec, &bt);
		tcflush(fd, TCIOFLUSH);
		sendSetRTC(fd, &bt);

		if (read_nbytes(fd, buf, sizeof(buf), 6, 1, 1, 0) >= 6) {
			if ((buf[0] == 0x06) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_SETRTC) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, 6)) {
					dprintf("checksum error\n");
				} else {
					dprintf("Set RTC OK\n");
					//writeTab102Status("rtc");
					break;
				}
			}
		}
		retry--;
	}

	return (retry > 0) ? 0 : 1;
}

void sendUploadConfirm(int fd)
{
	int retry = 3;
	unsigned char buf[1024];

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendUploadAck(fd);

		if (read_nbytes(fd, buf, sizeof(buf), 6, 3, 1, 0) >= 6) {
			if ((buf[0] == 0x06) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_UPLOADACK) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, 6)) {
					dprintf("checksum error\n");
				} else {
					dprintf("Upload confirm Acked\n");
					break;
				}
			}
		}
		retry--;
	}
}

void sendUploadPeakConfirm(int fd)
{
	int retry = 3;
	unsigned char buf[1024];

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendUploadPeakAck(fd);

		if (read_nbytes(fd, buf, sizeof(buf), 6, 20, 1, 0) >= 6) {
			if ((buf[0] == 0x06) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_UPLOADPEAKACK) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, 6)) {
					dprintf("checksum error\n");
				} else {
					dprintf("Upload Peak confirm Acked\n");
					break;
				}
			}
		}
		retry--;
	}
}

static size_t safe_fwrite(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fwrite((char*)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

void getTab102DataFilename(char* filename, int size, const char* path, int type)
{
	time_t t;
	struct tm tm;
	char hostname[128];

	t = time(NULL);
	localtime_r(&t, &tm);
	gethostname(hostname, 127);
	snprintf(filename, size,
		"%s/%s_%04d%02d%02d%02d%02d%02d_TAB102log_L.%s",
		path,
		hostname,
		tm.tm_year + 1900,
		tm.tm_mon + 1,
		tm.tm_mday,
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec,
		(type == TYPE_CONTINUOUS) ? "log" : "peak");
}

FILE* openTab102Data(int type)
{
	int l;
	char path[256];
	FILE* fp;
	char filename[256];

	FILE* diskfile = fopen(dvrcurdisk.getstring(), "r");
	if (diskfile) {
		l = fread(path, 1, 255, diskfile);
		fclose(diskfile);
		if (l > 2) {
			strcat(path, "/smartlog");
			getTab102DataFilename(filename, sizeof(filename), path, type);
		}
		dprintf("opening %s\n", filename);
		fp = fopen(filename, "w");
		if (fp) {
			return fp;
		}
	}

	/* HD not ready, write to ramfs */
	getTab102DataFilename(filename, sizeof(filename), "/var/dvr", type);
	dprintf("opening %s\n", filename);
	fp = fopen(filename, "w");
	if (fp) {
		return fp;
	}

	return NULL;
}

void writeTab102Data(unsigned char* buf, int len, int type)
{
	FILE* fp;
	fp = openTab102Data(type);
	if (fp) {
		dprintf("OK\n");
		safe_fwrite(buf, 1, len, fp);
		fclose(fp);
	}
}

int checkContinuousData(int fd)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int nbytes, surplus = 0, uploadSize;
	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendUploadRequest(fd);

		nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 3, 0, 0);
		if (nbytes >= UPLOAD_ACK_SIZE) {
			if ((buf[0] == 0x0a) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_UPLOAD) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, UPLOAD_ACK_SIZE)) {
					dprintf("checksum error\n");
				} else {
					uploadSize = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
					dprintf("UPLOAD acked:%d\n", uploadSize);
					ret = 0;
					if (uploadSize) {
						//1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
						// + 8(0g + checksum)
						int bufsize = uploadSize + 1024;
						unsigned char* tbuf = (unsigned char*)malloc(bufsize);
						if (!tbuf)
							return 1;

						// just in case we got more data
						if (nbytes > UPLOAD_ACK_SIZE) {
							surplus = nbytes - UPLOAD_ACK_SIZE;
							memcpy(tbuf, buf + UPLOAD_ACK_SIZE, surplus);
							//dprintf("surplus moved:%d\n", surplus);
						}

						nbytes = read_nbytes(fd, tbuf + surplus, bufsize - surplus,
							uploadSize + UPLOAD_ACK_SIZE + 8 - surplus,
							3, 0, 1);
						int downloaded = nbytes + surplus;
						dprintf("UPLOAD data:%d\n", downloaded);
						if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
							if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
								sendUploadConfirm(fd);
								writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
									TYPE_CONTINUOUS);
							}
						}
						free(tbuf);
					}
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int checkPeakData(int fd)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int nbytes, surplus = 0, uploadSize;
	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendUploadPeakRequest(fd);

		nbytes = read_nbytes(fd, buf, sizeof(buf), UPLOAD_ACK_SIZE, 20, 0, 0);
		//dprintf("%d bytes\n", nbytes);
		if (nbytes >= UPLOAD_ACK_SIZE) {
			if ((buf[0] == 0x0a) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_UPLOADPEAK) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, UPLOAD_ACK_SIZE)) {
					dprintf("checksum error\n");
				} else {
					uploadSize = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
					dprintf("UPLOAD acked:%d\n", uploadSize);
					ret = 0;
					if (uploadSize) {
						//1024 for room, actually UPLOAD_ACK_SIZE(upload ack)
						// + 8(0g + direction + checksum)
						int bufsize = uploadSize + 1024;
						unsigned char* tbuf = (unsigned char*)malloc(bufsize);
						if (!tbuf)
							return 1;

						// just in case we got more data
						if (nbytes > UPLOAD_ACK_SIZE) {
							surplus = nbytes - UPLOAD_ACK_SIZE;
							memcpy(tbuf, buf + UPLOAD_ACK_SIZE, surplus);
							//dprintf("surplus moved:%d\n", surplus);
						}
						int downloaded;
						downloaded = surplus;

						if (surplus < uploadSize + UPLOAD_ACK_SIZE + 8) {
							nbytes = read_nbytes(fd, tbuf + surplus, bufsize - surplus,
								uploadSize + UPLOAD_ACK_SIZE + 8 - surplus,
								20, 0, 1);
							//dprintf("%d bytes\n", nbytes);
							downloaded = nbytes + surplus;
						}
						dprintf("UPLOAD data:%d\n", downloaded);
						if (downloaded >= uploadSize + UPLOAD_ACK_SIZE + 8) {
							if (!verifyChecksum(tbuf + UPLOAD_ACK_SIZE, uploadSize + 8)) {
								sendUploadPeakConfirm(fd);
								writeTab102Data(tbuf, uploadSize + UPLOAD_ACK_SIZE + 8,
									TYPE_PEAK);
								//writeTab102Status("data");
							}
						}
						free(tbuf);
					}
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int get0G(int fd)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int packetlen = 13;

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		send0GRequest(fd);

		if (read_nbytes(fd, buf, sizeof(buf), packetlen, 1, 1, 0) >= packetlen) {
			if ((buf[0] == packetlen) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_GET0G) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, packetlen)) {
					dprintf("checksum error\n");
				} else {
					ret = 0;
					g_refX = (buf[5] << 8) | buf[6];
					g_refY = (buf[7] << 8) | buf[8];
					g_refZ = (buf[9] << 8) | buf[10];
					g_order = buf[11];
					dprintf("Got 0G x:%d, y:%d, z:%d\n", g_refX, g_refY, g_refZ);
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int startADC(int fd)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int packetlen = 6;

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendBootReady(fd);

		if (read_nbytes(fd, buf, sizeof(buf), packetlen, 1, 1, 0) >= packetlen) {
			if ((buf[0] == packetlen) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_BOOTREADY) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, packetlen)) {
					dprintf("checksum error\n");
				} else {
					ret = 0;
					dprintf("ADC started\n");
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int enablePeak(int fd)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int packetlen = 6;

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendEnablePeak(fd);

		if (read_nbytes(fd, buf, sizeof(buf), packetlen, 1, 1, 0) >= packetlen) {
			if ((buf[0] == packetlen) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_ENABLEPEAK) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, packetlen)) {
					dprintf("checksum error\n");
				} else {
					ret = 0;
					dprintf("Peak enabled\n");
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int enableDI(int fd, unsigned char enable)
{
	int ret = 1;
	int retry = 3;
	unsigned char buf[1024];
	int packetlen = 6;

	while (retry > 0) {
		tcflush(fd, TCIOFLUSH);
		sendEnableDI(fd, enable);

		if (read_nbytes(fd, buf, sizeof(buf), packetlen, 1, 1, 0) >= packetlen) {
			if ((buf[0] == packetlen) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == MCUCMD_ENABLEDI) && (buf[4] == 0x03)) {
				if (verifyChecksum(buf, packetlen)) {
					dprintf("checksum error\n");
				} else {
					ret = 0;
					dprintf("DI %s\n", enable ? "enabled" : "disabled");
					break;
				}
			}
		}
		retry--;
	}

	return ret;
}

int tab102b_init()
{
	tab102b_port_handle = openCom(tab102b_port_dev, tab102b_port_baudrate);
	if (tab102b_port_handle == -1) {
		return 1;
	}
	if (strstr(tab102b_port_dev, "USB")) {
		close(tab102b_port_handle);
		sleep(3);
		tab102b_port_handle = openCom(tab102b_port_dev, tab102b_port_baudrate);
		if (tab102b_port_handle == -1) {
			exit(1);
		}
	}
	dprintf("tab102b_init OK\n");

	if (setTab102RTC(tab102b_port_handle))
		return 1;

	if (checkContinuousData(tab102b_port_handle))
		return 1;

	if (get0G(tab102b_port_handle))
		return 1;

	if (startADC(tab102b_port_handle))
		return 1;

	if (enablePeak(tab102b_port_handle))
		return 1;

	if (enableDI(tab102b_port_handle, ENABLE_DI))
		return 1;

	return 0;
}

/* can be called many times until it returns 0 */
int getOnePacketFromBuffer(unsigned char* buf, int bufsize,
	unsigned char* rbuf, int rLen)
{
	int packetSize;

	if (rLen) {
		/* can't handle too big data */
		if (rLen > MCU_BUF_SIZE)
			rLen = MCU_BUF_SIZE;

		/* add received data to the mcu buffer */
		if (mcudatasize + rLen > MCU_BUF_SIZE) {
			/* we are receiving garbages or not handling fast enough */
			dprintf("overflow:giving up %d b\n", mcudatasize);
			mcudatasize = 0;
		}

		memcpy(mcubuf + mcudatasize, rbuf, rLen);
		mcudatasize += rLen;
	}

	if (mcudatasize == 0)
		return 0;

	packetSize = mcubuf[MCULEN];
	if (mcubuf[MCULEN] > MAX_MCU_RECV_BUF) {
		/* check the length, if it's too long, it's a wrong byte */
		if (mcudatasize < 4) {
			return 0;
		}

		dprintf("length %d too big for cmd %02x\n",
			mcubuf[MCULEN], mcubuf[MCUCMD]);
		/* discard the data */
		mcudatasize = 0;
		return 0;
	}

	if (mcudatasize >= packetSize) {
		if (bufsize < packetSize) {
			/* buf too small */
			mcudatasize = 0;
			return 0;
		}

		/* copy one frame to the buffer */
		memcpy(buf, mcubuf, packetSize);
		/* save the size to return in a temporary variable */
		bufsize = packetSize;
		if (mcudatasize > packetSize) {
			/* copy remaining bytes to the head of mcubuffer */
			memmove(mcubuf, mcubuf + packetSize, mcudatasize - packetSize);
		}
		mcudatasize -= packetSize;

		return bufsize;
	}

	return 0;
}

static int writeDIData()
{
	if (validgpsdata.year < 1900)
		return 1;

	gps_logprintf(15, ",%04X", (g_diHi << 8) | g_diLo);

	return 0;
}

static int writePeakData()
{
	double fb, lr, ud;
	unsigned short refFB, refLR, refUD, peakFB, peakLR, peakUD;

	if (((g_order & 0xf0) == 0x10) || ((g_order & 0xf0) == 0x20)) {
		if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
			refFB = g_refX;
			refLR = g_refY;
			refUD = g_refZ;
			peakFB = g_peakX;
			peakLR = g_peakY;
			peakUD = g_peakZ;
		} else {
			refFB = g_refY;
			refLR = g_refX;
			refUD = g_refZ;
			peakFB = g_peakY;
			peakLR = g_peakX;
			peakUD = g_peakZ;
		}
	} else if (((g_order & 0xf0) == 0x30) || ((g_order & 0xf0) == 0x40)) {
		if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
			refFB = g_refY;
			refLR = g_refZ;
			refUD = g_refX;
			peakFB = g_peakY;
			peakLR = g_peakZ;
			peakUD = g_peakX;
		} else {
			refFB = g_refZ;
			refLR = g_refY;
			refUD = g_refX;
			peakFB = g_peakZ;
			peakLR = g_peakY;
			peakUD = g_peakX;
		}
	} else if (((g_order & 0xf0) == 0x50) || ((g_order & 0xf0) == 0x60)) {
		if (((g_order & 0x0f) == 0x01) || ((g_order & 0x0f) == 0x02)) {
			refFB = g_refX;
			refLR = g_refZ;
			refUD = g_refY;
			peakFB = g_peakX;
			peakLR = g_peakZ;
			peakUD = g_peakY;
		} else {
			refFB = g_refZ;
			refLR = g_refX;
			refUD = g_refY;
			peakFB = g_peakZ;
			peakLR = g_peakX;
			peakUD = g_peakY;
		}
	}

	fb = ((int)peakFB - (int)refFB) / 37.0;
	lr = ((int)peakLR - (int)refLR) / 37.0;
	ud = ((int)peakUD - (int)refUD) / 37.0;
	dprintf("peak data:%.6lf, %.6lf, %.6lf\n", fb, lr, ud);

	if (validgpsdata.year < 1900)
		return 1;

	gps_logprintf(16,
		",%.2lf,%.2lf,%.2lf",
		fb, lr, ud);

	return 0;
}

#define MAX_MCU_PACKET_SIZE 2048
int handle_mcu(unsigned char* rxbuf, int len)
{
	int packetLen;
	unsigned char buf[MAX_MCU_PACKET_SIZE];

	while (1) {
		packetLen = getOnePacketFromBuffer(buf, MAX_MCU_PACKET_SIZE, rxbuf, len);
		if (len)
			len = 0; // used the buffer. Don't use it again.

		if (packetLen < 6)
			break;

		dump(buf, packetLen);
		if (verifyChecksum(buf, packetLen)) {
			dprintf("*Checksum Error*\n");
			continue;
		}

		if ((buf[0] == 0x0c) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == 0x1e) && (buf[4] == 0x02)) {
			if (packetLen < 12)
				continue;

			g_peakX = (buf[5] << 8) | buf[6];
			g_peakY = (buf[7] << 8) | buf[8];
			g_peakZ = (buf[9] << 8) | buf[10];
			dprintf("peak x:%d, y:%d, z:%d\n", g_peakX, g_peakY, g_peakZ);
			writePeakData();
		} else if ((buf[0] == 0x08) && (buf[1] == 0x00) && (buf[2] == 0x04) && (buf[3] == 0x1c) && (buf[4] == 0x02)) {
			if (packetLen < 8)
				continue;

			g_diHi = buf[5]; /* bit 8-11 */
			g_diLo = buf[6]; /* bit 0- 7 */
			dprintf("DI %02x %02x\n", g_diHi, g_diLo);
			writeDIData();
		}
	}

	return 0;
}

int tab102b_finish()
{
	if (tab102b_port_handle > 0)
		close(tab102b_port_handle);

	return 1;
}
#if 0
static void * tab102b_thread(void *param)
{
	int tab102b_start=0;
	unsigned char buf[1024] ;
	struct timeval tv;
	int bytes;

	while( app_state ) {
	  if( app_state==1 ) {
	if( tab102b_start==0 ) {
	  if( glog_ok ) {
		tab102b_init();  // delay tab102b log until log available
		tab102b_start=1;
	  }
	}
#if 1
	if(tab102b_start) {
	  tv.tv_sec = 1;
	  tv.tv_usec = 0;
	  if (blockUntilReadable(tab102b_port_handle, &tv) > 0) {
		bytes = read(tab102b_port_handle, buf, sizeof(buf));
		if (bytes > 0) {
		  handle_mcu(buf, bytes);
		}
	  }
#else
	  /* for testing */
	if(tab102b_start) {
	  tv.tv_sec = 0;
	  tv.tv_usec = 500000;
	  if (blockUntilReadable(tab102b_port_handle, &tv) > 0) {
		bytes = read(tab102b_port_handle, buf, sizeof(buf));
		if (bytes > 0) {
		  handle_mcu(buf, bytes);
		}
	  }
	  close(tab102b_port_handle);
	  sleep(1);
  tab102b_port_handle = openCom(tab102b_port_dev, tab102b_port_baudrate);
  if (tab102b_port_handle == -1) {
	pthread_exit(NULL);
  }
#endif
	} else {
	  sleep(1);
	}
	  } else {
	if( app_state==2 && tab102b_port_handle>0 ) {
	}
	sleep(1);
	  }
	}

	if( tab102b_start ) {
		tab102b_finish();
	}

	return NULL ;
}
#endif
pthread_t tab102b_threadid = 0;

unsigned char nmeaChecksum(unsigned char* buf, int size)
{
	int i;
	unsigned char cs;

	if (size < 1)
		return 0;

	cs = buf[0];
	for (i = 1; i < size; i++) {
		cs ^= buf[i];
	}

	return cs;
}


// return
//        0 : failed
//        1 : success
int appinit()
{
	int fd;
	int i;
	char* p;
	FILE* pidf;
	string serialport;
	string iomapfile;
	string tstr;
	config dvrconfig(dvrconfigfile);
	iomapfile = dvrconfig.getvalue("system", "iomapfile");

	p_dio_mmap = NULL;
	if (iomapfile.length() == 0) {
		return 0; // no DIO.
	}

	for (i = 0; i < 60; i++) { // retry 10 times
		fd = open(iomapfile.getstring(), O_RDWR);
		if (fd > 0) {
			break;
		}
		sleep(2);
	}
	if (fd <= 0) {
		//printf("GLOG: IO module not started!");
		return 0;
	}

	p = (char*)mmap(NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd); // fd no more needed
	if (p == (char*)-1 || p == NULL) {
		//printf( "GLOG:IO memory map failed!");
		return 0;
	}
	p_dio_mmap = (struct dio_mmap*)p;

	if (p_dio_mmap->glogpid > 0) {
		kill(p_dio_mmap->glogpid, SIGTERM);
		for (i = 0; i < 60; i++) {
			if (p_dio_mmap->glogpid <= 0)
				break;
			usleep(500);
		}
	}

	// init gps log
	p_dio_mmap->usage++;
	p_dio_mmap->glogpid = getpid();
	p_dio_mmap->gps_connection = 0;

	clear_gps_data();

	// get GPS port setting
	gps_port_disable = dvrconfig.getvalueint("glog", "gpsdisable");
	serialport = dvrconfig.getvalue("glog", "serialport");
	if (serialport.length() > 0) {
		strcpy(gps_port_dev, serialport.getstring());
	}
	i = dvrconfig.getvalueint("glog", "serialbaudrate");
	if (i >= 1200 && i <= 115200) {
		gps_port_baudrate = i;
	}

	// initialize time zone
	string tz;
	string tzi;

	tz = dvrconfig.getvalue("system", "timezone");
	if (tz.length() > 0) {
		tzi = dvrconfig.getvalue("timezones", tz.getstring());
		if (tzi.length() > 0) {
			p = strchr(tzi.getstring(), ' ');
			if (p) {
				*p = 0;
			}
			p = strchr(tzi.getstring(), '\t');
			if (p) {
				*p = 0;
			}
			setenv("TZ", tzi.getstring(), 1);
		} else {
			setenv("TZ", tz.getstring(), 1);
		}
	}
	tzset();

	// initialize gps log speed table
	tstr = dvrconfig.getvalue("glog", "degree1km");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%f", &degree1km);
	}

	tstr = dvrconfig.getvalue("glog", "dist_max");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%f", &max_distance);
	} else {
		max_distance = 500;
	}
	// convert distance to degrees
	max_distance *= degree1km / 1000.0;

	for (i = 0; i < SPEEDTABLESIZE; i++) {
		char iname[200];
		sprintf(iname, "speed%d", i);
		tstr = dvrconfig.getvalue("glog", iname);
		if (tstr.length() > 0) {
			sscanf(tstr.getstring(), "%f", &(speed_table[i].speed));
			sprintf(iname, "dist%d", i);
			tstr = dvrconfig.getvalue("glog", iname);
			if (tstr.length() > 0) {
				sscanf(tstr.getstring(), "%f", &(speed_table[i].distance));
			} else {
				speed_table[i].distance = 100;
			}
			speed_table[i].distance *= degree1km / 1000.0;
		} else {
			speed_table[i].speed = 0;
			break;
		}
	}

	tstr = dvrconfig.getvalue("glog", "inputdebounce");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%d", &inputdebounce);
	}

	// gforce logging enabled ?
	gforce_log_enable = dvrconfig.getvalueint("glog", "gforce_log_enable");

	tab102b_enable = dvrconfig.getvalueint("glog", "tab102b_enable");

	tab102b_data_enable = dvrconfig.getvalueint("glog", "tab102b_data_enable");
	if (tab102b_enable)
		gforce_log_enable = tab102b_enable;

	// get dvr disk directory
	dvrcurdisk = dvrconfig.getvalue("system", "currentdisk");

	// get sensor name
	for (i = 0; i < MAXSENSORNUM && i < p_dio_mmap->inputnum; i++) {
		char sec[10];
		sprintf(sec, "sensor%d", i + 1);
		sensorname[i] = dvrconfig.getvalue(sec, "name");
		sensor_invert[i] = dvrconfig.getvalueint(sec, "inverted");
	}

	gpslogfilename[0] = 0;

	pidf = fopen(pidfile, "w");
	if (pidf) {
		fprintf(pidf, "%d", (int)getpid());
		fclose(pidf);
	}

	for (i = 0; i < 60; ++i) {
		gps_port_handle = openCom(gps_port_dev, gps_port_baudrate);
		if (gps_port_handle > 0) {
			break;
		}
		sleep(3);
	}

	//printf( "GLOG: started!\n");

	return 1;
}

int re_appinit()
{
	int fd;
	int i;
	char* p;
	FILE* pidf;
	string serialport;
	string tstr;
	config dvrconfig(dvrconfigfile);

	clear_gps_data();

	// get GPS port setting
	gps_port_disable = dvrconfig.getvalueint("glog", "gpsdisable");
	serialport = dvrconfig.getvalue("glog", "serialport");
	if (serialport.length() > 0) {
		strcpy(gps_port_dev, serialport.getstring());
	}
	i = dvrconfig.getvalueint("glog", "serialbaudrate");
	if (i >= 1200 && i <= 115200) {
		gps_port_baudrate = i;
	}

	// initialize time zone
	string tz;
	string tzi;

	tz = dvrconfig.getvalue("system", "timezone");
	if (tz.length() > 0) {
		tzi = dvrconfig.getvalue("timezones", tz.getstring());
		if (tzi.length() > 0) {
			p = strchr(tzi.getstring(), ' ');
			if (p) {
				*p = 0;
			}
			p = strchr(tzi.getstring(), '\t');
			if (p) {
				*p = 0;
			}
			setenv("TZ", tzi.getstring(), 1);
		} else {
			setenv("TZ", tz.getstring(), 1);
		}
	}
	tzset();

	// initialize gps log speed table
	tstr = dvrconfig.getvalue("glog", "degree1km");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%f", &degree1km);
	}

	tstr = dvrconfig.getvalue("glog", "dist_max");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%f", &max_distance);
	} else {
		max_distance = 500;
	}
	// convert distance to degrees
	max_distance *= degree1km / 1000.0;

	for (i = 0; i < SPEEDTABLESIZE; i++) {
		char iname[200];
		sprintf(iname, "speed%d", i);
		tstr = dvrconfig.getvalue("glog", iname);
		if (tstr.length() > 0) {
			sscanf(tstr.getstring(), "%f", &(speed_table[i].speed));
			sprintf(iname, "dist%d", i);
			tstr = dvrconfig.getvalue("glog", iname);
			if (tstr.length() > 0) {
				sscanf(tstr.getstring(), "%f", &(speed_table[i].distance));
			} else {
				speed_table[i].distance = 100;
			}
			speed_table[i].distance *= degree1km / 1000.0;
		} else {
			speed_table[i].speed = 0;
			break;
		}
	}

	tstr = dvrconfig.getvalue("glog", "inputdebounce");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%d", &inputdebounce);
	}

	// gforce logging enabled ?
	gforce_log_enable = dvrconfig.getvalueint("glog", "gforce_log_enable");

	tab102b_enable = dvrconfig.getvalueint("glog", "tab102b_enable");

	tab102b_data_enable = dvrconfig.getvalueint("glog", "tab102b_data_enable");
	if (tab102b_enable)
		gforce_log_enable = tab102b_enable;

	// get dvr disk directory
	dvrcurdisk = dvrconfig.getvalue("system", "currentdisk");

	// get sensor name
	for (i = 0; i < MAXSENSORNUM && i < p_dio_mmap->inputnum; i++) {
		char sec[10];
		sprintf(sec, "sensor%d", i + 1);
		sensorname[i] = dvrconfig.getvalue(sec, "name");
		sensor_invert[i] = dvrconfig.getvalueint(sec, "inverted");
	}

	gpslogfilename[0] = 0;

	if (gps_port_handle > 0) {
		close(gps_port_handle);
		gps_port_handle = -1;
	}
	// create sensor log thread
	for (i = 0; i < 60; ++i) {
		gps_port_handle = openCom(gps_port_dev, gps_port_baudrate);
		if (gps_port_handle > 0) {
			break;
		}
		sleep(3);
	}

	return 1;
}

// app finish, clean up
void appfinish()
{

	if (tab102b_threadid != 0) {
		pthread_join(tab102b_threadid, NULL);
	}
	tab102b_threadid = 0;

	// close serial port
	if (gps_port_handle > 0) {
		close(gps_port_handle);
		gps_port_handle = -1;
	}
	//gps_close();

	// close log file
	gps_logclose();

	// clean up shared memory
	clear_gps_data();

	p_dio_mmap->glogpid = 0;
	p_dio_mmap->usage--;

	munmap(p_dio_mmap, sizeof(struct dio_mmap));

	// delete pid file
	unlink(pidfile);

	//printf( "GPS logging process ended!\n");
}

void closeall(int fd)
{
	int fdlimit = sysconf(_SC_OPEN_MAX);

	while (fd < fdlimit)
		close(fd++);
}

int daemon(int nochdir, int noclose)
{
	switch (fork()) {
	case 0:
		break;
	case -1:
		return -1;
	default:
		_exit(0);
	}

	if (setsid() < 0)
		return -1;

	switch (fork()) {
	case 0:
		break;
	case -1:
		return -1;
	default:
		_exit(0);
	}

	if (!nochdir)
		chdir("/");

	if (!noclose) {
		closeall(0);
		open("/dev/null", O_RDWR);
		dup(0);
		dup(0);
	}

	return 0;
}

int main(int argc, char** argv)
{
	int r;
	int validgpsbell = 0;
	struct gps_data gpsdata;

#ifdef DBG_A
	writeDebug("start\n");
#endif

	if ((argc >= 2) && !strcmp(argv[1], "-f")) {
		// force foreground
	} else {
#if 1
		if (daemon(0, 0) < 0) {
			perror("daemon");
			exit(1);
		}
#endif
	}

	prepare_semaphore();
#ifdef NET_DEBUG
	connectDebug();
	writeNetDebug("glog started");
#endif

	app_state = 1;
	// setup signal handler
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGCONT, sig_handler);
	signal(SIGPIPE, SIG_IGN);

	if (appinit() == 0) {
		return 1;
	}

	memset(&gpsdata, 0, sizeof(gpsdata));
	// validgpsdata = gpsdata ;
	memset(&validgpsdata, 0, sizeof(validgpsdata));

	while (app_state) {
		if (sigcap) {
			clear_gps_data();

			if (sigcap & 0x8000) { // Quit signal ?
				app_state = 0;
			} else {
				if (sigcap & 1) {  // SigUsr1?
					app_state = 2; // suspend running
				}
				if (sigcap & 2) {
					if (app_state == 1) {
						app_state = 0; // to let sensor thread exit
						//appfinish();
						re_appinit();
					}
					app_state = 1;
				}
			}
			sigcap = 0;
		}

		// do sensor log here instead in seperate thread ?
		sensor_log();

		if (app_state == 1 && gps_port_disable == 0) {
			if (gps_port_handle < 0) {
				gps_port_handle = openCom(gps_port_dev, gps_port_baudrate);
			}
			r = gps_readdata2(&gpsdata);
			if (r == 1) { // read a GPRMC sentence?
				// update dvrsvr OSD speed display
				struct tm ttm;
				ttm.tm_sec = 0;
				ttm.tm_min = gpsdata.minute;
				ttm.tm_hour = gpsdata.hour;
				ttm.tm_mday = gpsdata.day;
				ttm.tm_mon = gpsdata.month - 1;
				ttm.tm_year = gpsdata.year - 1900;
				ttm.tm_wday = 0;
				ttm.tm_yday = 0;
				ttm.tm_isdst = -1;

				struct gps g;
				g.gps_connection = 1;
				g.gps_valid = gpsdata.valid;
				g.gps_speed = gpsdata.speed;
				g.gps_direction = gpsdata.direction;
				g.gps_latitude = gpsdata.latitude;
				g.gps_longitud = gpsdata.longitude;
				g.gps_gpstime = (double)timegm(&ttm) + gpsdata.second;
				set_gps_data(&g);
				/*
				if (gpsdata.valid) {
					printf("year:%d month:%d day:%d hour:%d minute:%d second:%d latitude:%f longitude:%f speed:%f\n",
						gpsdata.year, gpsdata.month, gpsdata.day, gpsdata.hour, gpsdata.minute, gpsdata.second,
						gpsdata.latitude, gpsdata.longitude, gpsdata.speed);
				}
				*/
				// log gps position
				if (gpsdata.valid) {
					validgpsdata.valid = gpsdata.valid;
					validgpsdata.year = gpsdata.year;
					validgpsdata.month = gpsdata.month;
					validgpsdata.day = gpsdata.day;
					validgpsdata.hour = gpsdata.hour;
					validgpsdata.minute = gpsdata.minute;
					validgpsdata.second = gpsdata.second;
					validgpsdata.latitude = gpsdata.latitude;
					validgpsdata.longitude = gpsdata.longitude;
					validgpsdata.speed = gpsdata.speed;
					validgpsdata.direction = gpsdata.direction;
					gps_log();
				}
			} else if (r == 0) {
				clear_gps_data();
			}

		} else {
			if (gps_port_handle > 0) {
				close(gps_port_handle);
				gps_port_handle = -1;
			}
			//gps_close();
			gps_logclose();
			usleep(10000);
		}
	}

	appfinish();

	return 0;
}
