

#include <stdio.h>

#include "cfg.h"

#include "json/json.h"

#include "dvrsvr/crypt.h"
#include "dvrsvr/genclass.h"
#include "dvrsvr/cfg.h"

static const char ON_STR[]="on";

static char * rtrim( char * s )
{
    while( *s > 0 && *s <= ' ' ) {
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

// add config value to json obj
void add_json_item( 
    config & cfg, 
    json &jobj, 
    const char * section, 
    const char * key, 
    const char * json_name, 
    int valuetype )     // 0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
{  
    string & value = cfg.getvalue( section, key ) ;
    if( json_name == NULL )
        json_name = key ;
    if( valuetype == 0 )  // string
    {
        if( value.length()>0 ) {
            jobj.addStringItem( json_name, value  );
        }
    }
    else if( valuetype == 1 ) // integer number
    {
        int v ;
        if( sscanf((char*)value, "%d", &v)>0 ) {
            jobj.addNumberItem( json_name, v) ;
        }
    }
    else if( valuetype == 2 ) // generic (double) number
    {
        double v ;
        if( sscanf((char*)value, "%lg", &v)>0 ) {
            jobj.addNumberItem( json_name, v) ;
        }
    }
    else if( valuetype == 3 ) // "on"
    {
        int iv = 0 ;
        sscanf((char *)value, "%d", &iv);
        if( iv ) {
            jobj.addStringItem( json_name, ON_STR );
        }
    }
    else if( valuetype == 4 ) // reversed "on"
    {
        int iv = 0 ;
        sscanf((char *)value, "%d", &iv);
        if( iv == 0 ) {
            jobj.addStringItem( json_name, ON_STR );
        }
    }
}

struct cfg_table {
    const char * section ;
    const char * key ;
    const char * jfield ;
    int   valuetype ;       //0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
};

extern cfg_table system_table[] ;
extern cfg_table camera_table[] ;

int main()
{
    string s ;
    char * p ;
    int l;
    int i;
    config dvrconfig(CFG_FILE);
    struct cfg_table * ctable ;
    string tzi ;
    string value ;
    int ivalue ;
    char buf[100] ;
    FILE * fvalue ;
    FILE * f_id ;
    int sensor_number ;
    char section[80] ;

    // sensor number
    sensor_number=dvrconfig.getvalueint("io", "inputnum");
    if( sensor_number<=0 || sensor_number>32 ) {
        sensor_number=6 ;                   // default
    }

    fvalue = fopen("sensor_number", "w");
    if (fvalue)
    {
        fprintf(fvalue, "%d", sensor_number);
        fclose(fvalue);
    }

#ifdef PWII_APP
    // write mf id
    value = dvrconfig.getvalue("system", "mfid" );
    if( value.length()>0 ) {
        fvalue = fopen("manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose( fvalue );
        }
    }
#endif

#ifdef  TVS_APP
       // write TVS mf id
    value = dvrconfig.getvalue("system", "tvsmfid" );
    if( value.length()>0 ) {
        fvalue = fopen("manufacture_id", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose( fvalue );
        }
        fvalue = fopen("tvs_ivcs_prefix", "w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value+2 );
            fclose( fvalue );
        }
    }
#endif

    // write system_value
    fvalue = fopen( "system_value", "w");
    if( fvalue ) {
        json * j_system = new json(JSON_Object);

        ctable = system_table ;
        while( ctable->jfield !=NULL ) {
            add_json_item( dvrconfig, *j_system, ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
            ctable ++ ;
        }
        
        // dummy password
        j_system->addStringItem("password", "********");

        // dvr_time_zone
        value = dvrconfig.getvalue("system", "timezone");
        if( value.length()>0 ) {
            FILE * ftimezone ;
            ftimezone = fopen("timezone", "w");
            if( ftimezone ) {
                fprintf(ftimezone, "%s", (char *)value );
                fclose( ftimezone );
            }

            ftimezone = fopen("tz_env", "w");
            if( ftimezone ) {

                // time zone enviroment
                tzi=dvrconfig.getvalue( "timezones", (char *)value );
                if( tzi.length()>0 ) {
                    p=strchr(tzi, ' ' ) ;
                    if( p ) {
                        *p=0;
                    }
                    p=strchr(tzi, '\t' ) ;
                    if( p ) {
                        *p=0;
                    }
                    fprintf(ftimezone, "%s", (char *)tzi );
                }
                else {
                    fprintf(ftimezone, "%s", (char *)value );
                }
                fclose( ftimezone );
            }
        }


        char * mfkey = readfile( "/davinci/dvr/mfkey" );
        value = dvrconfig.getvalue("system", "filepassword");
        if( strcmp( mfkey, (char *)value ) == 0 ) {
            j_system->addStringItem( "en_use_default_password", ON_STR );
        }

        j_system->addStringItem( "file_password", "********" );

        ivalue=0 ;
        f_id = fopen( VAR_DIR "/gsensor", "r" );
        if( f_id ) {
            fscanf( f_id, "%d", &ivalue);
            fclose(f_id);
        }
        j_system->addNumberItem( "gforce_available", ivalue?1:0 );

        j_system->addStringItem( "objname", "system_value" );

        l = j_system->encode( value.setbufsize(2000), 2000);
        fwrite( (char *)value, 1, l, fvalue);
        delete j_system ;
        fclose( fvalue );
    }

    // write camera_value
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");
    if( camera_number<1 ) {
        camera_number=1 ;
    }
    fvalue = fopen("camera_number", "w" );
    if( fvalue ) {
        fprintf(fvalue, "%d", camera_number );
        fclose( fvalue );
    }

    for( i=1; i<=camera_number; i++ ) {
        fvalue=fopen(value.printf("camera_value_%d", i), "w");
        if(fvalue){
            fclose( fvalue ) ;
            json * j_camera = new json(JSON_Object);
            sprintf(section, "camera%d", i);

            j_camera->addNumberItem("cameraid", i);
            j_camera->addNumberItem("nextcameraid", i);
            ctable = camera_table ;
            while( ctable->jfield !=NULL ) {
                add_json_item( dvrconfig, *j_camera, ctable->section==NULL?section:ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
                ctable ++ ;
            }

            //          # Bitrate control
            //          bitrateen=1
            //          # Bitrate mode, 0:VBR, 1:CBR
            //          bitratemode=0
            //          bitrate=1200000
            // bit_rate_mode
            if( dvrconfig.getvalueint(section, "bitrateen")) {
                if( dvrconfig.getvalueint(section,"bitratemode") == 0 ) {
                    j_camera->addNumberItem("bit_rate_mode", 1);
                }
                else {
                    j_camera->addNumberItem("bit_rate_mode", 2);
                }
            }
            else {
                j_camera->addNumberItem("bit_rate_mode",0);
            }

            // trigger and osd
            for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
                string s ;
                int  itrig, iosd ;
                iosd  = dvrconfig.getvalueint( section, s.printf("sensorosd%d", ivalue) );
                if( iosd>0 ) {
                    j_camera->addStringItem(s.printf("sensor%d_osd", ivalue),"on");
                }
                itrig = dvrconfig.getvalueint( section, s.printf("trigger%d", ivalue ) );
                if( itrig ) {
                    if( itrig & 1 ) {
                        j_camera->addStringItem(s.printf("sensor%d_trigger_on", ivalue),"on");
                    }
                    if( itrig & 2 ) {
                        j_camera->addStringItem(s.printf("sensor%d_trigger_off", ivalue),"on");
                    }
                    if( itrig & 4 ) {
                        j_camera->addStringItem(s.printf("sensor%d_trigger_turnon", ivalue),"on");
                    }
                    if( itrig & 8 ) {
                        j_camera->addStringItem(s.printf("sensor%d_trigger_turnoff", ivalue),"on");
                    }
                }
            }

            j_camera->addStringItem("objname", value.printf( "camera_value_%d", i));
            j_camera->saveFile( value.printf("camera_value_%d", i));
            delete j_camera ;
        }
    }


#ifdef APP_PWZ8
    // body camera support
    // write bodycam_value

    camera_number = dvrconfig.getvalueint("system", "totalbodycam") ;
    if( camera_number<=0 ) {
        camera_number=1 ;
    }
    for( i=1; i<=camera_number; i++ ) {
        sprintf( section, "bodycam%d", i);
        sprintf( buf, "bodycam_value_%d", i);
        fvalue=fopen(buf, "w");
        if(fvalue) {
            // JSON head
            fprintf(fvalue, "{" );

            // bodycam id
            fprintf(fvalue, "\"bodycamid\":\"%d\",", i );
            fprintf(fvalue, "\"nextbodycamid\":\"%d\",", i );

            ivalue = dvrconfig.getvalueint( section, "enable");
            if( ivalue ) {
                fprintf( fvalue, "\"bcam_enable\":\"on\"," );
            }

            // bodycam ip address
            value = dvrconfig.getvalue(section, "ip") ;
            if( value.length()>0 ) {
                fprintf( fvalue, "\"bcam_ip\":\"%s\",", (char *)value );
            }

            // to trigger dvr by bodycam
            ivalue = dvrconfig.getvalueint( section, "bodycam_trigger");
            if( ivalue ) {
                fprintf( fvalue, "\"bcam_trigger\":\"on\"," );
            }

            // trigger by dvr
            ivalue = dvrconfig.getvalueint( section, "dvr_trigger");
            if( ivalue ) {
                fprintf( fvalue, "\"bcam_dvrtrigger\":\"on\"," );
            }
            fprintf(fvalue, "\"objname\":\"bodycam_value\" }" );
            fclose( fvalue );
        }
    }

#endif      // BODY CAMERA

    // write sensor_value
    fvalue = fopen("sensor_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // sensor number
        fprintf(fvalue, "\"sensor_number\":\"%d\",", sensor_number );

        for( i=1; i<=sensor_number; i++ ) {
            sprintf( section, "sensor%d", i );
            value = dvrconfig.getvalue(section, "name") ;
            // write sensor value
            if( value.length()>0 ) {
                fprintf( fvalue, "\"%s_name\":\"%s\",", section, (char *)value );
            }

            // inverted value
            fprintf( fvalue, "\"bool_%s_inverted\":\"on\",", section );
            if( dvrconfig.getvalueint(section, "inverted") ) {
                fprintf( fvalue, "\"%s_inverted\":\"on\",", section );
            }
            else {
                fprintf( fvalue, "\"%s_inverted\":\"off\",", section );
            }

            // event marker
            if( dvrconfig.getvalueint(section, "eventmarker") ) {
                fprintf( fvalue, "\"%s_eventmarker\":\"on\",", section );
            }


        }

        if(dvrconfig.getvalueint("io","sensor_powercontrol")){
            fprintf(fvalue, "\"sensor_powercontrol\":\"on\",");
        }

        fprintf(fvalue, "\"objname\":\"sensor_value\" }" );
        fclose( fvalue );
    }

    // write network_value
    fvalue = fopen("network_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

#ifndef APP_PWZ5
        // eth_ip
        value = dvrconfig.getvalue("network","eth_ip");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_ip\":\"%s\",", (char *)value );
        }

        // eth_mask
        value = dvrconfig.getvalue("network","eth_mask");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_mask\":\"%s\",", (char *)value );
        }

        // eth_bcast
        value = dvrconfig.getvalue("network","eth_bcast");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_bcast\":\"%s\",", (char *)value );
        }

        // gateway
        value = dvrconfig.getvalue("network","gateway");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gateway_1\":\"%s\",", (char *)value );
        }

        // wifi_ip
        value = dvrconfig.getvalue("network","wifi_ip");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_ip\":\"%s\",", (char *)value );
        }

        // enable dhcp client on wifi interface
        ivalue = dvrconfig.getvalueint("network", "wifi_dhcp" );
        if( ivalue>0 ) {
            fprintf(fvalue, "\"wifi_dhcp\":\"%s\",", "on" );
        }

        // wifi_mask
        value = dvrconfig.getvalue("network","wifi_mask");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_mask\":\"%s\",", (char *)value );
        }

        // wifi_bcast
        value = dvrconfig.getvalue("network","wifi_bcast");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_bcast\":\"%s\",", (char *)value );
        }

        // wifi_essid
        value = dvrconfig.getvalue("network","wifi_essid");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_essid\":\"%s\",", (char *)value );
        }

        // wifi_key
        value = dvrconfig.getvalue("network","wifi_key");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_key\":\"%s\",", (char *)value );
        }

        // wifi enc type.
        //        0 : Disable (no enc)
        //        1 : WEP open
        //        2 : WEP shared
        //        3 : WEP auto
        //        4 : WPA Personal TKIP
        //        5 : WPA Personal AES
        //        6 : WPA2 Personal TKIP
        //        7 : WPA2 Personal AES
        value = dvrconfig.getvalue("network","wifi_enc");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_enc\":\"%s\",", (char *)value );
        }

        // smartserver
        value = dvrconfig.getvalue( "network", "smartserver" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"smartserver\":\"%s\",", (char *)value );
        }

