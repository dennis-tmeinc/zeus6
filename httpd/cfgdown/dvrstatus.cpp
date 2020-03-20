
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "cfg.h"
#include "dvrsvr/dvr.h"
#include "dvrsvr/dir.h"
#include "ioprocess/diomap.h"
#include "json/json.h"

struct channelstate {
    int sig ;
    int rec ;
    int mot ;
} ;

// get channel status
//    return value: totol channels of status returned.
int getchannelstate(struct channelstate * chst, unsigned long * streambytes, int ch)
{
    struct dvr_req req ;
    struct dvr_ans ans ;
    int sockfd ;
    int i;
    FILE * portfile ;
    int port ;
    int rch = 0;
    portfile = fopen("dvrsvr_port", "r");
    if( portfile==NULL ) {
        return rch;
    }

    port=15114 ;
    fscanf(portfile, "%d", &port);
    sockfd = net_connect( (const char *)"127.0.0.1", port);

    if( sockfd>0 ) {
        req.reqcode=REQGETCHANNELSTATE ;
        req.data=0 ;
        req.reqsize=0 ;
        if( net_send(sockfd, &req, sizeof(req)) ) {
            if( net_recv(sockfd, &ans, sizeof(ans))) {
                if( ans.anscode==ANSGETCHANNELSTATE && ans.data>0 ) {
                    rch = ans.data ;
                    if( rch>ch ) {
                        rch=ch ;
                    }
                    net_recv( sockfd, (void *)chst, rch*sizeof( struct channelstate ) );
                }
            }
        }

        // get stream bytes
        for( i=0; i<rch; i++ ) {
            streambytes[i]=0;
            req.reqcode=REQ2GETSTREAMBYTES ;
            req.data=i ;
            req.reqsize=0 ;
            if( net_send(sockfd, &req, sizeof(req)) ) {
                if( net_recv(sockfd, &ans, sizeof(ans))) {
                    if( ans.anscode==ANS2STREAMBYTES ) {
                        streambytes[i]=ans.data ;
                    }
                }
            }
        }
        close( sockfd );
    }

    return rch ;
}

int memory_usage( int * mem_total, int * mem_free)
{
    char buf[256];
    * mem_total = 0 ;
    * mem_free = 0 ;
    FILE * fproc = fopen("/proc/meminfo", "r");
    if (fproc) {
        while (fgets(buf, 256, fproc)) {
            char header[20] ;
            int  v ;
            if( sscanf( buf, "%19s%d", header, &v )==2 ) {
                if( strcmp( header, "MemTotal:")==0 ) {
                     *mem_total=v ;
                }
                else if( strcmp( header, "MemFree:")==0 ) {
                    * mem_free+=v ;
                }
                else if( strcmp( header, "Inactive:")==0 ) {
                    * mem_free+=v ;
                }
            }
        }
        fclose(fproc);
    }
    return 1;
}

int disk_usage(int* disk_total, int* disk_free)
{
	*disk_free = 0;
	*disk_total = 0;
	dir disks(VAR_DIR "/disks");
	while (disks.find()) {
		if (disks.isdir()) {
			struct statfs stfs;
			if (statfs(disks.pathname(), &stfs) == 0) {
				*disk_free += stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
				*disk_total += stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
			}
		}
	}
	return 1;
}

int get_temperature( int * sys_temp, int * disk_temp)
{
    if( p_dio_mmap ) {
        dio_lock();
        *sys_temp = p_dio_mmap->iotemperature ;
        *disk_temp = p_dio_mmap->hdtemperature1 ;
        dio_unlock();
        return 1 ;
    }
    return 0 ;
}

struct dvrstat {
    struct timeval checktime ;
    float uptime, idletime ;
    unsigned long streambytes[16] ;
} savedstat ;

