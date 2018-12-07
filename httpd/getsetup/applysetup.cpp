
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "../../cfg.h"
#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

char tzfile[] = "tz_option" ;

pid_t  dvrpid=0 ;

void dvrsvr_down()
{
    dvrpid = 0 ;
    FILE * fdvrpid = fopen( VAR_DIR "/dvrsvr.pid", "r");
    if( fdvrpid ) {
        fscanf(fdvrpid, "%d", &dvrpid );
        fclose( fdvrpid );
    }
    if( dvrpid > 0 ) {
        kill( dvrpid, SIGUSR1 );
    }
}

void dvrsvr_up()
{
    if( dvrpid>0 ) {
        kill( dvrpid, SIGUSR2 );
    }
}

char buf[32000] ;

char * getsetvalue( char * name )
{
    static char v[500] ;
    char * h ;
    char * needle ;
    int l = strlen(name);
    int i ;
    h = buf ;
    while( (needle = strstr( h, name ))!=NULL ) {
        h=needle+l+1 ;
        if( *(needle-1)=='\"' && needle[l]=='\"' ) {
            needle=h ;
            // skip white space
            while( *needle<=' ' ) {
                needle++;
            }

            if( *needle==':' && *(needle+1)=='\"' ) {
                needle+=2;

                for( l=0; l<500; l++ ) {
                    char c = needle[l];
                    if( c=='\"' || c=='}' || c==0 ) {
                        break;
                    }
                    else {
                        v[l]=c ;
                    }
                }

                // trim string
                while( l>0 ) {
                    if( v[l-1]<=' ' )  {
                        l--;
                    }
                    else {
                        break;
                    }
                }
                v[l]=0;
                for( i=0; i<l; i++ ) {
                    if( v[i]>' ' )
                        break;
                }
                return &v[i] ;
            }
        }
    }
    return NULL ;
}

// generate a random charactor in [a-zA-Z0-9./]
int randomchar()
{
    int x ;
    FILE * fran ;
    fran = fopen("/dev/urandom", "r");
    x=fgetc(fran)%64 ;
    fclose( fran );
    if( x<26 ) {
        return 'A'+x ;
    }
    else if( x<52 ) {
        return 'a'+x-26 ;
    }
    else if( x<62 ) {
        return '0'+x-52 ;
    }
    else if( x<63 ){
        return '.' ;
    }
    else {
        return '/' ;
    }
}

void setadminpassword( char * password )
{
    char salt[20] ;
    char * key ;
    int i ;
    FILE * fpasswd;
    fpasswd = fopen( APP_DIR "/adminpasswd", "w");
    if( fpasswd ) {
        // generate passsword line
        strcpy(salt, "$1$12345678$" );
        for( i=3; i<=10; i++ ) {
            salt[i]=randomchar();
        }
        key = crypt(password, salt);
        if( key ) {
            fwrite( key, 1, strlen(key), fpasswd);
        }
        fclose( fpasswd);
    }
}

// c64 key should be 400bytes
void fileenckey( char * password, char * c64key )
{
    unsigned char filekey256[260] ;
    key_256( password, filekey256 );            // hash password
    bin2c64(filekey256, 256, c64key);               // convert to c64
}

void run( char * cmd, char * argu, int background )
{
    pid_t child = fork();
    if( child==0 ) {        // child process
        execlp( cmd, cmd, argu, NULL );
    }
    else {
        if( background==0 && child>0 ) {        //
            waitpid( child, NULL, 0 );
        }
    }
}

#include <arpa/inet.h>

char * getbroadcastaddr(char* ipaddr,char* ipmask)
{
    struct in_addr ip;
    struct in_addr netmask;
    struct in_addr network;
    struct in_addr broadcast;
    inet_aton(ipaddr, &ip);
    inet_aton(ipmask, &netmask);
    network.s_addr = ip.s_addr & netmask.s_addr;
    broadcast.s_addr = network.s_addr | ~netmask.s_addr;
    return inet_ntoa(broadcast);
}


static char * rtrim( char * s )
{
    while( *s > 1 && *s <= ' ' ) {
        s++ ;
    }
    return s ;
}

static char * ltrim( char * s )
{
    int l = strlen(s);
    while( l>0 ) {
        if( s[l-1] <= ' ' )
            l-- ;
        else
            break ;
    }
    s[l] = 0 ;
    return s ;
}

