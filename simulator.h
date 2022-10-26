/*****************************************************************//**
 * \file   simulator.h
 * \brief  Method and struct definitions for program simulator.c
 * 
 * \author Eloise
 * \date   October 2022
 *********************************************************************/

#include "PARKING.h"


typedef struct car {
    char *plate;
    char lvl;
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




