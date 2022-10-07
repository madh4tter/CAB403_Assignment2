/*****************************************************************//**
 * \file   simulator.h
 * \brief  Method definitions for program simulator.c
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#include "PARKING.h"

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
void queue_car(char *plate);

/**
 * Signal the conditional variable of the LPR when a car reaches the front of a queue
 * 
 */
void trigger_LPR(LPR_t LPR);

/**
 * 
 */
void assign_lvl(char display);
