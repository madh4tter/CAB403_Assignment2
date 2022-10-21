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

#define INITIAL_CAP 2
#define DV_GROWTH_FACTOR 1.25

typedef struct car {
    char *plate;
    char lvl;
    uint16_t entr_time;
    uint16_t exit_time; 
} car_t;

typedef struct mstimer{
    uint64_t elapsed; 
    pthread_mutex_t lock;
    pthread_cond_t cond;
} mstimer_t;

typedef struct car_vec {
	/// The current number of elements in the vector
	uint16_t size;

	/// The current storage capacity of the vector
	uint16_t capacity;

	/// The content of the vector.
	car_t* data;
} cv_t;

typedef struct node node_t;
typedef struct queue_node qnode_t;

/* Methods for managing shared memory */

// Create object with PARKING data, set mutexes and conds as shared
bool create_shared_object( shm_t* shm, const char* share_name );

void destroy_shared_object( shm_t* shm );

bool get_shared_object( shm_t* shm, const char* share_name );


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
//void *thf_entr(void);

/**
 * Thread function for checking 
 * 
 */
//void *thf_park(void);

/**
 * Thread Layout
 *
 * One thread for creating new cars and adding them onto random entrance queues
 *
 * One thread per entrance 
 * (5 total)
 * 
 * One thread for keeping time in ms (DONE)
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
 * 
 * QUESTIONS
 * dv ensure cap, dv growth factor?? what is that about
 * 
 */
