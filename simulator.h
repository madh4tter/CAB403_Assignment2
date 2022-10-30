/*****************************************************************//**
 * \file   simulator.h
 * \brief  API definition for the simulator.c file
 * 
 * \author CAB403 Group 69
 * \date   October 2022
 *********************************************************************/

#include "PARKING.h"

/* Struct defintions */
/**
 * @brief Struct to hold a car's licence plate, assigned level and 
 * assigned exiting time
 * 
 */
typedef struct car {
    char *plate;
    char lvl;
    uint16_t exit_time; 
} car_t;

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

/* Linked list struct definitions */
typedef struct node node_t;

/**
 * @brief Linked list to hold a list of cars
 * 
 */
struct node {
    car_t *car;
    node_t *next;
};

typedef struct queue_node qnode_t;

/**
 * @brief Linked list node to hold a list of car queues
 * 
 */
struct queue_node {
    node_t *queue;
    qnode_t *qnext;
    uint8_t ID;
};

/* Function Definitions */
/**
 * @brief Pushes a car onto the head of a linked list
 * 
 * @param head Head of a linked list of cars
 * @param car The car to push onto the list
 * @return node_t* The new head of the list
 */
node_t *node_push(node_t *head, car_t *car);

/**
 * @brief Remove a car from the end of a linked list of cars
 * 
 * @param head The head of the linked list
 * @return node_t* A node where car holds the car that was removed from the list 
 * and next holds the head of the changed list
 */
node_t *node_pop(node_t *head);

/**
 * @brief Find and delete a car from a linked list of cars
 * 
 * @param head Head of the linked list
 * @param car Car to remove with a valid licence plate
 * @return node_t* Head of the changed list
 */
node_t *snode_delete(node_t *head, car_t *car);

/**
 * @brief Push a linked list of cars onto a linked list of queues
 * 
 * @param head Head of the list of queues
 * @param queue The head of a linked list of cars
 * @return qnode_t* New head of the list
 */
qnode_t *qnode_push(qnode_t *head, node_t *queue);

/**
 * @brief Initialise all conditional variables of the shared memory and 
 * simulator.c
 * 
 */
void init_conds(void);

/**
 * @brief Initialise all conditional variables of the shared memory and 
 * simulator.c
 * 
 */
void init_mutexes(void);

/**
 * @brief Create a shared object 
 * 
 * @param shared_mem Location to store shared memory object
 * @param share_name Name of shared memory object
 * @return true If mapping was successful
 * @return false If mapping was unsuccessful
 */
bool create_shared_object( shm_t* shared_mem, const char* share_name );

/**
 * @brief Trigger an LPR and its conditional variable
 * 
 * @param LPR Address of the LPR to trigger
 * @param car Car with a valid licence plate at the LPR
 */
void trig_LPR(LPR_t *LPR, car_t *car);

/**
 * @brief Initialise temperatures of each level in the park
 * 
 */
void temp_start(void);

/**
 * @brief Thread function to change temperatures in a normal range
 * 
 * @param ptr Address of data to pass to the function (NULL)
 */
void thf_temp(void);

/**
 * @brief Increase temperatures in a fire simulation until firealarm.c determines a 
 * fire has started
 * 
 */
void rate_of_rise(void);

/**
 * @brief Set temperature of a random level to 100 degrees to simulate a fire until
 * firealarm.c determines a fire has started
 * 
 */
void fixed_temp(void);

/**
 * @brief Wait for the user to input a type of fire to simulate, or to end the simulation
 * 
 * @param temp_th Address of the thread controlling the temperatures
 * @return char Returns 'a' if an alarm has been triggered, or returns 'e' if the simulation
 * should end
 * 
 */
char check_user_input(pthread_t temp_th);

/**
 * @brief Initialise the variable in the shared memory
 * 
 * @param shared_mem Address of the shared memory object
 */
void init_shmvals(shm_t *shared_mem);

/**
 * @brief Open text file of approved cars and add cars to a linked list
 * 
 * @param file String of the file name
 * @param head Head of the linked list to add cars to
 * @return node_t* New head of the approved cars list
 */
node_t *read_file(char *file, node_t *head);

/**
 * @brief Free memory of a car and remove from list of existing cars
 * 
 * @param car A car to delete
 */
void destroy_car(car_t *car);

/**
 * @brief Simulate the opening and closing of gates
 * 
 * @param gate Address of a gate
 * @return char The current state of the gate
 */
char sim_gates(gate_t *gate);

/**
 * @brief Compare the current time to the leaving time of every car inside the parking lot.
 * 
 * @param head Head of a list of cars
 * @param elap_time The current time of the simulation
 * @return car_t* A car that is due to leave
 */
car_t *comp_times(node_t *head, uint64_t elap_time);

/**
 * @brief Thread function to keep the running time of the simulation in ms
 * 
 */
void thf_time(void);

/**
 * @brief Thread function to create approved and non-approved cars and add them onto a 
 * random entrance queue
 * 
 */
void thf_creator(void);

/**
 * @brief Thread function to move a car from the front of the queue to inside the car park
 * 
 */
void thf_entr(void *data);

/**
 * @brief Thread function to remove a car from the parking lot when it's time has
 * elapsed
 * 
 */
void thf_inside(void);

/**
 * @brief Thread function to handle cars exiting the car park
 * 
 */
void thf_exit(void *data);

/**
 * @brief Thread function to open all gates in an emergency
 * 
 */
void thf_boomAlarm(void *arg);