static char * readfile( char * filename )
{
    static char sbuf[1024] ;
    FILE * f ;
    f = fopen( filename, "r" );
    if( f ) {
        int r=fread( sbuf, 1, sizeof(sbuf)-1, f );
        fclose( f );
        if( r>0 ) {
            sbuf[r]=0 ;
            return ltrim(rtrim(sbuf)) ;
        }
    }
    return NULL ;
}

static void savefile( char * filename, char * v )
{
    FILE * f ;
    f = fopen( filename, "w" );
    if( f ) {
        fputs( v, f ) ;
        fclose( f );
    }
}

int main()
{
    int i, r;
    int ivalue ;
    char * v ;
    FILE * fvalue ;
    char section[80] ;
    int  sensor_number = 31;
    config dvrconfig(CFG_FILE);

    // suspend dvrsvr
    dvrsvr_down();

    // set system_value
    fvalue = fopen( "system_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

#ifdef  TVS_APP
        v=getsetvalue("tvs_medallion");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_medallion", v );
            // duplicate medallion to hostname
            dvrconfig.setvalue( "system", "hostname", v );
        }

        v=getsetvalue("tvs_licenseplate");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_licenseplate", v );
        }

        v=getsetvalue("tvs_ivcs_serial");
        if( v ) {
            dvrconfig.setvalue( "system", "tvs_ivcs_serial", v );
        }
#endif  // TVS_APP

#ifdef  PWII_APP

        // PW recording method (just in case option)
        v=getsetvalue("pw_recordmethod");
        if( v ) {
            dvrconfig.setvalueint( "system", "pw_recordmethod", 1 );
        }
        else {
            dvrconfig.setvalueint( "system", "pw_recordmethod", 0 );
        }

        v=getsetvalue("vehicleid");
        if( v ) {
            dvrconfig.setvalue( "system", "id1", v );
            // duplicate medallion to hostname
            dvrconfig.setvalue( "system", "hostname", v );
        }

        v=getsetvalue("district");
        if( v ) {
            dvrconfig.setvalue( "system", "id2", v );
        }

        v=getsetvalue("unit");
        if( v ) {
            dvrconfig.setvalue( "system", "serial", v );
        }

        // adminpassword
        v=getsetvalue("adminpassword");
        if( v!=NULL && strcmp(v, "*****" )!=0 ) {
            setadminpassword(v);
        }


        // file encryptions
        /*

        v = getsetvalue ("en_file_encryption");
        dvrconfig.setvalueint( "system", "fileencrypt", v?1:0 );

        if( v ) {
            v = getsetvalue ("en_use_default_password");
            if( v ) {
                // use default mf key
                char * mfkey = readfile( "/davinci/dvr/mfkey" );
                dvrconfig.setvalue( "system", "filepassword", mfkey );
            }
            else {
                // use user set password
                v = getsetvalue ("file_password");
                if( strcmp( v, "********" ) != 0 ) {
                    // do set password
                    char filec64key[400] ;
                    fileenckey( v, filec64key );
                    dvrconfig.setvalue("system", "filepassword", filec64key);   // save password to config file
                }
            }
        }
        */

        // all other values

        v = getsetvalue ("shutdown_delay");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "shutdowndelay", i );
        }

        v = getsetvalue ("standbytime");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "standbytime", i );
        }

        v = getsetvalue ("uploadingtime");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "uploadingtime", i );
        }

        v = getsetvalue ("archivingtime");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>36000 ) i=36000 ;
            dvrconfig.setvalueint( "system", "archivetime", i );
        }

        // trace mark event time (in seconds)
        v = getsetvalue ("tracemarktime");
        if( v ) {
            i=0 ;
            sscanf( v, "%d", &i);
            if( i<1 ) i=1 ;
            if( i>7200 ) i=7200 ;
            dvrconfig.setvalueint( "system", "tracemarktime", i );
        }

        // file buffer size
        v = getsetvalue ("filebuffersize");
        if( v ) {
            i=strlen(v);
            v[i]='k' ;
            v[i+1]=0 ;
            dvrconfig.setvalue( "system", "filebuffersize", v );
        }

        v = getsetvalue ("file_size");
        if( v ) {
            i=strlen(v);
            v[i]='M' ;
            v[i+1]=0 ;
            dvrconfig.setvalue( "system", "maxfilesize", v );
        }

        v = getsetvalue ("file_time");
        if( v ) {
            sscanf( v, "%d", &i);
            if( i<0 ) i=0 ;
            if( i>18000 ) i=18000 ;
            dvrconfig.setvalueint( "system", "maxfilelength", i );
        }

        // totalcamera
        v = getsetvalue ("totalcamera");
        if( v ) {
            dvrconfig.setvalue( "system", "totalcamera", v );
        }


