/*********************************************************************/
/* This File implements the protocol interface for the myftp server  */
/*********************************************************************/

#include "comm.h"
#include "server.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

/* -------------yet to give command line options for these #defines------------
*/
#define PASS_FILE "./passwd"
#define LOG_FILE  "/tmp/myftp.c9811014"

#define MAXFD     64

extern int errno;

/* ------------ Global Variables -----------
*/
static int status = NOT_CONNECTED;
connection sconn;
connection listen_conn;
connection data_conn;
char username[128];
char cip[16];
int cport;
FILE *logfp;

/* -------- All commands understood by the server----------
*/
struct commands_tag {
char comm[10];
int commid;
} commands[] = { { "USER ", USER },
		 { "PASS ", PASS },
		 { "PORT ", PORT },
		 { "LIST" , LIST },
		 { "RETR ", RETR },
		 { "STOR ", STOR },
		 { "QUIT" , QUIT },
		 { ""     , -1   } };


/* ------------------------MAIN---------------------------------
*/
int main(int argc, char **argv)
{
	int port = MYFTP_DEF_PORT;

	if (argc > 1) {
		if ( ( port = atoi(argv[1])) < 1024 && (geteuid() != 0))
			port = MYFTP_DEF_PORT;
	}

	daemonize();
	run_ftp();
}

/* --------------------make the server a DAEMON------------------
*/
int daemonize()
{
	int i;
	pid_t pid;

#ifdef DEBUG
	logfp = NULL;
	logfp = fopen(LOG_FILE, "w");
	if (logfp == NULL)
		printf("opening of logfile failed\n");
#endif
	if ( ( pid = fork()) != 0)
		exit(0);

	setsid();

	signal(SIGHUP, SIG_IGN);

        if ( ( pid = fork()) != 0)
                exit(0);

	for(i=0; i<MAXFD; i++)
		close(i);

	return 0;
}

/* -------------------initiate ftp server-------------------
*/
int run_ftp(int port)
{

	memset(&listen_conn, 0, sizeof(connection));

	listen_conn.saddr.sin_port = htons(port);

	if (create_listen_socket(&listen_conn) < 0) {
		printlog("%s","create_listen_socket FAILED\n");
		exit(1);
	}

	for (;;) {
		if (accept_connection( &listen_conn, &sconn) < 0) {
			printlog("%s","accept_connection FAILED\n");
			continue;
		}

		signal(SIGCHLD, SIG_IGN);

		if (fork() == 0) {
        		close_conn(listen_conn);

        		status = CONNECTED;

        		send_greetings();

        		command_loop();

        		close_conn(sconn);
        		return 0;
		}

        	close_conn(sconn);
	}

	return 0;
}

/* --------------------first response from server to client---------------
*/
int send_greetings()
{
	char buf[] = "220 Welcome to myftp server (Version 0.2.2) - VD\n";

	send_request(sconn, buf, strlen(buf));

	return 0;
}

