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


#include "PARKING.h"




int main(void){
    shm_t* PARK_shm;

    create_shared_object(&PARK_shm, SHARE_NAME);

    get_shared_object(&PARK_shm, SHARE_NAME);

    return EXIT_SUCCESS;
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


void destroy_shared_object( shm_t* shm ) {
    // Remove the shared memory object.
    munmap(shm, sizeof(shm_t));

    shm_unlink(shm->name);

    shm->fd = -1;
    shm->data = NULL;
}