#ifdef APP_PWZ8
        // totalcamera
        v = getsetvalue ("totalbodycam");
        if( v ) {
            dvrconfig.setvalue( "system", "totalbodycam", v );
        }
        else {
            dvrconfig.setvalue( "system", "totalbodycam", "0" );
        }
#endif

        // pre lock time
        v = getsetvalue ("pre_lock_time");
        if( v ) {
            dvrconfig.setvalue( "system", "prelock", v );
        }

        // post lock time
        v = getsetvalue ("post_lock_time");
        if( v ) {
            dvrconfig.setvalue( "system", "postlock", v );
        }

        if( getsetvalue( "bool_norecplayback" )!=NULL ) {
            v = getsetvalue( "norecplayback" );
            if( v ) {
                dvrconfig.setvalueint( "system", "norecplayback", 1 );
            }
            else {
                dvrconfig.setvalueint( "system", "norecplayback", 0 );
            }
        }

        if( getsetvalue( "bool_noreclive" )!=NULL ) {
            v = getsetvalue( "noreclive" );
            if( v ) {
                dvrconfig.setvalueint( "system", "noreclive", 1 );
            }
            else {
                dvrconfig.setvalueint( "system", "noreclive", 0 );
            }
        }

#ifdef APP_PWZ8
        // sd camera
        v = getsetvalue ("camsd");
        dvrconfig.setvalueint( "system", "camsd", v?1:0 );
#endif

        // gpslog enable
        v = getsetvalue ("en_gpslog");
        if( v ) {
            dvrconfig.setvalueint( "glog", "gpsdisable", 0 );
        }
        else {
            dvrconfig.setvalueint( "glog", "gpsdisable", 1 );
        }

        // gps port
        v = getsetvalue( "gpsport" );
        if( v ) {
            dvrconfig.setvalue( "glog", "serialport", v );
        }

        // gps baud rate
        v = getsetvalue( "gpsbaudrate" );
        if( v ) {
            dvrconfig.setvalue( "glog", "serialbaudrate", v );
        }

        // Video output selection
        v = getsetvalue( "videoout" );
        if( v ) {
            dvrconfig.setvalue(  "VideoOut", "startchannel", v );
        }

