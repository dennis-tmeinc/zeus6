#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "../cfg.h"

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/config.h"
#include "diomap.h"

// unsigned int outputmap ;	// output pin map cache
char dvrconfigfile[] = CFG_FILE ;

// return 
//        0 : failed
//        1 : success
int appinit()
{
    if(dio_mmap()==NULL)
        return 0;
	return (p_dio_mmap->iopid > 0 );
}

// app finish, clean up
void appfinish()
{
    // clean up shared memory
    dio_munmap();
}

int main(int argc, char * argv[])
{
	unsigned int panelled;
    
	if( argc<2 ) {
		printf("Usage: panelled [panel_led_map]\n");
		printf("       led map: bit0=USB FLASH, bit1=Error, bit2=Video lost Led\n");
		return 1 ;
	}
	
	panelled=0 ;
	sscanf(argv[1], "%i", &panelled);
	
	if( appinit()==0 ) {
		return 1;
	}

	p_dio_mmap->panel_led = panelled & 0xffff ;		
        if(panelled)
             p_dio_mmap->mcu_cmd=10;
        else
             p_dio_mmap->mcu_cmd=11;
	appfinish();
    return 0;
}

