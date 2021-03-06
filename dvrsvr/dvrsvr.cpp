
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dvr.h"
#include "dir.h"
#include "vri.h"

//#define OUTPUT_COMMAND

dvrsvr *dvrsvr::head = NULL;

// used by vri search, just a temperary solution
static struct dvrtime stream_time;

dvrsvr::dvrsvr(int fd)
{
	m_sockfd = fd;
	m_next = NULL;
	m_fifo = NULL;
	m_recvbuf = NULL;

	m_req.reqcode = 0;
	m_req.reqsize = 0;
	m_recvlen = sizeof(m_req);
	m_recvloc = 0;

	m_playback = NULL; // playback
	m_live = NULL;
	m_nfilehandle = NULL; // new clip file handle. (to support new file copying)

	m_conntype = CONN_NORMAL;
	m_keycheck = 0;

	// awkward fix for ply266.dll, which don't check key for every connections, so check
	//   if there are any existing connection from same client ip
	int peer = net_peer(m_sockfd);
	dvrsvr *walk = head;
	while (walk)
	{
		if (walk->m_keycheck && walk->m_sockfd > 0 && peer == net_peer(walk->m_sockfd))
		{
			m_keycheck = 1;
			break;
		}
		walk = walk->m_next;
	}

	mIframe = 0;
}

dvrsvr::~dvrsvr()
{
	close();
}

void dvrsvr::close()
{
	net_fifo *pfifo;
	if (m_sockfd > 0)
	{
		closesocket(m_sockfd);
		m_sockfd = -1;
	}
	while (m_fifo != NULL)
	{
		net_fifo *nfifo = m_fifo->next;
		if (m_fifo->buf)
		{
			mem_free(m_fifo->buf);
		}
		delete m_fifo;
		m_fifo = nfifo;
	}
	if (m_playback != NULL)
	{
		delete m_playback;
		m_playback = NULL;
	}
	if (m_live != NULL)
	{
		delete m_live;
		m_live = NULL;
	}
	if (m_nfilehandle != NULL)
	{
		fclose(m_nfilehandle);
		m_nfilehandle = NULL;
	}
	if (m_recvbuf != NULL)
	{
		mem_free(m_recvbuf);
		m_recvbuf = NULL;
	}
}

// return 0: no data, 1: may have more data
int dvrsvr::read()
{
	int n;
	if (m_sockfd <= 0)
	{
		return 0;
	}
	if (m_recvbuf == NULL)
	{ // to receive req struct
		n = ::recv(m_sockfd, ((char *)&m_req) + m_recvloc, m_recvlen - m_recvloc, 0);
		if (n == 0)
		{ // error or connection closed by peer
			close();
			return 0;
		}
		else if (n < 0)
		{
			if (errno != EWOULDBLOCK)
			{
				close();
			}
			return 0;
		}
		m_recvloc += n;
		if (m_recvloc < m_recvlen)
			return 1;

		if (m_req.reqsize > 500000 ||
			m_req.reqsize < 0 ||
			m_req.reqcode < REQOK ||
			m_req.reqcode >= REQMAX) // invalid req
		{
			close();
			return 0;
		}
		else if (m_req.reqsize > 0) // wait for extra data
		{
			m_recvbuf = (char *)mem_alloc(m_req.reqsize + 4);
			if (m_recvbuf == NULL)
			{
				close(); // no enough memory
				return 0;
			}
			else
			{
				m_recvlen = m_req.reqsize;
				m_recvloc = 0;
				return 1;
			}
		}
		else
		{ //reqsize == 0
			m_recvlen = 0;
		}
	}
	else
	{
		n = ::recv(m_sockfd, m_recvbuf + m_recvloc, m_recvlen - m_recvloc, 0);
		if (n == 0) // connection closed by peer
		{
			close();
			return 0;
		}
		else if (n < 0)
		{
			if (errno != EWOULDBLOCK) // error!
			{
				close();
			}
			return 0;
		}
		m_recvloc += n;
		if (m_recvloc < m_recvlen) // buffer not complete
		{
			return 1;
		}
		m_recvbuf[m_req.reqsize] = 0; // null terminated, just in case
	}

	// req completed, process it
	onrequest();

	if (m_recvbuf != NULL)
	{
		mem_free(m_recvbuf);
	}
	m_recvbuf = NULL;
	m_recvlen = sizeof(m_req);
	m_recvloc = 0;
	return 0;
}

int dvrsvr::write()
{
	if (m_fifo == NULL || m_sockfd <= 0)
	{
		return 0;
	}

	if (m_fifo->loc < m_fifo->bufsize && m_fifo->buf != NULL)
	{
		int n = ::send(m_sockfd, m_fifo->buf + m_fifo->loc,
					   m_fifo->bufsize - m_fifo->loc, 0);
		if (n < 0)
		{
			if (errno != EWOULDBLOCK)
			{
				close();
			}
			return 0;
		}
		else
		{
			m_fifo->loc += n;
		}
	}

	if (m_fifo->loc >= m_fifo->bufsize || m_fifo->buf == NULL)
	{
		if (m_fifo->buf != NULL)
			mem_free(m_fifo->buf);
		net_fifo *nfifo = m_fifo->next;
		delete m_fifo;
		m_fifo = nfifo;
	}
	return 1;
}

void dvrsvr::send_fifo(void *buf, int bufsize)
{
	net_fifo *pfifo;
	net_fifo *nfifo;

	if (isclose() || buf == NULL || bufsize <= 0)
	{
		return;
	}

	nfifo = new net_fifo;
	nfifo->buf = (char *)mem_addref(buf);
	nfifo->next = NULL;
	nfifo->bufsize = bufsize;
	nfifo->loc = 0;

	if (m_fifo == NULL)
	{
		m_fifo = nfifo;
	}
	else
	{
		pfifo = m_fifo;
		while (pfifo->next != NULL)
		{
			pfifo = pfifo->next;
		}
		pfifo->next = nfifo;
	}
}

void dvrsvr::Send(void *buf, int bufsize)
{

	char *cbuf = (char *)buf;
	int n;
	if (isclose() || buf == NULL || bufsize <= 0)
	{
		return;
	}

	if (m_fifo || mem_check(buf))
	{
		send_fifo(cbuf, bufsize);
	}
	else
	{
		while (bufsize > 0)
		{
			n = ::send(m_sockfd, cbuf, bufsize, 0);
			if (n < 0)
			{
				if (errno == EWOULDBLOCK)
				{
					send_fifo(cbuf, bufsize);
				}
				else
				{
					close(); // net work error!
				}
				return;
			}
			cbuf += n;
			bufsize -= n;
		}
	}
}

int dvrsvr::onframe(cap_frame *pframe)
{
	if (isclose())
	{
		return 0;
	}
	if ((m_conntype == CONN_REALTIME || m_conntype == CONN_LIVESTREAM) && m_connchannel == pframe->channel)
	{
		if (pframe->frametype == FRAMETYPE_KEYFRAME && m_fifo != NULL)
		{
			net_fifo *pfifo = m_fifo;
			net_fifo *nfifo = pfifo->next;
			while (nfifo != NULL)
			{
				if (nfifo->bufsize == sizeof(struct dvr_ans) &&
					nfifo->loc == 0 &&
					((struct dvr_ans *)nfifo->buf)->anscode == ANSSTREAMDATA &&
					((struct dvr_ans *)nfifo->buf)->data == FRAMETYPE_KEYFRAME)
				{
					// break the fifo link
					pfifo->next = NULL;

					// remove all tail fifos
					while (nfifo)
					{
						pfifo = nfifo->next;
						if (nfifo->buf)
							mem_free(nfifo->buf);
						delete nfifo;
						nfifo = pfifo;
					}

					break;
				}
				pfifo = nfifo;
				nfifo = pfifo->next;
			}
		}

		// **** send frame buffer header for PLY266
		struct dvr_ans *ans = (struct dvr_ans *)mem_alloc(sizeof(struct dvr_ans));
		ans->anscode = ANSSTREAMDATA;
		ans->anssize = pframe->framesize;
		ans->data = pframe->frametype;
		send_fifo((char *)ans, sizeof(*ans));
		mem_free(ans);
		send_fifo(pframe->framedata, pframe->framesize);

		return 1;
	}

	return 0;
}

