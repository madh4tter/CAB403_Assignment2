#include "PARKING.h"
#include "linked_list.h"
#include "hash_table_read.h"
#include "shm_methods.h"
#include "status_display.h"

#define NONE '\0' /* Null value in the LPR*/
#define RATE 0.05


// Threads
pthread_t enter_thread[ENTRANCES];
pthread_t exit_thread[EXITS];
pthread_t level_thread[LEVELS];
pthread_t boom_gates_enter[ENTRANCES];
pthread_t boom_gates_exit[EXITS];
 
// Global Variables
bool running = true;
int m_counter = 0;

// Mutex's needed for lpr_entrance funcation
pthread_mutex_t car_park_lock; /* New Car Array Lock*/

// Create link list
node_t *car_list = NULL;

// Create Hash tabe
htab_t htab;

shm_t shm;

// Send error message to shell window
void print_err(char *message){
    printf("%s", message);
    fflush(stdout);
}

// Get Allowed Cars //////////////////////////////////////////////////////////////
void htab_create(void)
{
    size_t buckets = 100;

    if(!htab_init(&htab, buckets))
    {
        print_err("Init fail");
    }
}

// Enternce of Car Park //////////////////////////////////////////////////////////

int car_count_level(node_t *head) /* Counts the amount of cars per level */
{
    int counter[LEVELS];
    int holder;
    for (; head != NULL; head = head->next)
    {
        holder = atoi(head->vehicle->level);
        counter[holder]++;
    }
    return counter[LEVELS];
}

char level_assign(int car_count[LEVELS]) /* Assign car to level*/
{
    int min = 0;
    for (int i = 0; i > LEVELS; i++)
    {
        if(car_count[i] < car_count[min])
        {
            min = i;
        }
    }

    char level_char = min + '0'; /* Convert to char */

    return level_char;
}

// Boomgates //////////////////////////////////////////////////////////////////////
void m_sim_gates(gate_t *gate){
   pthread_mutex_lock(&gate->lock);
   if(gate->status == 'C'){
    gate->status = 'R';
    pthread_cond_broadcast(&gate->cond);
    while(gate->status != 'O'){
        pthread_cond_wait(&gate->cond, &gate->lock);
    }
    usleep(20*1000);
    gate->status = 'L';
    pthread_cond_broadcast(&gate->cond);
    while(gate->status != 'C'){
        pthread_cond_wait(&gate->cond, &gate->lock);
    }
   }
   pthread_mutex_unlock(&gate->lock);
   pthread_cond_broadcast(&gate->cond);
}


void entrance_lpr(entrance_t *ent)
{
    int car_count_levels[LEVELS];
    char holder[6];
    char assign_level;
    node_t* find;

    while(running == true)
    { 
        pthread_mutex_lock(&ent->LPR.lock);

        pthread_cond_wait(&ent->LPR.cond, &ent->LPR.lock); /* Wait until a car appears */

        strcpy(holder, ent->LPR.plate);
        pthread_mutex_unlock(&ent->LPR.lock);

        find = node_find_lp(car_list, holder);

        if(search_plate(&htab, holder) == true && find == NULL) /* Checks if the car is allowed in */
        {
            pthread_mutex_lock(&car_park_lock);
            if (m_counter < (LEVELS * LEVEL_CAPACITY))
            {
                m_counter++;
                pthread_mutex_unlock(&car_park_lock);

                car_count_levels[LEVELS] = car_count_level(car_list);
                assign_level = level_assign(car_count_levels);

                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = assign_level; /* Sceen show level value */
                pthread_mutex_unlock(&ent->screen.lock);
                pthread_cond_signal(&ent->screen.cond);

                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t)); /* Create new levels */
                new_vehicle->licence_plate = strdup((char*)holder);
                new_vehicle->level = &assign_level;
                new_vehicle->arrival = clock();
                

                node_t *newhead = node_add(car_list, new_vehicle);
                car_list = newhead; /* Add car to linked list*/

                usleep(2000); /* ensure that sim is waiting for signal */
                m_sim_gates(&ent->gate);


            }
            else /* If the carpark is full */
            {
                pthread_mutex_unlock(&car_park_lock);
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = 'F'; /* Full message */
                pthread_mutex_unlock(&ent->screen.lock);
                pthread_cond_signal(&ent->screen.cond);
            }
        }
        else /* If the car is not allowed*/
        {
            pthread_mutex_lock(&ent->screen.lock);
            ent->screen.display = 'X';
            pthread_mutex_unlock(&ent->screen.lock);
            pthread_cond_signal(&ent->screen.cond);
        }       
    }  
}



// Level Adjustments ////////////////////////////////////////////////////////////

typedef struct level_tracker
{
    level_t *level;
    int floor;
} level_tracker_t;

void level_lpr(level_tracker_t *lvl)
{
    char holder;
    node_t* find;

    while (running == true)
    {
        pthread_mutex_lock(&lvl->level->LPR.lock);
        pthread_cond_wait(&lvl->level->LPR.cond, &lvl->level->LPR.lock);

        find = node_find_lp(car_list, lvl->level->LPR.plate);
        if (find != NULL)
        {
            holder = lvl->floor + '0';
            find->vehicle->level = &holder;
        }

        pthread_mutex_unlock(&lvl->level->LPR.lock);
    }
}

// Lets talk money $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$

void money(vehicle_t* car)
{
    FILE* fp = fopen(REVENUE_FILE, "a"); /* Open file */

    time_t exit_time = time(0) * 1000;
    double total_time = difftime(exit_time, car->arrival);
    double total_amount = total_time * RATE;

    fprintf(fp, "%s $%0.2f\n", car->licence_plate, total_amount); /* Write and close file*/
}


