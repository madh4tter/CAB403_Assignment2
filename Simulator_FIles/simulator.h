/*****************************************************************//**
 * \file   simulator.h
 * \brief  Method and struct definitions for program simulator.c
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#include "PARKING.h"
#include <stdbool.h>
#include <inttypes.h>

typedef struct car {
    char plate[6];
    short lvl;
    uint16_t entr_time;
    uint16_t exit_time;
    
} car_t;

typedef struct mstimer{
    uint64_t elapsed; 
    pthread_mutex_t lock;
    pthread_cond_t cond;
} mstimer_t;

/* Methods for managing shared memory */

// Create object with PARKING data, set mutexes and conds as shared
bool create_shared_object( shm_t* shm, const char* share_name );

void destroy_shared_object( shm_t* shm );

bool get_shared_object( shm_t* shm, const char* share_name );

/**
 * Generates a car with a random license plate with a 50% chance of being an approved plate
 *
 * PRE: n/a.
 *
 * POST: returns string of the license plate
 * AND car does not already exist in simulation
 * 
 */
char *create_car(void);

/**
 * Places a car onto end of queue of a random entrance
 * 
 */
void q_entr(char *plate);

/**
 * Signal the conditional variable of the LPR when a car reaches the front of a queue
 * 
 */
void trigger_LPR(LPR_t LPR);


/**
 * Free memory for the car and remove licence plate from list of existing
 * plates
 *
 */
void destroy_car(car_t car);

/**
 * assign whatever character is on the display to car_t->level 
 * has a small chance of assigning a random level instead (unusual behaviour)
 * if any non-expected symbols is displayed on the screen, return a failure
 */
bool assign_lvl(char display);

/**
 * thread function for keeping running time of simulation in ms
 * 
 */
void *thf_time(void *ptr);

/**
 * function for the thread responsible for creating cars and adding them onto the end
 * of a random entrance queue
 * 
 */
void *thf_creator(void);

/**
 * function for threads at each entrance
 * take cars off the entrance queue, wait 2 ms, signal the LPR, 
 * assign a level (if failure, destroy car), wait for signal to open,
 * wait 10ms, change boom gate from R to O,
 * wait 10ms, signal level LPR, add car to list of parked cars
 * wait for signal to close, wait 10ms, change boom gate from L to C
 * 
 * repeat
 */
void *thf_entr(void);

/**
 * Thread function for checking 
 * 
 */
void *thf_park(void);

/**
 * Thread Layout
 *
 * One thread for creating new cars and adding them onto random entrance queues
 *
 * One thread per entrance 
 * (5 total)
 * 
 * One thread for keeping time in ms
 * 
 * One thread for comparing the assigned time of cars inside the carpark and exiting them 
 * when necessary
 * (thread/job pool?)
 * 
 * One thread per exit
 * (5 total)
 * 
 * One thread per level for generating temperature
 * (5 total)
 *
 */
