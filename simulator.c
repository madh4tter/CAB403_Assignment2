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
char acc_cars[ACC_CAR_AMT][PLATE_LENGTH];
qnode_t *eqlist_head;



/*************** LINKED LIST METHODS ********************************************/
typedef struct node node_t;

struct node {
    car_t *car;
    node_t *next;
};

node_t *node_add(node_t *head, car_t *car){
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
    qnode_t *next;
    uint8_t entrID;
};

qnode_t *qnode_add(qnode_t *head, node_t *queue){
    /* create new node to add to list */
    qnode_t *new = (qnode_t *)malloc(sizeof(qnode_t));
    if (new == NULL)
    {
        printf("Memory allocation failure");
        fflush(stdout);
        return NULL;
    }

    new->queue = queue;
    new->next = head;

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
            head = node_add(head, newcar);
            memset(str, 0, 7);
        }

        i++;
    }

    fclose(text);
    return head;
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
        pthread_cond_signal(&runtime.cond);
        pthread_mutex_unlock(&runtime.lock);

    }

    return ptr;
}

void *thf_creator(void *ptr){
    /* Create linked list for accepted car number plates */
    node_t *acc_cars_head = NULL;
    acc_cars_head = read_file("plates.txt", acc_cars_head);

    /* Create linked list for cars existing in simulation */
    node_t *sim_cars_head = NULL;

    bool exists;
    int car_loc;
    int count;

    qnode_t *eq_temp;
    node_t *head_temp;
   
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
                new_car->plate[i] = '0' + rand() % 9;
                new_car->plate[i+3] = 'A' + rand() % 25;
                }
            }

            /* Check every value in existing cars list to see if it matches. If it matches, regenerate an LP*/
            head_temp = sim_cars_head;
            for (; head_temp != NULL; head_temp = head_temp->next){
                if(!strcmp(head_temp->car->plate, new_car->plate)){
                    exists = true;
                }
            }

        } while (exists == true);

        /* Add onto list of existing cars*/
        sim_cars_head = node_add(sim_cars_head, new_car);


        /* Randomly choose an entrance queue to add to */
        int entranceID = rand() % (ENTRANCES);

        /* Find dynamic car queue in list of entrances*/
        eq_temp = eqlist_head;
        for(; eq_temp != NULL; eq_temp = eq_temp->next){
            if(eq_temp->entrID == entranceID){
                break;
            }
        }

        /* Push new car onto start of queue */
        node_add(eq_temp->queue, new_car);
        
        
    }
    return ptr;
}

void *thf_entr(void *ptr){


    return ptr;
}

void *thf_test(void *ptr){
    /* Test access to variables 
    while(1){

        pthread_cond_wait(&runtime.cond, &runtime.lock);
        printf("%ld\n", runtime.elapsed);
        fflush(stdout);
        usleep(1000);

    }*/

    return ptr;
}

/************************** MAIN ***********************************************/
int main(void){
    pthread_mutex_t rand_lock;

    pthread_t time_th;
    pthread_t test_th;
    pthread_t creator_th;

    pthread_mutex_init(&runtime.lock, NULL);
    pthread_mutex_init(&rand_lock, NULL);

    pthread_cond_init(&runtime.cond, NULL);

    /* testing only */
    car_t *testcar = (car_t *)malloc(sizeof(car_t));
    testcar->plate = "TESTER";
    testcar->entr_time=1000;
    testcar->exit_time=1020;
    testcar->lvl=1;


    /* Create linked list that holds linked lists for entrance queues */
    for(int i=0; i<ENTRANCES; i++){
        node_t *queue = (node_t *)malloc(sizeof(node_t));
        eqlist_head = qnode_add(eqlist_head, queue);
        eqlist_head->entrID = i;
    }
    
    // start here
    node_add(eqlist_head->queue, testcar);

    
    

    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&test_th, NULL, thf_test, NULL);
    pthread_create(&creator_th, NULL, thf_creator, NULL);






    pthread_join(time_th, NULL);
    pthread_join(test_th, NULL);
    pthread_join(creator_th, NULL);



    return EXIT_SUCCESS;
    
}


