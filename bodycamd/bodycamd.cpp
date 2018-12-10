#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/un.h>

#include "net/net.h"

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

#include "../ioprocess/diomap.h"

#include "bodycam.h"

// dio mememoy
struct dio_mmap * p_dio_mmap ;

int     g_runtime ;
int     s_maxwaitms  ;
int g_run ;
int bodycamNum ;

// return runtime in milli seconds
int runtime()
{
    struct timespec ts ;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec*1000 + ts.tv_nsec/1000000) ;
}

void setMaxWait(int ms)
{
    if( s_maxwaitms>ms ) s_maxwaitms=ms ;
}

// get time string as AMBR spec ( 3.2.15 CAMERA_CLOCK), ex "2017-12-24 23:59:59"
// timebuf must be at lease 20 bytes
time_t getTime( char * timebuf )
{
    time_t t ;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t) current_time.tv_sec ;
    if( timebuf!=NULL ) {
        struct tm stm ;
        localtime_r(&t, &stm);
        sprintf( timebuf, "%04d-%02d-%02d %02d:%02d:%02d",
            stm.tm_year + 1900,
            stm.tm_mon + 1,
            stm.tm_mday,
            stm.tm_hour,
            stm.tm_min,
            stm.tm_sec );
    }
    return t ;
}

// local socket to wait for local trigger (send from ioprocess)
static int local_socket = -1;
static struct pollfd * local_sfd = NULL ;

int local_setpoll( struct pollfd * pfd, int max )
{
    local_sfd = NULL ;
    if( local_socket > 0 ) {
        pfd->events = POLLIN ;
        pfd->fd = local_socket ;
        pfd->revents = 0 ;
        local_sfd = pfd ;
        return 1 ;
    }
    return 0 ;
}

int local_process()
{
    if( local_sfd!=NULL && local_sfd->fd == local_socket && (local_sfd->revents & POLLIN) ) {
        // just read it and dump it
        char dummy[512] ;
        net_recv(local_socket, (void *)dummy, sizeof(dummy));
    }
}

void local_init()
{
    // init local listening socket
    local_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if( local_socket>0 ) {
        net_bind(local_socket, BODYCAM_PORT);
    }
}

#define DVRKEYINPUT 0x673a4427

void local_sendVKRec()
{
    if( local_socket>0 ) {
        unsigned int msgbuf[2] ;
        struct sockad sad ;
        net_addr(&sad, "127.0.0.1", 15114);
        msgbuf[0] = DVRKEYINPUT ;
        msgbuf[1] = 0xff000000 | VK_RECON ;
        net_sendto( local_socket, (void *)msgbuf, sizeof(msgbuf), &sad );
    }
}

void local_finish()
{
    if( local_socket>0 )
        close(local_socket);
    local_socket = 0;
}

void bodycam_run()
{
    int i;

    struct pollfd * pfd ;
    int nfd, pfdsize ;

    local_init();

    pfdsize = bodycamNum + 20 ;
    pfd = new pollfd [pfdsize] ;        // 100 ?

    bodycam * bdarray[bodycamNum+1] ;
    // setup all bodycamera
    for( i=0; i<bodycamNum ; i++ ) {
        bdarray[i] = new bodycam(i) ;
    }

    s_maxwaitms = 1000 ;

    while( g_run == 1 ) {

        // add local listener
        nfd = local_setpoll( pfd, pfdsize );

        for( i=0; i<bodycamNum; i++ ) {
            nfd += bdarray[i]->setpoll( pfd+nfd, pfdsize-nfd );
        }

        if( nfd>0 ) {
            i = poll( pfd, nfd, s_maxwaitms );
        }
        else {
            usleep( s_maxwaitms*1000 );
            i = 0;
        }
        s_maxwaitms =  60000 ;
        g_runtime = runtime() ;

        local_process();

        for( i=0; i<bodycamNum; i++ ) {
            bdarray[i]->process();
        }
    }

    for( i=0; i<bodycamNum ; i++ ) {
        delete bdarray[i] ;
    }

    delete [] pfd ;

    local_finish();
}

static void s_handler(int signum)
{
    if( signum == SIGUSR2 ) {
        // restart
        g_run = 2 ;
    }
    else if(signum == SIGHUP ) {    // do nothing
        g_run = 1 ;
    }
    else {
        // quit
        g_run = 0 ;
    }
}

void bodycam_init()
{
    config dvrconfig(CFG_FILE);

    // get number of body camera linked to this DVR
    bodycamNum = dvrconfig.getvalueint("system", "totalbodycam") ;
    if( bodycamNum < 0 || bodycamNum>MAX_BODYCAM ) {
        bodycamNum = 0 ;
    }

    // init timezone
    char * p ;
    string tz ;

    tz=dvrconfig.getvalue( "system", "timezone" );
    if( tz.length()>0 ) {
        string tzi=dvrconfig.getvalue( "timezones", tz.getstring() );
        if( tzi.length()>0 ) {
            p=strchr(tzi.getstring(), ' ' ) ;
            if( p ) {
                *p=0;
            }
            p=strchr(tzi.getstring(), '\t' ) ;
            if( p ) {
                *p=0;
            }
            setenv("TZ", tzi.getstring(), 1);
        }
        else {
            setenv("TZ", tz.getstring(), 1);
        }
    }

    // show start time
    getTime( tz.setbufsize(50) );
    printf("Init time: %s\n", (char *)tz);
}

void dio_init()
{
    int i;
    int fd ;
    void * p ;

    config dvrconfig(CFG_FILE);
    string iomapfile = dvrconfig.getvalue( "system", "iomapfile");

    p_dio_mmap=NULL ;
    for( i=0; i<10; i++ ) {             // retry 10 times
        fd = open(iomapfile.getstring(), O_RDWR );
        if( fd>0 ) {
            break ;
        }
        sleep(3);
    }
    if( fd<=0 ) {
        printf( "IO module not started!");
        return ;
    }

    p=mmap( NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    close( fd );                                // don't need fd to use memory map.
    if( p==(void *)-1 || p==NULL ) {
        printf( "IO memory map failed!");
        return ;
    }

    int pid = getpid();
    FILE * fpid = fopen(VAR_DIR"/bodycamd.pid", "w");
    if( fpid ) {
        fprintf( fpid, "%d", pid );
        fclose( fpid );
    }

    p_dio_mmap = (struct dio_mmap *)p ;
    // wait for ioprocess to come up
    for( i=0; i<10; i++ ) {
        if( p_dio_mmap->iopid ) {
            p_dio_mmap->bodycam_pid=pid ;
            return ;        // success
        }
        sleep(3);           // wait for 30 seconds
    }
}


void dio_finish()
{
    if( p_dio_mmap ) {
        munmap( p_dio_mmap, sizeof( struct dio_mmap ) );
        p_dio_mmap=NULL ;
    }
    unlink( VAR_DIR"/bodycamd.pid" );
}

int main()
{
    dio_init();

    // setup signal handler
    signal(SIGINT, s_handler);
    signal(SIGTERM, s_handler);
    signal(SIGHUP, s_handler);
    signal(SIGUSR2, s_handler);
    signal(SIGPIPE, SIG_IGN );

    g_run = 1 ;
    while( g_run ) {
        g_run = 1 ;
        bodycam_init();
        bodycam_run();
    }

    dio_finish();
    return 0 ;
}
