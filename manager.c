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

#define NONE '\0' // No value input in LPR
#define SLEEPTIME 2
#define GATETIME 1
#define RATE 0.05
#define CAPACITY 1
#define SHARE_NAME "/xyzzy_123"
#define MONEY_FILE "MONEY_FILE.txt"
#define PLATES_FILE "plates.txt"

// Need to change some define names to match the same in the PARKING.h file

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
pthread_t enter_thread[ENTRANCES];
pthread_t exit_thread[EXITS];
pthread_t level_thread[LEVELS];
pthread_t boom_gates_enter[ENTRANCES];
pthread_t boom_gates_exit[EXITS];
pthread_t temp_thread;
pthread_t user_thread;

// Global Variables
shm_t shm;
double revenue = 0;
bool rate_of_rise_fire = false;
bool fixed_tempture_fire = false;
bool end = false;

// Mutex's needed for lpr_enterence funcation
pthread_mutex_t car_park_lock; // New Car Array Lock

// Create link list
node_t *car_list = NULL;
node_t *allowed_list = NULL;

// Get Shared Objects
bool shared_objects(shm_t* shm, const char* name)
{
    int shm_fd;

    shm_unlink(name);
    shm->name = name;

    shm->fd = shm_open(shm->name, (O_RDWR |O_CREAT), 0666);
    if (shm->fd < 0)
    {  
        return false;
    }

    if (ftruncate(shm->fd, (off_t)(sizeof(shm_t))) == -1)
    {
        return false;
    }

    shm->data = mmap(0, sizeof(shm_t), (PROT_WRITE | PROT_READ), MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED)
    {
        return false;
    }

    // If we reach this point we should return true.
    return true;
}

void destroy_shared_object(shm_t* shm ) 
{
   munmap(shm->data, sizeof(shm_t));
   shm_unlink(shm->name);
   shm->fd = -1;
}

// Enternce of Car Park //////////////////////////////////////////////////////////

// Allowed cars in linked list
void allowed_add(node_t *head, const char *plates_file)
{
    FILE* text = fopen(plates_file, "r");
    char line[10];
    while(fgets(line, sizeof(line), text))
    {
        vehicle_t* new_vehicle = malloc(sizeof(vehicle_t));
        new_vehicle->licence_plate = line;
        new_vehicle->arrival = NULL;
        new_vehicle->level = NULL;
        node_t *newhead = node_add(allowed_list, new_vehicle);
        allowed_list = newhead;
    }
}

// Count cars per level linked list
int *car_count_level(node_t *head)
{
    int counter[LEVELS];
    char holder;
    for (; head != NULL; head->next)
    {
        holder = head->vehicle->level;
        counter[holder - '0']++;
    }

    return counter;
}

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