void dvrsvr::onrequest()
{

	//  dvr_log( "Onrequest: %d, data: %d, bufsize: %d", m_req.reqcode, m_req.data, m_req.reqsize );

	switch (m_req.reqcode)
	{
	case REQOK:
		ReqOK();
		break;
	case REQREALTIME:
		ReqRealTime();
		break;
	case REQCHANNELINFO:
		ChannelInfo();
		break;
	case REQSERVERNAME:
		DvrServerName();
		break;
	case REQGETSYSTEMSETUP:
		GetSystemSetup();
		break;
	case REQSETSYSTEMSETUP:
		SetSystemSetup();
		break;
	case REQGETCHANNELSETUP:
		GetChannelSetup();
		break;
	case REQSETCHANNELSETUP:
		SetChannelSetup();
		break;
	case REQKILL:
		ReqKill();
		break;
	case REQSETUPLOAD:
		SetUpload();
		break;
	case REQHOSTNAME:
		HostName();
		break;
	case REQFILERENAME:
	case REQRENAME:
		FileRename();
		break;
	case REQFILE:
	case REQFILECREATE:
		FileCreate();
		break;
	case REQFILEREAD:
		FileRead();
		break;
	case REQFILEWRITE:
		FileWrite();
		break;
	case REQFILECLOSE:
		FileClose();
		break;
	case REQFILESETPOINTER:
		FileSetPointer();
		break;
	case REQFILEGETPOINTER:
		FileGetPointer();
		break;
	case REQFILEGETSIZE:
		FileGetSize();
		break;
	case REQFILESETEOF:
		FileSetEof();
		break;
	case REQFILEDELETE:
		FileDelete();
		break;
	case REQDIRFINDFIRST:
		DirFindFirst();
		break;
	case REQDIRFINDNEXT:
		DirFindNext();
		break;
	case REQDIRFINDCLOSE:
		DirFindClose();
		break;
	case REQGETCHANNELSTATE:
		GetChannelState();
		break;
	case REQSETSYSTEMTIME:
	case REQSETTIME:
		SetDVRSystemTime();
		break;
	case REQGETSYSTEMTIME:
	case REQGETTIME:
		GetDVRSystemTime();
		break;
	case REQSETLOCALTIME:
		SetDVRLocalTime();
		break;
	case REQGETLOCALTIME:
		GetDVRLocalTime();
		break;
	case REQSETTIMEZONE:
		SetDVRTimeZone();
		break;
	case REQGETTIMEZONE:
		GetDVRTimeZone();
		break;
	case REQGETTZIENTRY:
		GetDVRTZIEntry();
		break;
	case REQGETVERSION:
		GetVersion();
		break;
	case REQPTZ:
		ReqPTZ();
		break;
	case REQAUTH:
		ReqAuth();
		break;
	case REQKEY:
		ReqKey();
		break;
	case REQDISCONNECT:
		ReqDisconnect();
		break;
	case REQCHECKID:
		ReqCheckId();
		break;
	case REQLISTID:
		ReqListId();
		break;
	case REQDELETEID:
		ReqDeleteId();
		break;
	case REQADDID:
		ReqAddId();
		break;
	case REQSHAREPASSWD:
		ReqSharePasswd();
		break;
	case REQRUN:
	case REQGETFILE:
	case REQPUTFILE:
	case REQSHAREINFO:
	case REQPLAYBACK:
		DefaultReq();
		break;
	case REQSTREAMOPEN:
		powerNum = 60;
		ReqStreamOpen();
		break;
	case REQSTREAMOPENLIVE:
		ReqStreamOpenLive();
		break;
	case REQOPENLIVE:
		ReqOpenLive();
		break;
	case REQSTREAMCLOSE:
		ReqStreamClose();
		break;
	case REQSTREAMSEEK:
		ReqStreamSeek();
		break;
	case REQSTREAMNEXTFRAME:
		ReqNextFrame();
		break;
	case REQSTREAMNEXTKEYFRAME:
		ReqNextKeyFrame();
		break;
	case REQSTREAMPREVKEYFRAME:
		ReqPrevKeyFrame();
		break;
	case REQSTREAMGETDATA:
		powerNum = 60;
		ReqStreamGetData();
		break;
	case REQ2STREAMGETDATAEX:
		powerNum = 60;
		ReqStreamGetDataEx();
		break;
	case REQSTREAMTIME:
		ReqStreamTime();
		break;
	case REQSTREAMDAYINFO:
		ReqStreamDayInfo();
		break;
	case REQSTREAMMONTHINFO:
		ReqStreamMonthInfo();
		break;
	case REQLOCKINFO:
		ReqLockInfo();
		break;
	case REQUNLOCKFILE:
		ReqUnlockFile();
		break;
	case REQSTREAMDAYLIST:
		ReqStreamDayList();
		break;
	case REQDAYCLIPLIST:
		ReqDayClipList();
		break;
	case REQDAYLIST:
		ReqDayList();
		break;
	case REQ2ADJTIME:
		Req2AdjTime();
		break;
	case REQ2SETLOCALTIME:
		Req2SetLocalTime();
		break;
	case REQ2GETLOCALTIME:
		Req2GetLocalTime();
		break;
	case REQ2SETSYSTEMTIME:
		Req2SetSystemTime();
		break;
	case REQ2GETSYSTEMTIME:
		Req2GetSystemTime();
		break;
	case REQ2GETZONEINFO:
		Req2GetZoneInfo();
		break;
	case REQ2SETTIMEZONE:
		Req2SetTimeZone();
		break;
	case REQ2GETTIMEZONE:
		Req2GetTimeZone();
		break;
	case REQSETLED:
		ReqSetled();
		break;
	case REQSETHIKOSD:
		ReqSetHikOSD();
		break;
	case REQ2GETCHSTATE:
		Req2GetChState();
		break;
	case REQ2GETSETUPPAGE:
		Req2GetSetupPage();
		break;

	// TVS/PW
	case REQCHECKKEY:
		ReqCheckKey();
		break;

	case REQGETVRI:
		ReqGetVri();
		break;

	case REQSENDDATA:
		ReqSendData();
		break;

	case REQGETDATA:
		ReqGetData();
		break;

#ifdef PWII_APP
	case REQ2KEYPAD:
		Req2Keypad();
		break;
	case REQ2PANELLIGHTS:
		Req2PanelLights();
		break;
#endif
	case REQ2GETSTREAMBYTES:
		Req2GetStreamBytes();
		break;
	case REQECHO:
		ReqEcho();
		break;
	default:
		DefaultReq();
		break;
	}
}

void dvrsvr::ReqOK()
{
	struct dvr_ans ans;
	ans.anscode = ANSOK;
	ans.data = 0;
	ans.anssize = 0;
	Send(&ans, sizeof(ans));
}

void dvrsvr::ReqEcho()
{
	struct dvr_ans ans;
	ans.anscode = ANSECHO;
	ans.data = 0;
	ans.anssize = m_req.reqsize;
	Send(&ans, sizeof(ans));
	if (m_req.reqsize > 0)
	{
		Send(m_recvbuf, m_req.reqsize);
	}
}

void dvrsvr::ReqRealTime()
{
	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		ans.anscode = ANSREALTIMEHEADER;
		ans.data = m_req.data;
		ans.anssize = 40;
		Send(&ans, sizeof(ans));
		Send(&(cap_channel[m_req.data]->m_header), ans.anssize);
		m_conntype = CONN_REALTIME;
		m_connchannel = m_req.data;
	}
	else
	{
		DefaultReq();
	}
}

struct channel_info
{
	int Enable;
	int Resolution;
	char CameraName[64];
};

