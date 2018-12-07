#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>    
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
#include <queue>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>
#include <fnmatch.h>
#include <termios.h>
#include <stdarg.h>
#include <sys/times.h>
#include <ctime>
#include "json/json.h"

#define   BODYCAMERAIPADDR   "192.168.1.90"
static int app_run=1;
static pthread_t bodycameraTcpThreadId;
static pthread_t bodycameraCtlThreadId;

static int gRecordMode=0;

enum {IDLE=0,CAMERA_RECORD,CAMERA_STOP,NONE};

void sig_handler(int signum)
{
	if (signum == SIGTERM)
	{
		app_run = 0;
	}
	else if (signum == SIGQUIT)
	{
		app_run = 0;
	}
	else if (signum == SIGINT)
	{
		app_run = 0;
	}
	else if (signum == SIGUSR2)
	{
		app_run = 0;
	}
	else if (signum == SIGUSR1)
	{
		app_run = 0;
	}
}

int sockRecvOk(int sockfd, int tout)
{
	struct timeval timeout;
	timeout.tv_sec = tout / 1000000;
	timeout.tv_usec = tout % 1000000;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	if (select(sockfd + 1, &fds, NULL, NULL, &timeout) > 0)
	{
		return FD_ISSET(sockfd, &fds);
	}
	else
	{
		return 0;
	}
};

int sockSendOk(int sockfd, int tout)
{
	struct timeval timeout;
	timeout.tv_sec = tout / 1000000;
	timeout.tv_usec = tout % 1000000;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	if (select(sockfd + 1, NULL, &fds, NULL, &timeout) > 0)
	{
		return FD_ISSET(sockfd, &fds);
	}
	else
	{
		return 0;
	}
};

int recvDataOverSocket(int sockfd, void *data, int datasize)
{
	int bytes;
	int total = 0;
	char *cbuf = (char *)data;
	while (1)
	{
		if (sockRecvOk(sockfd, 5000000))
		{
			bytes = recv(sockfd, cbuf, datasize, 0);
			printf("bytes:%d received\n", bytes);
			if (bytes <= 0)
				break;
			total += bytes;
			datasize -= bytes;
			if (datasize == 0)
				break;
			cbuf += bytes;
		}
		else
		{
			break;
		}
		if (app_run == 0)
			break;
	}
	return total;
};

int sendDataOverSocket(int sockfd,void* data,int datasize){
		int bytes;
		char * cbuf=(char*) data;
		
		while(1){
	bytes=send(sockfd,cbuf, datasize,0);
	printf("bytes:%d sent\n",bytes);
	if(bytes<0)
		return -1;
	datasize-=bytes;
	if(datasize==0)
		break;
				cbuf+=bytes;
			 if(app_run==0)
		break;
		}
		return 0;
};


int parseStartSession(char* pBuf,int mBufSize){
	int mToken=-1;
	Json::Value root;
	Json::Reader reader;
	bool success;

	char *pBegin=pBuf;
	char *pEnd=pBuf+mBufSize-1;;
	success = reader.parse(pBegin,pEnd, root, false);
	if(!success)
		return mToken;
	mToken=root.get("param","1").asInt();
	return mToken;
}

void parseNotification(char* pBuf,int mBufSize){
	int msg_id;
	std::string mType;
	Json::Value root;
	Json::Reader reader;
	char mNotification[128];
	bool success;

	char *pBegin=pBuf;
	char *pEnd=pBuf+mBufSize-1;;
	success = reader.parse(pBegin,pEnd, root, false);
	if(!success)
		return;
	msg_id=root.get("msg_id","7").asInt();  
	if(msg_id==7){
		 mType=root.get("type","hello").asString();
		 sprintf(mNotification,"%s",mType.c_str());
		 printf("Received notification:%s\n",mNotification);
	} else {
		printf("Message:%s is not notification\n",pBuf); 
	} 
	return;
}

