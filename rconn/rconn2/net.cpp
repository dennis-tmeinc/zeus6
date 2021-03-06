/*
  net.cpp

    network interface

*/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>

#include "net.h"

// get addrinfo 
static struct addrinfo * net_addrinfo( const char *netname, int port, int family, int socktype )
{	
    struct addrinfo hints;
    struct addrinfo *res;
    char service[20] ;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family ;
    hints.ai_socktype = socktype ;
    if( netname==NULL ) {
        hints.ai_flags = AI_PASSIVE ;
    }
    sprintf(service, "%u", port);
    res = NULL;
    if (getaddrinfo( netname, service, &hints, &res) == 0) {
		return res ;
	}
    return NULL;
}

// get sockad from string address
int net_addr(struct sockad *addr, const char *netname, int port, int family )
{	
    struct addrinfo *res;
    res = net_addrinfo( netname, port, family, SOCK_STREAM );
    if( res ) {
		memcpy(addr, res->ai_addr, res->ai_addrlen);
		addr->len = res->ai_addrlen;
		freeaddrinfo(res);
        return 1;
	}
	return 0 ;
}

// get sockad for AF_LOCAL, alway use abstract socket name
int net_addr_local(struct sockad *addr, const char *localname)
{	
    addr->s.saddr_un.sun_family = AF_LOCAL;
   
	// use abstract socket name only
	addr->s.saddr_un.sun_path[0] = 0 ;
	strcpy( addr->s.saddr_un.sun_path+1, localname );
	addr->len = offsetof(struct sockaddr_un, sun_path) + strlen(addr->s.saddr_un.sun_path+1) + 1 ;
	
	return 1;
}

// return host name and port number
char * net_name(struct sockad *sad, char * host, int hostlen )
{
    if( getnameinfo( SADDR(*sad), sad->len, host, hostlen, NULL, 0, NI_NUMERICHOST )==0 ) {
		return host ;
    }
    return NULL;
}

// return port number from sockad
int net_port( struct sockad *sad )
{
	char service[80] ;
	if( getnameinfo( SADDR(*sad), sad->len, NULL, 0, service, 80, NI_NUMERICSERV )==0 ) {
		return atoi(service);
    }
    return 0;
}

int net_connect( const char * host, int port )
{
	int s = -1 ;
	struct addrinfo *rai;
	struct addrinfo *ai;

    rai = net_addrinfo( host, port, AF_INET, SOCK_STREAM );
    if( rai ) {
		ai = rai ;
		while( ai!=NULL ) {
			s = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if(s>=0) {
				
				int flags=fcntl(s, F_GETFL, NULL);
                fcntl(s, F_SETFL, flags|O_NONBLOCK);
                
                int c = connect(s, ai->ai_addr, ai->ai_addrlen) ;
                if( c==0 ) {
					fcntl(s, F_SETFL, flags);
					break;
				}
				else if( errno==EINPROGRESS ) {
					if( net_srdy( s, 300000000 ) ) {
						fcntl(s, F_SETFL, flags);
						break ;
					}
				}
				
				close(s);
				s = -1 ;
			}
			ai = ai->ai_next ;
		}
		freeaddrinfo(rai);
	}
	return s ;
}

// connect to AF_LOCAL socket , always use abstract socket name
int net_connect_local(const char * sname)
{
	int s;
	struct sockad addr ;

    s = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(s < 0) return -1;

	net_addr_local( &addr, sname );
    if(connect(s, SADDR(addr), addr.len) < 0) {
		close(s);
		return -1 ;
    }

    return s;
}

int net_bind(int socket, int port, char * host)
{
    struct sockad sad ;
    net_addr(&sad, host, port);
    return bind(socket, SADDR(sad), sad.len) ;
}

int net_listen( int port )
{
	int s = -1 ;
	struct addrinfo *rai;
	struct addrinfo *ai;
    rai = net_addrinfo( NULL, port, AF_UNSPEC, SOCK_STREAM );
    if( rai ) {
		ai = rai ;
		while( ai!=NULL ) {
			s = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if(s>=0) {
				int reuse = 1 ;
				setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int) ) ;
				if( bind(s, ai->ai_addr, ai->ai_addrlen) == 0 ) {
					listen( s, 10 );
					break ;
				}
				close(s);
				s=-1 ;
			}
			ai = ai->ai_next ;
		}
		freeaddrinfo(rai);
	}
	return s ;
}

// get my routable ip address (not public ip)
unsigned int net_myip(struct sockad *addr)
{
    struct sockad sad ;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if( s>=0 ) {
		// try connect
		net_addr( &sad, "8.8.8.8", 43 ) ;
		connect(s, SADDR(sad), sad.len) ;
		getsockname( s, SADDR(sad), &(sad.len) );
		close(s);
        if( addr != NULL ) {
            *addr = sad ;
        }
    }
    return IP4_ADDR(sad);
}

int net_available(int s)
{
	int av = 0 ;
	ioctl(s, FIONREAD, &av);
	if( av<0 ) av = 0 ;
	return av ;
}

// ready for receive
int net_rrdy( int s, int usdelay)
{
	struct pollfd fds;
	fds.fd = s ;
	fds.events = POLLIN ;
	fds.revents = 0 ;
	return poll( &fds, 1, usdelay/1000 );
}

// ready for send
int net_srdy( int s, int usdelay)
{
	struct pollfd fds;
	fds.fd = s ;
	fds.events = POLLOUT ;
	fds.revents = 0 ;
	return poll( &fds, 1, usdelay/1000 );
}

int net_send(int s, void * packet, int psize )
{
   	return send(s, packet, psize, 0 );
}

int net_sendall( int s, char * data, int dsize )
{
	int ss = 0 ;
	while( ss < dsize ) {
		int w = net_send( s, (void *)(data+ss), dsize-ss );		
		if( w > 0 ) {
			ss+=w ;
		}
		else if( w<0 && errno != EWOULDBLOCK ) {
			return 0 ;
		}
		else {
			net_srdy( s, 2000000 );
		}
	}
	return ss ;
}

int net_sendx(int s, void * packet, int psize )
{
	return net_sendall(s, (char *)packet, psize);
}

int net_recv(int s, void * packet, int psize )
{
    return recv( s, packet, psize, 0 );
}

int net_recvx(int s, void * packet, int psize )
{
	int r, tr=0 ;
	char * p=(char *)packet ;
	while( tr<psize ) {
		r = net_recv( s, p+tr, psize-tr );
		if( r>0 ) {
			tr+=r ;
		}
		else if( r==0 ) {
			return tr ;
		}
		else if( errno == EWOULDBLOCK && net_srdy(s, 5000000) ) {
			continue ;
		}
		else {
			return 0 ;
		}
	}
	return tr ;
}

int net_sendto(int s, void * packet, int psize, struct sockad * sad)
{
	if( sad!=NULL ){
    	return sendto(s, packet, psize, 0, SADDR(*sad), sad->len);
    }
    else {
    	return send(s, packet, psize, 0 );
    }
}

int net_recvfrom(int s, void * packet, int psize, struct sockad * sad)
{
    if( sad!=NULL ) {
        sad->len = sizeof(sad->s) ;
        return recvfrom( s, packet, psize, 0, SADDR(*sad), &(sad->len) );
    }
    else {
        return recvfrom( s, packet, psize, 0, NULL, NULL );
    }
}
