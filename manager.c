/*******************************************************************
 * \file   manager.c
 * \brief  Manager of car park system
 * 
 * \author CAB403 Group 69
 * \date   October 2022
 *********************************************************************/
/* Include necessary libraries and definitions */
#include "PARKING.h"
#include "linked_list.h"
#include "hash_table_read.h"
#include "shm_methods.h"
#include "manager.h"

/* Define macros */
#define NONE '\0' /* Null value in the LPR*/
#define RATE 0.05
#define BUFFER 20

 /* Define global variables */
int m_counter = 0;
mstimer_t runtime;
shm_t shm;
FILE *fp;
int level_tracker[LEVELS] = {0};
pthread_mutex_t car_park_lock;      /* New Car Array Lock*/
node_t *car_list = NULL;            /* Create link list */
htab_t htab;                        /* Create Hash tabe */



/*************************** MISC FUNCTIONS ********************/

void print_err(char *message)
{
    /* Send error message to shell window */
    printf("%s", message);
    fflush(stdout);
}

void htab_create(void)
{
    size_t buckets = 100;

    if(!htab_init(&htab, buckets))
    {
        print_err("Init fail");
    }
}

char car_count(void) 
{
    /* Assign levels equally */
    int next_level = m_counter % LEVELS;
    if (level_tracker[next_level] < LEVEL_CAPACITY)
    {
        level_tracker[next_level]++;
    }
    /* Convert to character to display on screen */
    char level = next_level + '0'; 
    return level;
}

void m_sim_gates(gate_t *gate) 
{
    /* Wait to ensure sim is ready for signal */
    usleep(1000);
    pthread_mutex_lock(&gate->lock);

    /* Open gate */
    if(gate->status == 'C'){
    gate->status = 'R';
    pthread_mutex_unlock(&gate->lock);
    pthread_cond_broadcast(&gate->cond);
    pthread_mutex_lock(&gate->lock);
    while(gate->status != 'O'){
        pthread_cond_wait(&gate->cond, &gate->lock);
    }

    /* Close Gate */
    usleep(20*1000);
    gate->status = 'L';
    pthread_mutex_unlock(&gate->lock);
    pthread_cond_broadcast(&gate->cond);
    pthread_mutex_lock(&gate->lock);
    while(gate->status != 'C'){
        pthread_cond_wait(&gate->cond, &gate->lock);
    }
   }
   pthread_mutex_unlock(&gate->lock);
   pthread_cond_broadcast(&gate->cond);
}

void money(vehicle_t* car)
{
    fp = fopen(REVENUE_FILE, "a"); /* Open file */

    /* Determine how long the car has been in the park */
    pthread_mutex_lock(&runtime.lock);
    double total_time = runtime.elapsed - car->arrival;
    pthread_mutex_unlock(&runtime.lock);
    double total_amount = total_time * RATE;

    fprintf(fp, "%s $%0.2f\n", car->licence_plate, total_amount); /* Write and close file*/

    fclose(fp);
}

/********************* THREAD FUNCTIONS  *****************************/
void entrance_lpr(entrance_t *ent)
{
    /* Initialise loop variable */
    char holder[6];
    char assign_level;
    node_t* find = NULL;

    /* Loop until cancelled */
    while(true)
    { 
        /* Wait until a car appears */
        pthread_mutex_lock(&ent->LPR.lock);
        pthread_cond_wait(&ent->LPR.cond, &ent->LPR.lock); 

        /* Identify car plate */
        strcpy(holder, ent->LPR.plate);
        pthread_mutex_unlock(&ent->LPR.lock);

        /* Identify if car is allowed */
        find = node_find_lp(car_list, holder);
        if(search_plate(&htab, holder) == true && find == NULL) /* Checks if the car is allowed in */
        {
            /* Identify if there is space in car park */
            pthread_mutex_lock(&car_park_lock);
            if (m_counter < (LEVELS * LEVEL_CAPACITY))
            {
                m_counter++;

                /* Determine level to assign to car */
                assign_level = car_count();
                pthread_mutex_unlock(&car_park_lock);
                
                /* Sceen show level value */
                usleep(2000);
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = assign_level + 1; 
                pthread_mutex_unlock(&ent->screen.lock);
                pthread_cond_broadcast(&ent->screen.cond);

                /* Create new vehicle */
                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t)); 
                new_vehicle->licence_plate = strdup((char*)holder);
                new_vehicle->level = &assign_level;
                pthread_mutex_lock(&runtime.lock);
                new_vehicle->arrival = runtime.elapsed;
                pthread_mutex_unlock(&runtime.lock);
                
                /* Add car to linked list*/
                node_t *newhead = node_add(car_list, new_vehicle);
                car_list = newhead; 

                /* ensure that sim is waiting for signal */
                usleep(3000); 
                m_sim_gates(&ent->gate);


            }
            else /* If the carpark is full */
            {
                usleep(2000);
                pthread_mutex_unlock(&car_park_lock);
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = 'F'; /* Full message */
                pthread_mutex_unlock(&ent->screen.lock);
                pthread_cond_signal(&ent->screen.cond);
            }
        }
        else /* If the car is not allowed*/
        {
            usleep(2000);
            pthread_mutex_lock(&ent->screen.lock);
            ent->screen.display = 'X';
            pthread_mutex_unlock(&ent->screen.lock);
            pthread_cond_signal(&ent->screen.cond);
        }  
    }  
}

