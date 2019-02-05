#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>

#include "net/net.h"

#include "../cfg.h"
#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"
#include "../ioprocess/diomap.h"

#include "cjson/cjson.h"
#include "bodycam.h"

// num: number of the camera, start from 0
bodycam::bodycam(int num)
{
    recvPos = 0;
    enable = 0;

    if (num >= MAX_BODYCAM || num < 0)
    {
        printf("Error on bodycam number: %d\n", num);
        return;
    }

    config dvrconfig(CFG_FILE);
    string sect;
    sect.printf("bodycam%d", num + 1);
    camip = dvrconfig.getvalue(sect, "ip");
    camport = dvrconfig.getvalueint(sect, "port");
    if (camport <= 0)
    {
        camport = 7878; // amba control port
    }

    dvr_status = NULL;
    recordmode = 0;
    enable = dvrconfig.getvalueint(sect, "enable");
    if (enable)
    {
        dvrTrigger = dvrconfig.getvalueint(sect, "dvr_trigger");
        bodycamTrigger = dvrconfig.getvalueint(sect, "bodycam_trigger");

        // dio memory pointer
        if (p_dio_mmap != NULL)
        {
            dvr_status = &(p_dio_mmap->dvrstatus);
        }
    }
    else
    {
        dvrTrigger = 0;
        bodycamTrigger = 0;
    }

    sock = -1;
    sfd = NULL;
    activetime = g_runtime;
    waittime = 0;
    dvrRec = 0;
}

bodycam::~bodycam()
{
    if (sock > 0)
        close(sock);
}

void bodycam::wait(int idlems)
{
    activetime = g_runtime;
    waittime = idlems;
    setMaxWait(idlems);

    printf("call wait %d\n", idlems);
}

void bodycam::onRecord()
{
    if (recordmode == 0)
    {
        recordmode = 1;
        printf("Recording !\n");
    }
}

void bodycam::onStop()
{
    recordmode = 0;
    printf("Recording Stopped!\n");
}

void bodycam::onKeyInput(char *keyInput)
{
    printf("PARSED: key input: %s\n", keyInput);
    if (strcmp(keyInput, AMBA_BUTTON_RECORD) == 0)
    {
        printf("Start Recording\n");
        onRecord();
        if (bodycamTrigger && recordmode == 1)
        {
            local_sendVKRec();
        }
    }
    else if (strcmp(keyInput, AMBA_BUTTON_STOP) == 0)
    {
        printf("Stop Recording\n");
        onStop();
    }
    // ignor other inputs
}

// return 1: if received buffer can be parsed as json object
void bodycam::onrecv()
{
    const char *ep;
    while (recvPos > 2)
    { // may be parseable
        cJSON *j_root = cJSON_ParseWithOpts(recvBuf, &ep, 0);
        if (j_root)
        {
            // remove parsed buffer
            int bufLeft = recvPos - (ep - recvBuf);

            printf("recvPos: %d , left: %d\n", recvPos, bufLeft);

            if (bufLeft < 1)
            {
                recvPos = 0; // parsed all buffer
            }
            else
            {
                // could be more object available ( 2 or more button message came in single read)
                memmove(recvBuf, ep, bufLeft);
                recvPos = bufLeft;
            }

            int rval = -1;
            int msg_id = 0;
            string type;
            int paramInt = 0;
            string paramString;
            string key;

            cJSON *j_item;
            j_item = cJSON_GetObjectItem(j_root, "msg_id");
            if (j_item)
            {
                msg_id = j_item->valueint;
            }

            j_item = cJSON_GetObjectItem(j_root, "rval");
            if (j_item)
            {
                rval = j_item->valueint;
            }

            j_item = cJSON_GetObjectItem(j_root, "type");
            if (j_item)
            {
                if (j_item->type == cJSON_String)
                {
                    type = j_item->valuestring;
                }
            }

            j_item = cJSON_GetObjectItem(j_root, "param");
            if (j_item)
            {
                paramInt = j_item->valueint;
                if (j_item->type == cJSON_String)
                {
                    paramString = j_item->valuestring;
                }
            }

            j_item = cJSON_GetObjectItem(j_root, "key:");
            if (j_item)
            {
                if (j_item->type == cJSON_String)
                {
                    key = j_item->valuestring;
                }
            }

            cJSON_Delete(j_root);

            // processing json result
            if (key.length() > 0)
            {
                onKeyInput((char *)key);
            }
            else
            {
                printf("RECV: rval: %d  , id: %d, paramInt: %d, param: %s\n", rval, msg_id, paramInt, (char *)paramString);

                if (rval >= 0)
                {
                    if (msg_id == ID_AMBA_START_SESSION)
                    {
                        msg_token = paramInt;
                        printf("get token number: %d\n", msg_token);
                        syncTime();
                    }
                    else if (msg_id == ID_AMBA_GET_SETTING)
                    {
                        if (type == AMBA_SETTING_TYPE_STATUS)
                        {
                            if (paramString == "record")
                            {
                                // onRecord();
                            }
                            else
                            {
                                onStop();
                            }
                            wait(60000);
                        }
                    }
                }
                else if (msg_id > 0)
                {
                    if (sock > 0)
                        close(sock);
                    sock = -1;
                }
            }
        }
    }
}

