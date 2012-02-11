#ifndef UI_H
#define UI_H

#include <stdio.h>
#include "pi.h"

#define SUCCESS       1
#define FAIL          -1

int login();
int prompt();

#define OPEN  1
#define USER  2
#define PASS  3
#define LIST  4
#define LLIST 5
#define GET   6
#define PUT   7
#define CLOSE 8
#define QUIT  9
#define HELP  10

char *Fgets(char *, int, FILE *);
#endif /* UI_H */