void level_lpr(level_tracker_t *lvl)
{
    /* Intialise loop variables */
    char holder;
    node_t* find;

    while (true)
    {
        pthread_mutex_lock(&lvl->level->LPR.lock);
        pthread_cond_wait(&lvl->level->LPR.cond, &lvl->level->LPR.lock);

        find = node_find_lp(car_list, lvl->level->LPR.plate);
        /* Add car to appropriate floor list*/
        if (find != NULL)
        {
            holder = lvl->floor + '0';
            find->vehicle->level = &holder;
        }

        pthread_mutex_unlock(&lvl->level->LPR.lock);
    }
}

void exit_lpr(exit_t *ext)
{
    int level_holder;
    while(true)
    {
        /* Wait for car to signal the LPR */
        pthread_mutex_lock(&ext->LPR.lock);
        pthread_cond_wait(&ext->LPR.cond, &ext->LPR.lock);
        pthread_mutex_lock(&car_park_lock);

        /* Decrease total cars in lot */
        m_counter--;
        node_t *find = node_find_lp(car_list, ext->LPR.plate); /* Find car that is leaving */
        level_holder = (int)*find->vehicle->level - 48;
        if (level_tracker[level_holder] != 0)
        {
            level_tracker[level_holder]--;
        }
        pthread_mutex_unlock(&car_park_lock);

        usleep(2000); /* ensure that sim is waiting for signal */
        m_sim_gates(&ext->gate);

        money(find->vehicle); /* Charge the amount and save it*/
        car_list = node_delete(car_list, ext->LPR.plate); /* Remove from linked list*/
        
        pthread_mutex_unlock(&ext->LPR.lock);
    }
}

/*************************** DISPLAY FUNCTIONS ********************/
void get_entry(void){
	char* plate_num;
	char bg_status;
	char sign_display;
    shm_t *shm_ptr = &shm;

	/* save cursor location to use later */
	printf("\033[s");

	for(int i = 0; i < ENTRANCES; i++){
		/* lpr status */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].LPR.lock);
		plate_num = shm_ptr->data->entrances[i].LPR.plate;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].LPR.lock);

		/* gate status */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].gate.lock);
		bg_status = shm_ptr->data->entrances[i].gate.status;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].gate.lock);

		/* info screen */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].screen.lock);
		sign_display = shm_ptr->data->entrances[i].screen.display;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].screen.lock);

		/* print entrance */
		printf("\n***** ENTRANCE %d *****\n", i+1);
		printf("Entrance %d LPR status: %s\n", i+1, plate_num);
		printf("Entry %d Boomgate status: %c\n", i+1, bg_status);
		printf("Digital Sign %d Status: %c\n", i+1, sign_display);

	}
}

