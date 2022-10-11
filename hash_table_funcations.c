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

// Creating Hashtable
// Definine 6 char as the item to store in the hash table (6 chars in licence plate)
typedef struct item item_t;
struct item
{
    char key[1]; // Counter
    int value; // Level (value)
    item_t *next;
};

void item_print(item_t *i)
{
    printf("key=%s value=%d", i->key, i->car);
}

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

// Calculate the offset for the bucket for key in hash table (Where the number plate starts)
size_t htab_index(htab_t *h, vehicle_t *value)
{
    char temp[6];

    for (int i = 0; i < 6; i++)
    {
        temp[i] = value->licence_plate[i];
    }

    return djb_hash(temp) % h -> size;
}

// Find pointer to head of list for key in hash table.
item_t *htab_bucket(htab_t *h, char *value)
{
    return h->buckets[htab_index(h, value)];
}

item_t *htab_find(htab_t *h, char *value)
{
    for (item_t *i = htab_bucket(h, value); i != NULL; i = i->next)
    {
        if (strcmp(i->key, value) == 0)
        { 
            return i;
        }
    }
    return NULL;
}

// Hash table add
bool htab_add(htab_t *h, char *licence_plate)
{
    item_t *new_item = (item_t *)malloc(sizeof(item_t));
    if (new_item == NULL)
    {
        return false;
    }

    strncpy(new_item->value, licence_plate, 6);
    new_item->

    return true; // Check if the process worked
}

// Has table desotry
void htab_destory(htab_t *h)
{
    for (int i = 0; i < h->size; i++)
    {
        item_t* head = h->buckets[i];
        item_t* curr = head;
        item_t* next;
        while (curr != NULL)
        {
            next = curr->next;
            free(curr);
            curr = next;
        }
    }

    free(h->buckets);
    h->buckets = NULL;
    h->size = 0;
}