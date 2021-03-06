
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "../../cfg.h"
#include "../../ioprocess/diomap.h"
#include "../../dvrsvr/dvr.h"
#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/config.h"

char tzfile[] = "tz_option" ;

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
    port=15111 ;
    fscanf(portfile, "%d", &port);
    sockfd = net_connect("127.0.0.1", port);
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

int disk_usage( int * disk_total, int * disk_free)
{
    int res=0;
    char filename[256] ;
    struct statfs stfs;
    filename[0]=0;
    FILE * curdisk = fopen(VAR_DIR"/dvrcurdisk", "r");
    if( curdisk ) {
        if( fscanf(curdisk, "%s", filename ) ) {
            if (statfs(filename, &stfs) == 0) {
                *disk_free = stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
                *disk_total = stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
                res=1 ;
            }
        }
        fclose( curdisk );
    }
    return res ;
}

int get_temperature( int * sys_temp, int * disk_temp)
{
    if( dio_mmap() ) {
        *sys_temp = p_dio_mmap->iotemperature ;
        *disk_temp = p_dio_mmap->hdtemperature1 ;
        dio_munmap();
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
    int camera_number ;
    struct channelstate cs[16] ;
    struct dvrstat stat ;

    memset( &savedstat, 0, sizeof( savedstat ) );
    statfile = fopen( "savedstat", "rb");
    if( statfile ) {
        fread( &savedstat, sizeof(savedstat), 1, statfile );
        fclose( statfile );
    }

    statfile = fopen( "camera_number", "r");
    if( statfile ) {
        fscanf(statfile, "%d", &camera_number );
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

    //JSON head
    printf("{");

    // dvr time
    char timebuf[100] ;
    double timeinc ;
    gettimeofday(&stat.checktime, NULL);

    time_t ttnow ;
    ttnow = (time_t) stat.checktime.tv_sec ;
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S "
    strftime( timebuf, sizeof(timebuf), RFC1123FMT, localtime( &ttnow ) );
    printf("\"dvrtime\":\"%s\",", timebuf);

    if( savedstat.checktime.tv_sec==0 ) {
        timeinc = 1.0 ;
    }
    else {
        timeinc = (double)(stat.checktime.tv_sec-savedstat.checktime.tv_sec) +
            (double)(stat.checktime.tv_usec - savedstat.checktime.tv_usec)/1000000.0 ;
    }

    // get status from dvrsvr
    memset( cs, 0, sizeof(cs) );
    chno=getchannelstate(cs, stat.streambytes, 16);
    for( i=1; i<=camera_number ; i++ )
    {
        if( cs[i-1].sig ) {
            printf("\"camera_%d_signal_lost\":\"on\",", i );
        }
        if( cs[i-1].rec ) {
            printf("\"camera_%d_recording\":\"on\",", i );
        }
        if( cs[i-1].mot ) {
            printf("\"camera_%d_motion\":\"on\",", i );
        }

        double bitrate = (double)(stat.streambytes[i-1] - savedstat.streambytes[i-1]) / (timeinc*125.0) ;
        if( bitrate > 10000.0 ) {
            bitrate = 0.0 ;
        }

        printf("\"camera_%d_bitrate\":\"%d\",", i, (int)bitrate );

    }

    // calculate CPU usage
    FILE * uptimefile = fopen("/proc/uptime", "r" );
    if( uptimefile ) {
        fscanf( uptimefile, "%f %f", &stat.uptime, &stat.idletime ) ;
        fclose( uptimefile );
    }

    float cpuusage = stat.uptime - savedstat.uptime ;
    if( cpuusage < 0.1 ) {
        cpuusage = 99.0 ;
    }
    else {
        cpuusage = 100.0 * (cpuusage - (stat.idletime-savedstat.idletime)) / cpuusage ;
    }

    printf("\"cpu_usage\":\"%.1f\",", cpuusage );

    // print memory usage
    int stfree, sttotal ;
    if( memory_usage(&sttotal, &stfree) ) {
        printf("\"memory_total\":\"%.1f\",", ((float)sttotal)/1000.0 );
        printf("\"memory_free\":\"%.1f\",", ((float)stfree)/1000.0 );
    }

    // print disk usage
    if( disk_usage(&sttotal, &stfree) ) {
        printf("\"disk_total\":\"%d\",", sttotal );
        printf("\"disk_free\":\"%d\",", stfree );
    }
    else {
        printf("\"disk_total\":\"%d\",", 0 );
        printf("\"disk_free\":\"%d\",", 0 );
    }

    // print system temperature
    int systemperature=-128, hdtemperature=-128 ;
    get_temperature(&systemperature, &hdtemperature) ;
    if( systemperature>-127 && systemperature<127 )  {
        printf("\"temperature_system_c\":\"%d\",", systemperature );
        printf("\"temperature_system_f\":\"%d\",", systemperature*9/5+32 );
    }
    else {
        printf("\"temperature_system_c\":\" \"," );
        printf("\"temperature_system_f\":\" \"," );
    }
    if( hdtemperature>-127 && hdtemperature<127 )  {
        printf("\"temperature_disk_c\":\"%d\",", hdtemperature );
        printf("\"temperature_disk_f\":\"%d\",", hdtemperature*9/5+32 );
    }
    else {
        printf("\"temperature_disk_c\":\" \"," );
        printf("\"temperature_disk_f\":\" \"," );
    }

    printf("\"objname\":\"status_value\"}\r\n" );

    statfile = fopen( "savedstat", "wb");
    if( statfile ) {
        fwrite( &stat, sizeof(stat), 1, statfile );
        fclose( statfile );
    }

}


int print_cfgreport()
{
    string s ;
    int l;
    int i;
    config_enum enumkey ;
    config dvrconfig(CFG_FILE);
    string tzi ;
    string value ;
    int ivalue ;
    char buf[100] ;
    char section[100] ;
    int sensor_number ;
    string * sensornames ;
    FILE * ifile ;

    // get sensors name
    // sensor number
    sensor_number=dvrconfig.getvalueint("io", "inputnum");
    if( sensor_number<=0 || sensor_number>32 ) {
        sensor_number=6 ;                   // default
    }

    sensornames = new string [sensor_number] ;
    for( i=0; i<sensor_number; i++ ) {
        sprintf( section, "sensor%d", i+1 );
        sensornames[i] = dvrconfig.getvalue(section, "name") ;
    }

    // write TVS mf id
    value = dvrconfig.getvalue("system", "tvsmfid" );
    if( value.length()>0 ) {
        printf("TVS manufacturer ID : %s\n", (char *)value );
    }

    // write system_value
    value = dvrconfig.getvalue("system","tvs_licenseplate");
    if( value.length()>0 ) {
        printf("Cab license plate number : %s\n", (char *)value );
    }

    value = dvrconfig.getvalue("system","tvs_medallion");
    if( value.length()>0 ) {
        printf("Cab medallion : %s\n", (char *)value );
    }

    value = dvrconfig.getvalue("system","tvs_ivcs_serial");
    if( value.length()>0 ) {
        printf( "TVS controller serial No : %s\n", (char *)value );
    }

    ifile = fopen( APP_DIR"/firmwareid", "r" );
    if( ifile ) {
        l = fread( buf, 1, 99, ifile );
        if( l>0 ) {
            buf[l]=0 ;
            printf("Firmware version : %s\n", buf );
        }
        fclose( ifile );
    }

    ifile = fopen( VAR_DIR"/mcuversion", "r" );
    if( ifile ) {
        l = fread( buf, 1, 99, ifile );
        if( l>0 ) {
            buf[l]=0 ;
            printf("MCU firmware version : %s\n", buf );
        }
        fclose( ifile );
    }

    int camera_number = dvrconfig.getvalueint("system", "totalcamera") ;
    printf( "Camera number : %d\n", camera_number );
    printf("Sensor number : %d\n", sensor_number );
    printf("Led number : %d\n", dvrconfig.getvalueint("io", "outputnum") );

    int total, free ;
    memory_usage( &total, &free );
    printf("System total memory : %d Mbytes\n", total/1024 );
    printf("System free memory : %d Mbytes\n", free/1024 );

    disk_usage( &total, &free );
    printf("Total storage space : %d Mbytes\n", total );
    printf("Free storage space : %d Mbytes\n", free );

    if( get_temperature( &total, &free ) ) {
        printf("System temperature : %d degree C / %d degree F\n", total, total*9/5+32 );
    }

    // dvr_time_zone
    value = dvrconfig.getvalue("system", "timezone");
    if( value.length()>0 ) {
        printf("Time zone : %s\n", (char *)value );
    }

    // shutdown_delay
    value = dvrconfig.getvalue("system", "shutdowndelay");
    if( value.length()>0 ) {
        printf("Shutdown delay timer : %s\n", (char *)value );
    }

   // pre_lock_time
   value = dvrconfig.getvalue("system", "prelock");
   if( value.length()>0 ) {
       printf("Event marker pre-lock time : %s\n", (char *)value );
   }

    // post_lock_time
    value = dvrconfig.getvalue("system", "postlock");
    if( value.length()>0 ) {
        printf("Event marker post-lock time : %s\n", (char *)value );
    }

    // no rec playback
    ivalue = dvrconfig.getvalueint("system", "norecplayback");
    printf( "Stop recording when playback : %s\n" , ivalue >0 ? "yes": "no" );

    // no rec liveview
    ivalue = dvrconfig.getvalueint("system", "noreclive");
    printf( "Stop recording when live viewing throught network : %s\n" , ivalue >0 ? "yes": "no" );

    // gpslog enable
    ivalue = dvrconfig.getvalueint("glog", "gpsdisable");
    printf( "GPS loggin %s\n", ivalue==0 ? "enabled." : "disabled." );

    // gps port
    value = dvrconfig.getvalue("glog", "serialport");
    if( value.length()>0 ) {
           printf("GPS port : %s\n", (char *)value );
    }

    // gps baud rate
    value = dvrconfig.getvalue("glog", "serialbaudrate");
    if( value.length()>0 ) {
        printf("GPS baud rate : %s\n", (char *)value );
    }

    // write camera_value

    for( i=1; i<=camera_number; i++ ) {
        printf( "\nCamera #%d settings.\n", i );
        char section[10] ;

        sprintf(section, "camera%d", i);

         // camera_name
         value = dvrconfig.getvalue(section, "name");
         if( value.length()>0 ) {
            printf("Camera name : %s\n", (char *)value );
         }

        // enable_camera
        if( dvrconfig.getvalueint(section, "enable") ) {
            printf( "Camera enabled.\n" );
        }
        else {
            printf( "Camera disabled.\n" );
        }


        // recording_mode
        static const char * recordingmode[5]={
            "Continue",
            "Trigger by sensor",
            "Trigger by motion",
            "Trigger by sensor", "" } ;
        ivalue = dvrconfig.getvalueint(section, "recordmode");
        printf("Recording mode : %s\n" , recordingmode[ivalue] );

        // # resolution, 0:CIF, 1:2CIF, 2:DCIF, 3:4CIF
        // resolution
        static const char * picres[8]={
            "360x240",
            "720x240",
            "528x320",
            "720x480",
            "176x120",
            "1280x720 (720p)",
            "1920x1080 (1080p)",
            "Unknown" } ;
        ivalue = dvrconfig.getvalueint(section, "resolution");
        if( ivalue<0 || ivalue>7 ) ivalue = 7;
        printf("Picture resolution : %s\n", picres[ivalue] );

        // frame_rate
        ivalue = dvrconfig.getvalueint(section, "framerate");
        printf("Frame rate : %d\n", ivalue );

        //          # Bitrate control
        //          bitrateen=1
        //          # Bitrate mode, 0:VBR, 1:CBR
        //          bitratemode=0
        //          bitrate=1200000
        // bit_rate_mode
        ivalue = dvrconfig.getvalueint(section,"bitratemode");
        printf( "Bitrate mode : %s\n", ivalue==0 ? "VBR" : "CBR" );

        // bit_rate
        value = dvrconfig.getvalue(section, "bitrate");
        printf("Bit rate : %s\n",  (char *)value );

        //          # picture quality, 0:lowest, 10:highest
        static const char * picqua[11] = {
            "lowest",
            "lowest",
            "lowest",
            "lowest",
            "lowest",
            "low",
            "medium",
            "high",
            "hightest",
            "hightest",
            "hightest"
        } ;
        // picture_quaity
        ivalue = dvrconfig.getvalueint(section, "quality");
        if( ivalue<=10 ) printf("Picture quaity : %s\n" , picqua[ivalue] );

        // picture controls
        ivalue = dvrconfig.getvalueint(section, "brightness");
        printf("Brightness : %d\n", ivalue);

        ivalue = dvrconfig.getvalueint(section, "contrast");
        printf("Contrast : %d\n", ivalue);

        ivalue = dvrconfig.getvalueint(section, "saturation");
        printf("Saturation : %d\n", ivalue);

        ivalue = dvrconfig.getvalueint(section, "hue");
        printf("Hue : %d\n", ivalue);

        // pre_recording_time
        ivalue = dvrconfig.getvalueint(section, "prerecordtime");
        printf("Pre-recording time for trigger mode : %d (s)\n", ivalue );

        // post_recording_time
        ivalue = dvrconfig.getvalueint(section, "postrecordtime");
        printf("Post-recording time for trigger mode : %d (s)\n", ivalue );

        printf("    Triggering sensors: \n");
        // trigger
        for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
            char trigger[30] ;
            int  itrig ;
            sprintf(trigger, "trigger%d", ivalue );
            itrig = dvrconfig.getvalueint( section, trigger );
            if( itrig>0 ) {
                printf( "        %s : ", (char *)sensornames[ivalue-1] );
                if( itrig & 1 ) {
                    printf("on ");
                }
                if( itrig & 2 ) {
                    printf("off ");
                }
                if( itrig & 4 ) {
                    printf("turnon ");
                }
                if( itrig & 8 ) {
                    printf("turnoff ");
                }
                printf(";\n");
            }
        }

        // osd
        l=0 ;
        printf("Display sensor name:");
        for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
            char osd[30] ;
            int iosd ;
            sprintf(osd, "sensorosd%d", ivalue);
            iosd  = dvrconfig.getvalueint( section, osd );
            if( iosd>0 ) {
                if( l>0 ) {
                    printf(", ");
                }
                printf( "%s", (char *)sensornames[ivalue-1] );
                l++ ;
            }
        }
        if( l>0 ) {
            printf(".\n" );
        }
        else {
            printf("(None).\n");
        }

        // show_gps
        ivalue = dvrconfig.getvalueint(section, "showgps");
        printf("Display GPS speed : %s\n", ivalue>0? "on" : "off" );

        // gpsunit
        // speed_display
        ivalue = dvrconfig.getvalueint(section, "gpsunit");
        printf("GPS speed unit : %s\n", ivalue==0 ? "mph" : "km/h" ) ;

        ivalue = dvrconfig.getvalueint(section, "showgpslocation");
        printf( "Display GPS coordinate : %s\n", ivalue>0? "on" : "off" );

        ivalue = dvrconfig.getvalueint(section, "show_medallion");
        printf( "Display medallion # : %s\n", ivalue>0? "on" : "off" );

        ivalue = dvrconfig.getvalueint(section, "show_licenseplate");
        printf( "Display license plate # : %s\n", ivalue>0? "on" : "off" );

        ivalue = dvrconfig.getvalueint(section, "show_ivcs");
        printf( "Display controller serial # : %s\n", ivalue>0? "on" : "off" );

        ivalue = dvrconfig.getvalueint(section, "show_cameraserial");
        printf( "Display camera serial # : %s\n", ivalue>0? "on" : "off" );

        const char * alarmmode[8] = {
            "OFF",
            "ON",
            "0.5s Flash",
            "1s Flash",
            "2s Flash",
            "4s Flash",
            "8s Flash", "unknown" } ;
        // record_alarm_mode
        ivalue = dvrconfig.getvalueint( section, "recordalarm" );
        printf("Recording alarm # : %d\n", ivalue );

        ivalue = dvrconfig.getvalueint( section, "recordalarmpattern" );
        printf( "Recording alarm mode : %s\n", alarmmode[ivalue] );

        ivalue = dvrconfig.getvalueint( section, "videolostalarm" );
        printf("Video lost alarm # : %d\n", ivalue );

        ivalue = dvrconfig.getvalueint( section, "videolostalarmpattern" );
        printf( "Video lost alarm mode : %s\n", alarmmode[ivalue] );

    }

    // write sensor_value
    printf("\nSensor settings.\nSensor number : %d\n", sensor_number );

    for( i=1; i<=sensor_number; i++ ) {
        sprintf( section, "sensor%d", i );
        printf( "Sensor #%d\n", i );

        value = dvrconfig.getvalue(section, "name") ;
        printf( "    Name : %s\n", (char *)value );

        // inverted value
        ivalue = dvrconfig.getvalueint(section, "inverted") ;
        printf( "    Inverted : %s\n", ivalue>0 ? "on" : "off" );
    }

    // write network_value
    printf( "Network settings,\n");

    // eth_ip
    value = dvrconfig.getvalue("network", "eth_ip");
    printf( "Ethernet IP address : %s\n", (char *)value );

    // eth_mask
    value = dvrconfig.getvalue("network", "eth_mask");
    printf( "Ethernet net mask : %s\n", (char *)value );

    // gateway
    value = dvrconfig.getvalue("network", "gateway");
    printf( "Gateway : %s\n", (char *)value );

    // wifi_ip
    value = dvrconfig.getvalue("network", "wifi_ip");
    printf( "Wireless IP address : %s\n", (char *)value );

    // wifi_mask
    value = dvrconfig.getvalue("network", "wifi_mask");
    printf( "Wireless net mask : %s\n", (char *)value );

    // wifi_id
    value = dvrconfig.getvalue("network", "wifi_essid");
    printf( "Wireless essid : %s\n", (char *)value );

    return 0;
}

int main()
{
    FILE * f ;
    char id[10] ;
    f=fopen( VAR_DIR"/connectid", "r" ) ;
    if( f==NULL ) {
        return 0;
    }
    id[0]=0 ;
    fread( id, 1, 10, f ) ;
    fclose( f );
    if(
       (id[0]=='M' && id[1]=='F') ||
       (id[0]=='I' && id[1]=='N') ||
       (id[0]=='I' && id[1]=='S') )
    {
        print_cfgreport();
    }
    return 0;
}