void get_exit(void){
	char* plate_num;
	char bg_status;
    shm_t *shm_ptr = &shm;

	/* go to entrance starting line */
	printf("%s", GOTO_LINE1);

	for(int i = 0; i < EXITS; i++){
		/* lpr status */
		pthread_mutex_lock(&shm_ptr->data->exits[i].LPR.lock);
		plate_num = shm_ptr->data->exits[i].LPR.plate;
		pthread_mutex_unlock(&shm_ptr->data->exits[i].LPR.lock);
		/* gate status */
		pthread_mutex_lock(&shm_ptr->data->exits[i].gate.lock);
		bg_status = shm_ptr->data->exits[i].gate.status;
		pthread_mutex_unlock(&shm_ptr->data->exits[i].gate.lock);

		/* print exit */
		printf("\n\t\t\t\t  ");
		printf("*****   EXIT %d  *****\n", i+1);
		printf("\t\t\t\t  ");
		printf("Exit %d LPR status: %s\n", i+1, plate_num);
		printf("\t\t\t\t  ");
		printf("Exit %d Boomgate status: %c\n\n", i+1, bg_status);
	}

}

void get_level(void){
	char* plate_num;
	int16_t temperature;
    shm_t *shm_ptr = &shm;

	/* go to entrance starting line */
	printf("%s", GOTO_LINE1);

	for(int i = 0; i < LEVELS; i++){
		/* lpr status */
		pthread_mutex_lock(&shm_ptr->data->levels[i].LPR.lock);
		plate_num = shm_ptr->data->levels[i].LPR.plate;
		pthread_mutex_unlock(&shm_ptr->data->levels[i].LPR.lock);

		/* temp number */
		temperature = shm_ptr->data->levels[i].temp;

        pthread_mutex_lock(&car_park_lock);
        int occup_base = m_counter / LEVELS;
        int occup_add = m_counter % LEVELS;
        if (i + 1 < occup_add + 1)
        {
            occup_base = occup_base + 1;
        }
        pthread_mutex_unlock(&car_park_lock);

		/* print level */
		printf("\n\t\t\t\t\t\t\t\t  ");
		printf("*****  LEVEL %d *****\n", i+1);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d LPR status: %s\n", i+1, plate_num);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Temperature Sensor %d Status: %d\n", i+1, temperature);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d Occupancy: %d of %d\n", i+1, level_tracker[i], LEVEL_CAPACITY);
	}

}

double convertToNum(char a[]){
	double val;
	val = atof(a);
	return val;
}

void print_revenue(void){
	/* read file */
	FILE *bill_file;
	bill_file = fopen(REVENUE_FILE, "r");

	/* Declare useful constants */
	char buff[BUFFER];
	char revenue_str[BUFFER];
	double bill_total = 0;

	/* Clear buff array */
	memset(buff, 0, BUFFER);

	/* read line by line until EOF */
	while (fgets(buff, sizeof(buff), bill_file)) {
		/* scan line and store relevant part as char */
		sscanf(buff, "%*s $%s", revenue_str);
		/* convert scan result to double and add to bill total */
		bill_total += convertToNum(revenue_str);

		/* Clear bill_string array */
		memset(revenue_str, 0, BUFFER);

	}
	/* print bill total rounded to 2 dp */
	printf("\nTotal Revenue: $%.2f\n",bill_total);
	/* close file */
	fclose(bill_file);
}

bool check_for_fail(char* check){
	/* check if string contains "fail" */
	if (strstr(check, "fail") != NULL || strstr(check, "error") != NULL){
		return true;
	}
	return false;
}

void thf_display(void){
		int stdout_bk; /* is fd for stdout backup */
		int pipefd[2];
		pipe(pipefd); /* create pipe */
		char buf[1024];
		stdout_bk = dup(fileno(stdout));
		/* What used to be stdout will now go to the pipe. */
		dup2(pipefd[1], fileno(stdout));

		if ( get_shared_object( &shm, SHARE_NAME ) != true ) {
			printf("Shared memory connection failed.\n" );

		}

		while(true){
			fflush(stdout);

	    	close(pipefd[1]);

	    	dup2(stdout_bk, fileno(stdout)); /* restore stdout */

	    	read(pipefd[0], buf, 100);
			printf("%s\n", buf);

			/* if no fail print display otherwise break loop */
			if(check_for_fail(buf) != true){
				system("clear");
				get_entry();
				get_exit();
				get_level();
                
                printf("\n\n\n\n\n");fflush(stdout);
				print_revenue();

				usleep(50*1000);
				fflush(stdout);
			}
			else {
				break;
			}
		}
}