#else   // PWZ5 (include PWZ6,PWZ8)

        // eth_ip
        value = dvrconfig.getvalue( "network", "eth_ip" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_ip\":\"%s\",", (char *)value );
        }

        // eth_mask
        value = dvrconfig.getvalue( "network", "eth_mask" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"eth_mask\":\"%s\",", (char *)value );
        }

        // enable dhcp client on eth0
        ivalue = dvrconfig.getvalueint("network", "eth_dhcp");
        if( ivalue ) {
            fprintf(fvalue, "\"eth_dhcp\":\"on\"," );
        }

        // wifi mode , 0: disable, 1:station , 2: AP
        ivalue = dvrconfig.getvalueint("network", "wifi_mode" );
        fprintf(fvalue, "\"wifi_mode\":\"%d\",", ivalue);

        // ap ssid
        value = dvrconfig.getvalue( "network", "ap_ssid" );
        fprintf(fvalue, "\"ap_ssid\":\"%s\",", (char *)value );

        // ap key
        value = dvrconfig.getvalue( "network", "ap_key" );
        fprintf(fvalue, "\"ap_key\":\"%s\",", (char *)value );

        // ap channel
        ivalue = dvrconfig.getvalueint( "network", "ap_channel" );
        fprintf(fvalue, "\"ap_channel\":\"%d\",", ivalue );

        // wifi_ip
        value = dvrconfig.getvalue( "network", "wifi_ip" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_ip\":\"%s\",", (char *)value );
        }

        // wifi_mask
        value = dvrconfig.getvalue( "network", "wifi_mask" );
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_mask\":\"%s\",", (char *)value );
        }

        // wifi essid
        value = dvrconfig.getvalue("network", "wifi_essid");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_essid\":\"%s\",", (char *)value );
        }

        // wifi_key
        value = dvrconfig.getvalue("network","wifi_key");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"wifi_key\":\"%s\",", (char *)value );
        }

        // smart upload server
        value = dvrconfig.getvalue("network","smartserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"smartserver\":\"%s\",", (char *)value );
        }

        // gateway
        value = dvrconfig.getvalue("network","gateway");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"gateway_1\":\"%s\",", (char *)value );
        }

        // wifi authentication/encryption type
        ivalue = dvrconfig.getvalueint("network", "wifi_enc");
        if( ivalue<0 || ivalue>7 ) {
          ivalue=0 ;                   // default
        }
        fprintf(fvalue, "\"wifi_enc\":\"%d\",", ivalue );

        // enable dhcp client on wifi (wlan0)
        ivalue = dvrconfig.getvalueint("network", "wifi_dhcp");
        if( ivalue ) {
            fprintf(fvalue, "\"wifi_dhcp\":\"on\"," );
        }

        // internet access
        ivalue = dvrconfig.getvalueint("system", "nointernetaccess");
        if( ivalue == 0 ) {
            fprintf(fvalue, "\"internetaccess\":\"on\"," );
        }

        // internet key
        value = dvrconfig.getvalue("system", "internetkey") ;
        fprintf(fvalue, "\"internetkey\":\"%s\",", (char *)value );

