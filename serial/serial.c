#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <termios.h>

#define CONSOLE_INPUT (0)
#define CONSOLE_OUTPUT (1)

#define BUFFERSIZE (4096)
char buffer[BUFFERSIZE];

int main(int argc, char* argv[])
{
	char* serial_dev = "/dev/ttyS0";
	int serial_handle;
	int baud = 115200;
	int n;
	unsigned long long t1 = 0, t2 = 0;
	struct termios tios, oldtios;
	struct pollfd pf[2] ;

	if (argc > 1) {
		serial_dev = argv[1];
	}
	if (argc > 2) {
		baud = atoi(argv[2]);
	}

	// open serial port
	serial_handle = open(serial_dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if (serial_handle <= 0) {
		printf("Can't open %s\n\nUsage:\n\tSerial <device> <baudrate>\n", serial_dev);
		return 1;
	}

	// setup serial port
	fcntl(serial_handle, F_SETFL, 0); // don't know what is this, copied from some example codes from internet
	memset(&tios, 0, sizeof(tios));
	tcgetattr(serial_handle, &tios);
	// set serial port attributes
	tios.c_cflag = CS8 | CLOCAL | CREAD;
	tios.c_iflag = IGNPAR;
	tios.c_oflag = 0;
	tios.c_lflag = 0;	 // no ICANON;
	tios.c_cc[VMIN] = 0;  // minimum char 0
	tios.c_cc[VTIME] = 0; // 0.0 sec time out

	if (cfsetspeed(&tios, (speed_t)baud) != 0) {
		printf("Baud rate error!\n");
		close(serial_handle);
		return 1;
	}
	if (tcsetattr(serial_handle, TCSANOW, &tios) != 0) {
		printf("Serial port error!\n");
		return 1;
	}

	tcflush(serial_handle, TCIOFLUSH);

	// Make console no echo, no buffer, no control input
	if (isatty(CONSOLE_INPUT)) {
		tcgetattr(CONSOLE_INPUT, &oldtios);
		tios = oldtios;
		tios.c_lflag &= ~(ICANON | ECHO | ISIG);
		tcsetattr(CONSOLE_INPUT, TCSANOW, &tios);
	}
	else {
		printf("Warning: not running on a TTY!\n");
	}

	printf("<<< Port opended : %s@%d >>>\n<<< Press Ctrl-C or Ctrl-X twice quickly to quit! >>>\n",
		serial_dev, baud);
	fflush(stdout);

	pf[0].fd = serial_handle ;
	pf[0].events = POLLIN ;
	pf[1].fd = CONSOLE_INPUT ;
	pf[1].events = POLLIN ;

	while( poll( pf, 2, 100000 )>=0 ) {
		if ( pf[0].revents & POLLIN ) {
			n = read(serial_handle, buffer, BUFFERSIZE);
			if (n > 0) {
				write(CONSOLE_OUTPUT, buffer, n);
			}
			else {
				break;
			}
		}
		if ( pf[1].revents & POLLIN ) {
			n = read(CONSOLE_INPUT, buffer, BUFFERSIZE);
			if (n > 0) {
				// check repeated ctrl-c or ctrl-x?
				if (n == 1 && (buffer[0] == 3 || buffer[0] == 0x18)) { // single key input ctrl-c or ctrl-x
					struct timeval tv;
					gettimeofday(&tv, NULL);
					t2 = (unsigned long long)tv.tv_sec * 1000000 + tv.tv_usec;
					if (t2 > t1 && (t2 - t1) < 300000) { // repeat Ctrl-C to quit application
						break;
					}
					t1 = t2;
				}
				n = write(serial_handle, buffer, n);
			}
			else {
				break;
			}
		}
	}

	close(serial_handle);
	printf("\n<<< Serail Port Closed! >>>\n");

	// restore stdin
	if (isatty(CONSOLE_INPUT)) {
		tcsetattr(CONSOLE_INPUT, TCSANOW, &oldtios);
	}

	return 0;
}
