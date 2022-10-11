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

#include <PARKING.h>
#include <simulator.c>
#include <hash_table_funcations.c>
#include <linked_list.c>

#define NONE '\0' // No value input in LPR
#define SLEEPTIME 2
#define GATETIME 1

/* Shared Memory
bool get_shared_memory(shared_mem_t* shm, const char* share_name)
{
    if (shm->data == NULL || shm->name == NULL) 
    {
        return;
    }

    shm->fd = shm_open(share_name, O_RDWR, 0);
    if (shm->fd == -1)
    {
        shm->data = NULL;
        return false;
    }

    shm->data = mmap(0,sizeof(shared_data_t), (PROT_WRITE | PROT_READ), MAP_SHARED, shm->fd,0);
    if (shm->data < 0)
    {
        return false;
    }

    return true;
}
*/

// Threads
pthread_t enter_thread[ENTRY_GATES];
pthread_t exit_thread[EXIT_GATES];
pthread_t level_thread[LEVELS];
pthread_t boom_gates_enter[ENTRY_GATES];
pthread_t boom_gates_exit[EXIT_GATES];

// Mutex's needed for lpr_enterence funcation
pthread_mutex_t car_park_lock; // New Car Array Lock

// Vehicle
typedef struct vehicle
{
    char* licence_plate; // What is the licence plate of the vehicle
    int level; // Where level is the vehicle on
    time_t arrival; // When did the vehicle arrive (For billing)
}  vehicle_t;

// Visual array of where the vehicle can come and go from
// vehicle_t* car_park_1[LEVELS][CAPACITY] = { NULL }; // Not anymore (ha)
// Create link list
node_t *car_list = NULL;



// Enternce of Car Park //////////////////////////////////////////////////////////

// Car count total
int car_count_total(node_t *head)
{
    int car_count_total = 0;
    int car_count_levels[LEVELS] = car_count_level(head);
    for (int i = 0; i < LEVELS; i++)
    {
        car_count_total = car_count_total + car_count_levels[i];
    }
    return car_count_total;
}

// Assign Level
char level_assign(int *car_count[LEVELS])
{
    int min = 0;
    for (int i = 0; i > LEVELS; i++)
    {
        if(car_count[i] < car_count[min])
        {
            min = i;
        }
    }

    min++; // As 0 represents level 1;
    char level_char = min + '0'; // Convert to char

    return level_char;
}

// Count cars per level linked list
int *car_count_level(node_t *head)
{
    int counter[LEVELS]
    char holder
    for (; head != NULL; head->next)
    {
        holder = head->vehicle->level;
        counter[holder - '0']++;
    }

    return counter;
}

// Read licence plates, chooses where a car goes and saves it in an array
void enterence_lpr(p_enterance_t *ent)
{
    int car_count;
    int car_count_levels[LEVELS];
    int park;
    char assign_level;

    pthread_mutex_lock(&ent->lpr->lock);

    // Forever loop
    while(1)
    {
        // Consently Check for licence plate
        while(ent->lpr->plate[0] == NONE)
        {
            pthread_cond_wait(ent->lpr->cond, ent->lpr->lock); // Wait until another thread
            // Check if there is an emergenccy
        }

        if(ent->lpr->plate[0] != NONE) // If the vehcile is allowed in and if there is a car at the gate
        {
            pthread_mutex_lock(&car_park_lock);
            car_count = car_count_total(car_list); // funcation to get car_count

            if (car_count < (LEVELS * CAPACITY))
            {
                car_count_levels[LEVELS] = car_count_level(car_park_1);
                assign_level = level_assign(car_count_levels); // Send to level

                // Change screen to assigned level
                pthread_mutex_lock(&ent->screen->lock);
                ent->screen->display = assign_level; // Level value;
                pthread_mutex_unlock(&ent->screen->lock);

                // Create car and assign variables
                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t));
                new_vehicle->licence_plate = (char*)ent->lpr->plate;
                new_vehicle->level = (int)assign_level - '0';
                new_vehicle->arrival = clock();

                pthread_mutex_lock(&ent->screen->lock);
                ent->screen->display = '0'; // Clears the screen
                pthread_mutex_unlock(&ent->screen->lock);

                // Add car to linked list
                node_t *newhead = node_add(car_list, new_vehicle);
                pthread_mutex_unlock(&car_park_lock);
            }
            else
            {
                // If the carpark if full
                pthread_mutex_unlock(&car_park_lock);
                pthread_mutex_lock(&ent->screen->lock);
                *ent->screen->display = 'F'; // Full message
                pthread_mutex_unlock(&ent->screen->lock);
            }
        }
        else // To catch any fall throughs
        {
            pthread_mutex_lock(&ent->screen->lock);
            *ent->screen->display = 'X';
            pthread_mutex_unlock(&ent->screen->lock);
        }
        // ent->lpr->plate[0] == NONE;
        // pthread_cond_signal(ent->screen->cond);
    }

    pthread_mutex_unlock(ent->lpr->lock);
}



