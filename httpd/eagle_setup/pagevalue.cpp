#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#include <cfg.h>

#include "json/json.h"

int decode(const char * in, char * out, int osize );
char * getquery( const char * qname );

int hasfile( char * fn )
{
    struct stat sb ;
    if( stat( fn, &sb ) == 0 ) {
        return (sb.st_size > 5 );
    }
    return 0 ;
}

int savequery( char * valuefile )
{
    char qvalue[1024] ;
    char * qp ;
    char * query ;
    int x ;
    int item=0;

    // JSON obj
    json * jq = new json(JSON_Object) ;
    
    query = getenv("QUERY_STRING");
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
             qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    jq->addStringItem(qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' )
            break;
        else {
            query+=x+1;
        }
    }
    query = getenv("POST_STRING" );
    while( query && *query ) {
        x = decode( query, qvalue, sizeof(qvalue) ) ;
        if( x>1 ) {
            qp=strchr(qvalue,'=');
            if( qp ) {
                *qp=0 ;
                qp++;
                if( strcmp(qvalue, "page")!=0 ) {
                    jq->addStringItem(qvalue,  qp );
                    item++;
                }
            }
        }
        if( query[x]=='\0' )
            break;
        else {
            query+=x+1;
        }
    }
    jq->addStringItem("eobj","");
    jq->saveFile(valuefile);
    delete jq ;
    return item ;
}

void system_savevalue()
{
    char * v ;

    savequery("system_value");

    // change camera number
    v = getquery( "totalcamera" );
    if( v!=NULL ) {
        FILE * f = fopen( "camera_number", "w" ) ;
        if( f ) {
            fprintf(f,"%s", v );
            fclose( f );
        }
    }
}

void camera_savevalue()
{
    char * v ;
    char fcam_name[100] ;

    v = getquery( "cameraid" );
    if( v== NULL )
        return ;

    sprintf( fcam_name, "camera_value_%s", v );
    savequery( fcam_name );
}

#ifdef APP_PWZ8
void bodycam_savevalue()
{
    char * v ;
    char fcam_name[100] ;

    v = getquery( "bodycamid" );
    if( v== NULL )
        return ;

    sprintf( fcam_name, "bodycam_value_%s", v );
    savequery( fcam_name );
}
#endif

void sensor_savevalue()
{
    savequery("sensor_value");
}

void network_savevalue()
{
    savequery( "network_value" );
}

#ifdef POWERCYCLETEST
void cycletest_savevalue()
{
    savequery( "cycletest_value" );
}
#endif

int main()
{
    // check originated page
    char * page = getquery("page");
    if( page ) {
        if( strcmp(page, "system" )==0 ) {
            system_savevalue();
        }
        else if( strcmp( page, "camera" )==0 ) {
            camera_savevalue();
        }
#ifdef APP_PWZ8
        else if( strcmp( page, "bodycam" )==0 ) {
            bodycam_savevalue();
       }
#endif
        else if( strcmp( page, "sensor" )==0 ) {
            sensor_savevalue();
        }
        else if( strcmp( page, "network" )==0 ) {
            network_savevalue();
       }
#ifdef POWERCYCLETEST
        else if( strcmp( page, "cycletest" )==0 ) {
            cycletest_savevalue();
       }
#endif
        else {
            savequery(page);
        }
    }
    return 0;
}
