/*****************************************************************//**
 * \file   manager.h
 * \brief  API definition for the manager.c file 
 * 
 * \author CAB403 Group 69
 * \date   October 2022
 *********************************************************************/
#include "PARKING.h"

/* Struct defintions */
/**
 * @brief Struct to hold the running time of the simulation,
 * protected by a mutex and conditional variable
 * 
 */
typedef struct mstimer{
    uint64_t elapsed; 
    pthread_mutex_t lock;
    pthread_cond_t cond;
} mstimer_t;

/**
 * @brief Struct to hold the address and ID of a level
 * 
 */
typedef struct level_tracker
{
    level_t *level;
    int floor;
} level_tracker_t;


/* Method definitions */
/**
 * @brief Print and flush a message to the window, using the word error will stop the
 * display function
 * 
 * @param message String to print
 */
void print_err(char *message);

/**
 * @brief Create hashtable to store accepted cars
 * 
 */
void htab_create(void);

/**
 * @brief Assign a level to a car, filling up levels equally
 * 
 * @return char The level to dsiplay on the screen
 */
char car_count(void);

/**
 * @brief Send signals to open and close the gates
 * 
 * @param gate Address to the gate
 */
void m_sim_gates(gate_t *gate);

/**
 * @brief Thread function to operate the entrance of the car park
 * 
 * @param ent Adress of the relevant entrance
 */
void entrance_lpr(entrance_t *ent);

/**
 * @brief Thread function to detect a car entering and leaving a level
 * 
 * @param lvl Address of the level tracking struct for that level
 */
void level_lpr(level_tracker_t *lvl);

/**
 * @brief Charge cars at the exit
 * 
 * @param car The car leaving the park
 */
void money(vehicle_t* car);

/**
 * @brief Thread function to handle cars at the exit gates
 * 
 * @param ext 
 */
void exit_lpr(exit_t *ext);

/**
 * @brief Display the variables of each entrance in the window
 * 
 */
void get_entry(void);

/**
 * @brief Display the variables of each exit in the window
 * 
 */
void get_exit(void);

/**
 * @brief Display the variables of each level in the window
 * 
 */
void get_level(void);

/**
 * @brief Convert a string to a double
 * 
 * @param a A chacter array to convert
 * @return double The converted string
 */
double convertToNum(char a[]);

/**
 * @brief Get the total revenue from the file and display in window
 * 
 */
void print_revenue(void);

/**
 * @brief 
 * 
 * @param check String to check for a fail or error
 * @return true If check contains the words string or error
 * @return false If check contains no "string" or "error"
 */
bool check_for_fail(char* check);

/**
 * @brief Thread function to display variables in window
 * 
 * @param ptr Thread data (NULL)
 * 
 */
void thf_display(void);

/**
 * @brief Thread function to keep time of function in ms
 * 
 * @param ptr Thead data NULL
 */
void thf_time(void);