/*****************************************************************//**
 * \file   PARKING.h
 * \brief  API definition for the PARKING abstract data type.
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#pragma once
#include <semaphore.h>

/* Define characteristics of car-park */
#define ENTRANCES 1
#define EXITS 1
#define LEVELS 1
#define LEVEL_CAPACITY 1

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
    /// List of shared memory for each parking structure
    entrance_t entrances[ENTRANCES];
    exit_t exits[EXITS];
    level_t levels[LEVELS];

} PARKING;

/* Shared memory object */
typedef struct shared_memory {
    // The name of the shared memory object.
    const char* name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    /// Address of the shared data block. 
    PARKING data;

} shm_t;