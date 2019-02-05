#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

#include "net/net.h"

#include "rconn.h"
#include "adbclient.h"

#define ADB_PORT  5037
#define ADB_PORT_S "5037"

static int	adb_track_socket = 0;
static int  adb_device_num = 0 ;		// number of online device
static struct pollfd * adb_track_sfd ;

#define MAX_ADBCONN	(4)
static adb_conn * adbconn[MAX_ADBCONN] ;

// extra log
static void adb_log(char * msg) 
{
	FILE * flog = fopen("/tmp/rconn.log", "a");
	fprintf(flog, "ADB: %s\n", msg );
	fclose( flog );			
}

int usb_resetdev(int bus, int dev)
{
    char usbf[64];
    int fd;
    int rc;
    
    sprintf(usbf, "/dev/bus/usb/%03d/%03d", bus, dev );

    fd = open(usbf, O_WRONLY);
    if (fd < 0) {
        perror("Error opening output file");
        return 1;
    }

    rc = ioctl(fd, USBDEVFS_RESET, 0);
    if (rc < 0) {
        perror("Error in ioctl");
    }
    else {
		printf("Reset successful\n");
	}

    close(fd);
    return 0;
}

int usb_reset()
{
	// return usb_resetdev(1,1);
	return 0;
}

static int adb_local_connect()
{
	return net_connect("127.0.0.1", ADB_PORT);
}

static void adb_senddata(int fd, const char * data)
{
	int len ;
	char lpre[8] ;
	len = strlen( data ) ;
	if( len<1 || len>1000 ) {
		return ;
	}
	sprintf( lpre, "%04x", len ) ;
	
	net_sendall( fd, lpre, 4 ) ;
	net_sendall( fd, (void *)data, len );
}

// return -1 for error
static int adb_recvdata(int fd, char * data, int maxsize)
{
	int len ;
	char lpre[8] ;
	len = net_recvx( fd, lpre, 4 );
	if( len!=4 ) return -1 ;
	lpre[4] = 0 ;
	sscanf(lpre, "%x", &len);
	if( len<0 && len>=maxsize ) return -1 ;
	if( len>0 ) 
		return net_recvx( fd, data, len );
	else 
		return 0 ;
}

static int adb_status(int fd)
{
    unsigned char buf[4];
	if( net_rrdy( fd, 2000000 ) && net_recvx( fd, buf, 4 ) == 4 ) {
		return (memcmp(buf, "OKAY", 4)==0) ;
	}
	else {
		return 0 ;
	}
}

static int adb_service( int fd, const char * service, const char * device = NULL )
{
	if( device && strncmp( service, "host", 4 ) != 0) {
		// set transport
		char transport[128] ;
		sprintf( transport, "host:transport:%s", device );
		if( !adb_service( fd, transport, NULL ) ) {
			return 0 ;
		}
	}
	adb_senddata(fd, service) ;
	return adb_status(fd) ;
}

static void adb_startserver()
{
	printf("Start adb server.\n");
	system("/davinci/dvr/adb devices");
}

// scan/wait for android device
static int adb_scan_device()
{
	int i;
	int online = 0 ;
	if( net_rrdy( adb_track_socket ) ) {
	
		char data[2048] ;
		int l, n ;
		l = adb_recvdata( adb_track_socket, data, 2047 );
		if( l>=0 ) {
			data[l] = 0 ;		// null terminate
			printf( "Track-Data:%s\n", data );
			
			adb_conn * x_adbconn[MAX_ADBCONN] ;
			for( i=0;i<MAX_ADBCONN;i++) {
				x_adbconn[i]=adbconn[i];
				adbconn[i]=NULL;
			}

			online = 0;
			
			n=0;
			while( n<l && online<MAX_ADBCONN ) {
				int x = 0 ;
				char device_serial[100] ;
				char device[32] ;
				if( sscanf( data+n, "%99s %31s%n", device_serial, device, &x ) >= 2 ) {
					if( strcasecmp( device, "device" )==0 ) {
						// found online device
						printf("Found online device: %s\n", device_serial );
						
						int xdev=0;
						for( i=0; i<MAX_ADBCONN; i++ ) {
							if( x_adbconn[i] != NULL && strcmp( x_adbconn[i]->m_serialno,  device_serial )==0 ) {
								adbconn[online] = x_adbconn[i] ;
								x_adbconn[i] = NULL ;
								break;
							}
						}
						if( adbconn[online] == NULL ) {
							// create a new connection
							adbconn[online] = new adb_conn(device_serial);
						}
						
						online++;
					}
					n+=x ;
				}
				else {
					break ;
				}
			}
			
			// remove offline devices
			for( i=0;i<MAX_ADBCONN;i++) {
				if( x_adbconn[i]!=NULL ) {
					printf("Remove Offline dev: %s\n", x_adbconn[i]->m_serialno );
					delete x_adbconn[i];
				}
			}

		}
		else {
			close(adb_track_socket);
			adb_track_socket = -1 ;
		}
		
		printf("Total online devices: %d\n", online );

	}
	
	return online ;
}

