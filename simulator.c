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

/* Include shared memory struct definitions */
#include "PARKING.h"
#include "simulator.h"

#define ACC_CAR_AMT


/* this value can count up to 9*10^18 ms or 292 million years
   if that's too big for memory, I can account for an overflow
   uint32 is too small, only lasts 50 days

   need to protect this value with a mutex?
*/
mstimer_t runtime;

/********************** MISC FUNCTIONS ********************************************/
char *read_file(char *file) {
    uint8_t limit = 6;
   uint8_t car_amt = 100;
    FILE* text = fopen(file, "r");
    char line[limit];
    char accepted_cars[car_amt][limit];
    for(int i = 0; i<(car_amt); i++)
    {
        accepted_cars[i] = fgets(line, sizeof(line), text);
    } 
   return accepted_cars;
}


/**************** DYNAMIC VECTOR METHODS *****************************************/
void cv_init( cv_t* vec ) {
    vec->capacity = INITIAL_CAP;
    vec->size = 0;
    vec->data = malloc(INITIAL_CAP * sizeof(car_t));
    if(vec->data == NULL){
        printf("Failed to allocated memory for car vector\n");
        fflush(stdout);
    }
}

void cv_enscap( cv_t* vec, size_t new_size ) {
    if(new_size <= vec->capacity){
        // keep everything the same
    } else {
        /* ..................................................REMOVE THIS MAYBE*/
        if(vec->capacity * DV_GROWTH_FACTOR > new_size) {
            vec->capacity = vec->capacity * DV_GROWTH_FACTOR;
        } else {
            vec->capacity = new_size;
        }
        vec->data = realloc(vec->data, vec->capacity * sizeof(car_t));
        if(vec->data == NULL){
            printf("Realloc failed for car vector\n");
            fflush(stdout);
        }
    }
}

void cv_push( cv_t* vec, car_t new_item ) {
    cv_enscap(vec, vec->size + 1);
    vec->size += 1;
    vec->data[vec->size-1] = new_item;
}

car_t cv_pop( cv_t* vec ) {
    car_t *removed = malloc(sizeof(car_t));

    if(vec->size > 0){
        car_t *pop_data_location = &(vec->data[vec->size]);
        removed = pop_data_location;
        pop_data_location = NULL;
        vec->size -= 1;
    }
    return *removed;
}

/* ........................................... Might need to change this one to a search and compare*/
/*
car_t cv_remove_at( cv_t* vec, size_t pos ) {
    if(pos <= vec->size){
        car_t removed = vec->data[pos];
        car_t next;
        for(int i=pos; i<vec->size; i++){
            next = vec->data[i+1];
            vec->data[i] = next;
        }
        vec->size -= 1;

        return removed;
    } 
    return NULL;
}
*/

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

void *thf_creator(void *entr_qlist_void){
    cv_t *entr_qlist = entr_qlist_void;

    char *acc_cars = read_file("plates.txt");
   
    int counter=0;
    while(counter < 5){
        counter++;
        /* Wait between 1-100ms before generating another car */
        /* supposed to protect this with a mutex? seems to work fine without */
        int wait = (rand() % 99) + 1;
        usleep(wait * 1000);
        

        /* create car */
        car_t *new_car = malloc(sizeof(car_t));

        /* assign random number plate */
        bool exists;
        do {
            if(rand()%2){
                // select from approved list
                //new_car->plate = 
            } else {
                /* Generate random */
                for (int i=0; i<3;i++){
                new_car->plate[i] = '0' + rand() % 9;
                new_car->plate[i+3] = 'A' + rand() % 25;
                }
            }

            // Check every value in existing cars list to see if it matches
            exists = false;
        } while (exists == true);

        /* Randomly choose an entrance queue to add to */
        int entr_ID = rand() % ENTRANCES;
        if(entr_ID < 1){
            entr_ID = 1;
        }
        cv_t queue = entr_qlist[entr_ID];
        /* Push new car onto start of queue */
        cv_push(&queue, *new_car);

        printf("succesfully got here\n");
        fflush(stdout);
    }

    for(int i = 0; i<5; i++){
        printf("%s", entr_qlist[i].data->plate);
        fflush(stdout);
    }

    return entr_qlist;

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

    cv_t entr_qlist[ENTRANCES];
    for(int i = 0; i < ENTRANCES; i++){
        cv_init(&entr_qlist[i]);
    }




    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&test_th, NULL, thf_test, NULL);
    pthread_create(&creator_th, NULL, thf_creator, &entr_qlist);






    pthread_join(time_th, NULL);
    pthread_join(test_th, NULL);
    



    return EXIT_SUCCESS;
    
}