// Boomgates //////////////////////////////////////////////////////////////////////

// Enternace boom gate (best)
void boomgate_good(p_enterance_t *ent)
{

    // Start of closed
    // pthread_mutex_lock(&ent->gate->lock);
    // ent->gate->status = 'C';
    // pthread_mutex_unlock(&ent->gate->lock);

    while(1)
    {
        // Open
        if (ent->screen->display != '0' && ent->gate->status = 'C')
        {
            pthread_mutex_lock(&ent->gate->lock);
            ent->gate->status = 'R';
            while (ent->gate->status == 'R')
            {
                pthread_cond_wait(&ent->gate->cond, &ent->gate->lock);
                usleep(SLEEPTIME * 20);
            }
            usleep(20); // Gate Opening
            ent->gate->status = '0';

            usleep(SLEEPTIME * 1000);
            pthread_mutex_unlock(&ent->gate->lock);
        }

        if (ent->gate->display == 'O')
        {
            pthread_mutex_lock(&ent->gate->lock);
            ent->gate->status = 'L';
            while (ent->gate->status == 'L')
            {
                pthread_cond_wait(&ent->gate->cond, &ent->gate->lock);
            }
            pthread_mutex_unlock(&ent->gate->lock);
        }

        if (ent->gate->display == 'L')
        {
            pthreaed_mutex_lock(&ent->gate->lock);
            ent->gate->status = 'C';
            while(ent->gate->status == 'C')
            {
                pthread_cond_wait(&ent->gate->cond, &ent->gate->lock);
            }
            pthread_mutex_unlock(&ent->gate->lock);
        }
    }
}

// Enternace boom gate
void boomgate_other(p_enterance_t *ent)
{
    pthread_mutex_lock(&ent->boom->lock);
    ent->gate->status = 'C';

    // While there is no alarm going off in the building
    while(1)
    {
        // While there is no car that has been given permission to enter the boomgate
        while(ent->screen->display == NONE && ent->gate->status = 'C') // Neeed to add if alarm
        {
            pthread_cond_wait(&ent->gate->cond, &ent->gate->lock);
        }
        
        // Car has been approved for entering
        // Boomgate rises
        ent->gate->status = 'R'; // U = Up
        pthread_cond_signal(&ent->gate->cond);

        while(ent->gate->status == 'R')
        {
            pthread_cond_wait(&ent->gate->cond, &ent->gate->lock);
        }

        usleep(SLEEPTIME * 1000);

        // Bommgate closes
        ent->gate->status = 'D'; // D = Down
        pthread_cond_broadcast(&ent->gate->cond);

        while(ent->gate->status == 'D')
        {
            pthread_cond_wait(&ent->boom->cond, &ent->gate->lock);
        }

        ent->gate->status = 'C'; // C = Closed
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
    char level;
    node_t find;

    while (1)
    {
        pthread_mutex_lock(&lvl->level->lpr->lock)
        if (lvl->lpr->plate != NONE)
        {
           find = node_find_lp(car_list, lvl->level->lpr->plate)
           if (find != NULL)
           {
                find->vehicle->level = level_tracker_t->floor;
           }
        }
        pthread_mutex_unlock(&lvl->level->lpr->lock)
    }
}



// Exit of Car Park /////////////////////////////////////////////////////////////////////

void lpr_exit(exit_t *ext)
{
    pthread_mutex_lock(ext->lpr->lock);
    while(1)
    {
        while (ext->lpr->plate == NONE)
        {
            pthread_cond_wait(ext->lpr->cond, ext->lpr->lock); // Wait until another thread
        }

        if (ext->lpr->plate != NONE)
        {
            pthread_mutex_lock(&car_park_lock);

            // Remove from list
            car_list = node_delete(car_list, ext->lpr->plate);

            // Bill that person and save the bill
        }

    }
}


// Fire Alarm ///////////////////////////////////////////////////////////////////////////


// Main function
int main()
{
    int error; // Variable to determine if there is an error
    htab_t hash_table;
    if(!htab_init(&hash_table, LEVELS*CAPACITY))
    {
        printf("Failed to create hash table");
    }

    // Create enterance objects


    // Start threads
    for (int i = 0; i < ENTRY_GATES; i++)
    {
        error = pthread_create(&enter_thread[i], NULL, enterance_operation, NULL);
        if (error != 0)
        {
            printf("Error creating thread: {0}\n", i);
        }
    }


    // Start mutex's
    if (pthread_mutex_init(&car_park_lock, NULL) != 0)
    {
        printf("Error");
        return 0;
    }
    {
        printf("Error");
        return 0;
    }

    // Clean up everything
    munmap((void*)shm->data, sizeof(shm->data));
    close(shm->fd);
    return 0;
}

// :)