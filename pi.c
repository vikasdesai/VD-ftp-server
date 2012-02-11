#include <sys/stat.h>
#include <fcntl.h>
#include "pi.h"

/* ------------ Global Variables -----------
*/
int status = NOT_CONNECTED;
connection sconn;
connection listen_conn;
connection data_conn;

/*---------open connection with the server and handle the response--------
*/
int open_connection( char * server, unsigned short port )
{
	reply smesg;
	char buf[1024];
	int len;

	if ( open_sock_connection(server, port, &sconn) == -1 ) {
		printf( "open_sock_connection failed\n");
		return FAIL;
	}

	if ( ( len = get_server_response( sconn, &smesg)) < 0 ) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
		return FAIL;
	}

	printf("%s",smesg.text);

	switch(smesg.code) {
		case 120 :
			break;
		case 220 :
			status = CONNECTED;
			break;
		case 421 :
			printf(smesg.text);
			close_peer();
			status = NOT_CONNECTED;
			return FAIL;
		default :
			printf("reply not understood\n");
			printf(smesg.text);
			return FAIL;
	}

	if ( status == CONNECTED )
		return SUCCESS;

	return FAIL;
}

/* --------------Parse the reply received from the server--------------
*/
int parse_reply( char * buf, int len, reply *smesg)
{
	char code[5];
	int i = 0;
	int j = 0;

	while(buf[i] == ' ')
		i++;

	while((buf[i] != ' ') &&  (j < 4)) {
		code[j] = buf[i];
		j++;
		i++;
	}
	code[j] = '\0';

	smesg->code = atoi(code);
	strncpy(smesg->text, buf, len);
	smesg->text[len] = '\0';

	return i;
}

