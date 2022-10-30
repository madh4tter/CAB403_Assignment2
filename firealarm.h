#include <pthread.h>

#pragma once

/* Struct definition for linked list */
struct tempnode {
	int temperature;
	struct tempnode *next;
};

/* Function definitions */
/**
 * @brief Deletes all nodes after a certain node in a linked list
 * 
 * @param templist Head of a linked list of temperatures
 * @param after Index of the last desired element in list
 * @return struct tempnode* New head of the linked list
 */
struct tempnode *deletenodes(struct tempnode *templist, int after);

/**
 * @brief Compares temperature values, used in qsort to sort the 
 * temperatures into an ascending list
 * 
 * @param first A value to compare
 * @param second A value to compare
 * @return int first - second
 */
int compare(const void *first, const void *second);

/**
 * @brief Thread function to track smoothed temperatures of a certain 
 * level and detect a fire
 * 
 * @param level ID of the level assigned to the thread
 */
void tempmonitor(int level);

/**
 * @brief Thread function to open boomgate in an emergency
 * 
 * @param gate_addr Address to the gate (type gate_t) assigned to this thread 
 */
void openboomgate(void *gate_addr);

/**
 * @brief Thread function to display "EVACUATE " on every screen in an emergency
 * 
 * @param arg Address to the screen (type screen_t) assigned to this thread
 */
void evac_msg(void *screen_addr);

/**
 * @brief Function to cancel existing threads and create new threads to
 * handle an energency
 * 
 */
void emergency_mode(void);