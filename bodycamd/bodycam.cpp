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
#include <time.h>

#include "cfg.h"
#include "dvrsvr/config.h"
#include "dvrsvr/genclass.h"
#include "ioprocess/diomap.h"

#include "bodycam.h"
#include "net/net.h"
#include "json/json.h"

bodycam::bodycam()
{
    enable = 0;

    dvr_status = NULL;
    bodycam_status = NULL;
    recordmode = 0;

    dvrTrigger = 0;
    bodycamTrigger = 0;

    sock = -1;
    sfd = NULL;
    srdy = 0;
    recvPos = 0;
    state = 0; // start state
    activetime = g_runtime;
    dvrRec = 0;
    sent_msgid = 0;
}

// num: number of the camera, start from 0
void bodycam::init(int num)
{
    if (num >= MAX_BODYCAM || num < 0)
    {
        log("Error on bodycam number: %d", num);
        return;
    }

    config dvrconfig(CFG_FILE);
    string sect;
    sect.printf("bodycam%d", num + 1);
    enable = dvrconfig.getvalueint(sect, "enable");

    if (enable)
    {
        dvrTrigger = dvrconfig.getvalueint(sect, "dvr_trigger");
        bodycamTrigger = dvrconfig.getvalueint(sect, "bodycam_trigger");

        camip = dvrconfig.getvalue(sect, "ip");
        camport = dvrconfig.getvalueint(sect, "port");
        if (camport <= 0)
        {
            camport = 7878; // amba control port
        }

        // dio memory pointer
        if (p_dio_mmap != NULL)
        {
            dvr_status = &(p_dio_mmap->dvrstatus);
            bodycam_status = &(p_dio_mmap->bodycam_status[num]);
            *bodycam_status = 0;
        }
    }

    state = 0; // start state
    activetime = g_runtime;
}

bodycam::~bodycam()
{
	if (sock > 0)
		close(sock);

	if (bodycam_status)
		*bodycam_status = 0;
}

void bodycam::wait(int idlems)
{
    setMaxWait(idlems);
}

void bodycam::onRecord()
{
	if (bodycam_status)
		*bodycam_status |= 2;
	if (recordmode == 0) {
		recordmode = 1;
		if (bodycamTrigger) {
			local_sendVKRec();
		}
		log("Start Recording");
	}
}

void bodycam::onStop()
{
    if (bodycam_status)
        *bodycam_status &= ~2;
    if (recordmode != 0) {
        recordmode = 0;
        log("Stop Recording");
    }
}

void bodycam::onKeyInput(char* keyInput)
{
    log("key input: %s", keyInput);
    if (strcmp(keyInput, AMBA_BUTTON_RECORD) == 0 || strcmp(keyInput, AMBA_BUTTON_STOP) == 0) {
        srdy = 0;
        state = 3; // get app_status
    }
    // ignor other inputs
}

// return 1: if received buffer can be parsed as json object
void bodycam::onrecv()
{
    const char* ep;
    while (recvPos > 2) { // may be parseable
        json j_root;
        j_root.parse(recvBuf, &ep);
        if (j_root.isObject()) {
            // remove parsed buffer
            if ((ep - recvBuf) >= recvPos) {
                recvPos = 0; // parsed all buffer
            } else {
                // could be more object available ( 2 or more button message came in single read)
                memmove(recvBuf, ep, recvPos);
                recvPos -= (ep - recvBuf);
            }

            int rval = -1000;
            int msg_id = 0;
            string type;
            int paramInt = 0;
            string paramString;
            string key;
            json* j_item;

            msg_id = (int)j_root.getLeafNumber("msg_id");
            type = j_root.getLeafString("type");
            j_item = j_root.getLeaf("rval");
            if (j_item) {
                rval = j_item->getInt();
            }
            j_item = j_root.getLeaf("param");
            if (j_item) {
                if (j_item->isString()) {
                    paramString = j_item->getString();
                } else {
                    paramInt = j_item->getInt();
                }
            }
            key = j_root.getLeafString("key:");

            // processing json result
            if (key.length() > 1) {
                onKeyInput((char*)key);
            } else {
                log("RECV: rval: %d  , id: %d, paramInt: %d, param: %s", rval, msg_id, paramInt, (char*)paramString);

                if (rval >= 0) {
                    if (msg_id == ID_AMBA_START_SESSION) {
                        msg_token = paramInt;
                        log("get token number: %d", msg_token);
                        srdy = 0;
                        state = 2; // to sync bodycam time
                        if (bodycam_status)
                            *bodycam_status |= 1; // connected
                    }
                    if (msg_id == ID_AMBA_SET_SETTING && rval == 0) {
                        // could be sync time ack
                        if (type == AMBA_SETTING_TYPE_CLOCK) {
                            srdy = 0;
                            state = 3; // to get app status
                        } else {
                            state = 100; // idle noe
                        }
                        log("Ack set settings");
                    } else if (msg_id == ID_AMBA_GET_SETTING) {
                        if (type == AMBA_SETTING_TYPE_STATUS) {
                            if (paramString == "record") {
                                onRecord();
                            } else {
                                onStop();
                            }
                        }
                        state = 100;
                    } else if (msg_id == ID_AMBA_RECORD_START) {
                        onRecord();
                        state = 100;
                    } else if (msg_id == ID_AMBA_RECORD_STOP) {
                        onStop();
                        state = 100;
                    } else if (msg_id == ID_AMBA_PHOTOGRAPH) {
                        log("Take photo");
                    } else if (msg_id == ID_AMBA_QUERY_SESSION_HOLDER) {
                        log("Query session holder");
                        sendRsp(msg_id, 0);
                    } else if (msg_id == sent_msgid) {
                        log("ACK msgid : %d", msg_id);
                        sent_msgid = 0;
                    } else {
                        log("Unknown id: %d", msg_id);
                    }
                }
            }
        }
    }
}

