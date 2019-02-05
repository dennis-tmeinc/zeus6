
#include "list.h"
#include "rconn.h"

#ifndef __ADB_CONN__
#define __ADB_CONN__

class adb_conn
	: public rconn
{
protected:
	int pw_service;

	// connect to remote server, make it extendable
	virtual int connect_server();

public:
	char m_serialno[128];
	adb_conn(char* serialno);
};

int adb_setfd(struct pollfd* pfd, int max);
void adb_process();
void adb_reset();

#endif // __ADB_CONN__