/* ----------------Send the user name to the server-----------------
*/
int send_user(char * user)
{
	int len = 0;
	char request[1024];
	char buf[1024];
	reply smesg;

	strcpy(buf, "USER ");
	strcat(buf, user);
	strcat(buf, "\n");
	send_request(sconn, buf, strlen(buf));
	if ( ( len = get_server_response( sconn, &smesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
		return -1;
	}
	printf("%s",smesg.text);

	switch(smesg.code) {
		case 230 :
			status = LOGGED_IN;
			break;
		case 331 :
			status = SENDPASSWORD;
			break;
		case 421 :
			status = NOT_CONNECTED;
			printf(smesg.text);
			close_peer();
			break;
		default :
			printf("did not understand reply\n");
			break;
	}
	return status;
}

/* ------------------Send the password to the server--------------
*/
int send_password(char * password)
{
        int len = 0;
        char request[1024];
        char buf[1024];
	reply smesg;

        strcpy(buf, "PASS ");
        strcat(buf, password);
        strcat(buf, "\n");
        send_request(sconn, buf, strlen(buf));
        if ( ( len = get_server_response( sconn, &smesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }

        switch(smesg.code) {
                case 230 :
                        status = LOGGED_IN;
			printf(smesg.text);
                        break;
		case 530 :
			printf(smesg.text);
			break;
		case 421 :
			printf(smesg.text);
			status = NOT_CONNECTED;
			close_peer();
			break;
                default :
                        printf("did not understand reply\n");
                        break;
        }
        return status;
}

/* -----------------send request for listing the files on server---------------
*/
int get_remote_ls(char *options)
{
	int datafd;
	int listenfd;
	reply mesg;
	char buf[1024];
	int len;

	memset(&listen_conn, 0, sizeof(connection));
	if ( (listenfd = create_listen_socket(&listen_conn)) < 0 ) {
		printf("cannot open data connection\n");
		return(-1);
	}

	if ( send_port_info(listen_conn) < 0 )
		printf("error in sending data connection\n");

        if ( ( len = get_server_response( sconn, &mesg)) < 0 ) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }

        switch(mesg.code) {
        case 200 :
		printf("%s",mesg.text);
                break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
        default  :
                printf("%s",mesg.text);
		close_conn(&listen_conn);
                return -1;
        }

	strcpy(buf, "LIST");
	if (*options != '\0')
		strcat(buf, " ");
	strcat(buf, options);
	strcat(buf, "\n");
	send_request(sconn, buf, strlen(buf));

        if ( ( len = get_server_response( sconn, &mesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }

        printf("%s",mesg.text);

        switch(mesg.code) {
	case 125 :
		break;
	case 150 :
		if (accept_connection(&listen_conn, &data_conn) < 0) {
			printf("accept failed\n");
			return -1;
		}

		if (get_data(data_conn, 1)< 0) {
			printf("421 Server closed connection.\n");
			close_peer();
		}
		len = get_reply(sconn, buf, 1024);
		buf[len] = 0;
		printf("%s",buf);
		close_conn(&data_conn);
		break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
	case 450 :
		break;
	case 500 :
	case 501 :
	case 502 :
	case 530 :
		printf(mesg.text);
		break;
	close_conn(&listen_conn);
	}
}

/* ------------Send the port info for the data connection------------------
*/
int send_port_info(connection conn)
{
	char buf[1024];
	char portinfo[100];

	get_port_info(conn, portinfo);

	strcpy(buf, "PORT ");
	strcat(buf, portinfo);
	strcat(buf, "\n");

	send_request(sconn, buf, strlen(buf));

	return 0;
}

/* ------------------Get a file from the server----------------------
*/
int get_file(char *options)
{
	int i = 0;
	int j = 0;
        int datafd;
        int listenfd;
        int writefd;
        reply mesg;
        char buf[1024];
        int len;
	int recvlen;

	char rfname[1024];
	char lfname[1024];

	while( i < 1024 && options[i] == ' ' && options[i] != '\0' )
		i++;

	if ( options[i] == '\0' ) {
		printf("(remote-filename) ");
		scanf("%s",rfname);
/*
		printf("(local-filename) ");
		scanf("%s",lfname);
*/
	} else {
		j = 0;
		while( i < 1024 && options[i] != ' ' && options[i] != '\0' ) {
			rfname[j] = options[i];
			j++;i++;
		}
		rfname[j] = '\0';
	}

	strcpy(lfname, rfname);

	memset(&listen_conn, 0, sizeof(connection));
        if ( (listenfd = create_listen_socket(&listen_conn)) < 0 ) {
                printf("cannot open data connection\n");
                return(-1);
        }

        if ( send_port_info(listen_conn) < 0 )
                printf("error in sending data connection\n");

        if ( ( len = get_server_response( sconn, &mesg)) < 0 ) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
		return -1;
        }

        switch(mesg.code) {
        case 200 :
		printf("%s",mesg.text);
                break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
        default  :
		printf("%s",mesg.text);
		close_conn(&listen_conn);
                return -1;
        }

        strcpy(buf, "RETR ");
        strcat(buf, rfname);
        strcat(buf, "\n");
        send_request(sconn, buf, strlen(buf));

        if ( ( len = get_server_response( sconn, &mesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }

        printf("%s",mesg.text);

        switch(mesg.code) {
        case 125 :
                break;
        case 150 :
        	if (accept_connection(&listen_conn, &data_conn) < 0) {
               		printf("accept failed\n");
                	return -1;
        	}

		if ( (writefd = open(lfname, O_WRONLY | O_CREAT,
					     S_IRUSR | S_IWUSR)) < 0) {
			perror("failed in open file"); 
		}
                if ( ( recvlen = get_data(data_conn, writefd)) < 0) {
			printf("421 server closed connection.\n");
			close_peer();
		}
                len = get_reply(sconn, buf, 1024);
                buf[len] = 0;
                printf("%s",buf);
		printf("received %d bytes of data.\n",recvlen);
                close_conn(&data_conn);
                close_conn(&listen_conn);
		close(writefd);
                break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
        case 450 :
                break;
        case 500 :
        case 501 :
        case 502 :
        case 530 :
		printf(mesg.text);
                break;
        }
}

/* ---------------close a connection---------------------
*/
int close_conn(connection *conn)
{
	if ( conn->sock == sconn.sock )
		status = NOT_CONNECTED;
	close(conn->sock);

	memset(conn, 0, sizeof(connection));
}

/* --------------Send a file to the server---------------------
*/
int put_file(char *options)
{
        int i = 0;
        int j = 0;
        int datafd;
        int listenfd;
        int readfd;
        reply mesg;
        char buf[1024];
        int len;
	long datalen = 0;

        char rfname[1024];
        char lfname[1024];

        while( i < 1024 && options[i] == ' ' && options[i] != '\0' )
                i++;

        if ( options[i] == '\0' ) {
                printf("(local-filename) ");
                scanf("%s",lfname);
/*
                printf("(remote-filename) ");
                scanf("%s",rfname);
*/
        } else {
                j = 0;
                while( i < 1024 && options[i] != ' ' && options[i] != '\0' ) {
                        lfname[j] = options[i];
                        j++;i++;
                }
                lfname[j] = '\0';
        }

        strcpy(rfname, lfname);

        if ( (readfd = open(lfname, O_RDONLY, 0)) < 0) {
                perror("failed in open file");
		return -1;
        }

	memset(&listen_conn, 0, sizeof(connection));
        if ( (listenfd = create_listen_socket(&listen_conn)) < 0 ) {
                printf("cannot open data connection\n");
                return(-1);
        }

        if ( send_port_info(listen_conn) < 0 )
                printf("error in sending data connection\n");

        if ( ( len = get_server_response( sconn, &mesg)) < 0 ) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }

        switch(mesg.code) {
        case 200 :
		printf("%s",mesg.text);
                break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
        default  :
                printf("%s",mesg.text);
		close_conn(&listen_conn);
                return -1;
        }

        strcpy(buf, "STOR ");
        strcat(buf, rfname);
        strcat(buf, "\n");
        send_request(sconn, buf, strlen(buf));

        if ( ( len = get_server_response( sconn, &mesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }
        printf("%s",mesg.text);

        switch(mesg.code) {
        case 125 :
                break;
        case 150 :
        	if (accept_connection(&listen_conn, &data_conn) < 0) {
                	printf("accept failed\n");
                	return -1;
        	}

                if ( ( datalen = put_data(data_conn, readfd)) < 0) {
			printf("421 server closed connection.\n");
			close_peer();
		}

                close_conn(&data_conn);
                close_conn(&listen_conn);
                close(readfd);

                len = get_reply(sconn, buf, 1024);
                buf[len] = 0;
                printf("%s",buf);
		printf("%d bytes sent\n",datalen);
		printf("local: %s remote: %s\n",lfname,rfname);
                break;
	case 421 :
		status = NOT_CONNECTED;
		printf(mesg.text);
		close_peer();
		break;
        case 450 :
                break;
        case 500 :
        case 501 :
        case 502 :
        case 530 :
		printf(mesg.text);
                break;
        }
}

/* -----------------send QUIT to the server and close connection--------------
*/
int send_exit()
{
        int len = 0;
        char request[1024];
        char buf[1024];
        reply smesg;

	if (status == NOT_CONNECTED)
		return 0;

        strcpy(buf, "QUIT\n");
        send_request(sconn, buf, strlen(buf));
        if ( ( len = get_server_response( sconn, &smesg)) < 0) {
		if (status = CONNECTED) {
			close_peer();
			printf("421 server closed connection\n");
		}
		else
			printf("not connected\n");
		status = NOT_CONNECTED;
                return -1;
        }
        printf("%s",smesg.text);

	return smesg.code;
}

/* --------------------just close the connection--------------------
   this function is used normally when the server has already closed
   connection.
*/
int close_peer()
{
	int i;

	for(i=3; i < OPEN_MAX; i++)
		close(i);

	memset(&sconn,0,sizeof(connection));
	memset(&listen_conn,0,sizeof(connection));
	memset(&data_conn,0,sizeof(connection));

	status = NOT_CONNECTED;
}

/* -----------------Get response from the server and parse it------------------
*/
int get_server_response(connection conn, reply *resp)
{
	char buf[1024];
	int len;

	memset(buf, 0, 1024);
	memset(resp, 0, sizeof(reply));

       	if ( ( len = get_reply( conn, buf, 1024 )) < 0 ) {
		resp->code = 421;
		strcpy(resp->text, "server closed connection\n");
		return -1;
       	} else if ( len == 0 ) {
		resp->code = 421;
		strcpy(resp->text, "server closed connection\n");
		return -1;
	}

        if ( parse_reply( buf, len, resp) < 0 ) {
      		printf("parse_reply failed\n");
        	return -1;
       	}

	return len;
}

/* -------------------Print debug output --------------------
*/
int printlog(char *s, ...)
{
#ifdef DEBUG
        va_list ap;

        va_start(ap, fmt);

        vfprintf(stderr, fmt, ap);

        va_end(ap);
#endif
        return 0;
}
