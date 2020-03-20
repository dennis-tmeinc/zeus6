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

// return
//        0 : failed
//        1 : success
int appinit()
{
    if( dio_mmap() == NULL ) {
        return 0;
    }
    return (p_dio_mmap->iopid > 0);
}

// app finish, clean up
void appfinish()
{
    // clean up shared memory
    dio_munmap();
}

int main(int argc, char *argv[])
{
    unsigned int devicepower;

    if (appinit() == 0)
    {
        return 1;
    }

    if (argc >= 2)
    {
        devicepower = 0;
        sscanf(argv[1], "%i", &devicepower);
        p_dio_mmap->devicepower = devicepower;
    }

    printf("Device Power: 0x%08x \n", p_dio_mmap->devicepower);

    appfinish();
    return 0;
}
