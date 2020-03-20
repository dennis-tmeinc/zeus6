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
char dvrconfigfile[] = CFG_FILE;

// return
//        0 : failed
//        1 : success
int appinit()
{
    if( dio_mmap() == NULL ) {
        // no io map
        return 0;
    }
    if (p_dio_mmap->iopid > 0)
    {
        return 1;
    }
    else
    {
        dio_munmap();
        return 0;
    }
}

// app finish, clean up
void appfinish()
{
    dio_munmap();
}

int main(int argc, char *argv[])
{
    int buz_times = 1;
    int buz_interval_on = 1000;
    int buz_interval_off = 1000;

    if (argc < 2)
    {
        printf("Usage: buzzer <times> [interval_on_ms] [interval_off_ms]\n");
        return 1;
    }
    buz_times = atoi(argv[1]);
    if (buz_times <= 0 || buz_times > 50)
    {
        buz_times = 1;
    }

    if (argc > 2)
    {
        buz_interval_on = atoi(argv[2]);
        if (buz_interval_on < 10 || buz_interval_on > 10000)
        {
            buz_interval_on = 1000;
        }
        buz_interval_off = buz_interval_on;
    }

    if (argc > 3)
    {
        buz_interval_off = atoi(argv[3]);
        if (buz_interval_off < 10 || buz_interval_off > 10000)
        {
            buz_interval_off = 1000;
        }
    }

    if (appinit() == 0)
    {
        return 1;
    }

    while (buz_times--)
    {
        p_dio_mmap->outputmap |= 0x100; // buzzer on
        usleep(buz_interval_on * 1000);
        p_dio_mmap->outputmap &= (~0x100); // buzzer off
        usleep(buz_interval_off * 1000);
    }

    appfinish();
    return 0;
}
