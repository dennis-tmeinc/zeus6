

#include <stdio.h>

#include "cfg.h"

#include "json/json.h"

#include "dvrsvr/crypt.h"
#include "dvrsvr/genclass.h"
#include "dvrsvr/config.h"

static const char ON_STR[]="on";

static char * trim( char * s )
{
    while( *s > 0 && *s <= ' ' ) {
        s++ ;
    }
    int l = strlen(s);
    while( l>0 ) {
        if( s[l-1] <= ' ' ) {
            s[--l] = 0 ;
        }
        else
            break ;
    }
    return s ;
}

static char * readfile( char * filename, string &buf )
{
    FILE * f ;
    f = fopen( filename, "r" );
    if( f ) {
        char * b = buf.setbufsize(4096);
        int r=fread( b, 1, 4000, f );
        fclose( f );
        if( r>0 ) {
            b[r]=0 ;
            return trim(b) ;
        }
    }
    return NULL ;
}

#ifndef __STRUCT_CFG_TABLE__
#define __STRUCT_CFG_TABLE__
struct cfg_table {
    const char * section ;
    const char * key ;
    const char * jfield ;
    int   valuetype ;       //0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
};
#endif  // __STRUCT_CFG_TABLE__

extern cfg_table system_table[] ;
extern cfg_table camera_table[] ;
extern cfg_table bodycamera_table[];
extern cfg_table network_table[];

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
        jobj.addStringItem( json_name, value  );
    }
    else if( valuetype == 1 ) // integer number
    {
        int v ;
        if( sscanf((char*)value, "%d", &v)>0 ) {
            jobj.addNumberItem( json_name, v) ;
        }
        else {
            jobj.addNumberItem( json_name, 0) ;
        }
    }
    else if( valuetype == 2 ) // generic (double) number
    {
        double v ;
        if( sscanf((char*)value, "%lg", &v)>0 ) {
            jobj.addNumberItem( json_name, v) ;
        }
        else {
            jobj.addNumberItem( json_name, 0.0) ;
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
        json * j_bdcam = new json(JSON_Object);
        // bodycam id
        j_bdcam->addNumberItem("bodycamid", i);
        j_bdcam->addNumberItem("nextbodycamid", i);

        sprintf( section, "bodycam%d", i);
        ctable = bodycamera_table ;
        while( ctable->jfield !=NULL ) {
            add_json_item( dvrconfig, *j_bdcam, ctable->section==NULL?section:ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
            ctable ++ ;
        }

        value.printf("bodycam_value_%d", i);
        j_bdcam->addStringItem("objname", value);
        j_bdcam->saveFile(value);
        delete j_bdcam ;
    }

#endif      // BODY CAMERA

    // write sensor_value
    json * j_sensor = new json(JSON_Object);
    j_sensor->addNumberItem("sensor_number",sensor_number);
    add_json_item( dvrconfig, *j_sensor, "io","sensor_powercontrol", "sensor_powercontrol", 3 );

    for( i=1; i<=sensor_number; i++ ) {
        sprintf( section, "sensor%d", i );

        // write sensor value
        add_json_item( dvrconfig, *j_sensor, section, "name", value.printf("%s_name", section), 0 );

        // inverted value
        if( dvrconfig.getvalueint(section, "inverted") ) {
            j_sensor->addStringItem(value.printf("%s_inverted", section), "on");            
        }
        else {
            j_sensor->addStringItem(value.printf("%s_inverted", section), "off");            
        }

        // event marker
        add_json_item( dvrconfig, *j_sensor, section, "eventmarker", value.printf("%s_eventmarker", section), 3 );
    }

    j_sensor->addStringItem("objname",  "sensor_value");
    j_sensor->saveFile( "sensor_value");
    delete j_sensor ;

    // write network_value

    // write network_value
    json * j_net = new json(JSON_Object);

    ctable = network_table ;
    while( ctable->jfield !=NULL ) {
        add_json_item( dvrconfig, *j_net, ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
        ctable ++ ;
    }

    j_net->saveFile( "network_value");
    delete j_net ;
    // end of network_value

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
