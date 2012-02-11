/*****************************************************************/
/* This File implements the user interface for the myftp client  */
/*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "ui.h"

extern int status;

/* -------- All commands available to the user ----------
*/

struct command_tag {
	char name[100];
	int id;
} avail_comm[] = { { "open" , OPEN  },
		   { "user" , USER  },
		   { "pass" , PASS  },
		   { "ls"   , LIST  },
		   { "lls"  , LLIST },
		   { "get"  , GET   },
		   { "put"  , PUT   },
		   { "close", CLOSE },
		   { "quit" , QUIT  }, 
		   { "bye"  , QUIT  }, 
		   { "help" , HELP  }, 
		   { ""     , -1    } };


/* ------------ Global Variables -----------
*/

char ftp_server[100];


/* ------------ MAIN -------------
*/

main(int argc, char **argv)
{
	char s[100] = "";
	char p[100] = "";

	if (argc > 1)
		strcpy(s, argv[1]);
	if (argc > 2)
		strcpy(p, argv[2]);

	init_client(s, p);
#ifdef DEBUG
	printf("init_client returned\n");
	getchar();
#endif
	prompt();
	exit(0);
}

/* --------------- Connect to the server ------------
*/

int init_client(char *server, char *port)
{
        unsigned short ftp_port = MYFTP_DEF_PORT;

        if ( *server != '\0') {
                strcpy( ftp_server, server );
                if ( *port != '\0')
                        ftp_port = atoi( port );
#ifdef DEBUG
	printf("calling open_connection\n");
	getchar();
#endif
                if ( open_connection(ftp_server, ftp_port) == SUCCESS )
                        login();
        }
}

/* ----------- Login ---------------
*/

int login()
{
	char username[100] = "" ;
	char * password;
	int j=0;
	struct passwd *userinfo;

	userinfo = getpwuid(geteuid());

	printf("Name (%s:%s): ",ftp_server, userinfo->pw_name);

	Fgets(username, 100, stdin);

	if ( *username  == '\0' )
		strcpy(username, userinfo->pw_name);

	switch(send_user(username)) {
		case LOGGED_IN :
			break;
		case SENDPASSWORD :
			password = getpass("Password : ");
			send_password(password);
			break;
		default :
			printf("Error returned by send_user\n");
			break;
	}
	return 0;
}

/* ------------- Main Loop ----------------
*/

int prompt()
{
	char command[1024];
	char options[1024];
	char *password;
	int command_id;

#ifdef DEBUG
	printf("in function prompt\n");
	getchar();
#endif
	while(1) {
		fflush(stdin);
		printf("myftp>");
		Fgets(command,1024,stdin);

		if ( strlen(command) == 0)
			continue;

		command_id = get_command_id(command, options);

		switch(command_id) {
		case OPEN  :
				connect_peer(options);
			break;
		case USER  :
			if (status == NOT_CONNECTED) {
				printf("Not connected\n");
				break;
			}

			if (*options == 0) {
				login();
			}
			else {
        			switch(send_user(options)) {
                		case LOGGED_IN :
                        		break;
                		case SENDPASSWORD :
                        		password = getpass("Password : ");
                        		send_password(password);
                        		break;
                		default :
                        		printf("Error returned by send_user\n");
                        		break;
        			}
			}
			break;
		case PASS  :
			break;
		case LIST  :
			if (status == NOT_CONNECTED) {
				printf("Not connected\n");
				break;
			}
			get_remote_ls(options);
			break;
		case LLIST :
			get_local_ls(options);
			break;
		case GET   :
			if (status == NOT_CONNECTED) {
				printf("Not connected\n");
				break;
			}
			get_file(options);
			break;
		case PUT   :
			if (status == NOT_CONNECTED) {
				printf("Not connected\n");
				break;
			}
			put_file(options);
			break;
		case CLOSE :
			if (status == NOT_CONNECTED) {
				printf("Not connected\n");
				break;
			}
			send_exit();
			close_peer();
			break;
		case QUIT  :
			send_exit();
			exit(0);
			break;
		case HELP  :
			/* hardcoded help */
			printf("help   - this output\n");
			printf("open   - open servername serverport\n");
			printf("user   - user USERNAME\n");
			printf("ls     - list files on the server\n");
			printf("lls    - list files on the local server\n");
			printf("get    - get filename\n");
			printf("put    - put filename\n");
			printf("close  - closes connection to server\n");
			printf("quit   - exit\n");
			printf("bye    - exit\n");
			break;
		default :
			printf("command not found\n");
			break;
		}
	}
	return 0;
}

/* ----------------Figure out what command user entered ---------------
*/

int get_command_id(char * command, char * options)
{
	int i;
	int clen, aclen;

	while(*command == ' ')
		command++;

	for(i=0; *avail_comm[i].name != '\0'; i++) {
		clen  = strlen(command);
		aclen = strlen(avail_comm[i].name);
		if (strncmp(command, avail_comm[i].name, aclen) == 0 ) {
			if (clen == aclen)
				*options = '\0';
			else
				strcpy(options, command + aclen + 1);

			return avail_comm[i].id;
		}
	}
	return -1;
}

/* ---------------- lls implementation ---------------
*/

int get_local_ls(char *options)
{
	pid_t pid;
	int status;
        char *argv[100];
        int i,j;

	while (*options == ' ')
		options++;

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
/*		execlp("ls", "ls", options, NULL);*/
                execvp("ls", argv);
		exit(0);
	}
	wait(&status);
	return 1;
}

/* ------------- fgets without the newline ---------------
*/

char *Fgets(char *s, int size, FILE *stream)
{
	int i = 0;

	if ( fgets(s, size, stream) == NULL) {
		printf("fgets failed\n");
		return NULL;
	}

	for(i = 0; i < size && s[i] != '\0' && s[i] != '\n'; i++);

	if ( s[i] == '\n' )
		s[i] = '\0';

	return s;
}

/* -------------- connect to the server -------------------
*/

int connect_peer(char *inputs)
{
	char server[100] = "";
	char port[100] = "";
	int i,j;

	while(*inputs == ' ')
		inputs++;

	if (*inputs == '\0') {
		printf("usage: open host-name [port]\n");
		return 0;
	}

	i = 0;
	while (*inputs != ' ' && *inputs != '\0') {
		server[i] = *inputs;
		i++; inputs++;
	}

	if (atoi(inputs) != 0)
		strcpy(port, inputs);

	init_client(server, port);

	return 0;
}
