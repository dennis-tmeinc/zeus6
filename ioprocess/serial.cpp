/*
 file serial.cpp,
    serial port functions
 */

#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../cfg.h"

#ifdef EAGLE32
#include "../eaglesvr/eagle34/davinci_sdk.h"
#endif

const struct baud_table_t {
    speed_t baudv ;
    int baudrate ;
} baud_table[] = {
    { B1200, 	1200},
    { B2400, 	2400},
    { B4800,	4800},
    { B9600,	9600},
    { B19200,	19200},
    { B38400,	38400},
    { B57600,	57600},
    { B115200,	115200},
    { B230400,	230400},
    { 0, 0 }
} ;

// open serial port
void serial_setspeed( int hserial, int buadrate)
{
    int i;
	struct termios tios;
	speed_t baud_t;
	tcgetattr(hserial, &tios);
	// set serial port attributes
	tios.c_cflag = CS8 | CLOCAL | CREAD;
	tios.c_iflag = IGNPAR;
	tios.c_oflag = 0;
	tios.c_lflag = 0;	 // ICANON;
	tios.c_cc[VMIN] = 1;  // minimum char
	tios.c_cc[VTIME] = 0; // 0.1 sec time out

	baud_t = buadrate;
	for (i = 0; i < (sizeof(baud_table) / sizeof(baud_table[0])); i++) {
		if (buadrate == baud_table[i].baudrate) {
			baud_t = baud_table[i].baudv;
			break;
		}
	}
    cfsetspeed(&tios, baud_t);
    tcsetattr(hserial, TCSANOW, &tios);
    tcflush(hserial, TCIFLUSH);
}

// open serial port
int serial_open(char * device, int buadrate)
{
    int hserial ;
    int i;

    printf( "Open serial port :%s :%d\n",     (char *)device, buadrate );

    // check if serial device match stdin ?
    struct stat stdinstat ;
    struct stat devstat ;
    int r1, r2 ;
    r1 = fstat( 0, &stdinstat ) ;           // get stdin stat
    r2 = stat( device, &devstat ) ;

    if( r1==0 && r2==0 && stdinstat.st_dev == devstat.st_dev && stdinstat.st_ino == devstat.st_ino ) { // matched stdin
        hserial = dup(0);                   // duplicate stdin
        fcntl(hserial, F_SETFL, O_RDWR | O_NOCTTY );
    }
    else {
       hserial = open( device, O_RDWR | O_NOCTTY );

       if( hserial > 0 ) {
#ifdef EAGLE32
            if( strcmp( device, "/dev/ttyS1")==0 ) {    // this is hikvision specific serial port
                // Use Hikvision API to setup serial port (RS485)
                InitRS485(hserial, buadrate, DATAB8, STOPB1, NOPARITY, NOCTRL);
            }
            else
#endif
            {
                serial_setspeed(hserial,buadrate);
            }
            return hserial ;
        }
    }

    return 0 ;
}

//
int serial_dataready(int handle, int usdelay)
{
    struct pollfd fds ;
    fds.fd = handle ;
    fds.events = POLLIN ;
    return poll( &fds, 1, usdelay/1000)>0;
}