void dvrsvr::ChannelInfo()
{
	int i;
	struct dvr_ans ans;
	struct channel_info chinfo;

	ans.anscode = ANSCHANNELDATA;
	ans.data = cap_channels;
	ans.anssize = cap_channels * sizeof(struct channel_info);
	Send(&ans, sizeof(ans));
	for (i = 0; i < cap_channels; i++)
	{
		memset(&chinfo, 0, sizeof(chinfo));
		chinfo.Enable = cap_channel[i]->enabled();
		chinfo.Resolution = 0;
		strncpy(chinfo.CameraName, cap_channel[i]->getname(), 64);
		Send(&chinfo, sizeof(chinfo));
	}
}

void dvrsvr::HostName()
{
	struct dvr_ans ans;
	ans.anscode = ANSSERVERNAME;
	ans.anssize = strlen(g_hostname) + 1;
	ans.data = 0;
	Send(&ans, sizeof(ans));
	Send(g_hostname, ans.anssize);
}

void dvrsvr::DvrServerName()
{
	HostName();
}

void dvrsvr::GetSystemSetup()
{
	struct dvr_ans ans;
	struct system_stru sys;
	if (dvr_getsystemsetup(&sys))
	{
		ans.anscode = ANSSYSTEMSETUP;
		ans.anssize = sizeof(struct system_stru);
		ans.data = 0;
		Send(&ans, sizeof(ans));
		Send(&sys, sizeof(sys));
	}
	else
	{
		DefaultReq();
	}
}

void dvrsvr::SetSystemSetup()
{
	/*
	int sendsize = sizeof(struct dvr_ans);
	char * sendbuf = (char *)mem_alloc( sendsize);
	struct dvr_ans * pans = (struct dvr_ans *)sendbuf ;
	struct system_stru * psys = (struct system_stru *) m_recvbuf ;
	if( dvr_setsystemsetup(psys) ) {
		pans->anscode = ANSOK;
		pans->anssize = 0;
		pans->data = 0;
		Send( sendbuf, sendsize );
		mem_free( sendbuf );
		return ;
	}
	else {
		mem_free( sendbuf ) ;
	}
	*/
	DefaultReq();
}

void dvrsvr::GetChannelSetup()
{
	struct DvrChannel_attr dattr;
	struct dvr_ans ans;
	if (dvr_getchannelsetup(m_req.data, &dattr, sizeof(dattr)))
	{
		ans.anscode = ANSCHANNELSETUP;
		ans.anssize = sizeof(struct DvrChannel_attr);
		ans.data = m_req.data;
		Send(&ans, sizeof(ans));
		Send(&dattr, sizeof(dattr));
	}
	else
	{
		DefaultReq();
	}
}

