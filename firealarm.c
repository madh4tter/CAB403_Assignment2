#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


#include "firealarm.h"
#include "PARKING.h"
#include "shm_methods.h"



/* I've put your firealarm.h stuff in here because the compiling doesn't seem to work
otherwise. You can put this in a new .h file if you want, 
just needs to be under a different name */
int shm_fd = 0;
shm_t shm;



char alarm_active = '0';
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

pthread_t *fthreads;

#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30

struct boomgate {
	pthread_mutex_t boom_mutex;
	pthread_cond_t boom_cond;
	char boom_char;
};

struct parkingsign {
	pthread_mutex_t parking_mutex;
	pthread_cond_t parking_cond;
	char parking_display;
};

struct tempnode {
	int temperature;
	struct tempnode *next;
};

struct tempnode *deletenodes(struct tempnode *templist, int after) // !!NASA Power of 10: #9 (Function pointers are not allowed)
{
	if (templist->next) {
		templist->next = deletenodes(templist->next, after - 1);
	}
	if (after <= 0) {
		free(templist);
		return NULL;
	}
	return templist;
}

int compare(const void *first, const void *second)
{
	return *((const int *)first) - *((const int *)second);
}

void tempmonitor(int level)
{
	shm_t *shm_ptr = &shm;
	struct tempnode *templist = NULL; 
	struct tempnode *newtemp = NULL;
	struct tempnode *medianlist = NULL;
	struct tempnode *oldesttemp = NULL;
	int count = 0;
	//int temp_addr = 0;
	int temp = 0;
	int mediantemp = 0;
	int hightemps = 0;


	while(alarm_active == '0'){ // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- Fixed by changing it to only monitoring while the alarm is not active. Once the alarm is active, the system goes into 'alarm mode'. 
																					// This can only be changed by resetting the whole system.
		pthread_mutex_unlock(&alarm_mutex);
		// Calculate address of temperature sensor
		// temp_addr = (104 * level) + 2496; // Changed octal to integer...
		// int shm_addr = (int)(uintptr_t)shm_ptr;
		// temp = shm_addr + temp_addr;
		temp = shm_ptr->data->levels[level].temp;
		
		// Add temperature to beginning of linked list
		newtemp = malloc(sizeof(struct tempnode));
		newtemp->temperature = temp;
		newtemp->next = templist;
		templist = newtemp;
		
		// Delete nodes after 5th
		deletenodes(templist, MEDIAN_WINDOW);
		
		// Count nodes
		count = 0;
		for (struct tempnode *t = templist; t != NULL; t = t->next) {
			count++;
		}
		
		if (count == MEDIAN_WINDOW) { // Temperatures are only counted once we have 5 samples
			int *sorttemp = malloc(sizeof(int) * MEDIAN_WINDOW);
			count = 0;
			for (struct tempnode *t = templist; t != NULL; t = t->next) {
				count++;
				sorttemp[count] = t->temperature;
			}
			qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare); 
			mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];

			
			// Add median temp to linked list
			newtemp = malloc(sizeof(struct tempnode));
			newtemp->temperature = mediantemp;
			newtemp->next = medianlist;
			medianlist = newtemp;
			
			// Delete nodes after 30th
			deletenodes(medianlist, TEMPCHANGE_WINDOW);
			
			// Count nodes
			count = 0;
			hightemps = 0;
			
			for (struct tempnode *t = medianlist; t != NULL; t = t->next) {
				// Temperatures of 58 degrees and higher are a concern
				if (t->temperature >= 58) hightemps++;
				// Store the oldest temperature for rate-of-rise detection
				oldesttemp = t;
				count++;
			}
			
			if (count == TEMPCHANGE_WINDOW) {
				// If 90% of the last 30 temperatures are >= 58 degrees,
				// this is considered a high temperature. Raise the alarm
				if (hightemps >= TEMPCHANGE_WINDOW * 0.9){
					printf("Rate of Rise Recognised\n");
					fflush(stdout);
					pthread_cond_broadcast(&alarm_condvar);
				}
				
				// If the newest temp is >= 8 degrees higher than the oldest
				// temp (out of the last 30), this is a high rate-of-rise.
				// Raise the alarm
				if (templist->temperature - oldesttemp->temperature >= 8){
					printf("Fixed Temp Recognised\n");
					fflush(stdout);
					pthread_cond_broadcast(&alarm_condvar);
				}
			}
		
		usleep(2000);
		pthread_mutex_lock(&alarm_mutex);
		alarm_active = shm_ptr->data->levels[level].alarm;
		}
	}
}

