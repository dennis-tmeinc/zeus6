/* 
 *  GPS support
 *       log gps data and sensor data to smartlog files
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

#include "../cfg.h"

#include "../dvrsvr/config.h"
#include "../dvrsvr/genclass.h"
#include "../ioprocess/diomap.h"
#include "../ioprocess/serial.h"

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
} gpsdata ;

char gpslogfilename[512];
string dvrcurdisk;
int glog_ok = 0; // logging success.

int gps_disable = 0;
int gps_handle = -1;
char gps_port[256] = "/dev/ttyS2";
int gps_baudrate = 4800; // gps default baud

int gforce_log_enable = 0;

int build_year = 2019 ;

int inputdebounce = 3;
FILE* gps_logfile;

#define MAXSENSORNUM (32)

string sensorname[MAXSENSORNUM];
int sensor_invert[MAXSENSORNUM];
int sensor_value[MAXSENSORNUM];
double sensorbouncetime[MAXSENSORNUM];
double sensorbouncevalue[MAXSENSORNUM];

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = CFG_FILE;
const char* pidfile = "/var/dvr/glog.pid";

int app_state; // 0: quit, 1: running: 2: restart: 3: idling

char disksroot[128] = "/var/dvr/disks";

#define GPSBUFLEN 120
char gpsBuf[GPSBUFLEN+4];
int gpsBufPos = 0;

void clear_gps_data()
{
	gpsdata.valid = 0;
	p_dio_mmap->gps_connection = 0;
	p_dio_mmap->gps_valid = 0;
}

void sig_handler(int signum)
{
	if( signum == SIGTERM || signum == SIGINT ) {
		app_state = 0 ;
	}
	else if (signum == SIGUSR1) {
		app_state = 3 ;
	} 
	else if (signum == SIGUSR2) {
		app_state = 2 ;
	}
	else if (signum == SIGCONT) {
		if( app_state == 3 ) 
			app_state = 2 ;
	}
}

// return 1 for success and gpsdata filled
int gps_readdata()
{
	int bytes;
	struct timeval tv;
	unsigned char c;

	if( read(gps_handle, &c, 1) <= 0 ) {
		return 0;
	}

	if( c=='\n') {
		gpsBuf[gpsBufPos] = 0;
		gpsBufPos=0;

		printf("GPS:%s\n",gpsBuf );

		// a new line, parse gps sentence.
		int deg ;
		float min ;
		// NMEA RMC message, example: $GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,181019,020.3,E*68
		if(gpsBuf[0] == '$' && gpsBuf[3] == 'R' && gpsBuf[4] == 'M' && gpsBuf[5] == 'C'  ) {
			// got $GPRMC
			memset( &gpsdata, 0, sizeof(gpsdata) );

			// read time			
			char * b = strchr(gpsBuf, ',') ;
			if( b != NULL) {
				sscanf( b+1 ,
					" %2d%2d%f", &gpsdata.hour,	&gpsdata.minute, &gpsdata.second);
			}
			else {
				return 0;
			}

			// read fix
			gpsdata.valid = 0 ;
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				if( sscanf( b+1 , " %c", &c ) > 0 ) {
					gpsdata.valid = (c=='A') ;					
				}
			}
			else {
				return 0;
			}

			// read latitude 
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , " %2d%f", &deg, &min );
				gpsdata.latitude = (float)deg + min / 60.0f ;
			}
			else {
				return 0;
			}
			// read latitude direction
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , " %c", &c );
				if (c != 'N') {
					gpsdata.latitude = -gpsdata.latitude;
				}
			}
			else {
				return 0;
			}

			// read longitude
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , " %3d%f", &deg, &min );
				gpsdata.longitude = (float)deg + min / 60.0f ; 
			}
			else {
				return 0;
			}
			// read longitude direction
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , " %c", &c );
				if (c != 'E') {
					gpsdata.longitude = -gpsdata.longitude;
				}
			}
			else {
				return 0;
			}

			// read speed
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , "%f", &gpsdata.speed );
			}
			else {
				return 0;
			}

			// read direction
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , "%f", &gpsdata.direction );
			}
			else {
				return 0;
			}

			// read date
			b = strchr( b+1, ',') ;
			if( b != NULL) {
				sscanf( b+1 , " %2d%2d%d", &gpsdata.day, &gpsdata.month, &gpsdata.year);
				// fixing year
				gpsdata.year += 2000;				
			}
			else {
				return 0;
			}

			if( gpsdata.year < build_year || gpsdata.year > build_year+50 ) {
				gpsdata.valid = 0 ;		// invalid data
				return 0 ;
			}

			return 1 ;
		}
	}
	else if( c=='$') {
		gpsBuf[0] = c;
		gpsBufPos=1;
	}
	else if( gpsBufPos<GPSBUFLEN ) {
		gpsBuf[gpsBufPos++] = c ;
	}
	return 0;
}

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
	static int gps_logday;
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

	// no log before buildyear
	if (t.tm_year + 1900 < build_year || t.tm_year + 1900 > build_year + 50 ) {
		return 0;
	}

	logfile = gps_logopen(&t);
	if (logfile == NULL) {
		glog_ok = 0;
		return 0;
	}

	fprintf(logfile, "15,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f,%04x",
		(int)t.tm_hour,
		(int)t.tm_min,
		(int)t.tm_sec,
		gpsdata.latitude < 0.0000000 ? -gpsdata.latitude : gpsdata.latitude,
		gpsdata.latitude < 0.0000000 ? 'S' : 'N',
		gpsdata.longitude < 0.0000000 ? -gpsdata.longitude : gpsdata.longitude,
		gpsdata.longitude < 0.0000000 ? 'W' : 'E',
		(float)(gpsdata.speed * 1.852), // in km/h.
		(float)gpsdata.direction, logid);

	if( fmt!=NULL && *fmt != 0 ) {
		va_start(ap, fmt);
		vfprintf(logfile, fmt, ap);
		va_end(ap);
	}
	fprintf(logfile, "\n");

	if (gpsdata.speed < 1.0) {
		gps_logclose();
	}

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

	// no log before buildyear
	if (t.tm_year + 1900 < build_year || t.tm_year + 1900 > build_year + 50 ) {
		return 0;
	}

	logfile = gps_logopen(&t);
	if (logfile == NULL) {
		glog_ok = 0;
		return 0;
	}

	fprintf(logfile, "%02d,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f",
		logid,
		(int)t.tm_hour,
		(int)t.tm_min,
		(int)t.tm_sec,
		gpsdata.latitude < 0.0000000 ? -gpsdata.latitude : gpsdata.latitude,
		gpsdata.latitude < 0.0000000 ? 'S' : 'N',
		gpsdata.longitude < 0.0000000 ? -gpsdata.longitude : gpsdata.longitude,
		gpsdata.longitude < 0.0000000 ? 'W' : 'E',
		(float)(gpsdata.speed * 1.852), // in km/h.
		(float)gpsdata.direction);

	if( fmt!=NULL && *fmt != 0 ) {
		va_start(ap, fmt);
		vfprintf(logfile, fmt, ap);
		va_end(ap);
	}
	fprintf(logfile, "\n");

	if (gpsdata.speed < 1.0) {
		gps_logclose();
	}

	glog_ok = 1;
	return 1;
}

// log gps position
int gps_log()
{
	static float logtime = 0.0;
	static int stop = 0;

	float ti;
	float tdiff;
	int log = 0;

	ti = 3600.0 * gpsdata.hour + 60.0 * gpsdata.minute + gpsdata.second;

	//    dist+=gpsdata->speed * (1.852/3.6) * (ti-disttime) ;
	//    disttime=ti ;
	tdiff = ti - logtime;
	if (tdiff < 0.0)
		tdiff = 1000.0;

	if (gpsdata.speed < 2.0) { //original is 0.2 from harrison
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

	if (p_dio_mmap->pwii_VRI[0] && strcmp(st_pwii_VRI, p_dio_mmap->pwii_VRI) != 0) {
		strncpy(st_pwii_VRI, p_dio_mmap->pwii_VRI, sizeof(st_pwii_VRI) - 1);
		log_vri = 1;
	}

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

	if (gpsdata.year < build_year )
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
	string tstr;
	config dvrconfig(dvrconfigfile);

	if(p_dio_mmap==NULL) {
		// init io map
		if (dio_mmap() == NULL) {
			//printf( "GLOG:IO memory map failed!");
			return 0;
		}

		if (p_dio_mmap->glogpid > 0) {
			if( kill(p_dio_mmap->glogpid, SIGTERM) == 0 ) {
				for (i = 0; i < 100; i++) {
					if (p_dio_mmap->glogpid <= 0)
						break;
					usleep(100000);
				}
			}
		}

		// init glog
		p_dio_mmap->glogpid = getpid();
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
	tstr = dvrconfig.getvalue("glog", "inputdebounce");
	if (tstr.length() > 0) {
		sscanf(tstr.getstring(), "%d", &inputdebounce);
	}

	// gforce logging enabled ?
	gforce_log_enable = dvrconfig.getvalueint("glog", "gforce_log_enable");

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

	// get GPS port setting
	gps_disable = dvrconfig.getvalueint("glog", "gpsdisable");
	serialport = dvrconfig.getvalue("glog", "serialport");
	if (serialport.length() > 0) {
		strcpy(gps_port, serialport.getstring());
	}
	i = dvrconfig.getvalueint("glog", "serialbaudrate");
	if (i >= 1200) {
		gps_baudrate = i;
	}

	p_dio_mmap->gps_connection = 0;
	p_dio_mmap->gps_valid = 0;
	
	if(gps_handle>0) {
		close(gps_handle);
	}
	gps_handle = serial_open(gps_port, gps_baudrate);
	if( gps_handle<=0 ) {
		printf("Can't open serial port: %s\n", gps_port );
		return 0;
	}
	// set build year
	sscanf( __DATE__ + 7 , "%d", &build_year );
	return 1;
}

// app finish, clean up
void appfinish()
{
	// close log file
	gps_logclose();

	// clean up shared memory
	clear_gps_data();

	// close serial port
	if (gps_handle > 0) {
		close(gps_handle);
		gps_handle = -1;
	}

	p_dio_mmap->glogpid = 0;
	dio_munmap();

	// delete pid file
	unlink(pidfile);
}

int main(int argc, char** argv)
{
	time_t idletime, t1 ;

	t1 = time(&idletime);

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

	while (app_state) {
		if( app_state == 3 ) {
			clear_gps_data();
			gps_logclose();
			if(gps_handle>0) {
				close(gps_handle);
				gps_handle = -1 ;
			}
			sleep(1);
			continue ;
		}
		else if( app_state == 2 ) {
			// to restart ;
			app_state = 1 ;
			appinit();
		}

		// do sensor log here instead in seperate thread ?
		sensor_log();

		if( gps_handle <= 0 ) {
			usleep(100000);
			continue ;
		}

		if ( !gps_disable ) {
			if( serial_dataready(gps_handle, 1000000) ) {
				if( gps_readdata() ) {		// received $GPRMC

					p_dio_mmap->gps_connection = 1 ;
					if( gpsdata.valid ) {

						struct tm ttm;
						ttm.tm_sec = gpsdata.second;
						ttm.tm_min = gpsdata.minute;
						ttm.tm_hour = gpsdata.hour;
						ttm.tm_mday = gpsdata.day;
						ttm.tm_mon = gpsdata.month - 1;
						ttm.tm_year = gpsdata.year - 1900;
						ttm.tm_wday = 0;
						ttm.tm_yday = 0;
						ttm.tm_isdst = -1;
						time_t tgm = timegm(&ttm);

						// share data
						dio_lock();
						p_dio_mmap->gps_gpstime = tgm ;
						p_dio_mmap->gps_speed = gpsdata.speed;
						p_dio_mmap->gps_direction = gpsdata.direction;
						p_dio_mmap->gps_latitude = gpsdata.latitude;
						p_dio_mmap->gps_longitud = gpsdata.longitude;
						p_dio_mmap->gps_valid = gpsdata.valid;
						dio_unlock();

						// log gps position
						gps_log();
					}
					else {
						p_dio_mmap->gps_valid = gpsdata.valid;
					}

					t1 = time(&idletime);
					continue ;
				}
			}
			t1 = time(NULL) ;
			if( (t1-idletime) > 10 ) {		// no GPRMC received!
				clear_gps_data();
				idletime = t1 ;
			}
		}
		else {
			usleep(100000);
		}
	}

	appfinish();
	printf("glog quit!");

	return 0;
}
