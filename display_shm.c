#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>


#include "PARKING.h"

#define SHARE_NAME "PARKING"
#define REVENUE_FILE "billing.txt"
#define GOTO_LINE1 "\033[u" /* use saved cursor location */


