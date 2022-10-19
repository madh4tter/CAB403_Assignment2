#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "firealarm.h"

int compare(const void *first, const void *second)
{
	return *((const int *)first) - *((const int *)second);
}

void tempmonitor(int level)
{
	struct tempnode *templist = NULL; 
	struct tempnode *newtemp = NULL;
	struct tempnode *medianlist = NULL;
	struct tempnode *oldesttemp = NULL;
	int count = 0;
	int addr = 0;
	int temp = 0;
	int mediantemp = 0;
	int hightemps = 0;
	
	while(alarm_active == 0){ // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- Fixed by changing it to only monitoring while the alarm is not active. Once the alarm is active, the system goes into 'alarm mode'. 
																					// This can only be changed by resetting the whole system.
		// Calculate address of temperature sensor
		addr = 104 * level + 2496; // Changed octal to integer...
		temp = *((int16_t *)(shm + addr));
		
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
				sorttemp[count++] = t->temperature;
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
				if (hightemps >= TEMPCHANGE_WINDOW * 0.9)
					alarm_active = 1;
				
				// If the newest temp is >= 8 degrees higher than the oldest
				// temp (out of the last 30), this is a high rate-of-rise.
				// Raise the alarm
				if (templist->temperature - oldesttemp->temperature >= 8)
					alarm_active = 1;
			}
		}
		
		usleep(2000);
		
	}
}

void *openboomgate(void *arg) // !!NASA Power of 10: #9 (Function pointers are not allowed)
{
	struct boomgate *bg = arg;
	pthread_mutex_lock(&bg->m);
	while(bg->s != 'O') { // !!NASA Power of 10: #2 (loops have fixed bounds)!! Thread now waits till the gate is open, then leaves it. Once it's done, it unlocks. 
		if (bg->s == 'C') {
			bg->s = 'R';
			pthread_cond_broadcast(&bg->c);
		}
		pthread_cond_wait(&bg->c, &bg->m);
	} 
	pthread_mutex_unlock(&bg->m); 
}

void emergency_mode(void)
{
	// char key = NULL;
	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	
	// Handle the alarm system and open boom gates
	// Activate alarms on all levels
	for (int i = 0; i < LEVELS; i++) {
		int addr = 104 * i + 2498; // Octal use not allowed, changed to integer scalar
		char *alarm_trigger = (char *)shm + addr;
		*alarm_trigger = 1;
	}
	
	// Open up all boom gates
	pthread_t *boomgatethreads = malloc(sizeof(pthread_t) * (ENTRANCES + EXITS));
	for (int i = 0; i < ENTRANCES; i++) {
		int addr = 288 * i + 96;
		volatile struct boomgate *bg = shm + addr;
		pthread_create(boomgatethreads + i, NULL, openboomgate, bg);
	}
	for (int i = 0; i < EXITS; i++) {
		int addr = 192 * i + 1536;
		volatile struct boomgate *bg = shm + addr;
		pthread_create(boomgatethreads + ENTRANCES + i, NULL, openboomgate, bg);
	}
	
	// Show evacuation message on an endless loop
	do { // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- FIXED BY HAVING EXIT KEY PROGRAMMED
		char *evacmessage = "EVACUATE ";
		for (char *p = evacmessage; *p != '\0'; p++) {
			for (int i = 0; i < ENTRANCES; i++) {
				int addr = 288 * i + 192;
				volatile struct parkingsign *sign = shm + addr;
				pthread_mutex_lock(&sign->m);
				sign->display = *p;
				pthread_cond_broadcast(&sign->c);
				pthread_mutex_unlock(&sign->m);
			}
			usleep(20000);
			// key = getchar(); // Old idea, would reset once the 'q' key is pressed. Updated to only reset when alarm_active is reset. This can be handled by a controller program. 
		}
	} while(alarm_active == 1)
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_join(threads[i], NULL);
	}
	
	munmap((void *)shm, 2920);
	close(shm_fd);
}


int main(void) // Must have input declarations -- added 'void' input.
{
	shm_fd = shm_open("PARKING", O_RDWR, 0);
	shm = (volatile void *) mmap(0, 2920, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	
	pthread_t *threads = malloc(sizeof(pthread_t) * LEVELS);
	
	for (int i = 0; i < LEVELS; i++) {
		pthread_create(threads + i, NULL, (void *(*)(void *)) tempmonitor, (void *)i);
	}

	while(alarm_active == 0){
		usleep(1000);
	}
	emergency_mode();

	// for (;;) { // !!NASA Power of 10: #2 (loops have fixed bounds)!! -- FIXED
	// 	if (alarm_active) {
	// 		emergency_mode(); // !!NASA Power of 10: #1 (Avoid complex flow constructs, like goto)!! -- FIXED
	// 	}
	// 	usleep(1000);
	// }
}
