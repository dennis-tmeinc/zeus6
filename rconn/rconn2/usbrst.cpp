#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/usbdevice_fs.h>

int usb_resetdev(int bus, int dev)
{
    char usbf[64];
    int fd;
    int rc;
    
    sprintf(usbf, "/dev/bus/usb/%03d/%03d", bus, dev );

    fd = open(usbf, O_RDWR);
    if (fd < 0) {
        perror("Error opening output file");
        return 1;
    }

    rc = ioctl(fd, USBDEVFS_RESET, 0);
    if (rc < 0) {
        perror("Error in ioctl");
    }
    else {
		printf("Reset successful\n");
	}

    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int bus = 1, dev = 1;

    if (argc < 2) {
        printf("Usage:\n\t%s busnumber devnumber\n", argv[0]);
        return 0;
    }
    
    if( argc>1 ) {
		bus=atoi(argv[1]);
	}
    if( argc>2 ) {
		dev=atoi(argv[2]);
	}
	
	return usb_resetdev(bus, dev);
}
