#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "PARKING.h"
#include "linked_list.h"
#include "hash_table_read.h"

// gcc manager.c -o go -Wall -Wextra -lrt -lpthread


#define NONE '\0' // No value input in LPR
#define SLEEPTIME 2
#define GATETIME 1
#define RATE 0.05
#define CAPACITY 1
#define SHARE_NAME "PARKING"
#define MONEY_FILE "MONEY_FILE.txt"
#define PLATES_FILE "plates.txt"

// Need to change some define names to match the same in the PARKING.h file

// Threads
pthread_t enter_thread[ENTRANCES];
pthread_t exit_thread[EXITS];
pthread_t level_thread[LEVELS];
pthread_t boom_gates_enter[ENTRANCES];
pthread_t boom_gates_exit[EXITS];
pthread_t temp_thread;
pthread_t user_thread;

// Global Variables
shm_t shm;
int shm_fd;
bool rate_of_rise_fire = false;
bool fixed_tempture_fire = false;
bool end = false;
bool finish = false;
int counter = 0;

// Mutex's needed for lpr_enterence funcation
pthread_mutex_t car_park_lock; // New Car Array Lock

// Create link list
node_t *car_list = NULL;

// Create Hash tabe
htab_t htab;

// Shared Memory //////////////////////////////////////////////////////////////
bool get_shared_memory(shm_t* shm, const char* share_name)
{
    shm_fd = shm_open(share_name, O_RDWR, 0);
    if (shm->fd < 0)
    {  
        return false;
    }
	shm = mmap(0, 2920, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (&shm->data == MAP_FAILED)
    {
        return false;
    }

    return true;
}

void destroy_shared_object(shm_t* shm)
{
    munmap(&shm->data, sizeof(shm_t));
    close(shm_fd);
}

// Get Allowed Cars //////////////////////////////////////////////////////////////
void htab_create(void)
{
    size_t buckets = 100;

    if(!htab_init(&htab, buckets))
    {
        printf("Init Fail");
    }
}

// Enternce of Car Park //////////////////////////////////////////////////////////

// // Count cars per level linked list
int car_count_level(node_t *head)
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

// Assign Level
char level_assign(int car_count[LEVELS])
{
    int min = 0;
    for (int i = 0; i > LEVELS; i++)
    {
        if(car_count[i] < car_count[min])
        {
            min = i;
        }
    }

    //min++; // As 0 represents level 1;
    char level_char = min + '0'; // Convert to char

    return level_char;
}

// Read licence plates, chooses where a car goes and saves it in an array
void enterance_operation(entrance_t *ent)
{
    int car_count_levels[LEVELS];
    char holder[6];
    char assign_level;

    pthread_mutex_lock(&ent->LPR.lock);

    // Forever loop
    while(end == false && rate_of_rise_fire == false && fixed_tempture_fire == false)
    {
        // Consently Check for licence plate
        while(ent->LPR.plate[0] == NONE)
        {
            pthread_cond_wait(&ent->LPR.cond, &ent->LPR.lock); // Wait until another thread
            // Check if there is an emergenccy
        }

        strcpy(holder, ent->LPR.plate);
        if(search_plate(&htab, holder) == 1)//plate_check(&htab, holder) == true) // If the vehcile is allowed in and if there is a car at the gate
        {
            pthread_mutex_lock(&car_park_lock);
            if (counter < (LEVELS * CAPACITY))
            {
                counter++;
                pthread_mutex_unlock(&car_park_lock);

                car_count_levels[LEVELS] = car_count_level(car_list);
                assign_level = level_assign(car_count_levels); // Send to level

                // Change screen to assigned level
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = assign_level; // Level value;
                pthread_mutex_unlock(&ent->screen.lock);

                // Create car and assign variables
                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t));
                new_vehicle->licence_plate = (char*)ent->LPR.plate;
                new_vehicle->level = &assign_level;
                new_vehicle->arrival = clock() * 1000;

                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = '0'; // Clears the screen
                pthread_mutex_unlock(&ent->screen.lock);

                // Add car to linked list
                node_t *newhead = node_add(car_list, new_vehicle);
                car_list = newhead;

            }
            else
            {
                // If the carpark if full
                pthread_mutex_unlock(&car_park_lock);
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = 'F'; // Full message
                pthread_mutex_unlock(&ent->screen.lock);
            }
        }
        else // To catch any fall throughs
        {
            pthread_mutex_lock(&ent->screen.lock);
            ent->screen.display = 'X';
            pthread_mutex_unlock(&ent->screen.lock);
        }

        pthread_cond_signal(&ent->screen.cond);
    }

    pthread_mutex_unlock(&ent->LPR.lock);
}

