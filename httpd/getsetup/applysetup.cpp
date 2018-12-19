
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

/*
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
*/

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
        else if (item->isString())
        {
            cfg.setvalue(section, key, item->getString());
        }
        else if (item->isNumber())
        {
            cfg.setvalue(section, key, v.printf("%lg", item->getNumber()));
        }
    }
    else if (valuetype == 1) // integer number
    {
        if (item == NULL)
            return;
        else if (item->isNumber())
        {
            cfg.setvalueint(section, key, item->getNumber());
        }
        else if (item->isString())
        {
            cfg.setvalue(section, key, item->getString());
        }
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
        bool_item = jobj.getItem(v.printf("bool_%s", json_name));
        if (bool_item)
        {
            cfg.setvalueint(section, key, item == NULL ? 0 : 1);
        }
    }
    else if (valuetype == 4) // reversed "on"
    {
        bool_item = jobj.getItem(v.printf("bool_%s", json_name));
        if (bool_item)
        {
            cfg.setvalueint(section, key, item == NULL ? 1 : 0);
        }
    }
}

struct cfg_table
{
    const char *section;
    const char *key;
    const char *jfield;
    int valuetype; //0: string, 1: integer, 2: float, 3, "on" if trun, 4: on if false
};

extern cfg_table system_table[] ;
extern cfg_table camera_table[] ;

int main()
{
    int i, r;
    int ivalue ;
    char * v ;
    struct cfg_table * ctable ;
    FILE * fvalue ;
    char section[80] ;
    int  sensor_number = 31;
    config dvrconfig(CFG_FILE);
    string value ;

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
#endif  // PWII_APP
        
        delete j_system ;
    }


    // moved sensor setting ahead of camera setting , because camera setting use "sensor_number"

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
        v = getsetvalue("sensor_powercontrol");
        if (v)
        {
            dvrconfig.setvalueint("io", "sensor_powercontrol", 1);
        }
        else
        {
            dvrconfig.setvalueint("io", "sensor_powercontrol", 0);
        }
#endif

    }

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