#endif

        // end of network value
        fprintf(fvalue, "\"objname\":\"network_value\" }" );
        fclose( fvalue );
    }

#ifdef POWERCYCLETEST
    // write cycletest_value

    fvalue = fopen("cycletest_value", "w");
    if( fvalue ) {

        // JSON head
        fprintf(fvalue, "{" );

        // cycletest
        ivalue = dvrconfig.getvalueint("debug", "cycletest");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_cycletest\":\"on\"," );
            fprintf(fvalue, "\"cycletest\":\"on\"," );
        }

        // cyclefile
        value = dvrconfig.getvalue("debug", "cyclefile");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cyclefile\":\"%s\",", (char *)value );
        }

        // cycleserver
        value = dvrconfig.getvalue("debug", "cycleserver");
        if( value.length()>0 ) {
            fprintf(fvalue, "\"cycleserver\":\"%s\",", (char *)value );
        }

        // norecord
        ivalue = dvrconfig.getvalueint("system", "norecord");
        if( ivalue>0 ) {
            fprintf(fvalue, "\"bool_norecord\":\"on\"," );
            fprintf(fvalue, "\"norecord\":\"on\"," );
        }

        fprintf(fvalue, "\"objname\":\"cycletest_value\" }" );
        fclose( fvalue );

    }
