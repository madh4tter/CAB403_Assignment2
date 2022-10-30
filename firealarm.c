/*******************************************************************
 * \file   firealarm.c
 * \brief  Safety crtitcal program that assesses the temperatures of the car park 
 * levels to determine if there is a fire and, if so, override the manager
 * 
 * \author CAB403 Group 69
 * \date   October 2022
 *********************************************************************/
/* Include necessary libraries and definitions */
#include "firealarm.h"
#include "PARKING.h"
#include "shm_methods.h"

/* Define macros */
#define MEDIAN_WINDOW 5
#define TEMPCHANGE_WINDOW 30

/* Define global variables */
shm_t shm;
char alarm_active = '0';
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_condvar = PTHREAD_COND_INITIALIZER;

/*************** TEMPERATURE MONITORING METHODS *****************/
struct tempnode *deletenodes(struct tempnode *templist, int after)
{
	/* Delete nodes after index until end of list */
	if (templist->next != NULL) {
		templist->next = deletenodes(templist->next, after - 1);
	}
	/* Destroy linked list if after is 0 */
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
	/* Declare loop variables */
	shm_t *shm_ptr = &shm;
	struct tempnode *templist = NULL; 
	struct tempnode *newtemp = NULL;
	struct tempnode *medianlist = NULL;
	struct tempnode *oldesttemp = NULL;
	int count = 0;
	int temp = 0;
	int mediantemp = 0;
	int hightemps = 0;
	char local_check = '0';

	/* While alarm is not active or the program has not ended*/
	while(local_check == '0'){ 
		/* Temperature address gathered from shared memory pointer */
		temp = shm_ptr->data->levels[level].temp;
		
		/* Add temperature to beginning of linked list */
		newtemp = malloc(sizeof(struct tempnode));
		newtemp->temperature = temp;
		newtemp->next = templist;
		templist = newtemp;
		
		/* Delete nodes after 5th  */
		deletenodes(templist, MEDIAN_WINDOW);
		
		/*  Count nodes  */
		count = 0;
		for (struct tempnode *t = templist; t != NULL; t = t->next) {
			count++;
		}
		
		if (count == MEDIAN_WINDOW) { 
			/*  Temperatures are only counted once we have 5 samples  */
			int *sorttemp = malloc(sizeof(int) * MEDIAN_WINDOW);
			count = 0;
			for (struct tempnode *t = templist; t != NULL; t = t->next) {
				count++;
				sorttemp[count] = t->temperature;
			}
			qsort(sorttemp, MEDIAN_WINDOW, sizeof(int), compare); 
			mediantemp = sorttemp[(MEDIAN_WINDOW - 1) / 2];

			
			/*  Add median temp to linked list  */
			newtemp = malloc(sizeof(struct tempnode));
			newtemp->temperature = mediantemp;
			newtemp->next = medianlist;
			medianlist = newtemp;
			 
			/*  Delete nodes after 30th  */
			deletenodes(medianlist, TEMPCHANGE_WINDOW);
			
			/*  Count nodes  */
			count = 0;
			hightemps = 0;
			
			for (struct tempnode *t = medianlist; t != NULL; t = t->next) {
				/*  Temperatures of 58 degrees and higher are a concern  */
				if (t->temperature >= 58) hightemps++;
				/*  Store the oldest temperature for rate-of-rise detection  */
				oldesttemp = t;
				count++;
			}
			
			if (count == TEMPCHANGE_WINDOW) {
				/*  If 90% of the last 30 temperatures are >= 58 degrees,
					this is considered a high temperature. Raise the alarm  */
				if (hightemps >= TEMPCHANGE_WINDOW * 0.9){
					printf("Fixed Temp Recognised\n");
					fflush(stdout);
					pthread_cond_broadcast(&alarm_condvar);
				}
				
				/*  If the newest temp is >= 8 degrees higher than the oldest
					temp (out of the last 30), this is a high rate-of-rise.
					Raise the alarm  */
				if ((templist->temperature - oldesttemp->temperature) >= 8){
					printf("Rate of Rise Recognised\n");
					fflush(stdout);
					pthread_cond_broadcast(&alarm_condvar);
				}
			}
		/* Sleep for 2ms */
		usleep(2000);

		/* Update alarm */
		pthread_mutex_lock(&alarm_mutex);
		alarm_active = shm_ptr->data->levels[level].alarm;
		local_check = alarm_active;
		pthread_mutex_unlock(&alarm_mutex);
		}
	}
}

