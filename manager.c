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

#define ENTRY_GATES 1 //5
#define EXIT_GATES 1 //5
#define LEVELS 5
#define CAPACITY 20

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


// Vehicle
typedef struct vehicle
{
    char* licence_plate; // What is the licence plate of the vehicle
    int level; // Where level is the vehicle on
    bool left; // Is the vehicle still in the building
    time_t arrival; // When did the vehicle arrive (For billing)
}  vehicle_t;

// Visual array of where the vehicle can come and go from
// vehicle_t* car_park[LEVELS][CAPACITY];


// Creating Hashtable
// Definine 6 char as the item to store in the hash table (6 chars in licence plate)
typedef struct item item_t;
struct item
{
    char value[6];
    item_t *next;
};

// Hash table mapping
typedef struct htab htab_t;
struct htab
{
    item_t **buckets;
    size_t size;
};

// Initalise the hash table function
bool htab_init(htab_t *h, size_t n)
{
    h->size = n;
    h->buckets = calloc(n, sizeof(item_t*));
    return h->buckets != NULL;
}

// Bernstein hash funcation
size_t djb_hash(char *s)
{
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0')
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Calculate the offset for the bucket for key in hash table.
size_t htab_index(htab_t *h, char *value)
{
    char temp[6];

    for (int i = 0; i < 6; i++)
    {
        temp[i] = value[i];
    }

    return djb_hash(temp) % h -> size;
}

// Hash table add
bool htab_add(htab_t *h, char licence_plate)
{
    item_t *new_item = malloc(sizeof(item_t));
    if (new_item == NULL)
    {
        return false;
    }

    for (int i = 0; i < 6; i++)
    {
        new_item->value[i] = licence_plate[i]
    }

    new_item->next = NULL;

    size_t index = htab_index(h, licence_plate);
    h->buckets[index] = new_item;
}

// Hash table delete

// Has table desotry