static int adb_test( char * serialno )
{
	int s = adb_local_connect();
	if( s>0 ) {
		const char echostr[]="ghfcEOCO";
		char shellstr[80] ;
		sprintf(shellstr, "shell:echo %s", echostr);
		if( adb_service( s, shellstr, serialno ) ) {
			// give pwv service some time to start
			if( net_rrdy( s, 3000000) ) {
				if( net_recv( s, shellstr, 80 )>=4 ) {
					if( memcmp( shellstr, echostr, 8)==0 ) {
						close(s);
						return 1 ;
					}
				}
			}
		}
		close(s);
	}
	return 0 ;
}

static int adb_startPWservice( char * serialno )
{
	// use shell command to start pwv service
	int res = 0 ;
	int s ;
	s = adb_local_connect();
	if( s>0 ) {
		if( adb_service( s, "shell:svc power stayon usb ;am broadcast -n com.tme_inc.pwv/.PwBootReceiver;echo --eos", serialno ) ) {
			// give pwv service some time to start
			char rsp[256] ;

			while( net_rrdy( s, 10000000) ) {
				int l = net_recv( s, rsp, 255 ) ;
				if( l>0 ) {
					rsp[l] = 0;
					if( strstr(rsp,"--eos") ) {
						break;
					}
				}
				else {
					break;
				}
			}
			res = 1 ;
			printf( "Android device (%s) connected!\n", serialno);
		}
		else {
			printf("Device service failed!\n");
		}
		close( s ) ;
	}
							
	return res ;
}

int adb_conn::connect_server()
{
	int s = -1 ;
	if( pw_service ) {
		s = adb_local_connect();
		if( s>0 ) {
			// tcp connect to local
			char adbcmd[20] ;
			sprintf(adbcmd, "tcp:%d", g_port );
			if( adb_service( s, adbcmd, m_serialno ) ) {
				// connected
				printf("adbd server :%d connected\n", g_port);
				activetime = g_runtime ;
				return s ;				
			}
			else {
				// tcp connection error!
				printf("adbd pwv connection failed!\n");

				close( s ) ;
				
				if( adb_test(m_serialno)==0 ) {
					// adb echo test failed!
					usb_reset();
				}
				else {
					// use shell command to start pwv service
					if( !adb_startPWservice( m_serialno ) ) {
						pw_service=0;
					}
				}
			}
		}
	}

	return -1 ;
}

adb_conn::adb_conn( char * serialno )
	:rconn()
{
	maxidle=10000;
	pw_service = 1 ;	// assume it is a pw device
	strcpy( m_serialno, serialno ) ;
}

int adb_setfd( struct pollfd * pfd, int max )
{
	int nfd = 0 ;
	adb_track_sfd=NULL;

	if( adb_track_socket<= 0 ) {
		
		// reset all adb connections
		adb_reset();
		setMaxWait(5000);

		adb_startserver();					// start adb server, just in case it's not up yet

		// create tracking socket
		adb_track_socket = adb_local_connect();
		if( adb_track_socket>0 ){
			if( !adb_service( adb_track_socket, "host:track-devices", NULL ) ) {
				close(adb_track_socket);
				adb_track_socket = -1 ;
			}
		}
		
		setMaxWait(5000);
	}
	
	if( max > nfd && adb_track_socket > 0 ) {
		pfd->events = POLLIN ;
		pfd->revents = 0 ;
		pfd->fd = adb_track_socket ;
		adb_track_sfd = pfd ;
		nfd ++ ;
	}
	
	for( int i=0; i<MAX_ADBCONN && max>nfd ; i++ ) {
		if( adbconn[i] ) {
			nfd += adbconn[i]->setfd( pfd+nfd, max-nfd ); 
		}
	}

	return nfd ;
}

void adb_process()
{
	for( int i=0; i<MAX_ADBCONN; i++ ) {
		if( adbconn[i] ) {
			adbconn[i]->process();
		}
	}
	if( adb_track_socket>0 && adb_track_sfd && adb_track_sfd->fd == adb_track_socket && adb_track_sfd->revents ) {
		// new created tracking socket
		adb_scan_device();
	}
}

void adb_reset()
{
	for( int i=0; i<MAX_ADBCONN; i++ ) {
		if( adbconn[i] ) {
			delete adbconn[i] ;
			adbconn[i]=NULL;
		}
	}
	
	if( adb_track_socket>0 ) {
		close(adb_track_socket);
		adb_track_socket = -1 ;
	}
}