// Exit of Car Park /////////////////////////////////////////////////////////////////////

void exit_lpr(exit_t *ext)
{
    pthread_mutex_lock(&ext->LPR.lock);
    while(running == true)
    {
        while (ext->LPR.plate[0] == NONE)
        {
            pthread_cond_wait(&ext->LPR.cond, &ext->LPR.lock);
        }

        if (running == true) 
        {
            pthread_mutex_lock(&car_park_lock);
            m_counter--;
            pthread_mutex_unlock(&car_park_lock);

            node_t *find = node_find_lp(car_list, ext->LPR.plate); /* Find car that is leaving */

            money(find->vehicle); /* Charge the amount and save it*/

            car_list = node_delete(car_list, ext->LPR.plate); /* Remove from linked list*/

        }
    }

    pthread_mutex_unlock(&ext->LPR.lock);
}

/****************** DISPLAY FUNCTIONS ********************/
void get_entry(){
	char* plate_num;
	char bg_status;
	char sign_display;
    shm_t *shm_ptr = &shm;

	/* save cursor location to use later */
	printf("\033[s");

	for(int i = 0; i < ENTRANCES; i++){
		/* lpr status */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].LPR.lock);
        // printf("Passed LPR"); fflush(stdout);
		plate_num = shm_ptr->data->entrances[i].LPR.plate;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].LPR.lock);

		/* gate status */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].gate.lock);
        // printf("Passed Gate"); fflush(stdout);
		bg_status = shm_ptr->data->entrances[i].gate.status;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].gate.lock);

		/* info screen */
		pthread_mutex_lock(&shm_ptr->data->entrances[i].screen.lock);
        // printf("Passed screen"); fflush(stdout);
		sign_display = shm_ptr->data->entrances[i].screen.display;
		pthread_mutex_unlock(&shm_ptr->data->entrances[i].screen.lock);

		/* print entrance */
		printf("\n***** ENTRANCE %d *****\n", i+1);
		printf("Entrance %d LPR status: %s\n", i+1, plate_num);
		printf("Entry %d Boomgate status: %c\n", i+1, bg_status);
		printf("Digital Sign %d Status: %c\n", i+1, sign_display);

	}
}

void get_exit(){
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

void get_level(){
	char* plate_num;
	int16_t temperature;
	int occup = 4; /* **** need to get real value *** */
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

		/* print level */
		printf("\n\t\t\t\t\t\t\t\t  ");
		printf("*****  LEVEL %d *****\n", i+1);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d LPR status: %s\n", i+1, plate_num);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Temperature Sensor %d Status: %d\n", i+1, temperature);
		printf("\t\t\t\t\t\t\t\t  ");
		printf("Level %d Occupancy: %d of %d\n", i+1, occup, LEVEL_CAPACITY);
	}

}

double convertToNum(char a[]){
	double val;
	val = atof(a);
	return val;
}

void print_revenue(){
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

void *thf_display(void *ptr){
		shm_t shm;

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

		while(1){
			fflush(stdout);

	    	close(pipefd[1]);

	    	dup2(stdout_bk, fileno(stdout)); /* restore stdout */

	    	read(pipefd[0], buf, 100);
			printf("%s\n", buf);

			/* if no fail print display otherwise break loop */
			if(check_for_fail(buf) != true){
				system("clear");
				get_entry(&shm);
				get_exit(&shm);
				get_level(&shm);
                
				print_revenue();
                printf("Count: %d\n", m_counter);

				//usleep(50*1000);
                sleep(1.5);
				fflush(stdout);
			}
			else {
				break;
			}
		}
	return ptr;
}


// Main function
int main(void)
{
    int error;

    // Get Shared Memory
    if (get_shared_object(&shm, "PARKING") != true)
    {
        print_err("error getting shared memory\n");
    }

    //printf("%p", &shm);

    // Get Licence plates
    htab_create();
    get_plates(&htab, PLATES_FILE);


    // Start mutex's
    if (pthread_mutex_init(&car_park_lock, NULL) != 0)
    {
        print_err("error creating mutex for car park\n");
        return 0;
    }
    
    // Start threads
    pthread_t display_th;
    pthread_create(&display_th, NULL, thf_display, NULL);
    // entrance thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The entrances for the cars
        error = pthread_create(&enter_thread[i], NULL, (void*)entrance_lpr, &shm.data->entrances[i]);
        if (error != 0)
        {
            print_err("error creating threads\n");
        }

    }

    // Level Threads
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

    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The entrances for the cars
        error = pthread_create(&exit_thread[i], NULL, (void*)exit_lpr, &shm.data->exits[i]);
        if (error != 0)
        {
            print_err("error creating thread\n");
        }

    }

    // End threads
    // entrance thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The entrances for the cars
        pthread_join(enter_thread[i], NULL);
        // The boomgates for each entrance
        pthread_join(boom_gates_enter[i], NULL);
    }
    // Level Threads
    for (int i = 0; i < LEVELS; i++)
    {       
        pthread_join(level_thread[i], NULL);
    }
    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The entrances for the cars
        pthread_join(exit_thread[i], NULL);
        pthread_join(boom_gates_exit[i], NULL);
    }

    // // Clean up everything
    // pthread_mutex_destroy(&car_park_lock);
    // free(car_list);
    // destroy_shared_object(&shm);
    // htab_destroy(&htab);
    // printf("Made it to the end");
    return 0;
}

// :)