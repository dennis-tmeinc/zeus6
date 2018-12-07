#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <dirent.h>

#include "../dvrsvr/dir.h"

char * mountcmd ;
int tdev_mount(const char * path)
{
    dir dfind(path);
    pid_t childid ;
    int devfound=0 ;
    int mounted=0 ;     // partition mounte

    while( dfind.find() ) {
        if( dfind.isdir() ) {
            if( strncmp(dfind.filename(), "sd", 2)==0 || strncmp(dfind.filename(), "mmc", 3)==0 ) {
                if( tdev_mount( dfind.pathname() ) ) {
                    mounted = 1 ;
                }
            }
        }
    }

    if( mounted == 0 ) {
        dfind.rewind();
        while( dfind.find("dev") ) {
            if( dfind.isfile() && strcmp( dfind.filename(), "dev" )==0 ) {
                devfound=1 ;
            }
        }
    }

    if( devfound ) {
        char cmdbuf[1024] ;
        sprintf( cmdbuf, "%s add %s", mountcmd, path+4 ) ;
        system( cmdbuf ) ;
        return 1 ;
    }
    return mounted ;

}

int main(int argc, char *argv[])
{
    if( argc>1 ) {
        mountcmd = argv[1] ;
    }
    else {
        mountcmd = (char *)"tdevhotplug" ;
    }
    tdev_mount("/sys/block");
    return 0 ;
}