/************************** EMERGENCY METHODS *****************************************/
void openboomgate(void *gate_addr) 
{
	/* Correction of variable type */
	gate_t *boom_gate = (gate_t *)gate_addr;
	pthread_mutex_lock(&boom_gate->lock);
	while(boom_gate->status != 'O') { 
		/*  !!NASA Power of 10: #2 (loops have fixed bounds)!! 
		Thread now waits till the gate is open, then leaves it. Once it's done, it unlocks.  */
		if (boom_gate->status == 'C') {
			boom_gate->status = 'R';
			pthread_mutex_unlock(&boom_gate->lock);
			pthread_cond_broadcast(&boom_gate->cond);
		}
		pthread_mutex_lock(&boom_gate->lock);
		pthread_cond_wait(&boom_gate->cond, &boom_gate->lock);
	} 
	/* Unlock mutex */
	pthread_mutex_unlock(&boom_gate->lock); 
}

void evac_msg(void *screen_addr)
{
	/* Correction of variable type */
	screen_t *screen = (screen_t *)screen_addr;

	/*Initialise loop variables */
	shm_t *shm_ptr = &shm;
	char local_check = '1';
	printf("Showing EVACUATE on screens\n"); fflush(stdout);

	do { 
		pthread_mutex_unlock(&alarm_mutex);
		const char *evacmessage = "EVACUATE ";
		/* Show each letter of message on screen */
		for (const char *p = evacmessage; *p != '\0'; p++) {
			pthread_mutex_lock(&screen->lock);
			screen->display = *p;
			pthread_mutex_unlock(&screen->lock);
			pthread_cond_broadcast(&screen->cond);
			/* Sleep 2ms */
			usleep(2000);
		}
		pthread_mutex_lock(&alarm_mutex);
		/* Check if simulation has ended */
		for(int i = 0; i < LEVELS; i++){
			char alarm_active_check = shm_ptr->data->levels[i].alarm;
			if(alarm_active_check == 'e'){
				alarm_active = 'e';
				local_check = alarm_active;				
			}
		}
	} while(local_check == '1');
}

void emergency_mode(void)
{
	shm_t *shm_ptr = &shm;

	fprintf(stderr, "*** ALARM ACTIVE ***\n");
	
	/*  Handle the alarm system and open boom gates
		Activate alarms on all levels  */
	for (int i = 0; i < LEVELS; i++) {
		shm_ptr->data->levels[i].alarm = '1';
		printf("Alarms activated\n");
	}
	
	pthread_t screens_th[ENTRANCES];
	pthread_t entrboom_th[ENTRANCES];
	pthread_t exitboom_th[EXITS];
	gate_t *gate_addr;
	screen_t *screen_addr;

	/* Open up all boom gates */
	for (int i = 0; i < ENTRANCES; i++) {
		gate_addr = &shm_ptr->data->entrances[i].gate;
		/* pthread_mutex_unlock(&gate_addr->lock);  */
		if(pthread_create(&entrboom_th[i], NULL, (void *)openboomgate, gate_addr) != 0){
			printf("Failed to create threads\n"); fflush(stdout);
		}
	}
	for (int i = 0; i < EXITS; i++) {
		gate_addr = &shm_ptr->data->exits[i].gate;
		/* pthread_mutex_unlock(&gate_addr->lock);  */
		if(pthread_create(&exitboom_th[i], NULL, (void *)openboomgate, gate_addr) != 0){
			printf("Failed to create threads\n"); fflush(stdout);
		}
	}
	
	/* Create threads to show evacutaion message on every screen */
	for (int i = 0; i < ENTRANCES; i++) {
		screen_addr = &shm_ptr->data->entrances[i].screen;
		pthread_mutex_unlock(&screen_addr->lock);
		if(pthread_create(&screens_th[i], NULL, (void *)evac_msg, screen_addr) != 0){
			printf("Failed to create threads\n"); fflush(stdout);
		}
	}

	/* Wait for emergency threads to finish */
	for (int i = 0; i < EXITS; i++) {
		pthread_join(exitboom_th[i], NULL);
	}
	for (int i = 0; i < ENTRANCES; i++) {
		pthread_join(entrboom_th[i], NULL);
		pthread_join(screens_th[i], NULL);
	}

}

/**************************** MAIN ****************************************************/
int main(void) 
{
	/* Get shared memory address */
	if (get_shared_object(&shm, SHARE_NAME) == 0){
		printf("Failed to get memory");
	}
	
	/* Create threads for temperature monitoring */
	pthread_t fthreads[LEVELS];
	for (int i = 0; i < LEVELS; i++) {
		pthread_create(&fthreads[i], NULL, (void *) tempmonitor, (int *)(uintptr_t)i);
	}

	/* Wait until alarm is signalled */
	pthread_mutex_lock(&alarm_mutex);
	pthread_cond_wait(&alarm_condvar, &alarm_mutex);
	pthread_mutex_unlock(&alarm_mutex);

	/* Destroy current threads and begin emergency protocol */
	emergency_mode();

	/* End program once emergency is over and clean up*/
	destroy_shared_object(&shm);

	return 0;
}