// Boomgates //////////////////////////////////////////////////////////////////////

// Enternace boom gate (best)
void boomgate_good(entrance_t *ent)
{
    while(end == false && rate_of_rise_fire == false && fixed_tempture_fire == false)
    {
        // Open
        if (ent->screen.display != NONE && ent->gate.status == 'C')
        {
            pthread_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'R';
            pthread_mutex_unlock(&ent->gate.lock);

            do 
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
            } while (ent->gate.status != 'O');

        }

        usleep(2 * 1000);

        if (ent->gate.status == 'O')
        {
            pthread_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'L';
            pthread_mutex_unlock(&ent->gate.lock);

            do
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
            } while(ent->gate.status != 'C');
        }
    }
}

// Enternace boom gate
void boomgate_other(entrance_t *ent)
{
    pthread_mutex_lock(&ent->gate.lock);
    ent->gate.status = 'C';

    // While there is no alarm going off in the building
    while(end == false && rate_of_rise_fire == false && fixed_tempture_fire == false)
    {
        // While there is no car that has been given permission to enter the boomgate
        while(ent->screen.display != NONE && ent->gate.status == 'C') // Neeed to add if alarm
        {
            pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
        }
        
        // Car has been approved for entering
        // Boomgate rises
        ent->gate.status = 'R'; // U = Up
        pthread_cond_signal(&ent->gate.cond);

        while(ent->gate.status == 'R')
        {
            pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
        }

        usleep(SLEEPTIME * 1000);

        // Bommgate closes
        ent->gate.status = 'D'; // D = Down
        pthread_cond_broadcast(&ent->gate.cond);

        while(ent->gate.status == 'D')
        {
            pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
        }

        ent->gate.status = 'C'; // C = Closed
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

    while (end == false && rate_of_rise_fire == false && fixed_tempture_fire == false)
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
    // Open file
    FILE* fp = fopen(MONEY_FILE, "a");

    // Do fancy math
    time_t exit_time = time(0) * 1000;
    double total_time = difftime(exit_time, car->arrival);
    double total_amount = total_time * RATE;

    // Write and close file
    fprintf(fp, "%s $%0.2f\n", car->licence_plate, total_amount);
}

double total_money()
{
    double revenue = 0;
    FILE *fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t check;
    char reader[len];
    double money_double;
    char *start;

    fp = fopen(MONEY_FILE, "r");
    if (fp == NULL)
    {
        while((check = getline(&line, &len, fp)) != 1)
        {
            strncpy(reader, line + 8, len);
            money_double = strtod(reader, &start);
            revenue += money_double;
        }
    }

    return revenue;
}


// Exit of Car Park /////////////////////////////////////////////////////////////////////

void lpr_exit(exit_t *ext)
{
    pthread_mutex_lock(&ext->LPR.lock);
    while(end == false && rate_of_rise_fire == false && fixed_tempture_fire == false)
    {
        while (ext->LPR.plate[0] == NONE)
        {
            pthread_cond_wait(&ext->LPR.cond, &ext->LPR.lock); // Wait until another thread
        }

        if (rate_of_rise_fire == false && fixed_tempture_fire == false) 
        {
            pthread_mutex_lock(&car_park_lock);
            counter--;
            pthread_mutex_unlock(&car_park_lock);

            // Find car that is leaving
            node_t *find = node_find_lp(car_list, ext->LPR.plate);

            // Charge that person and save the amount
            money(find->vehicle);

            // Remove from list
            car_list = node_delete(car_list, ext->LPR.plate);

            printf("The total revenue is: %f", total_money());
        }
    }

    pthread_mutex_unlock(&ext->LPR.lock);
}

// Fire Alarm Controls//////////////////////////////////////////////////////////////

void tempture_start(shm_t *shm)
{
    int start_temp = 25;
    int base_temp = 10;

    for (int i = 0; i < LEVELS; i++)
    {
        shm->data->levels[i].temp = (rand() % start_temp) + base_temp;
    }
}

void tempture_controller(shm_t *shm)
{
    int change[3] = {-1, 0, 1};

    // Start random
    //srand(time(NULL));

    // At the start
    if(shm->data->levels[0].temp == 0)
    {
        printf("Temp start did not work");
    }
    // While running
    else
    {
        int random_index = rand() % 3;
        int random_change = change[random_index];
        for (int i = 0; i < LEVELS; i++)
        {
            if(shm->data->levels[i].temp < 11)
            {
                shm->data->levels[i].temp += change[2];
            }
            else if(shm->data->levels[i].temp > 39)
            {
                shm->data->levels[i].temp += change[0];
            }
            else
            {
                shm->data->levels[i].temp += random_change; 
            }
        }
    }
}

void rate_of_rise(shm_t *shm)
{
    int change = 1;
    int choose = rand() % LEVELS;
    
    for(int i = 0; i < LEVELS; i++)
    {
        if (choose == i)
        {
            shm->data->levels[i].temp += change; 
        }
    }
}

void fixed_tempture(shm_t *shm)
{
    int large_temp = 100;
    int choose = rand() % LEVELS;
    
    for(int i = 0; i < LEVELS; i++)
    {
        if (choose == i)
        {
            shm->data->levels[i].temp = large_temp; 
        }
    }
}

void tempture_update(shm_t *shm)
{
    printf("temp start\n");
    tempture_start(shm);
    
    while (finish == false)
    {
        if (rate_of_rise_fire)
        {
            rate_of_rise(shm);
            usleep(10);
        }
        else if (fixed_tempture_fire)
        {
            fixed_tempture(shm);
            usleep(10);
        }
        else
        {
            tempture_controller(shm);
            usleep(10);
        }
    }
}

void check_fire_start(void)
{
    char input;
    while(finish == false)
    {
        input = getchar();

        switch (input)
        {
            case 'r':
                    rate_of_rise_fire = true;
                    printf("rateofrise\n");
                    break;
            case 'f':
                    fixed_tempture_fire = true;
                    printf("fixedtempture\n");
                    break;
            case 'e':
                    end = true;
                    printf("end\n");
                    break;
            case 'd':
                    end = true;
                    finish = true;
                    printf("You have finished the program\n");
                    break;
        }
    }
}

// Main function
int main(void)
{
    int error;

    // Get Shared Memory
    if (get_shared_memory(&shm, "PARKING") != true)
    {
        printf("Error creating shared memory");
    }

    //Get Licence plates
    htab_create();
    get_plates(&htab, "plates.txt");

    // Start mutex's
    if (pthread_mutex_init(&car_park_lock, NULL) != 0)
    {
        printf("Error creating mutex for car park");
        return 0;
    }
    
    // Start threads
    // Enterence thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The enterances for the cars
        error = pthread_create(&enter_thread[i], NULL, (void*)enterance_operation, &shm.data->entrances[i]);
        if (error != 0)
        {
            printf("Error creating thread: %d\n", i);
        }

        // The boomgates for each enterance
        error = pthread_create(&boom_gates_enter[i], NULL, (void*)boomgate_good, &shm.data->entrances[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: %d\n", i);
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
            printf("Error creating boomgate: %d\n", i);
        }
    }
    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The enterances for the cars
        error = pthread_create(&exit_thread[i], NULL, (void*)lpr_exit, &shm.data->exits[i]);
        if (error != 0)
        {
            printf("Error creating thread: %d\n", i);
        }

        error = pthread_create(&boom_gates_exit[i], NULL, (void*)boomgate_good, &shm.data->exits[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: %d\n", i);
        }
    }
    // Temp
    error = pthread_create(&temp_thread, NULL, (void*)tempture_update, &shm);
    if (error != 0)
    {
        printf("Error creating thread: temp_thread\n");
    }
    // User check
    error = pthread_create(&user_thread, NULL, (void*)check_fire_start, &shm);
    if (error != 0)
    {
        printf("Error creating thread: user_thread");
    }

    printf("Threads are go\n");   


    while(finish != true)
    {
        usleep(1);
    }

    // End threads
    // Enterence thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The enterances for the cars
        pthread_join(enter_thread[i], NULL);
        // The boomgates for each enterance
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
        // The enterances for the cars
        pthread_join(exit_thread[i], NULL);
        pthread_join(boom_gates_exit[i], NULL);
    }
    // Temp
    pthread_join(temp_thread, NULL);
    // User check
    pthread_join(user_thread, NULL);

    // Print revenue
    //printf("The total revenue is: %f", total_money());

    // Clean up everything
    pthread_mutex_destroy(&car_park_lock);
    free(car_list);

    htab_destroy(&htab);
    destroy_shared_object(&shm);
    printf("\n\nIt worked, life is good\n");
    return 0;
}

// :)