// data from bodycam ready
int bodycam::receive()
{
    if (sock > 0)
    {
        int r = net_recv(sock, recvBuf + recvPos, BODYCAM_RECVBUFSIZE - recvPos - 1);
        if (r > 0)
        {
            recvPos += r;
            recvBuf[recvPos] = 0;
            printf("(%d) Receive message: \n%s", g_runtime, recvBuf);

            onrecv();
            activetime = g_runtime;
        }
        else
        {
            printf("socket closed\n");
            close(sock);
            sock = -1;
            recvPos = 0;
            wait(5000);
        }
    }
    else
    {
        recvPos = 0;
    }
    return 0;
}

int bodycam::send(char *buf)
{
    int s = 0;
    if (sock > 0 && net_srdy(sock, 1000000))
    {
        s = net_send(sock, buf, strlen(buf));

        printf("(%d - %d) Send: \n%s\n", g_runtime, s, buf);

        if (s <= 0)
        {
            close(s);
            sock = -1;
            wait(5000);
        }
    }
    return s;
}

int bodycam::sendCmd(int msg_id)
{
    return send(string().printf("{\"token\":%d,\"msg_id\":%d}", msg_token, msg_id));
}

int bodycam::sendCmd(int msg_id, const char *type)
{
    return send(string().printf("{\"token\":%d,\"msg_id\":%d,\"type\":\"%s\"}", msg_token, msg_id, type));
}

int bodycam::sendCmd(int msg_id, const char *type, const char *param)
{
    return send(string().printf("{\"token\":%d,\"msg_id\":%d,\"type\":\"%s\",\"param\":\"%s\"}", msg_token, msg_id, type, param));
}

void bodycam::getStatus()
{
    sendCmd(ID_AMBA_GET_SETTING, AMBA_SETTING_TYPE_STATUS);
    wait(2000);
}

void bodycam::startRecord()
{
    sendCmd(ID_AMBA_RECORD_START);
    recordmode = 2;
    wait(2000);
}

void bodycam::stopRecord()
{
    sendCmd(ID_AMBA_RECORD_STOP);
    recordmode = 0;
    wait(2000);
}

void bodycam::syncTime()
{
    string t;
    getTime(t.setbufsize(50));
    printf("Sync Time: %s\n", (char *)t);
    sendCmd(ID_AMBA_SET_SETTING, AMBA_SETTING_TYPE_CLOCK, (char *)t);
    wait(2000);
}

int bodycam::process()
{
    int r;
    if (sock > 0 && sfd != NULL && sfd->fd == sock && (sfd->revents & POLLIN))
    {
        receive();
    }

    if (sock > 0)
    {
        if (enable && isDVRTriggered() && dvrTrigger)
        {
            printf("Rec ---- Triggered!!!   recmode: %d   dvrRec: %d\n", recordmode, dvrRec);
            if (recordmode == 0 && dvrRec)
            {
                startRecord();
            }
            else if (recordmode == 2 && dvrRec == 0)
            {
                stopRecord();
            }
            wait(2000);
        }
        else
        {
            if (g_runtime - activetime > waittime)
            {
                printf("idle time out, to get status\n");
                // clear buffer first
                recvPos = 0;
                // get clear status
                getStatus();
            }
        }
    }
    else
    {
        wait(5000);
    }

    return 1;
}

int bodycam::setpoll(struct pollfd *pfd, int max)
{
    sfd = NULL;
    if (sock <= 0)
    {
        // to open new socket
        sock = net_connect(camip, camport);
        if (sock > 0)
        {
            if (net_srdy(sock, 1000000))
            {

                printf("socket opened!");

                msg_token = 0;                  // reset msg token
                sendCmd(ID_AMBA_START_SESSION); // start a new session
            }
            else
            {
                close(sock);
                sock = -1;
            }
        }
    }

    if (sock > 0 && max > 0)
    {
        pfd->events = POLLIN;
        pfd->fd = sock;
        pfd->revents = 0;
        sfd = pfd;
        return 1;
    }
    return 0;
}
