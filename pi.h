#ifndef PI_H
#define PI_H

#include "comm.h"
#include "ui.h"

/* -----------------client states----------------
*/
#define NOT_CONNECTED 0
#define CONNECTED     1
#define LOGGED_IN     2
#define SENDPASSWORD  3

int open_connection(char *, unsigned short);
int printlog(char *s, ...);

#endif /* PI_H */