#endif  // PWII_APP

        // Time Zone
        v = getsetvalue ("dvr_time_zone");
        if( v ) {
            dvrconfig.setvalue( "system", "timezone", v );
        }

        v = getsetvalue ("custom_tz");
        if( v ) {
            dvrconfig.setvalue( "timezones", "Custom", v );
        }

    }

    // write sensor_value
    fvalue = fopen( "sensor_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

        v = getsetvalue ("sensor_number");
        if( v ) {
            sscanf(v, "%d", &sensor_number);
            if( sensor_number<0 || sensor_number>32 ) {
                sensor_number=6 ;
            }
            dvrconfig.setvalueint( "io", "inputnum", sensor_number );
        }

        for( i=1; i<=sensor_number; i++ ) {
            char sensorkey[30] ;
            sprintf( section, "sensor%d", i );
            sprintf( sensorkey,   "sensor%d_name", i );
            v = getsetvalue(sensorkey);
            if( v ) {
                dvrconfig.setvalue(section, "name", v );
            }

            sprintf( sensorkey, "sensor%d_inverted", i );
            v = getsetvalue(sensorkey);
            if( v && strcmp( v, "on" )==0 ) {
                dvrconfig.setvalueint(section, "inverted", 1 );
            }
            else {
                dvrconfig.setvalueint(section, "inverted", 0 );
            }

            sprintf( sensorkey, "sensor%d_eventmarker", i );
            v = getsetvalue(sensorkey);
            if( v && strcmp( v, "on" )==0 ) {
                dvrconfig.setvalueint(section, "eventmarker", 1 );
            }
            else {
                dvrconfig.setvalueint(section, "eventmarker", 0 );
            }

        }

#if defined(TVS_APP) || defined(PWII_APP)
       v=getsetvalue("sensor_powercontrol");
       if(v){
           dvrconfig.setvalueint("io","sensor_powercontrol",1);
       }
       else {
           dvrconfig.setvalueint("io","sensor_powercontrol",0);
       }
#endif

    }

    // set camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<=0 ) {
        camera_number=2 ;
    }

    for( i=1; i<=camera_number; i++ ) {
        char camfilename[20] ;
        sprintf(camfilename, "camera_value_%d", i);
        fvalue=fopen(camfilename, "r");
        if(fvalue){
            r=fread( buf, 1, sizeof(buf), fvalue );
            buf[r]=0;
            fclose(fvalue);
            sprintf(section, "camera%d", i);

            // enable_camera
            if(getsetvalue( "bool_enable_camera" )!=NULL ) {
                v=getsetvalue( "enable_camera" );
                if( v ) {
                    dvrconfig.setvalueint(section,"enable",1);
                }
                else {
                    dvrconfig.setvalueint(section,"enable",0);
                }
            }

            // camera_name
            v=getsetvalue( "camera_name" );
            if( v ) {
                dvrconfig.setvalue(section,"name",v);
            }


            // m_enablejicaudio, record audio in JIC mode
            v=getsetvalue( "enablejicaudio" );
            if( v ) {
                dvrconfig.setvalueint(section,"enablejicaudio", 1);
            }
            else {
                dvrconfig.setvalueint(section,"enablejicaudio", 0);
            }

            // camera_type
            v=getsetvalue( "camera_type" );
            if( v ) {
                dvrconfig.setvalue(section,"type",v);
            }

            // ip camera_url
            v=getsetvalue( "ipcamera_url" );
            if( v && strlen(v)>5 ) {
                dvrconfig.setvalue(section,"stream_URL",v);
            }

            // Physical channel
            v=getsetvalue( "channel" );
            if( v ) {
                dvrconfig.setvalue(section,"channel",v);
            }

            // recording_mode
            v=getsetvalue( "recording_mode" );
            if( v ) {
                dvrconfig.setvalue(section,"recordmode",v);
            }

#ifdef APP_PWZ5
            // force record channel (camera direction)
            v=getsetvalue( "forcerecordchannel" );
            if( v ) {
                dvrconfig.setvalue(section,"forcerecordchannel",v);
            }
#endif

            // resolution
            v=getsetvalue( "resolution" );
            if( v ) {
                dvrconfig.setvalue(section,"resolution",v);
            }

            // frame_rate
            v=getsetvalue( "frame_rate" );
            if( v ) {
                dvrconfig.setvalue(section,"framerate",v);
            }

            // bit_rate_mode
            v=getsetvalue( "bit_rate_mode" );
            if( v ) {
                if( *(char *)v == '0' ) {
                    dvrconfig.setvalueint(section,"bitrateen", 0);
                }
                else if( *(char *)v == '1' ) {
                    dvrconfig.setvalueint(section,"bitrateen", 1);
                    dvrconfig.setvalueint(section,"bitratemode", 0);
                }
                else {
                    dvrconfig.setvalueint(section,"bitrateen", 1);
                    dvrconfig.setvalueint(section,"bitratemode", 1);
                }
            }

            // bit_rate
            v=getsetvalue( "bit_rate" );
            if( v ) {
                dvrconfig.setvalue(section,"bitrate",v);
            }

            // trigger and osd
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                char trigger[30], osd[30], sensor[30] ;
                int  tr;
                sprintf(trigger, "trigger%d", ivalue );
                sprintf(osd, "sensorosd%d", ivalue);

                // osd
                if( getsetvalue( "bool_sensor_osd" )!=NULL ) {
                    sprintf(sensor, "sensor%d_osd", ivalue );
                    v=getsetvalue( sensor ) ;
                    if( v ) {
                        dvrconfig.setvalueint(section, osd, 1);
                    }
                    else {
                        dvrconfig.setvalueint(section, osd, 0);
                    }
                }

                // trigger selections (PWII / TVS )
              tr=0 ;
              sprintf(sensor, "sensor%d_trigger_on", ivalue );
              v=getsetvalue( sensor ) ;
              if( v ) {
                  tr|=1 ;
              }
              sprintf(sensor, "sensor%d_trigger_off", ivalue );
              v=getsetvalue( sensor ) ;
              if( v ) {
                  tr|=2 ;
              }
              sprintf(sensor, "sensor%d_trigger_turnon", ivalue );
              v=getsetvalue( sensor ) ;
              if( v ) {
                  tr|=4 ;
              }
              sprintf(sensor, "sensor%d_trigger_turnoff", ivalue );
              v=getsetvalue( sensor ) ;
              if( v ) {
                  tr|=8 ;
              }

              dvrconfig.setvalueint(section, trigger, tr);

            }

            // gforce_trigger
            v=getsetvalue( "gforce_trigger" );
            if( v ) {
                dvrconfig.setvalueint(section,"gforce_trigger", 1 );
            }
            else {
                dvrconfig.setvalueint(section,"gforce_trigger", 0 );
            }

            // gforce_trigger_value
            v=getsetvalue( "gforce_trigger_value" );
            if( v ) {
                dvrconfig.setvalue(section,"gforce_trigger_value", v );
            }

            // speed_trigger
            v=getsetvalue( "speed_trigger" );
            if( v ) {
                dvrconfig.setvalueint(section,"speed_trigger", 1 );
            }
            else {
                dvrconfig.setvalueint(section,"speed_trigger", 0 );
            }

            // speed_trigger_value
            v=getsetvalue( "speed_trigger_value" );
            if( v ) {
                dvrconfig.setvalue(section,"speed_trigger_value", v );
            }

            // pre_recording_time
            v=getsetvalue( "pre_recording_time" );
            if( v ) {
                dvrconfig.setvalue(section,"prerecordtime",v);
            }

            // post_recording_time
            v=getsetvalue( "post_recording_time" );
            if( v ) {
                dvrconfig.setvalue(section,"postrecordtime",v);
            }

            // show_gps
            if( getsetvalue( "bool_show_gps" )!=NULL ) {
                v=getsetvalue( "show_gps" );
                if( v ) {
                    dvrconfig.setvalueint(section,"showgps",1);
                }
                else {
                    dvrconfig.setvalueint(section,"showgps",0);
                }
            }

            // gpsunit
            // speed_display
            v=getsetvalue( "speed_display" );
            if( v ) {
                dvrconfig.setvalue(section,"gpsunit",v);
            }

            // show_gps_coordinate
            if( getsetvalue( "bool_show_gps_coordinate" )!=NULL ) {
                v=getsetvalue( "show_gps_coordinate" );
                if( v ) {
                    dvrconfig.setvalueint(section,"showgpslocation",1);
                }
                else {
                    dvrconfig.setvalueint(section,"showgpslocation",0);
                }
            }

#ifdef TVS_APP

            // show_medallion
            if( getsetvalue( "bool_show_medallion" )!=NULL ) {
                v=getsetvalue( "show_medallion" );
                dvrconfig.setvalueint(section,"show_medallion",v?1:0);
            }
            // show_licenseplate
            if( getsetvalue( "bool_show_licenseplate" )!=NULL ) {
                v=getsetvalue( "show_licenseplate" );
                dvrconfig.setvalueint(section,"show_licenseplate",v?1:0);
            }
            // show_ivcs
            if( getsetvalue( "bool_show_ivcs" )!=NULL ) {
                v=getsetvalue( "show_ivcs" );
                dvrconfig.setvalueint(section,"show_ivcs",v?1:0);
            }
            // show_medallion
            if( getsetvalue( "bool_show_cameraserial" )!=NULL ) {
                v=getsetvalue( "show_cameraserial" );
                dvrconfig.setvalueint(section,"show_cameraserial",v?1:0);
            }

#endif

#ifdef PWII_APP

            // show_vri
            if( getsetvalue( "bool_show_vri" )!=NULL ) {
                v=getsetvalue( "show_vri" );
                if( v ) {
                    dvrconfig.setvalueint(section,"show_vri",1);
                }
                else {
                    dvrconfig.setvalueint(section,"show_vri",0);
                }
            }

            // show_policeid
            if( getsetvalue( "bool_show_policeid" )!=NULL ) {
                v=getsetvalue( "show_policeid" );
                if( v ) {
                    dvrconfig.setvalueint(section,"show_policeid",1);
                }
                else {
                    dvrconfig.setvalueint(section,"show_policeid",0);
                }
            }

#endif

            // show_gforce sensor value
            if( getsetvalue( "bool_show_gforce" )!=NULL ) {
                v=getsetvalue( "show_gforce" );
                if( v ) {
                    dvrconfig.setvalueint(section,"show_gforce",1);
                }
                else {
                    dvrconfig.setvalueint(section,"show_gforce",0);
                }
            }

             // record_alarm_mode
            v=getsetvalue( "record_alarm_mode" ) ;
            if( v ) {
                dvrconfig.setvalue( section,"recordalarmpattern", v );
            }
            v=getsetvalue( "record_alarm_led" ) ;
            if( v ) {
                dvrconfig.setvalue( section,"recordalarm", v );
            }

             // video_lost_alarm_mode
            v=getsetvalue( "video_lost_alarm_mode" ) ;
            if( v ) {
                dvrconfig.setvalue( section,"videolostalarmpattern", v );
            }
            v=getsetvalue( "video_lost_alarm_led" ) ;
            if( v ) {
                dvrconfig.setvalue( section,"videolostalarm", v );
            }

        }
    }