// data from bodycam ready
int bodycam::receive()
{
    if (sock > 0) {
        int r = net_recv(sock, recvBuf + recvPos, BODYCAM_RECVBUFSIZE - recvPos - 1);
        if (r > 0) {
            recvPos += r;
            recvBuf[recvPos] = 0;
            log("Receive message:\n%s", recvBuf);

            onrecv();
        } else {
            log("Recv 0, socket closed");
            close(sock);
            sock = -1;
            recvPos = 0;
        }
    } else {
        recvPos = 0;
    }
    return 0;
}

int bodycam::send(char* buf)
{
    int s = 0;
    if (sock > 0 && net_srdy(sock, 10000000)) {
        s = net_send(sock, buf, strlen(buf));

        log("Send (%d): \n%s", s, buf);

        if (s <= 0) {
            close(s);
            sock = -1;
            wait(5000);
        }

        srdy = 0;
    }
    return s;
}

int bodycam::sendCmd(int msg_id, const char* type, const char* param)
{
    sent_msgid = msg_id;

    json jmsg(JSON_Object);
    jmsg.addNumberItem("token", msg_token);
    jmsg.addNumberItem("msg_id", msg_id);
    if (type) {
        jmsg.addStringItem("type", type);
    }
    if (param) {
        jmsg.addStringItem("param", param);
    }

    activetime = g_runtime;
    state = 1;

    char buf[500];
    jmsg.encode(buf, 500);
    return send(buf);
}

int bodycam::sendRsp(int msg_id, int rval)
{
    json jmsg(JSON_Object);
    jmsg.addNumberItem("token", msg_token);
    jmsg.addNumberItem("msg_id", msg_id);
    jmsg.addNumberItem("rval", rval);
    char buf[500];
    jmsg.encode(buf, 500);
    return send(buf);
}

void bodycam::getStatus()
{
    sendCmd(ID_AMBA_GET_SETTING, AMBA_SETTING_TYPE_STATUS);
}

void bodycam::startRecord()
{
    sendCmd(ID_AMBA_RECORD_START);
    recordmode = 2;
}

void bodycam::stopRecord()
{
    sendCmd(ID_AMBA_RECORD_STOP);
    recordmode = 0;
}

void bodycam::syncTime()
{
    char t[100];
    getTime(t);
    sendCmd(ID_AMBA_SET_SETTING, AMBA_SETTING_TYPE_CLOCK, t);
    log("Sync Time: %s", t);
}

int bodycam::process()
{
    int r;

    if (enable && sock <= 0 && (g_runtime - activetime) > 10000) {
        // to open new socket
        if (bodycam_status != NULL)
            *bodycam_status = 0;
        sfd = NULL;
        srdy = 0;
        recvPos = 0;
        state = 0; // start state
        activetime = g_runtime;
        sock = net_connect_nb(camip, camport);
        log("Connect %d", sock);
        return 0;
    }

    if (sock > 0 && sfd != NULL && sfd->fd == sock) {
        if (sfd->revents & POLLOUT) {
            srdy = 1;
        }
        if (sfd->revents & POLLIN) {
            receive();
            activetime = g_runtime;
        } else if (g_runtime - activetime > 30000 || (sfd->revents & (POLLERR | POLLHUP))) {
            log("Timeout, close socket!");
            close(sock);
            sock = -1;
        }
    }

    if (sock > 0 && srdy) {
        // state machine
        switch (state) {
        case 0: //	starting
            msg_token = 0; // reset msg token
            sendCmd(ID_AMBA_START_SESSION); // start a new session
            break;
        case 1: // wait for response
            if (g_runtime - activetime > 10000) {
                log("No response, close socket!");
                close(sock);
                sock = -1;
            }
            break;
        case 2: // sync time
            syncTime();
            break;
        case 3: // get app status
            getStatus();
            break;
        default: // idle or other
            if (dvrTrigger && isDVRTriggered()) {
                log("Rec ---- Triggered!!!   recmode: %d   dvrRec: %d", recordmode, dvrRec);
                if (recordmode == 0 && dvrRec) {
                    startRecord();
                } else if (recordmode == 2 && dvrRec == 0) {
                    stopRecord();
                }
            } else {
                if (g_runtime - activetime > 15000) {
                    log("idle time out, to get status");
                    // clear buffer first
                    recvPos = 0;
                    srdy = 0;
                    state = 3;
                }
            }
            break;
        }
    }
    return 1;
}

int bodycam::setpoll(struct pollfd* pfd, int max)
{
    sfd = NULL;
    if (enable && sock > 0 && max > 0) {
        pfd->fd = sock;
        pfd->events = POLLIN;
        if (srdy == 0) {
            pfd->events |= POLLOUT;
        }
        pfd->revents = 0;
        sfd = pfd;
        return 1;
    }
    return 0;
}

void bodycam::log(const char* fmt, ...)
{
    if (g_log) {
        va_list vl;
        printf("%d:%s: ", g_runtime, (char*)camip);
        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);
        printf("\n");
    }
}