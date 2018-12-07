#ifndef __BODYCAM_H__
#define __BODYCAM_H__

#define MAX_BODYCAM (16)

extern int g_runtime;

int runtime();
void setMaxWait(int ms);
time_t getTime(char *timebuf);

void local_sendVKRec();

// bodycam msg id
#define ID_AMBA_GET_SETTING (1)
#define ID_AMBA_SET_SETTING (2)
#define ID_AMBA_START_SESSION (257)
#define ID_AMBA_STOP_SESSION (258)
#define ID_AMBA_RECORD_START (513)
#define ID_AMBA_RECORD_STOP (514)

// AMBA settings types
#define AMBA_SETTING_TYPE_CLOCK ("camera_clock")
#define AMBA_SETTING_TYPE_STATUS ("app_status")

// AMBA button inputs
#define AMBA_BUTTON_STOP ("STOP BUTTON")
#define AMBA_BUTTON_RECORD ("RECORD BUTTON")
#define AMBA_BUTTON_POWER ("POWER BUTTON")
#define AMBA_BUTTON_LED ("LED BUTTON")
#define AMBA_BUTTON_LIGHT ("LIGHT BUTTON")

#define BODYCAM_RECVBUFSIZE (2048)

class bodycam
{

  private:
	string camip; // camera ip or host name
	int camport;  // camera control port
	int sock;
	struct pollfd *sfd; // poll event
	int activetime;		// io time
	int waittime;		// idle time to wait for next getStatus ( ping )

	int msg_token; // session number return from camera

	int enable;			// enable?
	int dvrTrigger;		// trigger (bodycam) recording by DVR%
	int bodycamTrigger; // trigger (DVR) recording by bodycam
	int recordmode;		// body camera recording mode, 0: not recording, 1: by bodycam buttons, 2: trigger by DVR

	// dio mapping
	int *dvr_status;
	int *rec_trigger;
	int *rec_button;
	int *rec_status;

	int save_trigger;
	int save_button;
	int save_status;

	char recvBuf[BODYCAM_RECVBUFSIZE];
	int recvPos;

  public:
	bodycam(int num); // num: serial number of camera, start form 1
	virtual ~bodycam();

	int dvrRec;
	// DVR recording triggered? 0: 1: triggered
	int isDVRTriggered()
	{
		if (dvr_status != NULL)
		{
			int r = (*dvr_status) & 2;
			if (dvrRec != r)
			{
				dvrRec = r;
				return 1;
			}
		}
		return 0;
	}
	// camera status
	//

	// methods
	int process();
	int setpoll(struct pollfd *pfd, int max);

	void wait(int idlems); // wait for next process (getStatus)

  protected:
	int sendCmd(int msg_id);
	int sendCmd(int msg_id, char *type);
	int sendCmd(int msg_id, char *type, char *param);

	int onrecv();
	void onKeyInput(char *keyInput);

	void onRecord();
	void onStop();

	void getStatus();
	void startRecord();
	void stopRecord();
	void syncTime();

	int send(char *buf);
	int receive();
	int getSession();
};

#endif
