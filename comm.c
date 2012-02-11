/***********************************************************************/
/* This File implements the common code used by myftp client and server*/
/* it mainly contains all the socket code for myftp and some support   */
/* functions.                                                          */
/***********************************************************************/

#include "comm.h"
#include <errno.h>


extern int errno;

/*--- Open a socket connection and populate the connection structure ---
*/
int open_sock_connection( char * server,
                          unsigned short port,
                          connection * myconn  )
{
	struct hostent * hostinfo = NULL;

	if ( ( hostinfo = gethostbyname(server) ) == NULL ) {
		printlog("gethostbyname for %s FAILED\n",server);
		exit(h_errno);
	}

	printlog("%s","after gethostbyname\n");

	printlog(" sizeof(s_Addr) = %d, h_length = %d\n",
			sizeof(myconn->saddr.sin_addr.s_addr),
			hostinfo->h_length);
	memcpy( (char *)&(myconn->saddr.sin_addr.s_addr),
                                hostinfo->h_addr_list[0],
                                       hostinfo->h_length );
	printlog("%s","after memcpy\n");
	printlog ( "addr = %s\n", inet_ntoa(myconn->saddr.sin_addr));

	myconn->saddr.sin_port = htons(port);
	myconn->saddr.sin_family = AF_INET;

	if ( ( myconn->sock = socket( AF_INET, SOCK_STREAM, 0)) < 0 ) {
		printlog ( "%s","socket failed \n" );
		exit(1);
	}

	if (connect(myconn->sock,( SA * )&myconn->saddr,sizeof( SI )) < 0 ) {
		perror("connect failed");
		exit(1);
	}
	printlog ( "%s","connect successful\n" );

	return(myconn->sock);
}


/* ----------- Get the server response -----------------
*/
int get_reply( connection myconn, char * buf, int len)
{
	int rlen = 0;

	if ( (rlen = recv( myconn.sock, buf, len, 0)) < 0 ) {
		printlog ( "%s","recv failed\n" );
		return(-1);
	}
	return(rlen);
}

/* ----------------Send request to the server----------------
*/
int send_request( connection conn, char * buf, int len)
{
	int sent = 0;
	int rlen = len;

	while(len > 0) {
		if ( ( sent = send( conn.sock, buf, len, 0)) < 0 ) {
			printlog("%s","send failed\n");
			return -1;
		}
		len = len - sent;
	}

	return rlen;
}

/* -------------------Create a listening socket----------------------
   used by the server for control and by client for data connection.
*/
int create_listen_socket(connection *listenfd)
{
	int len;
	int optval;

	len = sizeof(SI);

	listenfd->sock = socket(AF_INET, SOCK_STREAM, 0);

        optval = 1;
        setsockopt(listenfd->sock, SOL_SOCKET, SO_REUSEADDR,
                                     &optval, sizeof(int));

	if (listenfd->saddr.sin_addr.s_addr == 0)
		listenfd->saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (listenfd->saddr.sin_family == 0)
		listenfd->saddr.sin_family = AF_INET;

	/* This statement not needed as u can see
	*/
	if (listenfd->saddr.sin_port == 0)
		listenfd->saddr.sin_port = htons(0);

	if (bind(listenfd->sock, (SA *)&listenfd->saddr, len) < 0)
	{
		printlog("%s","bind failed\n");
		return -1;
	}

	listen(listenfd->sock, SOMAXCONN);

	getsockname(listenfd->sock, (SA *)&listenfd->saddr, &len);

	printlog("port = %d\n",ntohs(listenfd->saddr.sin_port));

	return listenfd->sock;
}

/* ----------convert struct sockaddr to "," seperated string format----------
   used by the client to convert its data socket info to string to be sent 
   to the server with the PORT command.
*/
int get_port_info(connection conn, char *info)
{
	char hostname[512];
	char hostip[512];
	struct hostent *hostinfo = NULL;
	int port;
	int i;
	struct in_addr temp;

	gethostname(hostname, 512);

        if ( ( hostinfo = gethostbyname(hostname) ) == NULL ) {
                printlog("gethostbyname for %s FAILED\n",hostname);
                return -1;
        }

        memcpy( (char *)&temp, hostinfo->h_addr_list[0], hostinfo->h_length );

        strcpy(hostip, inet_ntoa(temp));

	port = ntohs(conn.saddr.sin_port);

	for(i=0; hostip[i] != '\0'; i++) {
		if ( hostip[i] == '.' )
			hostip[i] = ',';
	}

	sprintf(info, "%s,%d,%d",hostip,port/256,port%256);

	return 0;
}

/* -----------implementation of accept taking care of interrupts -----------
*/
int accept_connection(connection *lconn, connection *aconn)
{

	int len;

	len = sizeof(struct sockaddr_in);

	aconn->sock = -1;

	while (aconn->sock < 0) {
		aconn->sock = accept(lconn->sock, (SA *)&aconn->saddr, &len);
		if (aconn->sock == -1)
			if (errno == EINTR)
				continue;
			else
				break;
	}
	printlog("accepted : %d\n",aconn->sock);

	return aconn->sock;
}

/* -------------get data from connection ---------------
*/
int get_data(connection conn, int fd)
{
	return read_write_data(conn.sock, fd);
}

/* -------------put data to connection ---------------
*/
int put_data(connection conn, int fd)
{
	return read_write_data(fd, conn.sock);
}

/* -------------generic read write funtion----------------
   called by get_data() and put_data() for data transfer
*/
int read_write_data(int inputfd, int outputfd)
{
	char buf[1024];
	int nread = 0;
	int total = 0;
	int nwrite = 0;

	while( ( nread = read(inputfd, buf, sizeof(buf))) > 0) {
		total = total + nread;
		while(nread > 0) {
			nwrite = write(outputfd, buf, nread);
			if(nwrite < 0) {
				printlog("%s","write error\n");
				return -1;
			}
			nread = nread - nwrite;
		}
	}

	if ( nread == 0 )
		return total;
	else
		return -1;
}

/* --------------convert string port info to int---------------
   used by the server to convert the data sent by client along with 
   the PORT command.
*/
int get_data_connection_info( char *portstr, char * ip, int *port)
{
	int i,j;
	char *tmp;
	char tmpport[4];

	tmp = portstr;

	for(i=0; tmp[i] == ' ' && tmp[i] != '\0' ; i++)
		tmp++;

	i = 0;j = 0;
	while(i<4) {
		if(*tmp==',') {
			ip[j] = '.';
			i++;
		}
		else
			ip[j] = *tmp;
		tmp++;
		j++;
	}

	ip[j-1] = '\0';

	j=0;

	while(*tmp != ',') {
		tmpport[j] = *tmp;
		tmp++;
		j++;
	}
	tmp++;

	tmpport[j] = '\0';

	*port = atoi(tmpport) * 256 + atoi(tmp);

#ifdef DEBUG
	printf("str = %s, ip = %s, port = %d.\n",portstr,ip,*port);
#endif
}