/* ------------------infinite loop to handle client requests------------
*/
int command_loop()
{
	reply smesg;
	char  repstr[1024];
	int   filefd;

	for(;;) {
		if (get_command(sconn, &smesg) <= 0 ) {
			return 0;
		}

		switch(smesg.code) {
		case USER :
			strncpy(username,smesg.text,127);
			sprintf(repstr, "331 Password required for %s.\n",
					username);
			send_request(sconn, repstr, strlen(repstr));
			break;
		case PASS :
			if (check_user(username, smesg.text)) {
				sprintf(repstr, "230 User %s logged in.\n",
						username);
				send_request(sconn, repstr, strlen(repstr));
				status = LOGGED_IN;
			}
			else {
				sprintf(repstr, "530 login failed.\n");
				send_request(sconn, repstr, strlen(repstr));
			}
			break;
		case PORT :
			if (!loggedin())
				break;
			get_data_connection_info(smesg.text, cip, &cport);
			sprintf(repstr, "200 port command succesfull.\n");
			send_request(sconn, repstr, strlen(repstr));
			break;
		case LIST :
			if (!loggedin())
				break;
			if (open_sock_connection(cip, cport, &data_conn) < 0) {
				sprintf(repstr, "425 Can't build data connection: Connection refused.\n");
				send_request(sconn, repstr, strlen(repstr));
				close_conn(data_conn);
				break;
			}
			sprintf(repstr, "150 Opening data connection.\n");
			send_request(sconn, repstr, strlen(repstr));
			put_local_ls(data_conn.sock, smesg.text);
			sprintf(repstr, "226 Transfer complete.\n");
			send_request(sconn, repstr, strlen(repstr));
			close_conn(data_conn);
			break;
		case RETR :
			if (!loggedin())
				break;
        		if ( (filefd = open(smesg.text, O_RDONLY, 0)) < 0) {
				sprintf(repstr, "550 %s\n",strerror(errno));
                                send_request(sconn, repstr, strlen(repstr));
				break;
        		}
                        if (open_sock_connection(cip, cport, &data_conn) < 0) {
                                sprintf(repstr, "425 Can't build data connection: Connection refused.\n");
                                send_request(sconn, repstr, strlen(repstr));
                                close_conn(data_conn);
                                break; 
                        }
                        sprintf(repstr, "150 Opening data connection.\n");
                        send_request(sconn, repstr, strlen(repstr));
			put_data(data_conn, filefd);
                        sprintf(repstr, "226 Transfer complete.\n");
                        send_request(sconn, repstr, strlen(repstr));
                        close_conn(data_conn);
			close(filefd);
			break;
		case STOR :
			if (!loggedin())
				break;
                        if ( (filefd = open(smesg.text, O_WRONLY | O_CREAT,
					    S_IRUSR | S_IWUSR)) < 0) {
                                sprintf(repstr, "550 %s\n",strerror(errno));
                                send_request(sconn, repstr, strlen(repstr));
                                break;
                        }
                        if (open_sock_connection(cip, cport, &data_conn) < 0) {
                                sprintf(repstr, "425 Can't build data connection
: Connection refused.\n");
                                send_request(sconn, repstr, strlen(repstr));
                                close_conn(data_conn);
                                break;
                        }
                        sprintf(repstr, "150 Opening data connection.\n");
                        send_request(sconn, repstr, strlen(repstr));
			get_data(data_conn, filefd);
                        sprintf(repstr, "226 Transfer complete.\n");
                        send_request(sconn, repstr, strlen(repstr));
                        close_conn(data_conn);
                        close(filefd);
                        break;
		case QUIT :
			sprintf(repstr, "221 GoodBye.\n");
			send_request(sconn, repstr, strlen(repstr));
			exit(0);
			break;
		default :
			strcpy(repstr, "500 command not understood\n");
			send_request(sconn, repstr, strlen(repstr));
			break;
		}
	}
}

/* ---------------close a connection---------------------
*/
int close_conn(connection conn)
{
        if ( conn.sock == sconn.sock )
                status = NOT_CONNECTED;
        close(conn.sock);
}

/* ---------------Send the local file listing-----------------
*/
int put_local_ls(int fd, char *options)
{
        pid_t pid;
        int status;
	char *argv[100];  
	int i,j;

        /* I know this is ugly but saves time and menory leaks*/
	argv[0] = (char *)malloc(20);
	strcpy(argv[0], "ls");
	i = 1;
        while (*options == ' ' && *options != '\0')
               	options++;
	while (*options != '\0') {
		argv[i] = (char *)malloc(20);
		j = 0;
		while (*options != ' ' && *options != '\0') {
			argv[i][j] = *options;
			options++;
			j++;
		}
		argv[i][j] = '\0';

        	while (*options == ' ' && *options != '\0')
                	options++;
		i++;
	}

	argv[i] = NULL;

        if (fork() == 0) {
		dup2(fd, 1);
		dup2(fd, 3);
/*                execlp("ls", "ls", options, NULL);*/
                execvp("ls", argv);
                exit(0);
        }

        wait(&status);
	i = 0;
	while(argv[i] != NULL)
		free(argv[i++]);

	return 1;
}

/* ------------------check the user login and password----------------
*/
int check_user(char *user, char *password)
{
        FILE *fp;
        char buf[1024];
        char u[100];
        char p[100];

        char *sn;
        char *sp;
        fp = fopen(PASS_FILE,"r");

        while(1) {
                memset(buf,0,1024);
                memset(u,0,100);
                memset(p,0,100);
                sp = buf;
                if ( fgets( buf, 1024, fp) == NULL)
                        break;

                sn = strchr(sp, ':');
                strncpy(u,sp, sn - sp);
                sp = sn + 1;
                sn = strchr(sp, ':');
		if (sn == NULL)
			sn = strchr(sp, '\n');
                strncpy(p,sp, sn - sp);

		if ((strcmp(user, u) == 0) && (strcmp(password, p) == 0))
			return 1;
	}
	return 0;
}

/* ------------------get the next command from the client-------------------
*/
int get_command(connection conn, reply * mesg)
{
	int len;
	char buf[1024];

        if ( ( len = get_reply( conn, buf, 1024 )) < 0 ) {
                printlog ( "%s","get_reply failed\n" );
                exit(1);
        } else if ( len == 0 ) {
                printlog("%s","Client Closed Connection\n");
                return -1;
        }
	buf[len] = '\0';

        if ( parse_command(buf, len, mesg) < 0) {
                printlog("%s","parse_command failed\n");
		mesg->code = 500;
        }

	return mesg->code;
}

/* --------------parse the command and figure out what is requested-----------
*/
int parse_command( char *buf, int len, reply *mesg)
{
	int i,j;

	memset(mesg, 0, sizeof(reply));

	i = 0;
	while (commands[i].commid !=  -1) {
		printlog("mesg = %s, comm = %s\n",buf, commands[i].comm);
		if (strncmp(buf,commands[i].comm,
				strlen(commands[i].comm)) == 0) {
			mesg->code = commands[i].commid;
			strcpy(mesg->text, buf + strlen(commands[i].comm));
			for(j=0; j < len; j++)
				if (mesg->text[j] == '\r' || 
				    mesg->text[j] == '\n')
					mesg->text[j] = '\0';
			return mesg->code;
		}
		i++;
	}

	return -1;
}

/* -------------------Check if a user has currently logged in---------------
*/
int loggedin()
{
	char repstr[1024];

	if (status == LOGGED_IN) {
		return 1;
	}

        sprintf(repstr, "530 Please login with USER and PASS.\n");
        send_request(sconn, repstr, strlen(repstr));

	return 0;
}

/* -----------------Print debug information-----------------
*/
int printlog(const char *fmt, ...)
{

#ifdef DEBUG
	FILE *fp;
	va_list ap;

	va_start(ap, fmt);

	if (logfp == NULL)
		fp = stdout;
	else
		fp = logfp;

	fflush(fp);
	vfprintf(fp, fmt, ap);

	va_end(ap);

#endif
	return 0;
}
