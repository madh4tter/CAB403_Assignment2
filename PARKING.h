/*****************************************************************//**
 * \file   PARKING.h
 * \brief  API definition for the PARKING abstract data type.
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#pragma once

/* Include necessary standard libraries */
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define SHARE_NAME "PARKING"
#define PLATES_FILE "plates.txt"
#define REVENUE_FILE "billing.txt"
#define GOTO_LINE1 "\033[u" /* use saved cursor location */


/* Define characteristics of car-park */
#define ENTRANCES 5
#define EXITS 5
#define LEVELS 5
#define LEVEL_CAPACITY 10



/* Structure for simulated hardware */
typedef struct LPR {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char plate[6];
    char padding[2];
} LPR_t;

typedef struct Boom_Gate {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char status;
    char padding[7];
} gate_t;

typedef struct Info_Screen {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    char display;
    char padding[7];
} screen_t;


/* Structure for entrance, exit and level memory */
typedef struct Entrance {
    LPR_t LPR;
    gate_t gate;
    screen_t screen;
} entrance_t;

typedef struct Exit {
    LPR_t LPR;
    gate_t gate;
} exit_t;

typedef struct Level {
    LPR_t LPR;
    int16_t temp;
    char alarm;
    char padding[5];
} level_t;

/* Shared memory structure containing all memory for this program */
typedef struct PARKING {
    /* List of shared memory for each parking structure */
    entrance_t entrances[ENTRANCES];
    exit_t exits[EXITS];
    level_t levels[LEVELS];

} PARKING_t;

/* Shared memory object */
typedef struct shared_memory {
    /* The name of the shared memory object */
    const char* name;

    /* The file descriptor used to manage the shared memory object */
    int fd;

    /* Address of the shared data block */
    PARKING_t* data;

} shm_t;


#ifndef GET_SHARED_MEM
#define GET_SHARED_MEM
bool get_shared_object( shm_t* shm, const char* share_name );
#endif

#ifndef DES_SHARED_MEM
#define DES_SHARED_MEM
void destroy_shared_object( shm_t* shm );
#endif