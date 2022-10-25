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
#include "shared_prac.h"

// gcc manager.c -o go -Wall -Wextra -lrt -lpthread
// Need to change some define names to match the same in the PARKING.h file

#define NONE '\0' /* Null value in the LRP*/
#define RATE 0.05
#define MONEY_FILE "MONEY_FILE.txt"
#define PLATES_FILE "plates.txt"

// Threads
pthread_t enter_thread[ENTRANCES];
pthread_t exit_thread[EXITS];
pthread_t level_thread[LEVELS];
pthread_t boom_gates_enter[ENTRANCES];
pthread_t boom_gates_exit[EXITS];

// Global Variables
bool m_finish = false;
int m_counter = 0;

// Mutex's needed for lpr_enterence funcation
pthread_mutex_t car_park_lock; /* New Car Array Lock*/

// Create link list
node_t *car_list = NULL;

// Create Hash tabe
htab_t htab;

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

void entrance_lpr(entrance_t *ent)
{
    int car_count_levels[LEVELS];
    char holder[6];
    char assign_level;

    pthread_mutex_lock(&ent->LPR.lock);

    while(m_finish == false)
    {
        while(ent->LPR.plate[0] == NONE) /* Checks if there is a car at the LPR */
        {
            pthread_cond_wait(&ent->LPR.cond, &ent->LPR.lock); /* Wait until a car appears */
        }
        strcpy(holder, ent->LPR.plate);
        if(search_plate(&htab, holder) == 1) /* Checks if the car is allowed in */
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

                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t)); /* Create new levels */
                new_vehicle->licence_plate = (char*)ent->LPR.plate;
                new_vehicle->level = &assign_level;
                new_vehicle->arrival = clock() * 1000;

                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = '0'; /* Clear the screen */
                pthread_mutex_unlock(&ent->screen.lock);

                node_t *newhead = node_add(car_list, new_vehicle);
                car_list = newhead; /* Add car to linked list*/

            }
            else /* If the carpark is full */
            {
                pthread_mutex_unlock(&car_park_lock);
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = 'F'; /* Full message */
                pthread_mutex_unlock(&ent->screen.lock);
            }
        }
        else /* If the car is not allowed*/
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

void boomgate(entrance_t *ent) /* Boom gate funcation */
{
    while(m_finish == false)
    {
        if (ent->screen.display != NONE && ent->gate.status == 'C') /* Rising */
        {
            pthread_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'R';
            pthread_mutex_unlock(&ent->gate.lock);

            do 
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
            } while (ent->gate.status != 'O');
        }

        //usleep(2 * 1000);

        if (ent->gate.status == 'O') /* Lowering */
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

    while (m_finish == false)
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
    FILE* fp = fopen(MONEY_FILE, "a"); /* Open file */

    time_t exit_time = time(0) * 1000;
    double total_time = difftime(exit_time, car->arrival);
    double total_amount = total_time * RATE;

    fprintf(fp, "%s $%0.2f\n", car->licence_plate, total_amount); /* Write and close file*/
}

double total_money() /* Find the total amount of revenue */
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

void exit_lpr(exit_t *ext)
{
    pthread_mutex_lock(&ext->LPR.lock);
    while(m_finish == false)
    {
        while (ext->LPR.plate[0] == NONE)
        {
            pthread_cond_wait(&ext->LPR.cond, &ext->LPR.lock);
        }

        if (m_finish == false) 
        {
            pthread_mutex_lock(&car_park_lock);
            m_counter--;
            pthread_mutex_unlock(&car_park_lock);

            node_t *find = node_find_lp(car_list, ext->LPR.plate); /* Find car that is leaving */

            money(find->vehicle); /* Charge the amount and save it*/

            car_list = node_delete(car_list, ext->LPR.plate); /* Remove from linked list*/

            printf("The total revenue is: %f", total_money());
        }
    }

    pthread_mutex_unlock(&ext->LPR.lock);
}

// Main function
int main(void)
{
    shm_t shm;
    int error;

    // Get Shared Memory
    // if (get_shared_object(&shm, "PARKING") != true)
    // {
    //     printf("Error getting shared memory\n");
    // }

    create_shared_object(&shm, SHARE_NAME);

    //Get Licence plates
    htab_create();
    get_plates(&htab, "plates.txt");

    // // Start mutex's
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
        error = pthread_create(&enter_thread[i], NULL, (void*)entrance_lpr, &shm.data->entrances[i]);
        if (error != 0)
        {
            printf("Error creating thread: %d\n", i);
        }

        // The boomgates for each enterance
        error = pthread_create(&boom_gates_enter[i], NULL, (void*)boomgate, &shm.data->entrances[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: %d\n", i);
        }
    }
    printf("Done Enternace\n");
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
    printf("Done Level\n");
    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The enterances for the cars
        error = pthread_create(&exit_thread[i], NULL, (void*)exit_lpr, &shm.data->exits[i]);
        if (error != 0)
        {
            printf("Error creating thread: %d\n", i);
        }

        error = pthread_create(&boom_gates_exit[i], NULL, (void*)boomgate, &shm.data->exits[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: %d\n", i);
        }
    }
    printf("Done Exit\n");
    
    while(m_finish == false)
    {
        sleep(0.5);
        printf("WE DID IT");
        m_finish = true;
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

    // // Print revenue
    // //printf("The total revenue is: %f", total_money());

    // Clean up everything
    pthread_mutex_destroy(&car_park_lock);
    free(car_list);
    destroy_shared_object(&shm);
    htab_destroy(&htab);
    printf("\n\nIt worked, life is good\n");
    return 0;
}

// :)