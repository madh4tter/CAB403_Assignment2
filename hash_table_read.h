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
struct item {
    char plate[6];
    item_t *next;
};

typedef struct htab htab_t;
struct htab
{
    item_t **buckets;
    size_t size;
};

bool htab_init(htab_t *h, size_t n)
{
    h->size = n;
    h->buckets = malloc(n * sizeof(item_t*));
    if (h->buckets == NULL) {
        return false;
    }
    return true;   
}

size_t djb_hash(char s[6])
{
    size_t hash = 5381;
    for (size_t i = 0; i < 6; i++)
    {
        hash = ((hash << 5) + hash) + s[i];
    }
    return hash;
}

size_t htab_index(htab_t *h, char *key)
{    
    return djb_hash(key) % h->size;
}

bool htab_add(htab_t *h, char *key)
{
    item_t *newhead = (item_t *)malloc(sizeof(item_t));
    if (newhead == NULL){
        return false;
    }

    for (int i = 0; i < 6; i++) {
        newhead->plate[i] = key[i];
    }
    newhead->next = h->buckets[htab_index(h, key)];
    h->buckets[htab_index(h, key)] = newhead;
    return true;
}

bool htab_destroy(htab_t *h)
{
    for (size_t i = 0; i < h->size; i++){
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
    return true;
}

void get_plates(htab_t *h, const char *input){
    FILE* text  = fopen(input, "r");
    char line[11];
    while (NULL != fgets(line, sizeof(line), text))
    {
        htab_add(h, line);
    }
}
    
bool search_bucket(item_t *item, char *input)
{
    item_t *holder = item;
    for (; holder->plate != NULL; holder = holder->next)
    {
        if(strcmp(holder->plate, input) == 0)
        {
            return true;
        }
    }
    return false;
}

bool search_plate(htab_t *h, char *input){
    item_t *head = h->buckets[(djb_hash((char*)input) % h->size)];
    if (head == NULL)
    {
        return false;
    } 
    else
    {
        return search_bucket(head, input);
    }
}