#endif

    // write tz_option
    fvalue = fopen("tz_option", "w");
    if (fvalue)
    {
        // initialize enumkey
        int line = dvrconfig.findsection("timezones");
        while ((p = dvrconfig.enumkey(line)) != NULL)
        {
            fprintf(fvalue, "<option value=\"%s\">%s ", p, p);
            s = p;
            tzi = dvrconfig.getvalue("timezones", (char *)s);
            if (tzi.length() > 0)
            {
                p = tzi;
                while (*p > ' ' )
                {
                    p++;
                }
                if (strlen(p) > 1)
                {
                    fprintf(fvalue, "-%s", p);
                }
            }
            fprintf(fvalue, "</option>\n");
        }
        fclose(fvalue);
    }

    // write led_number
    ivalue=dvrconfig.getvalueint("io", "outputnum");
    if( ivalue<=0 || ivalue>32 ) {
        ivalue=4 ;
    }
    fvalue = fopen("led_number","w");
    if( fvalue ) {
        fprintf(fvalue, "%d", ivalue );
        fclose(fvalue);
    }

    // link firmware version
    symlink( APP_DIR"/firmwareid", "./firmware_version");

    // write dvrsvr port number, used by eagle_setup
    value = dvrconfig.getvalue("network", "port") ;
    if( value.length()>0 ){
        fvalue = fopen("dvrsvr_port","w");
        if( fvalue ) {
            fprintf(fvalue, "%s", (char *)value );
            fclose(fvalue);
        }
    }

    remove("savedstat");

    return 0;
}
