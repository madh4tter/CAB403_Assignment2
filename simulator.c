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

/* Include shared memory struct definitions */
#include "PARKING.h"
#include "simulator.h"

#define ACC_CAR_AMT 100
#define PLATE_LENGTH 6


/* this value can count up to 9*10^18 ms or 292 million years
   if that's too big for memory, I can account for an overflow
   uint32 is too small, only lasts 50 days

   need to protect this value with a mutex?
*/
mstimer_t runtime;

qnode_t *eqlist_head;
pthread_mutex_t eq_lock;
pthread_cond_t eq_cond;

/* Create linked list for cars existing in simulation */
node_t *sim_cars_head = NULL;
pthread_mutex_t sim_cars_lock;


/*************** LINKED LIST METHODS ********************************************/
typedef struct node node_t;

struct node {
    car_t *car;
    node_t *next;
};

node_t *node_push(node_t *head, car_t *car){
    /* create new node to add to list */
    node_t *new = (node_t *)malloc(sizeof(node_t));
    if (new == NULL)
    {
        printf("Memory allocation failure");
        fflush(stdout);
        return NULL;
    }

    new->car = car;
    new->next = head;

    return new;
}

node_t *node_pop(node_t *head)
{
    /* Remove and return the last car of a linked list */
    node_t *ret = (node_t *)malloc(sizeof(node_t));
    node_t *temp_head = head;

    if (head == NULL){
        /* return NULL if list is empty */
        return NULL;
    } else if(head->next->car == NULL) {
        /* case where only one car in queue */
        ret->car = head->car;
        ret->next = NULL;
        return ret;
    } else {
        /* 2 or more cars */
        for(; temp_head != NULL; temp_head = temp_head->next){
            /* Find second last node */
            if(temp_head->next->next->car == NULL){
                /* Remove reference to end node */
                ret->car = temp_head->next->car;
                temp_head->next = NULL;
                ret->next = head;

                /* return popped node */
                return ret;
            }
        }
    }
    return NULL;
}

node_t *node_find_LP(node_t *head, char *plate){
    for (; head != NULL; head = head->next)
    {
        if (strcmp(plate, head->car->plate) == 0)
        {
            return head;
        }
    }
    return NULL;
}

node_t *node_delete(node_t *head, char *plate)
{
    node_t *previous = NULL;
    node_t *current = head;
    while (current != NULL)
    {
        if (strcmp(plate, current->car->plate) == 0)
        {
            node_t *newhead = head;
            if (previous == NULL) // first item in list
                newhead = current->next;
            else
                previous->next = current->next;
            free(current);
            return newhead;
        }
        previous = current;
        current = current->next;
    }

    // name not found
    return head;
}

void node_print(node_t *head){
    for (; head != NULL; head = head->next)
    {
        fprintf(stderr, head->car->plate);
        fflush(stdout);
    }
}

typedef struct queue_node qnode_t;

struct queue_node {
    node_t *queue;
    qnode_t *qnext;
    uint8_t entrID;
};

qnode_t *qnode_push(qnode_t *head, node_t *queue){
    /* create new node to add to list */
    qnode_t *new = (qnode_t *)malloc(sizeof(qnode_t));
    if (new == NULL)
    {
        printf("Memory allocation failure");
        fflush(stdout);
        return NULL;
    }

    new->queue = queue;
    new->qnext = head;

    return new;
}


/*************************** SHARED MEMORY METHODS *********************************/

void init_conds(shm_t* shm){
    for(int i =0; i<ENTRANCES; i++){
        pthread_cond_init(&(shm->data->entrances[i].LPR.cond), NULL);
        pthread_cond_init(&(shm->data->entrances[i].gate.cond), NULL);
        pthread_cond_init(&(shm->data->entrances[i].screen.cond), NULL);
    }

    for(int i =0; i<EXITS; i++){
        pthread_cond_init(&(shm->data->exits[i].LPR.cond), NULL);
        pthread_cond_init(&(shm->data->exits[i].gate.cond), NULL);
    }    

    for(int i =0; i<LEVELS; i++){
        pthread_cond_init(&(shm->data->levels[i].LPR.cond), NULL);
    }   

    pthread_cond_init(&runtime.cond, NULL);
    pthread_cond_init(&eq_cond, NULL);
}