void *openboomgate(void *arg) // !!NASA Power of 10: #9 (Function pointers are not allowed)
{
	struct boomgate *boom_gate = arg;
	pthread_mutex_lock(&boom_gate->boom_mutex);
	while(boom_gate->boom_char != 'O') { // !!NASA Power of 10: #2 (loops have fixed bounds)!! Thread now waits till the gate is open, then leaves it. Once it's done, it unlocks. 
		if (boom_gate->boom_char == 'C') {
			boom_gate->boom_char = 'R';
			pthread_cond_broadcast(&boom_gate->boom_cond);
		}
		pthread_cond_wait(&boom_gate->boom_cond, &boom_gate->boom_mutex);
	} 
	pthread_mutex_unlock(&boom_gate->boom_mutex);
	return arg; 
}

void emergency_mode(void)
{
	shm_t *shm_ptr = &shm;

	// char key = NULL;
	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	
	// Handle the alarm system and open boom gates
	// Activate alarms on all levels
	for (int i = 0; i < LEVELS; i++) {
		// int lvl_addr = (104 * i) + 2498; // Octal use not allowed, changed to integer scalar
		// char *shm_char = (char *)shm_ptr;
		// char *alarm_trigger = shm_char + lvl_addr;
		shm_ptr->data->levels[i].alarm = '1';
		printf("Alarms activated\n");
		//alarm_trigger = 1;
	}
	
	// Open up all boom gates
	pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (ENTRANCES + EXITS));
	for (int i = 0; i < ENTRANCES; i++) {
		int ent_addr = (288 * i) + 96;
		void *shm_void_entrance = (void*)shm_ptr;
		struct boomgate *bg = shm_void_entrance + ent_addr;
		pthread_create(boomgatethreads + i, NULL, openboomgate, &bg);
	}
	for (int i = 0; i < EXITS; i++) {
		int exit_addr = (192 * i) + 1536;
		void *shm_void_exit = (void*)shm_ptr;
		struct boomgate *bg = shm_void_exit + exit_addr;
		pthread_create(boomgatethreads + ENTRANCES + i, NULL, openboomgate, &bg);
	}
	
	// Show evacuation message on an endless loop
	do { // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- FIXED BY HAVING EXIT KEY PROGRAMMED
		pthread_mutex_unlock(&alarm_mutex);
		const char *evacmessage = "EVACUATE ";
		for (const char *p = evacmessage; *p != '\0'; p++) {
			for (int i = 0; i < ENTRANCES; i++) {
				int sign_addr = (288 * i) + 192;
				void *shm_void_sign = (void*)shm_ptr;
				struct parkingsign *sign = shm_void_sign + sign_addr;
				pthread_mutex_lock(&sign->parking_mutex);
				sign->parking_display = *p;
				pthread_cond_broadcast(&sign->parking_cond);
				pthread_mutex_unlock(&sign->parking_mutex);
			}
			usleep(20000);
			// key = getchar(); // Old idea, would reset once the 'q' key is pressed. Updated to only reset when alarm_active is reset. This can be handled by a controller program. 
		}
		pthread_mutex_lock(&alarm_mutex);
		for(int i = 0; i < LEVELS; i++){
			char alarm_active_check = shm_ptr->data->levels[i].alarm;
			if(alarm_active_check){alarm_active = '1'; break;}
		}
	} while(alarm_active == '1');
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_join(fthreads[i], NULL);
	}
}


int main(void) // Must have input declarations -- added 'void' input.
{
	if (get_shared_object(&shm, SHARE_NAME) == 0){
		printf("Failed to get memory");
	}
	shm_t *shm_ptr = &shm;

	printf("shm_t addr %p", shm_ptr->data);
	fflush(stdout);
	shm_fd = shm_open("PARKING", O_RDWR, 0);
	
	fthreads = malloc(sizeof(pthread_t) * LEVELS);
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_create(fthreads + i, NULL, (void *) tempmonitor, (int *)(uintptr_t)i);
	}

	// while(alarm_active == 0){ // This will block the pthread alarm mutex, need better soln; This unlocks, waits for 1sec, then locks and checks alarm variable
	// 	pthread_mutex_unlock(&alarm_mutex);
	// 	usleep(1000);
	// 	pthread_mutex_lock(&alarm_mutex);
	// }
	pthread_mutex_lock(&alarm_mutex);

	pthread_cond_wait(&alarm_condvar, &alarm_mutex);

	pthread_mutex_unlock(&alarm_mutex);

	emergency_mode();

	destroy_shared_object(&shm);
	// for (;;) { // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- FIXED
	// 	if (alarm_active) {
	// 		emergency_mode(); // !!NASA Power of 10: #1 (Avoid complex flow constructs, like goto)!! -- FIXED
	// 	}
	// 	usleep(1000);
	// }

	
	return 0;
}
