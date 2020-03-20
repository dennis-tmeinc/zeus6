#include <arpa/inet.h>
#include <errno.h>
#include <linux/rtc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>

#include "cfg.h"
#include "dvrsvr/config.h"
#include "dvrsvr/genclass.h"
#include "ioprocess/diomap.h"

#include "net/net.h"

#include "bodycam.h"

int g_log = 1;
int g_runtime;
int g_run;
static int bodycamNum;

// return runtime in milli seconds
int runtime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int s_maxwaitms;
void setMaxWait(int ms)
{
    if (ms < s_maxwaitms)
        s_maxwaitms = ms;
}

// get time string as AMBR spec ( 3.2.15 CAMERA_CLOCK), ex "2017-12-24 23:59:59"
// timebuf must be at lease 20 bytes
time_t getTime(char* timebuf)
{
    time_t t;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    t = (time_t)current_time.tv_sec;
    if (timebuf != NULL) {
        struct tm stm;
        localtime_r(&t, &stm);
        sprintf(timebuf, "%04d-%02d-%02d %02d:%02d:%02d",
            stm.tm_year + 1900,
            stm.tm_mon + 1,
            stm.tm_mday,
            stm.tm_hour,
            stm.tm_min,
            stm.tm_sec);
    }
    return t;
}

// local socket to wait for local trigger (send from ioprocess)
static int local_socket = -1;
static int local_port;
static struct pollfd* local_sfd = NULL;

void local_init()
{
    // init local listening socket
    local_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (local_socket > 0) {
        net_bind(local_socket, BODYCAM_PORT);
    }
}

void local_finish()
{
    if (local_socket > 0)
        close(local_socket);
}

int local_setpoll(struct pollfd* pfd, int max)
{
    local_sfd = NULL;
    if (local_socket > 0) {
        pfd->events = POLLIN;
        pfd->fd = local_socket;
        pfd->revents = 0;
        local_sfd = pfd;
        return 1;
    }
    return 0;
}

int local_process()
{
    if (local_sfd != NULL && local_sfd->fd == local_socket && (local_sfd->revents & POLLIN)) {
        // just read it and dump it
        char dummy[16];
        return net_recv(local_socket, (void*)dummy, sizeof(dummy));
    }
    return 0;
}

#define DVRKEYINPUT 0x673a4427
void local_sendVKRec()
{
    if (local_socket > 0) {
        unsigned int msgbuf[2];
        struct sockad sad;
        net_addr(&sad, "127.0.0.1", local_port);
        msgbuf[0] = DVRKEYINPUT;
        msgbuf[1] = 0xff000000 | VK_RECON;
        net_sendto(local_socket, (void*)msgbuf, sizeof(msgbuf), &sad);
    }
}

void bodycam_run()
{
    int i;

    bodycam* bdarray;
    struct pollfd* pfd;
    int nfd, pfdsize;

    g_runtime = runtime();

    local_init();

    bdarray = new bodycam[bodycamNum];
    // setup all bodycamera
    for (i = 0; i < bodycamNum; i++) {
        bdarray[i].init(i);
    }
    pfdsize = bodycamNum + 10;
    pfd = new pollfd[pfdsize];

    s_maxwaitms = 1000;

    while (g_run == 1) {

        // add local listener
        nfd = local_setpoll(pfd, pfdsize);

        for (i = 0; i < bodycamNum; i++) {
            nfd += bdarray[i].setpoll(pfd + nfd, pfdsize - nfd);
        }

        if (nfd > 0) {
            i = poll(pfd, nfd, s_maxwaitms);
        } else {
            usleep(1000000);
            i = 0;
        }
        g_runtime = runtime();
        s_maxwaitms = 5000;

        local_process();

        for (i = 0; i < bodycamNum; i++) {
            bdarray[i].process();
        }
    }

    delete[] bdarray;
    delete[] pfd;

    local_finish();
}

static void s_handler(int signum)
{
    if (signum == SIGTERM || signum == SIGINT) {
        // quit
        g_run = 0;
    } else if (signum == SIGUSR1) { // enter idling mode
        g_run = 3;
    } else if (signum == SIGUSR2) { // reload
        g_run = 2;
    } else if (signum == SIGCONT) {
        if (g_run == 3) // reload if idling
            g_run = 2;
    }
}

void bodycam_init()
{
    config dvrconfig(CFG_FILE);

    // get number of body camera linked to this DVR
    bodycamNum = dvrconfig.getvalueint("system", "totalbodycam");
    if (bodycamNum < 0 || bodycamNum > MAX_BODYCAM) {
        bodycamNum = 0;
    }

    if (p_dio_mmap) {
        p_dio_mmap->bodycam_num = bodycamNum;
        printf("Bodycam number: %d\n", bodycamNum);
    }

    // init timezone
    char* p;
    string tz;

    tz = dvrconfig.getvalue("system", "timezone");
    if (tz.length() > 0) {
        string tzi = dvrconfig.getvalue("timezones", tz.getstring());
        if (tzi.length() > 0) {
            p = strchr(tzi.getstring(), ' ');
            if (p) {
                *p = 0;
            }
            p = strchr(tzi.getstring(), '\t');
            if (p) {
                *p = 0;
            }
            setenv("TZ", tzi.getstring(), 1);
        } else {
            setenv("TZ", tz.getstring(), 1);
        }
    }

    // dvrsvr port
    local_port = dvrconfig.getvalueint("network", "port");
    if (local_port == 0)
        local_port = 15114;

    // show start time
    getTime(tz.setbufsize(100));
    printf("Init time: %s\n", (char*)tz);
}

void dio_init()
{
    dio_mmap();
    if (p_dio_mmap == NULL) {
        printf("IO module not started!");
        exit(1);
    }

    // save pid
    int pid = getpid();
    FILE* fpid = fopen(VAR_DIR "/bodycamd.pid", "w");
    if (fpid) {
        fprintf(fpid, "%d", pid);
        fclose(fpid);
    }
}

void dio_finish()
{
    dio_munmap();
    unlink(VAR_DIR "/bodycamd.pid");
}

int main()
{
    dio_init();

    g_run = 1;

    // setup signal handler
    signal(SIGINT, s_handler);
    signal(SIGTERM, s_handler);
    signal(SIGUSR1, s_handler);
    signal(SIGUSR2, s_handler);
    signal(SIGCONT, s_handler);
    signal(SIGPIPE, SIG_IGN);

    while (g_run) {
        if (g_run == 2) {
            g_run = 1; // to restart
        } else if (g_run == 3) { // idling
            sleep(60);
        } else {
            bodycam_init();
            bodycam_run();
        }
    }

    dio_finish();
    return 0;
}