void *bodyCameraTcpThread(void *para){  
		unsigned short port ; /* port number client will connect to */
		struct sockaddr_in server; /* server address */
	 
		int s; /* client socket */ 
		int mBufLen=0;
		int mTokenNum=-1;
		int mRecordMode=IDLE;
		char srBuff[512];
		
		port = 7878;   
		s = socket(AF_INET, SOCK_STREAM, 0);
		if( s == -1 ) {
			printf("Socket was not created.\n");
			return NULL;
		}
	 // printf("Socket created successfully.\n");
		while(app_run){
			server.sin_family = AF_INET; /* set up the server name */
			server.sin_port = htons(port);
			server.sin_addr.s_addr = inet_addr(BODYCAMERAIPADDR);
					//  printf("connect to ipaddr:%s\n",gPeuAddr);
			/* connect to the server */
			if( connect(s,(const struct sockaddr *)&server, sizeof(server)) < 0) {
							usleep(1000);
				continue;
			}
			break;
		}
		
		//send start session
		sprintf(srBuff,"{\"token\":0,\"msg_id\":257}");
		mBufLen=strlen(srBuff)+1;
		if(sendDataOverSocket(s,srBuff,mBufLen)<0){
			 printf("send start session failed\n");
			 close(s);
			 s=0;
	} 
	else {
		while(app_run){
			mBufLen=recvDataOverSocket(s,srBuff,128);
			if(mBufLen>0){
				srBuff[mBufLen]='\0';
				printf("Start session return:%s\n",srBuff);	     
				mTokenNum=parseStartSession(srBuff,mBufLen+1);
				break;
			}
		}
		}
		printf("Token Num:%d\n",mTokenNum);

		while(app_run){
				//check whether the socket is still valid. If not,reconnect it again
		while(app_run){
			if(s>0)
				break;
			mTokenNum=-1;
				s = socket(AF_INET, SOCK_STREAM, 0);
				if(s<0){	      
				 usleep(1000);
				 continue;
			}
			while(app_run){
					server.sin_family = AF_INET; /* set up the server name */
					server.sin_port = htons(port);
					server.sin_addr.s_addr = inet_addr(BODYCAMERAIPADDR);
				//  printf("connect to ipaddr:%s\n",gPeuAddr);
					/* connect to the server */
					if( connect(s,(const struct sockaddr *)&server, sizeof(server)) < 0) {
						usleep(1000);
						continue;
					}
					
					break;
			}
			break;	    	  
		}     
		if(mTokenNum<0){
			sprintf(srBuff,"{\"token\":0,\"msg_id\":257}");
			mBufLen=strlen(srBuff)+1;
			if(sendDataOverSocket(s,srBuff,mBufLen)<0){
				printf("send start session failed\n");
				close(s);
				s=0;
			} else {
				while(app_run){
				mBufLen=recvDataOverSocket(s,srBuff,128);
				if(mBufLen>0){
				srBuff[mBufLen]='\0';
				printf("Start session return:%s\n",srBuff);	     
				mTokenNum=parseStartSession(srBuff,mBufLen+1);
				break;
				}
				}
			}	  	  
		}
		if(mTokenNum<0){
			usleep(1000);
			continue;
		}
		//check whether there are notifications from body bodyCamera
		mBufLen=recvDataOverSocket(s,srBuff,128);
		if(mBufLen>0){
			 // parseNotification(srBuff,mBufLen);
			 srBuff[mBufLen]='\0';
			 printf("get data from Camera:%s\n",srBuff);	  
		}
	//check the status of body camera
	if(s>0){
			if(mTokenNum>0){
		sprintf(srBuff,"{\"token\":%d,\"msg_id\":1,\"type\":\"app_status\"}",mTokenNum);
		mBufLen=strlen(srBuff)+1;
		if(sendDataOverSocket(s,srBuff,mBufLen)<0){
			printf("send get camera current status failed\n");
			close(s);
			s=0;
		} else {
			while(app_run){
					mBufLen=recvDataOverSocket(s,srBuff,128);
					if(mBufLen>0){
			srBuff[mBufLen]='\0';
			printf("get app_status return:%s\n",srBuff);	     
			break;
					}
			}
		}	  	
				
			}
	}
	//check the camera control status
	if(mRecordMode!=gRecordMode){
		 if(gRecordMode==CAMERA_RECORD){
				if(s>0){
			//send recording control to camera
			sprintf(srBuff,"{\"token\":%d,\"msg_id\":513}",mTokenNum);
			mBufLen=strlen(srBuff)+1;
			if(sendDataOverSocket(s,srBuff,mBufLen)<0){
				printf("send send start recording failed\n");
				close(s);
				s=0;
				continue;
			} else {
				while(app_run){
			mBufLen=recvDataOverSocket(s,srBuff,128);
			if(mBufLen>0){
				srBuff[mBufLen]='\0';
				printf("send start recording return:%s\n",srBuff);	     
				break;
			}
				}
			}	  		       
				}
		 } else if(gRecordMode==CAMERA_STOP){
				if(s>0){
			//send stop recording control to camera
			sprintf(srBuff,"{\"token\":%d,\"msg_id\":514}",mTokenNum);
			mBufLen=strlen(srBuff)+1;
			if(sendDataOverSocket(s,srBuff,mBufLen)<0){
				printf("send send stop recording failed\n");
				close(s);
				s=0;
				continue;
			} else {
				while(app_run){
			mBufLen=recvDataOverSocket(s,srBuff,128);
			if(mBufLen>0){
				srBuff[mBufLen]='\0';
				printf("send stop recording return:%s\n",srBuff);	     
				break;
			}
				}
			}	  		     
				}
		 }
		 mRecordMode=gRecordMode;
	}
	
	
	usleep(1000);
		}
		//send stop session
		if(s>0){
				printf("Token:%d ============\n",mTokenNum);
				if(mTokenNum>0){
			sprintf(srBuff,"{\"token\":%d,\"msg_id\":258}",mTokenNum);
			mBufLen=strlen(srBuff)+1;
			if(sendDataOverSocket(s,srBuff,mBufLen)<0){
				printf("send start session failed\n");
				close(s);
				s=0;
			} else {
				while(1){
			mBufLen=recvDataOverSocket(s,srBuff,128);
			if(mBufLen>0){
				srBuff[mBufLen]='\0';
				printf("Stop session return:%s\n",srBuff);	     
				break;
			}
			if(app_run==0)
				break;
				}
			}
	}
		}
		if(s>0)
			close(s);
		printf("Done\n");
}