#ifdef APP_PWZ8
    // bodycam values
    camera_number = dvrconfig.getvalueint("system", "totalbodycam");

    for( i=1; i<=camera_number; i++ ) {
        sprintf(section, "bodycam_value_%d", i);
        fvalue=fopen(section, "r");
        if(fvalue){
            r=fread( buf, 1, sizeof(buf), fvalue );
            buf[r]=0;
            fclose( fvalue );
            sprintf( section, "bodycam%d", i);
            // bodycam enable
            v = getsetvalue( "bcam_enable" );
            if( v!=NULL && strcasecmp(v,"on")==0 ) {
                dvrconfig.setvalueint(section,"enable",1);
            }
            else {
                dvrconfig.setvalueint(section,"enable",0);
            }

            // bodycam ip
            v = getsetvalue( "bcam_ip" );
            if( v==NULL )
                v=(char *)"" ;
            dvrconfig.setvalue(section,"ip",v);

            // bodycam trigger
            v = getsetvalue( "bcam_trigger" );
            if( v!=NULL && strcasecmp(v,"on")==0 ) {
                dvrconfig.setvalueint(section,"bodycam_trigger",1);
            }
            else {
                dvrconfig.setvalueint(section,"bodycam_trigger",0);
            }

            // bodycam trigger by DVR
            v = getsetvalue( "bcam_dvrtrigger" );
            if( v!=NULL && strcasecmp(v,"on")==0 ) {
                dvrconfig.setvalueint(section,"dvr_trigger",1);
            }
            else {
                dvrconfig.setvalueint(section,"dvr_trigger",0);
            }
        }
    }