void dvrsvr::SetChannelSetup()
{
	struct dvr_ans ans;
	struct DvrChannel_attr *pchannel = (struct DvrChannel_attr *)m_recvbuf;
	dvr_log("SetChannelSetup received");
	if (dvr_setchannelsetup(m_req.data, pchannel, m_req.reqsize))
	{
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqKill()
{
	DefaultReq();
}

void dvrsvr::SetUpload()
{
	DefaultReq();
}

void dvrsvr::FileRename()
{
	DefaultReq();
}

void dvrsvr::FileCreate()
{
	DefaultReq();
}

void dvrsvr::FileRead()
{
	DefaultReq();
}

void dvrsvr::FileWrite()
{
	DefaultReq();
}

void dvrsvr::FileClose()
{
	DefaultReq();
}

void dvrsvr::FileSetPointer()
{
	DefaultReq();
}

void dvrsvr::FileGetPointer()
{
	DefaultReq();
}

void dvrsvr::FileGetSize()
{
	DefaultReq();
}

void dvrsvr::FileSetEof()
{
	DefaultReq();
}

void dvrsvr::FileDelete()
{
	DefaultReq();
}

void dvrsvr::DirFindFirst()
{
	DefaultReq();
}

void dvrsvr::DirFindNext()
{
	DefaultReq();
}

void dvrsvr::DirFindClose()
{
	DefaultReq();
}

void dvrsvr::GetChannelState()
{

	int i;
	struct channelstate
	{
		int sig;
		int rec;
		int mot;
	} cs;
	struct dvr_ans ans;

	ans.anscode = ANSGETCHANNELSTATE;
	ans.anssize = sizeof(struct channelstate) * cap_channels;
	ans.data = cap_channels;
	Send(&ans, sizeof(ans));
	for (i = 0; i < cap_channels; i++)
	{
		if (cap_channel != NULL && cap_channel[i] != NULL)
		{
			cs.sig = (cap_channel[i]->getsignal() == 0); // signal lost?
			cs.rec = rec_state(i);						 // channel recording?
			cs.mot = cap_channel[i]->getmotion();		 // motion detections
		}
		else
		{
			cs.sig = 0;
			cs.rec = 0;
			cs.mot = 0;
		}
		Send(&cs, sizeof(cs));
	}
}

void dvrsvr::SetDVRSystemTime()
{
	DefaultReq();
}

void dvrsvr::GetDVRSystemTime()
{
	DefaultReq();
}

void dvrsvr::SetDVRLocalTime()
{
	DefaultReq();
}

void dvrsvr::GetDVRLocalTime()
{
	DefaultReq();
}

void dvrsvr::SetDVRTimeZone()
{
	DefaultReq();
}

void dvrsvr::GetDVRTimeZone()
{
	DefaultReq();
}

void dvrsvr::GetDVRTZIEntry()
{
	DefaultReq();
}

void dvrsvr::GetVersion()
{
	int version;
	struct dvr_ans ans;
	ans.anscode = ANSGETVERSION;
	ans.anssize = 4 * sizeof(int);
	ans.data = 0;
	Send(&ans, sizeof(ans));
#if USENEWVERSION
	version = VERSION0;
	Send(&version, sizeof(version));
	version = VERSION1;
	Send(&version, sizeof(version));
	version = VERSION2;
	Send(&version, sizeof(version));
	version = VERSION3;
	Send(&version, sizeof(version));
#else
	version = DVRVERSION0;
	Send(&version, sizeof(version));
	version = DVRVERSION1;
	Send(&version, sizeof(version));
	version = DVRVERSION2;
	Send(&version, sizeof(version));
	version = DVRVERSION3;
	Send(&version, sizeof(version));
#endif
}

struct ptz_cmd
{
	DWORD command;
	DWORD param;
};

void dvrsvr::ReqPTZ()
{
	struct ptz_cmd *pptz;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		pptz = (struct ptz_cmd *)m_recvbuf;
		if (ptz_msg(m_req.data, pptz->command, pptz->param) != 0)
		{
			struct dvr_ans ans;
			ans.anscode = ANSOK;
			ans.anssize = 0;
			ans.data = 0;
			Send(&ans, sizeof(ans));
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::ReqAuth()
{
	DefaultReq();
}

void dvrsvr::ReqKey()
{
	DefaultReq();
}

void dvrsvr::ReqDisconnect()
{
	DefaultReq();
}

void dvrsvr::ReqCheckId()
{
	DefaultReq();
}

void dvrsvr::ReqListId()
{
	DefaultReq();
}

void dvrsvr::ReqDeleteId()
{
	DefaultReq();
}

void dvrsvr::ReqAddId()
{
	DefaultReq();
}

void dvrsvr::ReqSharePasswd()
{
	DefaultReq();
}

#define DVRSTREAMHANDLE(p) (((int)(void *)(p)) & 0x3fffffff)

//
//  m_req.data is stream channel
void dvrsvr::ReqStreamOpen()
{
	if (!m_keycheck && g_keycheck)
	{
		DefaultReq();
		return;
	}

	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		if (m_playback)
		{
			delete m_playback;
		}

		m_playback = new playback(m_req.data);
		if (m_playback != NULL)
		{
			ans.anscode = ANSSTREAMOPEN;
			ans.anssize = 0;
			ans.data = DVRSTREAMHANDLE(m_playback);
			Send(&ans, sizeof(ans));
			//    dvr_log("Stream Open : %d", m_req.data);
			return;
		}
	}
	DefaultReq();
}

//
//  m_req.data is stream channel
void dvrsvr::ReqStreamOpenLive()
{
	if (!m_keycheck && g_keycheck)
	{
		DefaultReq();
		return;
	}

	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{

		if (m_live != NULL)
		{
			delete m_live;
		}
		m_live = new live(m_req.data);
		if (m_live)
		{
			ans.anscode = ANSSTREAMOPEN;
			ans.anssize = 0;
			ans.data = DVRSTREAMHANDLE(m_live);
			Send(&ans, sizeof(ans));
			return;
		}
	}
	DefaultReq();
}

// Open live stream (2nd vesion)
//  m_req.data is stream channel
void dvrsvr::ReqOpenLive()
{

	if (!m_keycheck && g_keycheck)
	{
		DefaultReq();
		return;
	}

	struct dvr_ans ans;
	int hlen;

	// vri hack
	stream_time.year = 0;

	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		ans.anscode = ANSSTREAMOPEN;
		ans.data = m_req.data;
		ans.anssize = 0;
		Send(&ans, sizeof(ans));

		ans.anscode = ANSSTREAMDATA;
		ans.anssize = 40;
		ans.data = FRAMETYPE_264FILEHEADER;
		Send(&ans, sizeof(ans));
		Send(&(cap_channel[m_req.data]->m_header), ans.anssize);
		m_conntype = CONN_LIVESTREAM;
		m_connchannel = m_req.data;

		return;
	}
	DefaultReq();
}
void dvrsvr::ReqStreamClose()
{
	struct dvr_ans ans;
	if (m_req.data == DVRSTREAMHANDLE(m_playback))
	{
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
		if (m_playback)
		{
			delete m_playback;
			m_playback = NULL;
		}
		return;
	}
	else if (m_req.data == DVRSTREAMHANDLE(m_live))
	{
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
		if (m_live)
		{
			delete m_live;
			m_live = NULL;
		}
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqStreamSeek()
{
	struct dvr_ans ans;
	char H264header[40];
	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback &&
		m_recvbuf != NULL &&
		m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		dvrtime *dvrt = (struct dvrtime *)m_recvbuf;
		m_playback->seek(dvrt);
		ans.anscode = ANSOK;
		ans.anssize = sizeof(struct hd_file);
		ans.data = 0;
		Send(&ans, sizeof(ans));
		//send 40 bytes file header
		m_playback->getfileheader(H264header, sizeof(struct hd_file));
		Send(H264header, sizeof(struct hd_file));
	}
	else
	{
		DefaultReq();
	}
}

void dvrsvr::ReqNextFrame()
{
	struct dvr_ans ans;
	ans.anscode = ANSERROR;
	ans.data = 0;
	ans.anssize = 0;
	Send(&ans, sizeof(ans));
}

void dvrsvr::ReqNextKeyFrame()
{
	struct dvr_ans ans;
	ans.anscode = ANSERROR;
	ans.data = 0;
	ans.anssize = 0;
	Send(&ans, sizeof(ans));
}

void dvrsvr::ReqPrevKeyFrame()
{
	struct dvr_ans ans;
	ans.anscode = ANSERROR;
	ans.data = 0;
	ans.anssize = 0;
	Send(&ans, sizeof(ans));
}

void dvrsvr::ReqStreamGetDataEx()
{
	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback)
	{
		int framesize = m_playback->readframe(NULL);
		if (framesize > 0 && framesize < 5000000)
		{
			frame_info fi;
			fi.framesize = framesize;
			fi.framebuf = (char *)mem_alloc(framesize);
			m_playback->readframe(&fi);
			struct dvr_ans ans;
			ans.anscode = ANS2STREAMDATAEX;
			ans.anssize = sizeof(struct dvrtime) + framesize;
			ans.data = fi.frametype; //frametype;
			Send(&ans, sizeof(struct dvr_ans));

			// Patch for get VRI
			stream_time = fi.frametime;

			Send(&(fi.frametime), sizeof(struct dvrtime));
			Send(fi.framebuf, framesize);
			mem_free(fi.framebuf);
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::ReqStreamGetData()
{

	void *pbuf = NULL;
	int getsize = 0;
	int frametype;

	static struct timeval tm;
	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback)
	{
		int framesize = m_playback->readframe(NULL);
		if (framesize > 0 && framesize < 5000000)
		{
			frame_info fi;
			fi.frametype = 0;
			fi.framesize = framesize;
			fi.framebuf = (char *)mem_alloc(framesize);
			m_playback->readframe(&fi);
			struct dvr_ans ans;
			ans.anscode = ANSSTREAMDATA;
			ans.anssize = framesize;
			ans.data = fi.frametype;
			Send(&ans, sizeof(struct dvr_ans));
			Send(fi.framebuf, framesize);
			mem_free(fi.framebuf);
			return;
		}
	}
	else if (m_req.data == DVRSTREAMHANDLE(m_live) &&
			 m_live)
	{
		m_live->getstreamdata(&pbuf, &getsize, &frametype);
		if (getsize >= 0 && pbuf)
		{
			struct dvr_ans ans;
			ans.anscode = ANSSTREAMDATA;
			ans.anssize = getsize;
			ans.data = frametype;
			Send(&ans, sizeof(ans));
			if (getsize > 0)
			{
				Send(pbuf, getsize);
			}
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::ReqStreamTime()
{
	struct dvrtime streamtime;
	struct dvr_ans ans;

	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback)
	{
		if (m_playback->getstreamtime(&streamtime))
		{
			ans.anscode = ANSSTREAMTIME;
			ans.anssize = sizeof(struct dvrtime);
			ans.data = 0;
			Send(&ans, sizeof(ans));
			Send(&streamtime, ans.anssize);
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::ReqStreamDayInfo()
{
	struct dvrtime *pday;
	struct dvr_ans ans;

	array<struct dayinfoitem> dayinfo;
	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback != NULL &&
		m_recvbuf != NULL &&
		m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		pday = (struct dvrtime *)m_recvbuf;
		m_playback->getdayinfo(dayinfo, pday);
		ans.anscode = ANSSTREAMDAYINFO;
		ans.data = dayinfo.size();
		ans.anssize = dayinfo.size() * sizeof(struct dayinfoitem);
		Send(&ans, sizeof(ans));

		if (ans.anssize > 0)
		{
			Send(dayinfo.at(0), ans.anssize);
		}
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqStreamMonthInfo()
{
	DWORD monthinfo;
	struct dvr_ans ans;
	struct dvrtime *pmonth;

	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback != NULL &&
		m_recvbuf != NULL &&
		m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		pmonth = (struct dvrtime *)m_recvbuf;
		monthinfo = m_playback->getmonthinfo(pmonth);
		ans.anscode = ANSSTREAMMONTHINFO;
		ans.anssize = 0;
		ans.data = (int)monthinfo;
		;
		Send(&ans, sizeof(ans));
#ifdef NETDBG
		printf("Stream Month Info %04d-%02d-%02d ",
			   pmonth->year, pmonth->month, pmonth->day);
		int x;
		for (x = 0; x < 32; x++)
		{
			if (monthinfo & (1 << x))
			{
				printf(" %d", x + 1);
			}
		}
		printf("\n");
#endif
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqStreamDayList()
{
	int *daylist;
	int daylistsize;
	struct dvr_ans ans;

	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback != NULL)
	{
		daylistsize = m_playback->getdaylist(&daylist);
		ans.anscode = ANSSTREAMDAYLIST;
		ans.data = daylistsize;
		ans.anssize = daylistsize * sizeof(int);
		Send(&ans, sizeof(ans));
		if (ans.anssize > 0)
		{
			Send((void *)daylist, ans.anssize);
		}
		return;
	}
	DefaultReq();
}

// get clip file list by disk number
void dvrsvr::ReqDayClipList()
{
	struct dvr_ans ans;

	if (m_recvbuf != NULL &&
		m_req.reqsize >= sizeof(int))
	{
		int bcddate = *(int *)m_recvbuf;
		struct dvrtime day;
		memset(&day, 0, sizeof(day));
		day.year = bcddate / 10000;
		day.month = bcddate % 10000 / 100;
		day.day = bcddate % 100;
		array<f264name> flist;
		disk_pw_listdaybydisk(flist, &day, m_req.data);

		// calculate buffer size
		int bsize = 0;
		char *fname;
		int len;
		int i;
		for (i = 0; i < flist.size(); i++)
		{
			fname = (char *)(flist[i]);
			len = strlen(fname);
			bsize += len + 1;
		}

		char *buf = (char *)mem_alloc(bsize + 4);
		char *cbuf = buf;
		for (i = 0; i < flist.size(); i++)
		{
			fname = (char *)(flist[i]);
			len = strlen(fname);
			memcpy(cbuf, fname, len);
			cbuf[len] = ',';
			cbuf += len + 1;
		}

		ans.anscode = ANSDAYCLIPLIST;
		ans.data = flist.size();
		ans.anssize = bsize;
		Send(&ans, sizeof(ans));
		if (ans.anssize > 0)
		{
			Send(buf, ans.anssize);
		}
		mem_free(buf);
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqDayList()
{
	array<int> daylist;
	disk_getdaylist(daylist, -1);
	int lsize = daylist.size();
	struct dvr_ans ans;
	ans.anscode = ANSDAYLIST;
	ans.data = lsize;
	ans.anssize = sizeof(int) * lsize;
	Send(&ans, sizeof(ans));
	if (lsize > 0)
	{
		int *d_start = daylist.at(0);
		Send(daylist.at(0), ans.anssize);
	}
	return;
}

void dvrsvr::ReqLockInfo()
{
	struct dvrtime *pday;
	struct dvr_ans ans;
	array<struct dayinfoitem> dayinfo;

	if (m_req.data == DVRSTREAMHANDLE(m_playback) &&
		m_playback != NULL &&
		m_recvbuf != NULL &&
		m_req.reqsize >= (int)sizeof(struct dvrtime))
	{

		pday = (struct dvrtime *)m_recvbuf;

		m_playback->getlockinfo(dayinfo, pday);

		ans.anscode = ANSSTREAMDAYINFO;
		ans.data = dayinfo.size();
		ans.anssize = dayinfo.size() * sizeof(struct dayinfoitem);
		Send(&ans, sizeof(ans));
		if (ans.anssize > 0)
		{
			Send(dayinfo.at(0), ans.anssize);
		}
		return;
	}
	DefaultReq();
}

void dvrsvr::ReqUnlockFile()
{
	struct dvrtime *tbegin;
	struct dvrtime *tend;
	struct dvr_ans ans;

	if (m_recvbuf != NULL && m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		tbegin = (struct dvrtime *)m_recvbuf;
		tend = tbegin + 1;
		if (disk_unlockfile(tbegin, tend))
		{
			ans.anscode = ANSOK;
			ans.anssize = 0;
			ans.data = 0;
			Send(&ans, sizeof(ans));
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::Req2AdjTime()
{
	struct dvr_ans ans;
	if (m_recvbuf != NULL && m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		time_adjtime((struct dvrtime *)m_recvbuf);
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
	}
	else
	{
		DefaultReq();
	}
	return;
}

void dvrsvr::Req2SetLocalTime()
{
	struct dvr_ans ans;
	if (m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		time_setlocaltime((struct dvrtime *)m_recvbuf);
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
	}
	else
	{
		DefaultReq();
	}
}

void dvrsvr::Req2GetLocalTime()
{
	struct dvr_ans ans;
	struct dvrtime dvrt;

	time_now(&dvrt);
	ans.anscode = ANS2TIME;
	ans.anssize = sizeof(struct dvrtime);
	ans.data = 0;
	Send(&ans, sizeof(ans));
	Send(&dvrt, sizeof(dvrt));

	return;
}

void dvrsvr::Req2SetSystemTime()
{
	struct dvr_ans ans;
	time_setutctime((struct dvrtime *)m_recvbuf);
	ans.anscode = ANSOK;
	ans.anssize = 0;
	ans.data = 0;
	Send(&ans, sizeof(ans));
	return;
}

void dvrsvr::Req2GetSystemTime()
{
	struct dvr_ans ans;
	struct dvrtime dvrt;
	ans.anscode = ANS2TIME;
	ans.anssize = sizeof(struct dvrtime);
	ans.data = 0;
	Send(&ans, sizeof(ans));
	time_utctime(&dvrt);
	Send(&dvrt, sizeof(dvrt));
	return;
}

void dvrsvr::Req2GetZoneInfo()
{
	struct dvr_ans ans;
	char *zoneinfobuf;
	int i, zisize;
	array<string> zoneinfo;
	string s;
	char *p;
	readtxtfile("/usr/share/zoneinfo/zone.tab", zoneinfo);
	if (zoneinfo.size() > 1)
	{
		zoneinfobuf = (char *)mem_alloc(100000); // 100kbytes
		zisize = 0;
		for (i = 0; i < zoneinfo.size(); i++)
		{
			p = zoneinfo[i].getstring();
			while (*p == ' ' ||
				   *p == '\t' ||
				   *p == '\r' ||
				   *p == '\n')
				p++;
			if (*p == '#') // a comment line
				continue;

			// skip 2 colume
			while (*p != '\t' &&
				   *p != ' ' &&
				   *p != 0)
				p++;
			if (*p == 0)
				continue;

			while (*p == '\t' ||
				   *p == ' ')
				p++;

			while (*p != '\t' &&
				   *p != ' ' &&
				   *p != 0)
				p++;
			if (*p == 0)
				continue;

			while (*p == '\t' ||
				   *p == ' ')
				p++;
			if (*p == 0)
				continue;

			strcpy(&zoneinfobuf[zisize], p);
			zisize += strlen(p);
			zoneinfobuf[zisize++] = '\n';
		}
		zoneinfobuf[zisize++] = 0;
		int sendsize = sizeof(struct dvr_ans) + zisize;
		char *sendbuf = (char *)mem_alloc(sendsize);
		char *senddata = sendbuf + sizeof(struct dvr_ans);
		struct dvr_ans *pans = (struct dvr_ans *)sendbuf;
		memcpy(senddata, zoneinfobuf, zisize);
		mem_free(zoneinfobuf);
		pans->anscode = ANS2ZONEINFO;
		pans->anssize = zisize;
		pans->data = 0;
		Send(sendbuf, sendsize);
		mem_free(sendbuf);
		return;
	}
	else
	{
		config dvrconfig(dvrconfigfile);
		string tzi;

		// initialize enumkey
		int line = dvrconfig.findsection("timezones");
		while ((p = dvrconfig.enumkey(line)) != NULL)
		{
			tzi = dvrconfig.getvalue("timezones", p);
			if (tzi.length() <= 0)
			{
				s = p;
				zoneinfo.add(s);
			}
			else
			{
				zoneinfobuf = tzi.getstring();
				while (*zoneinfobuf != ' ' &&
					   *zoneinfobuf != '\t' &&
					   *zoneinfobuf != 0)
				{
					zoneinfobuf++;
				}
				s.setbufsize(strlen(p) + strlen(zoneinfobuf) + 10);
				sprintf(s.getstring(), "%s -%s", p, zoneinfobuf);
				zoneinfo.add(s);
			}
		}
		if (zoneinfo.size() > 0)
		{
			ans.anscode = ANS2ZONEINFO;
			ans.anssize = 0;
			ans.data = 0;
			for (i = 0; i < zoneinfo.size(); i++)
			{
				ans.anssize += zoneinfo[i].length() + 1;
			}
			ans.anssize += 1; // count in null terminate char
			Send(&ans, sizeof(ans));
			for (i = 0; i < zoneinfo.size(); i++)
			{
				Send(zoneinfo[i].getstring(), zoneinfo[i].length());
				Send((void *)"\n", 1);
			}
			Send((void *)"\0", 1); // send null char
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::Req2SetTimeZone()
{
	struct dvr_ans ans;
	if (m_recvbuf != NULL && m_req.reqsize > 2)
	{
		m_recvbuf[m_req.reqsize - 1] = 0;
		time_settimezone(m_recvbuf);

		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
	}
	else
	{
		DefaultReq();
	}
}

void dvrsvr::Req2GetTimeZone()
{
	struct dvr_ans ans;
	char timezone[256];

	ans.anscode = ANS2TIMEZONE;
	ans.data = 0;

	if (time_gettimezone(timezone))
	{
		ans.anssize = strlen(timezone) + 1;
		Send(&ans, sizeof(ans));
		Send(timezone, ans.anssize);
	}
	else
	{
		ans.anssize = 4;
		Send(&ans, sizeof(ans));
		Send((void *)"UTC", 4);
	}
	return;
}

void dvrsvr::ReqSetled()
{
	struct dvr_ans ans;
	ans.anscode = ANSOK;
	ans.anssize = 0;
	ans.data = 0;
	Send(&ans, sizeof(ans));
	setdio(m_req.data != 0);
}

void dvrsvr::ReqSetHikOSD()
{
	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels && m_req.reqsize >= (int)sizeof(hik_osd_type) && cap_channel[m_req.data]->type() == CAP_TYPE_HIKLOCAL)
	{
		capture *pcap = (capture *)cap_channel[m_req.data];
		struct hik_osd_type *posd = (struct hik_osd_type *)m_recvbuf;
		pcap->setremoteosd();
		pcap->setosd(posd);
		ans.anscode = ANSOK;
		ans.anssize = 0;
		ans.data = 0;
		Send(&ans, sizeof(ans));
		return;
	}
	DefaultReq();
}

void dvrsvr::Req2GetChState()
{
	struct dvr_ans ans;
	int ch = m_req.data;
	if (ch >= 0 && ch < cap_channels)
	{
		ans.anscode = ANS2CHSTATE;
		ans.anssize = 0;
		ans.data = 0;
		if (cap_channel[ch]->getsignal())
		{ // bit 0: signal ok.
			ans.data |= 1;
		}
		if (cap_channel[ch]->getmotion())
		{ // bit 1: motion detected
			ans.data |= 2;
		}
		Send(&ans, sizeof(ans));
	}
	else
	{
		DefaultReq();
	}
}

static char *www_genserialno(char *buf, int bufsize)
{
	time_t t;
	FILE *sfile;
	int i;
	unsigned int c;
	// generate random serialno
	sfile = fopen("/dev/urandom", "r");
	for (i = 0; i < (bufsize - 1); i++)
	{
		c = (((unsigned int)fgetc(sfile)) * 256 + (unsigned int)fgetc(sfile)) % 62;
		if (c < 10)
			buf[i] = '0' + c;
		else if (c < 36)
			buf[i] = 'a' + (c - 10);
		else if (c < 62)
			buf[i] = 'A' + (c - 36);
		else
			buf[i] = 'A';
	}
	buf[i] = 0;
	fclose(sfile);

	printf("genserial\n");

	sfile = fopen(WWWSERIALFILE, "w");
	time(&t);
	if (sfile)
	{
		fprintf(sfile, "%u %s %s", (unsigned int)t, buf, g_usbid);
		fclose(sfile);
	}
	return buf;
}

// initialize www data for setup page
static void www_setup()
{
	const char *getsetup = "cgi/getsetup";
	pid_t childpid;
	childpid = fork();
	if (childpid == 0)
	{
		chdir(WWWROOT);

		execl(getsetup, getsetup, NULL);
		exit(0);
	}
	else if (childpid > 0)
	{
		waitpid(childpid, NULL, 0);
	}
}

void dvrsvr::ReqCheckKey()
{
#if defined(TVS_APP) || defined(PWII_APP)
	struct key_data *key;
	struct dvr_ans ans;
	if (m_recvbuf && m_req.reqsize >= (int)sizeof(struct key_data))
	{

		key = (struct key_data *)m_recvbuf;

		// check key block ;
		if (strcmp(g_mfid, key->manufacturerid) == 0 &&
			memcmp(g_filekey, key->videokey, 256) == 0)
		{
			m_keycheck = 1;
			ans.data = 0;
			ans.anssize = 0;
			ans.anscode = ANSOK;
			Send(&ans, sizeof(ans));

			// do connection log
			dvr_logkey(1, key);

			return;
		}
		else
		{
			m_keycheck = 0;
			dvr_log("Key check failed!");
		}
	}
#endif
	DefaultReq();
}

void dvrsvr::Req2GetSetupPage()
{
#if defined(TVS_APP) || defined(PWII_APP)
	struct dvr_ans ans;
	char serno[60];
	char pageuri[100];
	if (m_keycheck || g_keycheck == 0)
	{
		// prepare setup web pages
		www_genserialno(serno, sizeof(serno));
		www_setup();

		sprintf(pageuri, "/system.html?ser=%s", serno);

		ans.anscode = ANS2SETUPPAGE;
		ans.anssize = strlen(pageuri) + 1;
		ans.data = 0;

		Send(&ans, sizeof(ans));
		Send(pageuri, ans.anssize);
		return;
	}
	DefaultReq();
#else
	struct dvr_ans ans;
	char *pageuri = "/system.html";

	// prepare setup pages
	//    system("preparesetup");

	ans.anscode = ANS2SETUPPAGE;
	ans.anssize = strlen(pageuri) + 1;
	ans.data = 0;

	Send(&ans, sizeof(ans));
	Send(pageuri, ans.anssize);
	return;
#endif
}

void dvrsvr::Req2GetStreamBytes()
{
	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		ans.data = cap_channel[m_req.data]->streambytes();
		ans.anssize = 0;
		ans.anscode = ANS2STREAMBYTES;
		Send(&ans, sizeof(ans));
		return;
	}
	DefaultReq();
}

void dvrsvr::DefaultReq()
{
	struct dvr_ans ans;
	ans.anscode = ANSERROR;
	ans.anssize = 0;
	ans.data = 0;
	Send(&ans, sizeof(ans));
}

static char *str_trimtail(char *str)
{
	int l = strlen(str);
	while (l > 0 && str[l - 1] <= ' ')
	{
		l--;
	}
	str[l] = 0;
	return str;
}

static void setpoliceid(const char *newid)
{
	array<string> idlist;
	FILE *fid;
	int i;

	if (newid == NULL)
	{
		g_policeid[0] = 0;
		return;
	}

	string snid;
	snid = newid;
	snid.trim();
	if (snid.length() <= 0)
	{
		g_policeid[0] = 0;
		return;
	}

	if (strcmp(g_policeid, snid) == 0)
		return;

	strcpy(g_policeid, snid);

	// save new police id list
	idlist[0] = g_policeid;
	fid = fopen(g_policeidlistfile, "r");
	if (fid)
	{
		for (i = 1; i < 20;)
		{
			if (fgets(snid.setbufsize(120), 119, fid))
			{
				snid.trim();
				if (snid.length() > 0 && strcmp(snid, g_policeid) != 0)
				{
					idlist[i++] = snid;
				}
			}
			else
			{
				break;
			}
		}
		fclose(fid);
	}

	// writing new id list to file
	fid = fopen(g_policeidlistfile, "w");
	if (fid)
	{
		for (i = 0; i < idlist.size(); i++)
		{
			fprintf(fid, "%s\n", (char *)idlist[i]);
		}
		fclose(fid);
	}

	// let screen display new policeid
	dvr_log("New Police ID detected : %s", g_policeid);
}

// Get single line of VRI
//      input dvrtime
void dvrsvr::ReqGetVri()
{
	if (m_recvbuf != NULL &&
		m_req.reqsize >= (int)sizeof(struct dvrtime))
	{
		struct dvr_ans ans;
		char *vribuf = vri_lookup((struct dvrtime *)m_recvbuf);
		if (vribuf != NULL)
		{
			ans.anscode = ANSGETVRI;
			ans.data = 1;
			ans.anssize = vri_isize();
			Send(&ans, sizeof(ans));
			Send(vribuf, ans.anssize);
			mem_free(vribuf);
			return;
		}
	}
	DefaultReq();
}

void dvrsvr::ReqSendData()
{
	struct dvr_ans ans;

	// dvr_log( "REQSendData: %d - %d ", m_req.data, m_req.reqsize );

	switch (m_req.data)
	{
	case PROTOCOL_PW_SETPOLICEID:
		if (m_recvbuf && m_req.reqsize > 0)
		{
			// select a new offer ID.
			setpoliceid(m_recvbuf);
		}
		else
		{
			setpoliceid("");
		}
		//ans.anscode = ANSGETDATA ;
		ans.anscode = ANSOK;
		ans.data = 0;
		ans.anssize = 0;
		Send(&ans, sizeof(ans));
		break;

	case PROTOCOL_PW_SETVRILIST:

		//dvr_log( "PW TAG : %d : %s",  m_req.reqsize, m_recvbuf );

		if (m_recvbuf && m_req.reqsize > 0)
		{
			// select a new offer ID.
			m_recvbuf[m_req.reqsize]=0;
			vri_log(m_recvbuf);
		}
		ans.anscode = ANSOK;
		ans.data = 0;
		ans.anssize = 0;
		Send(&ans, sizeof(ans));
		break;

	case PROTOCOL_PW_SETCOVERTMODE:
		if (m_recvbuf && m_req.reqsize > 0)
		{
			dio_covert_mode(*m_recvbuf != 0);
		}
		else
		{
			dio_covert_mode(0);
		}

		ans.anscode = ANSOK;
		ans.data = 0;
		ans.anssize = 0;
		Send(&ans, sizeof(ans));
		break;

	default:
		DefaultReq();
	}
}

void dvrsvr::ReqGetData()
{
	struct dvr_ans ans;
	int i;

	switch (m_req.data)
	{
	case PROTOCOL_PW_GETSTATUS:
		// return status in bytes, one bytes for a channel
		//     in each byte, bit 0: sig, bit 1: motion, bit 2: rec
		unsigned char chstatus[MAXCHANNEL];
		for (i = 0; i < cap_channels; i++)
		{
			chstatus[i] = 0;
			if (cap_channel != NULL && cap_channel[i] != NULL)
			{
				if (cap_channel[i]->getsignal() == 0)
					chstatus[i] |= 1; // signal lost?
				if (cap_channel[i]->getmotion())
					chstatus[i] |= 2; // motion detections
				if (rec_state(i))
					chstatus[i] |= 4; // channel recording?
				if (rec_staterec(i))
					chstatus[i] |= 8; // channel force recording mode
				// force record channel
				chstatus[i] |= rec_forcechannel(i) << 4;
			}
		}
		ans.anscode = ANSGETDATA;
		ans.data = 0;
		ans.anssize = cap_channels;
		Send(&ans, sizeof(ans));
		Send(&chstatus, ans.anssize);

		break;

	case PROTOCOL_PW_GETSYSTEMSTATUS:
	{
		char *xbuf = new char[4096];
		char *pbuf = xbuf;
		int n;

		static float x_uptime = 0.0;
		static float x_idletime = 0.0;
		float uptime;
		float idletime;

		FILE *stfile = fopen("/proc/uptime", "r");
		if (stfile)
		{
			fscanf(stfile, "%f %f", &uptime, &idletime);
			fclose(stfile);
		}

		float usage;
		float s_uptime = uptime - x_uptime;
		if (s_uptime < 0.01)
		{
			usage = 100.0;
		}
		else
		{
			usage = 100.0 * (s_uptime - (idletime - x_idletime)) / s_uptime;
		}
		n = sprintf(pbuf, "CPU_USAGE: %f\n", usage);
		pbuf += n;

		// Get Mem state
		int mem_total = 0, mem_free = 0;
		stfile = fopen("/proc/meminfo", "r");
		if (stfile)
		{
			char mbuf[256];
			while (fgets(mbuf, 256, stfile))
			{
				char header[20];
				int v;
				if (sscanf(mbuf, "%19s%d", header, &v) == 2)
				{
					if (strcmp(header, "MemTotal:") == 0)
					{
						mem_total = v;
					}
					else if (strcmp(header, "MemFree:") == 0)
					{
						mem_free += v;
					}
					else if (strcmp(header, "Inactive:") == 0)
					{
						mem_free += v;
					}
				}
			}
			fclose(stfile);
		}
		n = sprintf(pbuf, "MEM_TOTAL: %d\nMEM_FREE: %d\n", mem_total, mem_free);
		pbuf += n;

		// Get Disk Space

		int disk_free = 0;
		int disk_total = 0;
		dir disks(VAR_DIR "/disks");
		while (disks.find())
		{
			if (disks.isdir())
			{
				struct statfs stfs;
				if (statfs(disks.pathname(), &stfs) == 0)
				{
					disk_free += stfs.f_bavail / ((1024 * 1024) / stfs.f_bsize);
					disk_total += stfs.f_blocks / ((1024 * 1024) / stfs.f_bsize);
				}
			}
		}
		n = sprintf(pbuf, "DISK_TOTAL: %d\nDISK_FREE: %d\n", disk_total, disk_free);
		pbuf += n;

		// Get System Temperature
		n = sprintf(pbuf, "SYSTEM_TEMPERATURE: %d\n", dio_get_temperature(0));
		pbuf += n;
		pbuf++;

		ans.anscode = ANSGETDATA;
		ans.data = 0;
		ans.anssize = pbuf - xbuf;
		Send(&ans, sizeof(ans));
		Send(xbuf, ans.anssize);
		delete xbuf;
	}

	break;

	// get police id list
	case PROTOCOL_PW_GETPOLICEIDLIST:

		char policeid[20][64];
		memset(policeid, 0, sizeof(policeid));

		FILE *fpid;
		fpid = fopen(g_policeidlistfile, "r");
		i = 0;
		int npid;
		npid = 0;
		if (fpid)
		{
			string spoliceid;
			for (i = 0; i < 20; i++)
			{
				if (fgets(spoliceid.setbufsize(64), 63, fpid))
				{
					spoliceid.trim();
					if (spoliceid.length() > 0)
					{
						strcpy(policeid[npid], spoliceid);
						npid++;
					}
				}
				else
				{
					break;
				}
			}
			fclose(fpid);
		}
		ans.anscode = ANSGETDATA;
		ans.data = npid;
		ans.anssize = npid * 64;
		Send(&ans, sizeof(ans));
		Send(&policeid, ans.anssize);

		break;

	// get vri list
	case PROTOCOL_PW_GETVRILISTSIZE:
	{
		char vrics[80];
		int l = sprintf(vrics, "1,%d", vri_isize());
		ans.anscode = ANSGETDATA;
		ans.data = 0;
		ans.anssize = l + 1;
		Send(&ans, sizeof(ans));
		Send((void *)vrics, ans.anssize);
	}
	break;

	// get vri list
	case PROTOCOL_PW_GETVRILIST:
	{
		ans.anscode = ANSGETDATA;
		ans.data = 0;
		char *vribuf = vri_lookup(&stream_time);
		if (vribuf != NULL)
		{
			ans.anssize = vri_isize();
			Send(&ans, sizeof(ans));
			Send(vribuf, ans.anssize);
			mem_free(vribuf);
		}
		else
		{
			ans.anssize = 0;
			Send(&ans, sizeof(ans));
		}
	}
	break;

	case PROTOCOL_PW_GETDISKINFO:
	{
		char *info = new char[2048];
		int l = 0;
		for (i = 0; i < 3; i++)
		{
			l += sprintf(info + l, "%d,%d,%d,%d,%d,%d,%d,%d\n",
						 i,
						 pw_disk[i].mounted,
						 pw_disk[i].totalspace,
						 pw_disk[i].freespace,
						 pw_disk[i].full, // disk full
						 pw_disk[i].l_len,
						 pw_disk[i].n_len,
						 pw_disk[i].reserved);
		}
		// print dio app modes
		l += sprintf(info + l, "100,%d\n", dio_curmode());

		ans.anscode = ANSGETDATA;
		ans.data = PROTOCOL_PW_GETDISKINFO;
		ans.anssize = l;
		Send(&ans, sizeof(ans));
		Send(info, ans.anssize);
		delete[] info;
	}
	break;

	case PROTOCOL_PW_GETEVENTLIST:
	{
		// get list of user events (TRACE MARK events for now)
		extern const char *logfilelnk;
		char *fbuf = NULL;
		char *fbend = NULL;
		FILE *flog = fopen(logfilelnk, "r");
		if (flog != NULL)
		{
			fseek(flog, 0L, SEEK_END);
			int bufsize = ftell(flog);
			rewind(flog);
			if (bufsize > 1000)
			{
				bufsize += 1024;
				fbuf = new char[bufsize];
				fbend = fbuf;
				while (bufsize - (fbend - fbuf) >= 1024 && fgets(fbend, 1024, flog) != NULL)
				{
					if (strstr(fbend, "TraceMark") != NULL)
					{
						fbend += strlen(fbend);
					}
				}
			}
			fclose(flog);
		}

		ans.anscode = ANSGETDATA;
		ans.data = PROTOCOL_PW_GETEVENTLIST;
		ans.anssize = fbend - fbuf;
		Send(&ans, sizeof(ans));
		if (fbuf != NULL)
		{
			if (ans.anssize > 0)
			{
				Send(fbuf, ans.anssize);
			}
			delete[] fbuf;
		}
	}
	break;

	default:
		DefaultReq();
	}
}

#if defined(PWII_APP)
void dvrsvr::Req2Keypad()
{
	struct dvr_ans ans;
	ans.data = 0;
	ans.anssize = 0;
	ans.anscode = ANSOK;
	Send(&ans, sizeof(ans));
	//screen_key( (int)(m_req.data&0xff), (int)(m_req.data>>8)&1 );
	event_key((int)(m_req.data & 0xff), (int)(m_req.data >> 8) & 1);
	return;
}

void dvrsvr::Req2PanelLights()
{
	struct dvr_ans ans;
	if (m_req.data >= 0 && m_req.data < cap_channels)
	{
		ans.data = cap_channel[m_req.data]->streambytes();
		ans.anssize = 0;
		ans.anscode = ANS2STREAMBYTES;
		Send(&ans, sizeof(ans));
		return;
	}
}

#endif

// client side support

// open live stream from remote dvr
int dvr_openlive(int sockfd, int channel, struct hd_file *hd264)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQREALTIME;
	req.data = channel;
	req.reqsize = 0;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSREALTIMEHEADER && ans.anssize == (int)sizeof(struct hd_file))
		{
			if (net_recv(sockfd, hd264, sizeof(struct hd_file)))
			{
				return 1;
			}
		}
	}
	return 0;
}

// get remote dvr system setup
// return 1:success
//        0:failed
int dvr_getsystemsetup(int sockfd, struct system_stru *psys)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQGETSYSTEMSETUP;
	req.data = 0;
	req.reqsize = 0;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSSYSTEMSETUP && ans.anssize >= (int)sizeof(struct system_stru))
		{
			if (net_recv(sockfd, psys, sizeof(struct system_stru)))
			{
				return 1;
			}
		}
	}
	return 0;
}

// set remote dvr system setup
// return 1:success
//        0:failed
int dvr_setsystemsetup(int sockfd, struct system_stru *psys)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQSETSYSTEMSETUP;
	req.data = 0;
	req.reqsize = sizeof(struct system_stru);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, psys, sizeof(struct system_stru));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			dvr_log("dvr_setsystemsetup ok");
			return 1;
		}
	}
	dvr_log("dvr_setsystemsetup failed");
	return 0;
}

// get channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_getchannelsetup(int sockfd, int channel, struct DvrChannel_attr *pchannelattr)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQGETCHANNELSETUP;
	req.data = channel;
	req.reqsize = 0;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSCHANNELSETUP && ans.anssize >= (int)sizeof(struct DvrChannel_attr))
		{
			if (net_recv(sockfd, pchannelattr, sizeof(struct DvrChannel_attr)))
			{
				return 1;
			}
		}
	}
	return 0;
}

// set channel setup of remote dvr
// return 1:success
//        0:failed
int dvr_setchannelsetup(int sockfd, int channel, struct DvrChannel_attr *pchannelattr)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQSETCHANNELSETUP;
	req.data = channel;
	req.reqsize = sizeof(struct DvrChannel_attr);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, pchannelattr, sizeof(struct DvrChannel_attr));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

int dvr_sethikosd(int sockfd, int channel, struct hik_osd_type *posd)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQSETHIKOSD;
	req.data = channel;
	req.reqsize = sizeof(struct hik_osd_type);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, posd, sizeof(struct hik_osd_type));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

// set remote dvr time
// return 1: success, 0:failed
int dvr_settime(int sockfd, struct dvrtime *dvrt)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2SETLOCALTIME;
	req.data = 0;
	req.reqsize = sizeof(struct dvrtime);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, dvrt, sizeof(struct dvrtime));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_gettime(int sockfd, struct dvrtime *dvrt)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2GETLOCALTIME;
	req.data = 0;
	req.reqsize = 0;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANS2TIME && ans.anssize >= (int)sizeof(struct dvrtime))
		{
			if (net_recv(sockfd, dvrt, sizeof(struct dvrtime)))
			{
				return 1;
			}
		}
	}
	return 0;
}

