#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "PARKING.h"

#define BUFFER 20

/*  Get and print entrance status display  */
void get_entry(shm_t* shm);

/*  Get and print exit status display  */
void get_exit(shm_t* shm);

/*  Get and print level status display  */
void get_level(shm_t* shm);

/*  Get and print total revenue  */
void print_revenue();

/*  Convert char into double  */
double convertToNum(char a[]);
