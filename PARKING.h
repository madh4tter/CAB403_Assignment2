/*****************************************************************//**
 * \file   PARKING.h
 * \brief  API definition for the PARKING abstract data type.
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#pragma once
#include <semaphore.h>

#define SHARE_NAME "PARKING"

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

} PARKING_t;

/* Shared memory object */
typedef struct shared_memory {
    // The name of the shared memory object.
    const char* name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    /// Address of the shared data block. 
    PARKING_t* data;

} shm_t;

bool get_shared_object( shm_t* shm, const char* share_name ) {
    // Get a file descriptor connected to shared memory object and save in 
    // shm->fd. If the operation fails, ensure that shm->data is 
    // NULL and return false.
    shm->fd = shm_open(share_name, O_RDWR, 0666);
    // Check if shm_open worked
    if(shm->fd == -1){
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    shm->data = mmap(NULL, sizeof(PARKING_t), PROT_READ | PROT_WRITE, MAP_SHARED, 
                    shm->fd, 0);
    if(shm->data == (void *)-1){
        return false; 
    }

    // Modify the remaining stub only if necessary.
    return true;
}