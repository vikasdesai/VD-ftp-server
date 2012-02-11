#ifndef PI_H
#define PI_H

#include "comm.h"

#define NOT_CONNECTED 0
#define CONNECTED     1
#define LOGGED_IN     2
#define SENDPASSWORD  3

#define USER 1
#define PASS 2
#define PORT 3
#define LIST 4
#define RETR 5
#define STOR 6
#define QUIT 7

int printlog(const char *s, ...);
#endif /* PI_H */