// generate status page
void print_status()
{
    // generate camera status.

    FILE * statfile ;
    int i;
    int  chno=0 ;
    struct channelstate cs[16] ;
    struct dvrstat stat ;
    char strbuf[4096] ;

    memset( &stat, 0, sizeof( stat ) );
    memset( &savedstat, 0, sizeof( savedstat ) );
    statfile = fopen( "savedstat", "rb");
    if( statfile ) {
        fread( &savedstat, sizeof(savedstat), 1, statfile );
        fclose( statfile );
    }

    // set timezone
    char tz[128] ;
    FILE * ftz = fopen("tz_env", "r");
    if( ftz ) {
        fscanf(ftz, "%s", tz);
        fclose(ftz);
        setenv("TZ", tz, 1);
    }

    json dvrstatus(JSON_Object) ;

    // dvr time
    double timeinc ;
    gettimeofday(&stat.checktime, NULL);

    time_t ttnow ;
    ttnow = (time_t) stat.checktime.tv_sec ;

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S "

    strftime( strbuf, sizeof(strbuf), RFC1123FMT, localtime( &ttnow ) );
    dvrstatus.addStringItem("dvrtime", strbuf);

    if( savedstat.checktime.tv_sec==0 ) {
        timeinc = 1.0 ;
    }
    else {
        timeinc = (double)(stat.checktime.tv_sec-savedstat.checktime.tv_sec) +
            (double)(stat.checktime.tv_usec - savedstat.checktime.tv_usec)/1000000.0 ;
    }

    dvrstatus.addNumberItem("cpu_checktime", (double)(stat.checktime.tv_usec)/1000000.0 + stat.checktime.tv_sec );

    // get status from dvrsvr
    memset( cs, 0, sizeof(cs) );
    chno=getchannelstate(cs, stat.streambytes, 16);
    for( i=1; i<=chno ; i++ )
    {
        if( cs[i-1].sig ) {
            sprintf(strbuf,"camera_%d_signal_lost", i);
            dvrstatus.addStringItem(strbuf, "on");
        }
        if( cs[i-1].rec ) {
            sprintf(strbuf,"camera_%d_recording", i);
            dvrstatus.addStringItem(strbuf, "on");
        }
        if( cs[i-1].mot ) {
            sprintf(strbuf,"camera_%d_motion", i);
            dvrstatus.addStringItem(strbuf, "on");
        }

        sprintf(strbuf,"camera_%d_streambytes", i);
        dvrstatus.addNumberItem(strbuf, stat.streambytes[i-1]);

        double bitrate = (double)(stat.streambytes[i-1] - savedstat.streambytes[i-1]) / (timeinc*125.0) ;
        if( bitrate > 10000.0 ) {
            bitrate = 0.0 ;
        }

        sprintf(strbuf,"camera_%d_bitrate", i);
        dvrstatus.addNumberItem(strbuf, (int)bitrate);
    }

    // calculate CPU usage
    FILE * uptimefile = fopen("/proc/uptime", "r" );
    if( uptimefile ) {
        if( fscanf( uptimefile, "%f %f", &stat.uptime, &stat.idletime )>0 ) {
            dvrstatus.addNumberItem("cpu_uptime", stat.uptime);
            dvrstatus.addNumberItem("cpu_idletime", stat.idletime);
        } 
        fclose( uptimefile );
    }

    float cpuusage = stat.uptime - savedstat.uptime ;
    if( cpuusage < 0.1 ) {
        cpuusage = 0.0 ;
    }
    else {
        cpuusage = 100.0 * (cpuusage - (stat.idletime-savedstat.idletime)) / cpuusage ;
    }

    dvrstatus.addNumberItem("cpu_usage", cpuusage);

    // print memory usage
    int stfree, sttotal ;
    if( memory_usage(&sttotal, &stfree) ) {
        dvrstatus.addNumberItem("memory_total", sttotal/1000);
        dvrstatus.addNumberItem("memory_free", stfree/1000);
    }

    // print disk usage
    if( disk_usage(&sttotal, &stfree) ) {
        dvrstatus.addNumberItem("disk_total", sttotal);
        dvrstatus.addNumberItem("disk_free", stfree);
    }
    else {
        dvrstatus.addNumberItem("disk_total", 0);
        dvrstatus.addNumberItem("disk_free", 0);
    }

    // information from dio_map
    dio_mmap();
    
    // print system temperature
    int systemperature=0, hdtemperature=0 ;
    get_temperature(&systemperature, &hdtemperature) ;
    dvrstatus.addNumberItem("temperature_system_c", systemperature);
    dvrstatus.addNumberItem("temperature_disk_c", hdtemperature);
	
	// battery status & voltage
    dvrstatus.addNumberItem("battery_state", p_dio_mmap->battery_state);
    dvrstatus.addNumberItem("battery_voltage", p_dio_mmap->battery_voltage);
	
	// GPS coordinates
    dvrstatus.addNumberItem("gps_valid", p_dio_mmap->gps_valid);
    dvrstatus.addNumberItem("gps_latitude", p_dio_mmap->gps_latitude);
    dvrstatus.addNumberItem("gps_longitude", p_dio_mmap->gps_longitud);
	
	// pre-recording status
	if( chno > 16 ) chno=16 ;
    for( i=1; i<=chno ; i++ )
    {
		int st = p_dio_mmap->camera_status[i-1] ;
        sprintf(strbuf,"camera_%d_recordstate", i);
		//         2: recording
		//         3: force-recording
		//         4: lock recording
		//         5: pre-recording
		//         6: in-memory pre-recording
		if( st & 16 ) {
            dvrstatus.addStringItem(strbuf, "Locked recording");
		}
		else if( st & 4 ) {
            dvrstatus.addStringItem(strbuf, "Recording");
		}
		else if( st & (1<<5) ) {
			if( st & (1<<6) ) {
                dvrstatus.addStringItem(strbuf, "In-RAM Pre-Recording");
			}
			else {
                dvrstatus.addStringItem(strbuf, "Pre-Recording");
			}
		}
		else {
            dvrstatus.addStringItem(strbuf, "No recording");
		}
	}

    // bodycamera status
    chno = p_dio_mmap->bodycam_num ;
    dvrstatus.addNumberItem("bodycam_num", chno);
    for( i=0; i<chno; i++) {
        int st = p_dio_mmap->bodycam_status[i] ;
        sprintf(strbuf,"bodycam%d_connect", i+1);
        if(st & 1) {
            dvrstatus.addStringItem(strbuf, "on");
        }
        sprintf(strbuf,"bodycam%d_rec", i+1);
        if(st & 2) {
            dvrstatus.addStringItem(strbuf, "on");
        }
    }
	
    dio_munmap();

    dvrstatus.addStringItem("objname", "status_value");

    statfile = fopen( "savedstat", "wb");
    if( statfile ) {
        fwrite( &stat, sizeof(stat), 1, statfile );
        fclose( statfile );
    }

    // output dvrstatus
    dvrstatus.encode(strbuf,4096);
    printf("%s", strbuf);
}

//  function from getquery
int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

void check_synctime()
{
    char * synctime = getquery("synctime");
    if( synctime ) {
            long long int stime = 0 ;
            sscanf(synctime, "%Ld", &stime );
            if( stime>0 ) {                     // milliseconds since 1970/01/01
                struct timeval tv ;
                tv.tv_sec = stime/1000 ;
                tv.tv_usec = (stime%1000)*1000 ;
                settimeofday( &tv, NULL );

                // kill -USR2 dvrsvr.pid
                FILE * fvalue = fopen( VAR_DIR "/dvrsvr.pid", "r" );
                if( fvalue ) {
                    int i=0 ;
                    fscanf(fvalue, "%d", &i) ;
                    fclose( fvalue );
                    if( i> 0 ) {
                        kill( (pid_t)i, SIGUSR2 );      // re-initial dvrsvr
                    }
                }

                system( APP_DIR "/dvrtime utctomcu > /dev/null" );
                system( APP_DIR "/dvrtime utctortc > /dev/null" );
            }
    }
    return ;
}

// return 0: for keep alive
int main()
{
    check_synctime();

    // printf headers
    printf( "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" );
    
    print_status();

    return 1;
}