#endif

    // write network_value
    fvalue = fopen( "network_value", "r");
    if( fvalue ) {
        r=fread( buf, 1, sizeof(buf), fvalue );
        buf[r]=0;
        fclose(fvalue);

        string ip ;
        string mask ;

        // eth_ip
        v=getsetvalue("eth_ip");
        if( v ) {
            ip = v;
        }
        else {
            ip = "192.168.1.100" ;
        }
        dvrconfig.setvalue( "network", "eth_ip", ip );

        // eth_mask
        v=getsetvalue("eth_mask");
        if( v ) {
            mask = v;
        }
        else {
            mask = "255.255.255.0" ;
        }
        dvrconfig.setvalue( "network", "eth_mask", mask );

        //eth broadcast
        dvrconfig.setvalue( "network", "eth_broadcast", getbroadcastaddr( ip, mask ) );

        // dhcp client on eth0
        v=getsetvalue("eth_dhcp");
        if( v!=NULL && strcmp(v,"on")==0 ) {
            dvrconfig.setvalueint( "network", "eth_dhcp", 1 );
        }
        else {
            dvrconfig.setvalueint( "network", "eth_dhcp", 0 );
        }

        // wifi ip
        v=getsetvalue("wifi_ip");
        if( v ) {
            ip = v;
        }
        else {
            ip = "192.168.3.100" ;
        }
        dvrconfig.setvalue( "network", "wifi_ip", ip );

        // wifi_mask
        v=getsetvalue("wifi_mask");
        if( v ) {
            mask = v;
        }
        else {
            mask = "255.255.255.0" ;
        }
        dvrconfig.setvalue( "network", "wifi_mask", mask );

        //wifi broadcast
        dvrconfig.setvalue( "network", "wifi_broadcast", getbroadcastaddr( ip, mask ) );

        // dhcp client on wlan0
        v=getsetvalue("wifi_dhcp");
        if( v!=NULL && strcmp(v,"on")==0 ) {
            dvrconfig.setvalueint( "network", "wifi_dhcp", 1 );
        }
        else {
            dvrconfig.setvalueint( "network", "wifi_dhcp", 0 );
        }

        // set wifi on AP mode (for body camera feature )
        v=getsetvalue("wifi_mode");
        if( v!=NULL )
            dvrconfig.setvalue( "network", "wifi_mode", v );

        // ap ssid
        v=getsetvalue("ap_ssid");
        if( v ) {
            dvrconfig.setvalue( "network", "ap_ssid", v );
        }

        // ap key
        v=getsetvalue("ap_key");
        if( v ) {
            dvrconfig.setvalue( "network", "ap_key", v );
        }

        // ap channel
        v=getsetvalue("ap_channel");
        if( v ) {
            dvrconfig.setvalue( "network", "ap_channel", v );
        }

        // gateway
        v=getsetvalue("gateway_1");
        if( v ) {
            dvrconfig.setvalue( "network", "gateway", v );
        }
        else {
            dvrconfig.setvalue( "network", "gateway", "192.168.1.1" );
        }

        // wifi authentication/encription
        v=getsetvalue("wifi_enc");
        if( v ) {
            int value = 0 ;
            sscanf(v, "%d", &value);
            if( value<0 || value>8 ) {
                value=0 ;
            }
            dvrconfig.setvalueint( "network", "wifi_enc", value );
        }

        // wifi_essid
        v = getsetvalue("wifi_essid");
        if( v ) {
            dvrconfig.setvalue( "network", "wifi_essid", v);
        }

        // wifi_key
        v = getsetvalue("wifi_key");
        if( v ) {
            dvrconfig.setvalue( "network", "wifi_key", v);
        }

        v = getsetvalue("smartserver");
        if( v!=NULL ) {
            dvrconfig.setvalue( "network", "smartserver", v);
        }

        // internet access
        v = getsetvalue("internetaccess");

        if( v!=NULL ) {
            dvrconfig.setvalueint( "system", "nointernetaccess", 0);
        }
        else {
            dvrconfig.setvalueint( "system", "nointernetaccess", 1);
        }
        // internet key
        v = getsetvalue("internetkey");
        if( v ) {
            dvrconfig.setvalue( "system", "internetkey", v);
        }

    }

    // save setting
    sync();
    dvrconfig.save();
    sync();

    // reset network settings
    run( APP_DIR"/setnetwork", NULL, 1 );

    // resume dvrsvr
    dvrsvr_up();

    // re-initialize glog
    fvalue = fopen(VAR_DIR"/glog.pid", "r");
    if( fvalue ) {
        r=0;
        if( fscanf(fvalue, "%d", &r)==1 ) {
            if(r>0) {
                kill( (pid_t)r, SIGUSR2);
            }
        }
        fclose( fvalue );
    }

    // re-initialize ioprocess
    fvalue = fopen(VAR_DIR"/ioprocess.pid", "r");
    if( fvalue ) {
        r=0;
        if( fscanf(fvalue, "%d", &r)==1 ) {
            if(r>0) {
                kill( (pid_t)r, SIGUSR2);
            }
        }
        fclose( fvalue );
    }

    // re-start bodycamd
    fvalue = fopen(VAR_DIR"/bodycamd.pid", "r");
    if( fvalue ) {
        r=0;
        if( fscanf(fvalue, "%d", &r)==1 ) {
            if(r>0) {
                kill( (pid_t)r, SIGUSR2);
            }
        }
        fclose( fvalue );
    }

    return 0;

}