// Read licence plates, chooses where a car goes and saves it in an array
void enterance_operation(entrance_t *ent)
{
    int car_count;
    int car_count_levels[LEVELS];
    char holder;
    char assign_level;
    node_t *find;

    pthread_mutex_lock(&ent->LPR.lock);

    // Forever loop
    while(1)
    {
        // Consently Check for licence plate
        while(ent->LPR.plate[0] == NONE)
        {
            pthread_cond_wait(ent->LPR.cond, ent->LPR.lock); // Wait until another thread
            // Check if there is an emergenccy
        }

        holder = ent->LPR.plate;
        if(node_find_lp(allowed_list, holder) != NULL) // If the vehcile is allowed in and if there is a car at the gate
        {
            pthread_mutex_lock(&car_park_lock);
            car_count = car_count_total(car_list); // funcation to get car_count

            if (car_count < (LEVELS * CAPACITY))
            {
                car_count_levels[LEVELS] = car_count_level(car_list);
                assign_level = level_assign(car_count_levels); // Send to level

                // Change screen to assigned level
                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = assign_level; // Level value;
                pthread_mutex_unlock(&ent->screen.lock);

                // Create car and assign variables
                vehicle_t* new_vehicle = malloc(sizeof(vehicle_t));
                new_vehicle->licence_plate = (char*)ent->LPR.plate;
                new_vehicle->level = (int)assign_level - '0';
                new_vehicle->arrival = clock() * 1000;

                pthread_mutex_lock(&ent->screen.lock);
                ent->screen.display = '0'; // Clears the screen
                pthread_mutex_unlock(&ent->screen.lock);

                // Add car to linked list
                node_t *newhead = node_add(car_list, new_vehicle);
                car_list = newhead;
                pthread_mutex_unlock(&car_park_lock);
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
        ent->LPR.plate[0] == NONE;
        pthread_cond_signal(ent->screen.cond);
    }

    pthread_mutex_unlock(ent->LPR.lock);
}



// Boomgates //////////////////////////////////////////////////////////////////////

// Enternace boom gate (best)
void boomgate_good(entrance_t *ent)
{

    // Start of closed
    // pthread_mutex_lock(&ent->gate->lock);
    // ent->gate->status = 'C';
    // pthread_mutex_unlock(&ent->gate->lock);

    while(1)
    {
        // Open
        if (ent->screen.display != NONE && ent->gate.status == 'C')
        {
            pthread_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'R';
            while (ent->gate.status == 'R')
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
                usleep(SLEEPTIME * 20);
            }
            usleep(20); // Gate Opening
            ent->gate.status = '0';

            usleep(SLEEPTIME * 1000);
            pthread_mutex_unlock(&ent->gate.lock);
        }

        if (ent->gate.status == 'O')
        {
            pthread_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'L';
            while (ent->gate.status == 'L')
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
            }
            pthread_mutex_unlock(&ent->gate.lock);
        }

        if (ent->gate.status == 'L')
        {
            pthreaed_mutex_lock(&ent->gate.lock);
            ent->gate.status = 'C';
            while(ent->gate.status == 'C')
            {
                pthread_cond_wait(&ent->gate.cond, &ent->gate.lock);
            }
            pthread_mutex_unlock(&ent->gate.lock);
        }
    }
}

// Enternace boom gate
void boomgate_other(entrance_t *ent)
{
    pthread_mutex_lock(&ent->gate.lock);
    ent->gate.status = 'C';

    // While there is no alarm going off in the building
    while(1)
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
    char level;
    node_t* find;

    while (1)
    {
        pthread_mutex_lock(&lvl->level->LPR.lock);
        if (lvl->level->LPR.plate != NONE)
        {
           find = node_find_lp(car_list, lvl->level->LPR.plate);
           if (find != NULL)
           {
                find->vehicle->level = lvl->floor;
           }
        }
        pthread_mutex_unlock(&lvl->level->LPR.lock);
    }
}



// Exit of Car Park /////////////////////////////////////////////////////////////////////

void lpr_exit(exit_t *ext)
{
    pthread_mutex_lock(ext->LPR.lock);
    while(1)
    {
        while (ext->LPR.plate == NONE)
        {
            pthread_cond_wait(ext->LPR.cond, ext->LPR.lock); // Wait until another thread
        }

        if (ext->LPR.plate != NONE)
        {
            pthread_mutex_lock(&car_park_lock);

            // Find car that is leaving
            node_t *find = node_find_lp(car_list, ext->LPR.plate);

            // Charge that person and save the amount
            money(find->vehicle);

            // Remove from list
            car_list = node_delete(car_list, ext->LPR.plate);
            pthread_mutex_unlock(&car_park_lock);
        }

    }

    pthread_mutex_unlock(&ext->LPR.lock);
}