/* Thread function to keep track of time in ms*/
void thf_time(void){
    struct timespec start, end;
    /* determine start time of thread */
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while(true){
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
}


/******************************* MAIN  *****************************/
int main(void)
{
    /* Clear revenue file */
    fp = fopen(REVENUE_FILE, "w");
    fclose(fp);
    int error;

    /* Get Shared Memory */
    if (get_shared_object(&shm, "PARKING") != true)
    {
        print_err("error getting shared memory\n");
    }
    shm_t *shm_ptr = &shm;

    /* Get Licence plates */
    htab_create();
    get_plates(&htab, PLATES_FILE);


    /* Start mutex's */
    if (pthread_mutex_init(&car_park_lock, NULL) != 0)
    {
        print_err("error creating mutex for car park\n");
        return 0;
    }
    
    /* Start threads */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* Threads */
    pthread_t enter_thread[ENTRANCES];
    pthread_t exit_thread[EXITS];
    pthread_t level_thread[LEVELS];
    pthread_t display_th;
    pthread_t time_th;

    /* Create threads */
    pthread_create(&display_th, NULL, (void *)thf_display, NULL);
    pthread_create(&time_th, NULL, (void *)thf_time, NULL);
    for (int i = 0; i < ENTRANCES; i++)
    {
        /* The entrances for the cars */
        if (pthread_create(&enter_thread[i], NULL, (void*)entrance_lpr, &shm.data->entrances[i]) != 0)
        {
            print_err("error creating threads\n");
        }

    }

    /* Level Threads */
    for (int i = 0; i < LEVELS; i++)
    {
        level_tracker_t new_level;
        new_level.floor = i;
        new_level.level = &shm.data->levels[i];

        error = pthread_create(&level_thread[i], NULL, (void*)level_lpr, &new_level);
        if (error != 0)
        {
            print_err("error creating levels\n");
        }
    }

    /* Exit threads */
    for (int i = 0; i < EXITS; i++)
    {
        /* The entrances for the cars */
        error = pthread_create(&exit_thread[i], NULL, (void*)exit_lpr, &shm.data->exits[i]);
        if (error != 0)
        {
            print_err("error creating thread\n");
        }

    }

    char local_alarm = '0';
    /* Wait for alarm to activate */
    while(local_alarm != '1' || local_alarm != 'e'){
        for(int i=0; i<LEVELS; i++){
            if(shm_ptr->data->levels[i].alarm == '1' || shm_ptr->data->levels[i].alarm == 'e'){
                local_alarm = shm_ptr->data->levels[i].alarm;
            }
        usleep(1000);
        }
    }

    /* End all existing threads except display*/
    pthread_cancel(time_th);
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_cancel(enter_thread[i]);
    }
    for (int i = 0; i < EXITS; i++) {
        pthread_cancel(exit_thread[i]);
    }
    for (int i = 0; i < LEVELS; i++) {
        pthread_cancel(level_thread[i]);
    }
    if (local_alarm == 'e')
    {
        pthread_cancel(display_th);
        system("clear");
        printf("Manager Finsihed");
    }

    /* Unlock all mutexes */
    pthread_cancel(time_th);
    for (int i = 0; i < ENTRANCES; i++) {
        pthread_mutex_unlock(&shm_ptr->data->entrances[i].gate.lock);
        pthread_mutex_unlock(&shm_ptr->data->entrances[i].screen.lock);
        pthread_mutex_unlock(&shm_ptr->data->entrances[i].LPR.lock);
    }
    for (int i = 0; i < EXITS; i++) {
        pthread_mutex_unlock(&shm_ptr->data->exits[i].gate.lock);
        pthread_mutex_unlock(&shm_ptr->data->exits[i].LPR.lock);
    }
    for (int i = 0; i < LEVELS; i++) {
        pthread_mutex_unlock(&shm_ptr->data->levels[i].LPR.lock);
    }


    // Clean up everything
    pthread_mutex_destroy(&car_park_lock);
    delete_list(car_list, 0);
    destroy_shared_object(&shm);
    htab_destroy(&htab);
    return 0;
}