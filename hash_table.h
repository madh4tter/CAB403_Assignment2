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

typedef struct item item_t;
struct item
{
    char value[6]; // Level (value)
    item_t *next;
};

void item_print(item_t *i)
{
    printf("key=%s value=%d", i->value);
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
size_t djb_hash(char s[6])
{
    size_t hash = 5381;
    int c;
    for (int i = 0; i < 6; i++)
    {
        hash = ((hash << 5) + hash) + s[i];
    }
    return hash;
}

// Calculate the offset for the bucket for key in hash table (Where the number plate starts)
size_t htab_index(htab_t *h, char *value)
{
    char temp[6];
    strcpy(temp, value);
    return djb_hash(temp) % h -> size;
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
    h->buckets[htab_index(h, licence_plate)] = new_item;

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

bool check_bucket(item_t *head, char *input)
{
    for (; head != NULL; head = head->next)
    {
        if(strcmp(input, head->value))
        {
            return true;
        }
    }

    return false;
}

bool plate_check(htab_t *h, char *input)
{
    item_t *head = h->buckets[(djb_hash((char*)input) % h->size)];
    if (head == NULL)
    {
        return false;
    }

    return true;
}

// DEBUGGING
void print_htab_bucket(item_t* bucket){
    for (int i = 0; i < 6; i++)
    {
        printf("%c", bucket->value[i]);
    }
    printf(" -> ");
    if (bucket->next != NULL)
    {
        print_htab_bucket(bucket->next);
    }
    
    
}

// DEBUGGING
void print_htab(htab_t* h) {
    for (int i = 0; i < h->size; i++) {
        printf("Bucket %d: ", i);
        if (h->buckets[i] == NULL)
        {
            printf(" EMPTY");
        } else {
            print_htab_bucket(h->buckets[i]);
        }
        printf("\n");
    }   
}
