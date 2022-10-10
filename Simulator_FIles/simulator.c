/* Include necessary standard libraries */
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <pthread.h>

/* Include shared memory struct definitions */
#include "PARKING.h"
#include "simulator.h"


/* this value can count up to 9*10^18 ms or 292 million years
   if that's too big for memory, I can account for an overflow
   uint32 is too small, only lasts 50 days

   need to protect this value with a mutex?
*/
struct timespec start, end;
mstimer_t runtime;
car_t test_car;




/* Thread function to keep track of time in ms*/
void *thf_time(void *ptr){
    /* determine start time of thread*/
    clock_gettime(CLOCK_MONOTONIC, &start);
    

    while(1){
        // sleep for one millisecond
        usleep(1000);

        // read time
        clock_gettime(CLOCK_MONOTONIC, &end);

        // Acquire mutex
        pthread_mutex_lock(&runtime.lock);
        pthread_cond_wait(&runtime.cond, &runtime.lock);

        /* Compare time to start time of thread */
        runtime.elapsed = (end.tv_sec - start.tv_sec) * 1000 
        + (end.tv_nsec - start.tv_nsec) / 1000000;

        printf("%ld\n", runtime.elapsed);
        fflush(stdout);

        pthread_cond_signal(&runtime.cond);
        pthread_mutex_unlock(&runtime.lock);


    }
}

void *thf_test(void *ptr){
    /*test_car.entr_time = 1000;
    printf("Car Entered\n"); 
    fflush(stdout);

    test_car.exit_time = test_car.entr_time + 5000;

    if(runtime.elapsed >= test_car.exit_time){
        printf("Car Left\n"); 
        fflush(stdout);
    }
    
    pthread_mutex_lock(&runtime.lock);
    pthread_cond_wait(&runtime.cond, &runtime.lock);
    printf("%ld", runtime.elapsed);
    fflush(stdout);
    pthread_cond_signal(&runtime.cond);
    pthread_mutex_unlock(&runtime.lock);
    */

    return NULL;
    
}


int main(void){

    pthread_t time_th;
    pthread_t test_th;

    pthread_mutex_init(&runtime.lock, NULL);
    pthread_cond_init(&runtime.cond, NULL);

    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&test_th, NULL, thf_test, NULL);

    pthread_join(time_th, NULL);
    pthread_join(test_th, NULL);
    



    return EXIT_SUCCESS;
    
}

