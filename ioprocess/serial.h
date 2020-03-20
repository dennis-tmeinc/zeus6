/*
 file serial.cpp,
    serial port functions
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#define SERIAL_DELAY	(1000000) //(100000)
#define DEFSERIALBAUD	(115200)

// open serial port
int serial_open(char * device, int buadrate);
int serial_dataready(int handle, int usdelay);
void serial_setspeed( int hserial, int buadrate);

#define serial_read read
#define serial_write write
#define serial_close close

#endif // __SERIAL_H__
