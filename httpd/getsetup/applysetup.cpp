
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "cfg.h"
#include "dvrsvr/crypt.h"
#include "dvrsvr/genclass.h"
#include "dvrsvr/cfg.h"

#include "json/json.h"

char tzfile[] = "tz_option" ;

pid_t  dvrpid=0 ;

void dvrsvr_down()
{
	dvrpid = 0;
	FILE *fdvrpid = fopen(VAR_DIR "/dvrsvr.pid", "r");
	if (fdvrpid)
	{
		fscanf(fdvrpid, "%d", &dvrpid);
		fclose(fdvrpid);
	}
	if (dvrpid > 0)
	{
		kill(dvrpid, SIGUSR1);
	}
}

void dvrsvr_up()
{
    if( dvrpid>0 ) {
        kill( dvrpid, SIGUSR2 );
    }
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

void setadminpassword( const char * password )
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

void run( const char * cmd, char * argu, int background )
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

// set json item to configure file
void set_json_item(
    config &cfg,
    json &jobj,
    const char *section,
    const char *key,
    const char *json_name,
    int valuetype) // 0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
{
    string v;
    json *bool_item;
    json *item = jobj.getItem(json_name); // get item with name (for objects)
    if (valuetype == 0)                   // string
    {
        if (item == NULL)
            return;
        cfg.setvalue(section, key, item->getString());
    }
    else if (valuetype == 1) // integer number
    {
        if (item == NULL)
            return;
        cfg.setvalueint(section, key, item->getNumber());
    }
    else if (valuetype == 2) // generic (double) number
    {
        if (item == NULL)
            return;
        else if (item->isNumber())
        {
            cfg.setvalue(section, key, v.printf("%lg", item->getNumber()));
        }
        else if (item->isString())
        {
            cfg.setvalue(section, key, item->getString());
        }
    }
    else if (valuetype == 3) // "on"
    {
        if( item != NULL ) {
            cfg.setvalueint(section, key, 1);
        }
        else {
            bool_item = jobj.getItem(v.printf("bool_%s", json_name));
            if (bool_item)
            {
                cfg.setvalueint(section, key, 0);
            }
        }
    }
    else if (valuetype == 4) // reversed "on"
    {
        if( item != NULL ) {
            cfg.setvalueint(section, key, 0);
        }
        else {
            bool_item = jobj.getItem(v.printf("bool_%s", json_name));
            if (bool_item)
            {
                cfg.setvalueint(section, key, 1);
            }
        }
    }
}

int main()
{
    int i, r;
    int ivalue ;
    string value ;
    struct cfg_table * ctable ;
    FILE * fvalue ;
    char section[80] ;
    int  sensor_number = 31;
    config dvrconfig(CFG_FILE);

    // suspend dvrsvr
    dvrsvr_down();

    // set system_value
    fvalue = fopen( "system_value", "r");
    if( fvalue ) {
        fclose(fvalue);

        json * j_system = new json(JSON_Object);
        j_system->loadFile("system_value");

        ctable = system_table ;
        while( ctable->jfield ) {
            set_json_item( dvrconfig, *j_system, ctable->section, ctable->key, ctable->jfield, ctable->valuetype);
            ctable++ ;
        }

#ifdef TVS_APP
        // make medallion as host name also
        set_json_item( dvrconfig, *j_system, "system", "hostname", "tvs_medallion", 0);
#endif

#ifdef  PWII_APP
        set_json_item( dvrconfig, *j_system, "system", "hostname", "vehicleid", 0);

        // adminpassword
        json * it = j_system->getItem("adminpassword");
        if( it && it->isString() && strcmp(it->getString(), "*****" )!=0  ) {
            setadminpassword(it->getString());    
        }
#endif  // PWII_APP
        
        delete j_system ;
    }


    // moved sensor setting ahead of camera setting , because camera setting use "sensor_number"

    // write sensor_value
    json * j_sensor = new json(JSON_Object);
    j_sensor->loadFile("sensor_value");

    sensor_number=6 ;
    json * j_sn = j_sensor->getItem("sensor_number");
    if( j_sn ) {
        sensor_number=j_sn->getInt();
    }
    dvrconfig.setvalueint( "io", "inputnum", sensor_number );
    set_json_item( dvrconfig, *j_sensor, "io","sensor_powercontrol", "sensor_powercontrol", 3 );

    for( i=1; i<=sensor_number; i++ ) {
        sprintf( section, "sensor%d", i );

        // write sensor name
        set_json_item( dvrconfig, *j_sensor, section, "name", value.printf("%s_name", section), 0 );

        // inverted value
        j_sn = j_sensor->getItem(value.printf("%s_inverted", section));
        if( j_sn && strcmp(j_sn->getString(),"on")==0 ) {
            dvrconfig.setvalueint(section, "inverted", 1 );
        }
        else {
            dvrconfig.setvalueint(section, "inverted", 0 );
        }
        // event marker
        set_json_item( dvrconfig, *j_sensor, section, "eventmarker", value.printf("%s_eventmarker", section), 3 );
    }
    delete j_sensor ;

    // this is new camera nubmer settings
    int camera_number = dvrconfig.getvalueint("system", "totalcamera");

    for( i=1; i<=camera_number; i++ ) {
        string s ;
        sprintf(section, "camera%d", i);

        json * j_camera = new json(JSON_Object);
        j_camera->loadFile(s.printf("camera_value_%d", i));

        ctable = camera_table ;
        while( ctable->jfield ) {
            set_json_item( dvrconfig, *j_camera, ctable->section==NULL?section:ctable->section, ctable->key, ctable->jfield, ctable->valuetype);
            ctable++ ;
        }

        // bit_rate_mode
        json * item = j_camera->getItem( "bit_rate_mode" );
        if( item && item->isString() ) {
            char v = *item->getString();
            if( v == '0' ) {
                dvrconfig.setvalueint(section,"bitrateen", 0);
            }
            else if( v == '1' ) {
                dvrconfig.setvalueint(section,"bitrateen", 1);
                dvrconfig.setvalueint(section,"bitratemode", 0);
            }
            else {
                dvrconfig.setvalueint(section,"bitrateen", 1);
                dvrconfig.setvalueint(section,"bitratemode", 1);
            }
        }

        // trigger and osd
        for( ivalue=1; ivalue<=sensor_number; ivalue++ ) {
            int  tr = 0;

            // osd
            if( j_camera->getItem( s.printf("sensor%d_osd", ivalue) ) ) {
                dvrconfig.setvalueint(section, s.printf("sensorosd%d", ivalue), 1);
            }
            else {
                dvrconfig.setvalueint(section, s.printf("sensorosd%d", ivalue), 0);
            }

            // trigger selections (PWII / TVS )
            if( j_camera->getItem( s.printf( "sensor%d_trigger_on", ivalue) ) ) {
                tr|=1 ;
            }
            if( j_camera->getItem( s.printf( "sensor%d_trigger_off", ivalue) ) ) {
                tr|=2 ;
            }
            if( j_camera->getItem( s.printf( "sensor%d_trigger_turnon", ivalue) ) ) {
                tr|=4 ;
            }
            if( j_camera->getItem( s.printf( "sensor%d_trigger_turnoff", ivalue) ) ) {
                tr|=8 ;
            }

            dvrconfig.setvalueint(section, s.printf("trigger%d", ivalue), tr);

        }

        delete j_camera ;
    }
    

#ifdef APP_PWZ8
    // bodycam values
    camera_number = dvrconfig.getvalueint("system", "totalbodycam");
    for( i=1; i<=camera_number; i++ ) {
        json * j_bdcam = new json(JSON_Object);
        j_bdcam->loadFile(value.printf("bodycam_value_%d", i));

        sprintf( section, "bodycam%d", i);
        ctable = bodycamera_table ;
        while( ctable->jfield !=NULL ) {
            set_json_item( dvrconfig, *j_bdcam, ctable->section==NULL?section:ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
            ctable ++ ;
        }
        delete j_bdcam ;
    }
#endif

    // write network_value
    json * j_net = new json(JSON_Object);
    j_net->loadFile( "network_value");

    ctable = network_table ;
    while( ctable->jfield !=NULL ) {
        set_json_item( dvrconfig, *j_net, ctable->section, ctable->key, ctable->jfield, ctable->valuetype );
        ctable ++ ;
    }

    string ip ;
    string mask ;
    //eth broadcast
    ip = dvrconfig.getvalue( "network", "eth_ip" );
    mask = dvrconfig.getvalue( "network", "eth_mask" );
    dvrconfig.setvalue( "network", "eth_broadcast", getbroadcastaddr( ip, mask ) );

    //wifi broadcast
    ip = dvrconfig.getvalue( "network", "wifi_ip" );
    mask = dvrconfig.getvalue( "network", "wifi_mask" );
    dvrconfig.setvalue( "network", "wifi_broadcast", getbroadcastaddr( ip, mask ) );

    delete j_net ;
    // end of network_value

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
