#ifndef COMM_H
#define COMM_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SA struct sockaddr
#define SI struct sockaddr_in

/* ---------------------Default FTP port for myftp--------------
*/
#define MYFTP_DEF_PORT 21000

typedef struct {
	int sock;
	struct sockaddr_in saddr;
}connection; 

typedef struct {
	int code;
	char text[1024];
} reply;

int open_sock_connection( char * server,
                          unsigned short port,
                          connection * myconn  );

#endif /* COMM_H */
