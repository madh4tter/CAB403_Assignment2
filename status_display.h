#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "PARKING.h"

#define BUFFER 20

/*  Get and print entrance status display  */
void get_entry();

/*  Get and print exit status display  */
void get_exit();

/*  Get and print level status display  */
void get_level();

/*  Get and print total revenue  */
void print_revenue();

/*  Convert char into double (for total revenue calculation)  */
double convertToNum(char a[]);

/* check for a fail message, stop printing display if fail occurs */
bool check_for_fail(char* check);