// Fire Alarm ///////////////////////////////////////////////////////////////////////////
void tempture_controller(shm_t *shm)
{
    int start_temp = 25;
    int base_temp = 10;
    int change[3] = {-1, 0, 1};

    // Start random
    srand(time(NULL));

    // At the start
    if(shm->data.levels[0].temp == 0)
    {
        int temp = (rand() % start_temp) + base_temp;
    }
    // While running
    else
    {
        int random_index = rand() % len(change);
        int random_change = change[random_index];
        for (int i = 0; i < LEVELS; i++)
        {
            if(shm->data.levels[i].temp > 10)
            {
                shm->data.levels[i].temp += change[2];
            }
            if(shm->data.levels[i].temp < 40)
            {
                shm->data.levels[i].temp += change[0];
            }
            else
            {
                shm->data.levels[i].temp += random_change; 
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
        if (choose = i)
        {
            shm->data.levels[i].temp += change; 
        }
    }
}

void fixed_tempture(shm_t *shm)
{
    int large_temp;
    int choose = rand() % LEVELS;
    
    for(int i = 0; i < LEVELS; i++)
    {
        if (choose = i)
        {
            shm->data.levels[i].temp = 60; 
        }
    }
}

void tempture_update(shm_t *shm)
{
    while (1)
    {
        if (fixed_tempture_fire)
        {
            rate_of_rise(shm);
            sleep(1);
        }
        if (rate_of_rise_fire)
        {
            fixed_tempture(shm);
            sleep(1);
        }
        else
        {
            tempture_controller(shm);
            sleep(1);
        }
    }
    
}

void check_fire_start(shm_t *shm)
{
    char input;
    while(1)
    {
        input = getchar();
        if(input == "r")
        {
            rate_of_rise_fire = true;
        }
        if(input == "f")
        {
            fixed_tempture_fire = true;
        }
        if(input = "q")
        {
            end = true;
        }
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

void total_money()
{
    FILE *fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(MONEY_FILE, "r");
    if (fp == NULL)
    {
        while(read = getline(&line, &len, fp) != 1)
        {
            char money_string[len];
            double money_double;
            char *start;
            strncpy(money_string, line + 8, len);
            money_double = strtod(money_string, &start);
            revenue += money_double;
        }
    }
}


// Main function
int main(void)
{
    int error; // Variable to determine if there is an error
    bool check;

    // Get Shared Memory
    check = shared_objects(&shm, SHARE_NAME);

    // Pass through allowed licence plates
    allowed_add(allowed_list, PLATES_FILE);

    // Start mutex's
    if (pthread_mutex_init(&car_park_lock, NULL) != 0)
    {
        printf("Error");
        return 0;
    }

    // Start threads
    // Enterence thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The enterances for the cars
        error = pthread_create(&enter_thread[i], NULL, (void*)enterance_operation, &shm.data.entrances[i]);
        if (error != 0)
        {
            printf("Error creating thread: {0}\n", i);
        }

        // The boomgates for each enterance
        error = pthread_create(&boom_gates_enter[i], NULL, (void*)boomgate_good, &shm.data.entrances[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: {0}\n", i);
        }
    // Level Threads
    for (int i = 0; i < LEVELS; i++)
    {
        error = pthread_create(&level_thread[i], NULL, (void*)level_lpr, &shm.data.levels[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: {0}\n", i);
        }
    }
    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The enterances for the cars
        error = pthread_create(&exit_thread[i], NULL, (void*)lpr_exit, &shm.data.exits[i]);
        if (error != 0)
        {
            printf("Error creating thread: {0}\n", i);
        }

        error = pthread_create(&boom_gates_exit[i], NULL, (void*)boomgate_good, &shm.data.exits[i]);
        if (error != 0)
        {
            printf("Error creating boomgate: {0}\n", i);
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
        printf("Error creating thread: user_thread", i);
    }   


    while(end == false)
    {
        usleep(100);
    }


    // End threads
    // Enterence thread
    for (int i = 0; i < ENTRANCES; i++)
    {
        // The enterances for the cars
        error = pthread_join(&enter_thread[i], NULL);
        if (error != 0)
        {
            printf("Error ending thread: {0}\n", i);
        }

        // The boomgates for each enterance
        error = pthread_join(&boom_gates_enter[i], NULL);
        if (error != 0)
        {
            printf("Error ending boomgate: {0}\n", i);
        }
    }
    // Level Threads
    for (int i = 0; i < LEVELS; i++)
    {
        error = pthread_join(&level_thread[i], NULL);
        if (error != 0)
        {
            printf("Error ending boomgate: {0}\n", i);
        }
    }
    // Exit threads
    for (int i = 0; i < EXITS; i++)
    {
        // The enterances for the cars
        error = pthread_join(&exit_thread[i], NULL);
        if (error != 0)
        {
            printf("Error ending thread: {0}\n", i);
        }

        error = pthread_join(&boom_gates_exit[i], NULL);
        if (error != 0)
        {
            printf("Error ending boomgate: {0}\n", i);
        }
    }
    // Temp
    error = pthread_join(&temp_thread, NULL);
    if (error != 0)
    {
        printf("Error ending thread: temp_thread\n");
    }
    // User check
    error = pthread_join(&user_thread, NULL);
    if (error != 0)
    {
        printf("Error ending thread: user_thread", i);
    }   

    // Print revenue
    printf("The total revenue is: {0}", revenue);

    // Clean up everything
    destroy_shared_object(&shm);
    pthread_mutex_destory(car_park_lock);
    return 0;
}

// :)