void *bodyCameraCtlThread(void *para){
	int startCount=0; 
	gRecordMode=IDLE; 
	while(app_run){ 
		if(startCount==100){ 
			if(gRecordMode==IDLE){ 
				gRecordMode=CAMERA_RECORD; 
			} else if(gRecordMode==CAMERA_STOP){ 
				gRecordMode=CAMERA_RECORD; 
			} else if(gRecordMode==CAMERA_RECORD){ 
				gRecordMode=CAMERA_STOP; 
			} 
		}
		if(startCount==600){ 
			startCount=0; 
		} 
		sleep(1); 
		startCount++; 
	}
}

void appinit(){
	 bodycameraTcpThreadId=0;
	 bodycameraCtlThreadId=0;
	 pthread_create(&bodycameraTcpThreadId,NULL,bodyCameraTcpThread,NULL);
	 pthread_create(&bodycameraCtlThreadId,NULL,bodyCameraCtlThread,NULL);   
}

void appfinish(){
	 if(bodycameraTcpThreadId){
		 pthread_join(bodycameraTcpThreadId,NULL);
		 bodycameraTcpThreadId=0;
	} 
	if(bodycameraCtlThreadId){
		 pthread_join(bodycameraCtlThreadId,NULL);
		 bodycameraCtlThreadId=0;
	}
}
int main(int argc, char *argv[]){
	// setup signal handler	
		signal(SIGQUIT, sig_handler);
		signal(SIGINT,  sig_handler);
		signal(SIGTERM, sig_handler);
		signal(SIGUSR2, sig_handler);
		// ignor these signal
		signal(SIGPIPE, SIG_IGN);	
		bodycameraTcpThreadId=0;
		app_run=1;
		appinit();
		while(app_run){
			sleep(1); 
		}
		appfinish();  
}