// get remote dvr time in utc
// return 1: success, 0:failed
int dvr_getutctime(int sockfd, struct dvrtime *dvrt)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2GETSYSTEMTIME;
	req.data = 0;
	req.reqsize = 0;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANS2TIME && ans.anssize >= (int)sizeof(struct dvrtime))
		{
			if (net_recv(sockfd, dvrt, sizeof(struct dvrtime)))
			{
				return 1;
			}
		}
	}
	return 0;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_setutctime(int sockfd, struct dvrtime *dvrt)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2SETSYSTEMTIME;
	req.data = 0;
	req.reqsize = sizeof(struct dvrtime);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, dvrt, sizeof(struct dvrtime));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

// get remote dvr time
// return 1: success, 0:failed
int dvr_adjtime(int sockfd, struct dvrtime *dvrt)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2ADJTIME;
	req.data = 0;
	req.reqsize = sizeof(struct dvrtime);
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, dvrt, sizeof(struct dvrtime));
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

// set remote dvr time zone (for IP cam)
// return 1: success, 0: failed
int dvr_settimezone(int sockfd, char *tzenv)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2SETTIMEZONE;
	req.data = 0;
	req.reqsize = strlen(tzenv) + 1;
	net_clean(sockfd);
	net_send(sockfd, &req, sizeof(req));
	net_send(sockfd, tzenv, req.reqsize);
	if (net_recv(sockfd, &ans, sizeof(ans)))
	{
		if (ans.anscode == ANSOK)
		{
			return 1;
		}
	}
	return 0;
}

// get channel status
//    return value:
//    bit 0: signal lost
//    bit 1: motion detected
//    -1 : error
int dvr_getchstate(int sockfd, int ch)
{
	struct dvr_req req;
	struct dvr_ans ans;

	req.reqcode = REQ2GETCHSTATE;
	req.data = ch;
	req.reqsize = 0;
	net_clean(sockfd);
	if (net_send(sockfd, &req, sizeof(req)))
	{
		if (net_recv(sockfd, &ans, sizeof(ans)))
		{
			if (ans.anscode == ANS2CHSTATE)
			{
				return ans.data;
			}
		}
	}
	return -1;
}