void init_mutexes(shm_t* shm){
    for(int i =0; i<ENTRANCES; i++){
        pthread_mutex_init(&(shm->data->entrances[i].LPR.lock), NULL);
        pthread_mutex_init(&(shm->data->entrances[i].gate.lock), NULL);
        pthread_mutex_init(&(shm->data->entrances[i].screen.lock), NULL);
    }

    for(int i =0; i<EXITS; i++){
        pthread_mutex_init(&(shm->data->exits[i].LPR.lock), NULL);
        pthread_mutex_init(&(shm->data->exits[i].gate.lock), NULL);
    }    

    for(int i =0; i<LEVELS; i++){
        pthread_mutex_init(&(shm->data->levels[i].LPR.lock), NULL);
    }   

    pthread_mutex_init(&runtime.lock, NULL);
    pthread_mutex_init(&eq_lock, NULL);
    pthread_mutex_init(&sim_cars_lock, NULL);
}

bool create_shared_object( shm_t* shm, const char* share_name ) {
    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(share_name);

    // Assign share name to shm->name.
    shm->name = share_name;

    // Create the shared memory object, allowing read-write access, and saving the
    // resulting file descriptor in shm->fd. If creation failed, ensure 
    // that shm->data is NULL and return false.
    shm->fd = shm_open(share_name, O_RDWR | O_CREAT, 0666);

    if(shm->fd == -1){
        shm->data = NULL;
        return false;
    }
    

    // Set the capacity of the shared memory object via ftruncate. If the 
    // operation fails, ensure that shm->data is NULL and return false. 
    if(ftruncate(shm->fd, sizeof(PARKING_t)) == -1){
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

    // Initialise mutexes
    init_mutexes(shm);

    // Initialise conditional variables
    init_conds(shm);

    // If we reach this point we should return true.
    return true;
}

void destroy_shared_object( shm_t* shm ) {
    // Remove the shared memory object.
    munmap(shm, sizeof(shm_t));

    shm_unlink(shm->name);

    shm->fd = -1;
    shm->data = NULL;
}

void trig_LPR(LPR_t *LPR, car_t *car){
    /* Lock mutex */
    pthread_mutex_lock(&LPR->lock);

    /* Store plate in LPR */
    for(int i=0; i<PLATE_LENGTH; i++){
        LPR->plate[i] = car->plate[i];
    }

    /* Unlock mutex */
    pthread_mutex_unlock(&LPR->lock);

    /* Signal conditional variable */
    pthread_cond_signal(&LPR->cond);

}

/********************** MISC FUNCTIONS ********************************************/
node_t *read_file(char *file, node_t *head) {
    FILE* text = fopen(file, "r");
    if(text == NULL){
        printf("Could not open plates.txt\n");
        fflush(stdout);
    }

    int i= 0;
    char str[PLATE_LENGTH+1];
    while(fgets(str, PLATE_LENGTH+1, text)) {
        if(!(i%2)){
            car_t *newcar = (car_t *)malloc(sizeof(car_t));
            newcar->plate = strdup(str);
            head = node_push(head, newcar);
            memset(str, 0, 7);
        }

        i++;
    }

    fclose(text);
    return head;
}

void destroy_car(car_t *car){
    pthread_mutex_lock(&sim_cars_lock);
    sim_cars_head = node_delete(sim_cars_head, car->plate);
    pthread_mutex_unlock(&sim_cars_lock);
}

char sim_gates(gate_t *gate){
   while(gate->status == 'O' || gate->status == 'C'){
      pthread_cond_wait(&gate->cond, &gate->lock);
   }
   if(gate->status == 'L'){
      usleep(10000);
      gate->status = 'C';
   } else {
      usleep(10000);
      gate->status = 'O';
   }
   char ret_val = gate->status;
   pthread_mutex_unlock(&gate->lock);
   return ret_val;
}
/*************************** THREAD FUNCTIONS ******************************/

/* Thread function to keep track of time in ms*/
void *thf_time(void *ptr){
    struct timespec start, end;
    /* determine start time of thread */
    clock_gettime(CLOCK_MONOTONIC, &start);
    

    while(1){
        /* sleep for one millisecond */
        usleep(1000);

        /* Read time on arbitrary clock */
        clock_gettime(CLOCK_MONOTONIC, &end);

        /* Acquire mutex */
        pthread_mutex_lock(&runtime.lock);

        /* Compare time to start time of thread */
        runtime.elapsed = (end.tv_sec - start.tv_sec) * 1000 
        + (end.tv_nsec - start.tv_nsec) / 1000000;

        /* Signal that change has occured and unlock the mutex */
        pthread_mutex_unlock(&runtime.lock);
        pthread_cond_signal(&runtime.cond);

    }

    return ptr;
}

void *thf_creator(void *ptr){
    /* Create linked list for accepted car number plates */
    node_t *acc_cars_head = NULL;
    acc_cars_head = read_file("plates.txt", acc_cars_head);

    /* Initialise local loop variables */
    bool exists;
    int car_loc;
    int count;
    qnode_t *eq_temp;
    node_t *head_temp;
    char str[6];
   
    while(1){
        /* Wait between 1-100ms before generating another car */
        /* supposed to protect this with a mutex? seems to work fine without */
        int wait = (rand() % 99) + 1;
        usleep(wait * 1000);
        

        /* create car */
        car_t *new_car = malloc(sizeof(car_t));

        /* Initialise other values of the car */
        new_car->entr_time = 0;
        new_car->exit_time = 0;
        new_car->lvl = 0;

        /* assign random number plate */
        count = 0;
        do {
            exists = false;
            if(rand()%2){
                /* select from approved list */
                car_loc = rand() % ACC_CAR_AMT;
                head_temp = acc_cars_head;
                for (; head_temp != NULL; head_temp = head_temp->next){
                    if (count >= car_loc){
                        new_car->plate = head_temp->car->plate;
                        break;
                    } else {
                        count++;
                    }
                }
            } else {
                /* Generate random */
                for (int i=0; i<3;i++){
                str[i] = '0' + rand() % 9;
                str[i+3] = 'A' + rand() % 25;
                new_car->plate = strdup(str);
                }
            }

            /* Check every value in existing cars list to see if it matches. 
            If it matches, regenerate an LP*/
            pthread_mutex_lock(&sim_cars_lock);
            head_temp = sim_cars_head;
            pthread_mutex_unlock(&sim_cars_lock); 
            for (; head_temp != NULL; head_temp = head_temp->next){
                if(!strcmp(head_temp->car->plate, new_car->plate)){
                    exists = true;
                }
            }

        } while (exists == true);

        /* Add onto list of existing cars*/
        pthread_mutex_lock(&sim_cars_lock);
        sim_cars_head = node_push(sim_cars_head, new_car);
        pthread_mutex_unlock(&sim_cars_lock);

        /* Randomly choose an entrance queue to add to */
        int entranceID = rand() % (ENTRANCES);

        /* Lock mutex of global simulator-shared variable */
        pthread_mutex_lock(&eq_lock);

        /* Find car queue in list of entrances*/
        eq_temp = eqlist_head;
        for(; eq_temp != NULL; eq_temp = eq_temp->qnext){
            if(eq_temp->entrID == entranceID){
                break;
            }
        }

        /* Push new car onto start of queue */
        eq_temp->queue = node_push(eq_temp->queue, new_car);

        /* Unlock mutex and signal that a new car has arrived */
        pthread_mutex_unlock(&eq_lock);
        pthread_cond_signal(&eq_cond);
    }
    return ptr;
}

void *thf_entr(void *data){
    /* Correct variable type */
    int entranceID = *((char *)data);

    /* Get shared memory data */
    shm_t shm;
    if(!get_shared_object(&shm, SHARE_NAME)){
        printf("Shared memory connection failed\n");
        return NULL;
    }

    /* Lock mutex of entrance queue list */
    pthread_mutex_lock(&eq_lock);

    /* Identify head of queue relevant to this thread */
    qnode_t *qhead = eqlist_head;
    for (; qhead != NULL; qhead = qhead->qnext){
        if(qhead->entrID == entranceID){
            break;
        }
    }

    /* Acquire the entrance simulated hardware */ 
    shm_t *shm_ptr = &shm;
    entrance_t *entrance = &shm_ptr->data->entrances[entranceID];
    level_t *level;

    /* Unlock the mutex */
    pthread_mutex_unlock(&eq_lock);

    /* Initialise loop local variables */
    node_t *popped_node;
    car_t *popped_car;
    while(1){
        /* Lock mutex of entrance queue list */
        pthread_mutex_lock(&eq_lock);

        /* Check if queue is empty. If so, wait for a car to be added */
        while(qhead->queue->car == NULL){
            pthread_cond_wait(&eq_cond, &eq_lock);
        }

        /* Remove car from end of queue */
        popped_node = node_pop(qhead->queue);

        /* Popped node holds the popped car and head of the queue with car removed */
        popped_car = popped_node->car;
        qhead->queue = popped_node->next;

        /* Unlock mutex */
        pthread_mutex_unlock(&eq_lock);
        
        /* Wait 2ms */
        usleep(2000);

        /* Trigger LPR */
        trig_LPR(&entrance->LPR, popped_car);

        /* Watch Info Screen for assigned level */
        pthread_cond_wait(&entrance->screen.cond, &entrance->screen.lock);
        short assigned_lvl = (short) entrance->screen.display;
        if(!(assigned_lvl >= '0' && assigned_lvl <= ('0' + ENTRANCES-1))){
            /* Remove car from simulation */
            destroy_car(popped_car);
        } else {
            popped_car->lvl = assigned_lvl;
            level = &shm_ptr->data->levels[assigned_lvl];
        
        
            /* Raise boom gate */
            while( sim_gates(&entrance->gate) != 'O'){}
        
        
            /* Assign leaving time to car */
            pthread_cond_wait(&runtime.cond, &runtime.lock);
            popped_car->exit_time = runtime.elapsed + (rand() % 9900) + 100;
            pthread_mutex_unlock(&runtime.cond);
        
            /* Wait for car to drive to spot */
            usleep(10000);
        
            /* Signal Level LPR */
            //pthread_mutex_lock(&level->LPR->)

            /* Close boom gate */
            // Forever loop? what to do about this?
            while( sim_gates(&entrance->gate) != 'C');

        }

    }

    return NULL;
}


/************************** MAIN ***********************************************/
int main(void){
    shm_t shm;
    if(!create_shared_object(&shm, SHARE_NAME)){
        printf("Shared memory creation failed\n");
        return EXIT_FAILURE;
    }

    pthread_t time_th;
    pthread_t creator_th;

    pthread_t entr_threads[ENTRANCES];
    int th_entrID[ENTRANCES];

    /* Create linked list that holds linked lists for entrance queues */
    for(int i=0; i<ENTRANCES; i++){
        node_t *queue = (node_t *)malloc(sizeof(node_t));
        eqlist_head = qnode_push(eqlist_head, queue);
        eqlist_head->entrID = i;
    }


    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&creator_th, NULL, thf_creator, NULL);

    for (int i = 0; i < ENTRANCES; i++)
    {
        th_entrID[i] = i;
        pthread_create(&entr_threads[i], NULL, thf_entr, (void *)&th_entrID[i]);
    }


    sleep(4);
    shm_t *shm_ptr = &shm;
    entrance_t *entr = &shm_ptr->data->entrances[0];
    pthread_mutex_lock(&entr->screen.lock);
    entr->screen.display = '0';
    pthread_mutex_unlock(&entr->screen.lock);
    pthread_cond_signal(&entr->screen.cond);




    for (int i = 0; i < ENTRANCES; i++)
    {
        th_entrID[i] = i;
        pthread_join(entr_threads[i], NULL);
    }
    pthread_join(time_th, NULL);
    pthread_join(creator_th, NULL);



    return EXIT_SUCCESS;
    
}


