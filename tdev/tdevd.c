#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <asm/types.h>
#include <linux/netlink.h>

/* environment buffer, the kernel's size in lib/kobject_uevent.c should fit in */
#define HOTPLUG_BUFFER_SIZE     8192
#define HOTPLUG_NUM_ENVP        16

char * pidfile = "/var/dvr/tdevd.pid" ;
static char buffer[HOTPLUG_BUFFER_SIZE];

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_nl snl;
    int retval;
    pid_t childid ;
    FILE * pidf ;

    if (getuid() != 0) {
        printf("need to be root, exit\n");
        exit(1);
    }

    if( argc<2 ) {
        printf("tdevd hotplug\n");
        exit(1);
    }

    memset(&snl, 0, sizeof(snl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (sock == -1) {
        printf("error getting socket, exit\n");
        exit(1);
    }

    retval = bind(sock, (struct sockaddr *) &snl,
              sizeof(struct sockaddr_nl));
    if (retval < 0) {
        printf("bind failed, exit\n");
        goto exit;
    }

    pidf = fopen( pidfile, "w" );
    if( pidf ) {
        fprintf(pidf, "%d", (int)getpid());
        fclose(pidf);
    }

    while (1) {
        char *action;
        char *devpath;
        char *envp[HOTPLUG_NUM_ENVP];
        int i;
        char *pos;
        size_t bufpos;
        size_t buflen;

        buflen = recv(sock, &buffer, sizeof(buffer)-1, 0);
        if (buflen <=  0) {
            printf("error receiving message\n");
            continue;
        }

        // add nul terminate char
        buffer[buflen] = '\0';

        //printf("hotplug:%s\n",buffer);
        // save start of payload
        bufpos = strlen(buffer) + 1;

        // action string
        action = buffer;
        pos = strchr(buffer, '@');
        if (!pos)
            continue;
        *pos = '\0';

        /* sysfs path */
        devpath = &pos[1];

        /* hotplug events have the environment attached - reconstruct envp[] */
        for (i = 0; (bufpos < buflen) && (i < HOTPLUG_NUM_ENVP-1); i++) {
            envp[i] = &buffer[bufpos] ;
            bufpos += strlen( envp[i] )+1;
        }

        // last env
        envp[i] = NULL;
        //printf("devpath:%s\n",devpath);

        if( (childid=fork())==0 ) {
            execle( argv[1], argv[1], action, devpath, NULL, envp );
            // will not return
            exit(1) ;
        }
        else {
            waitpid(childid, NULL, 0);
        }
    }

    // delete pid file
    unlink(pidfile);

exit:
    close(sock);
    exit(1);
}
