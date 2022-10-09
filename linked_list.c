#include <stddef.h> // for NULL
#include <stdio.h>  // for printf()
#include <stdlib.h> // for EXIT_SUCCESS
#include <string.h> // for strcmp()

#include <manager.c>

typedef struct node node_t
struct node
{
    vehicle_t *vehicle;
    node_t next*;
};

node_t *node_add(node_t *head, vehicle_t *car)
{
    node_t *new (node_t *)malloc(sizeof(node_t));
    if (new == NULL)
    {
        return NULL;
    }

    // Copy over infomation
    new->vehicle = car;
    new->next = head;
    return new;
}

node_t *node_find_level(node_t *head, char *level)
{
    for (; head != NULL; head = head->next)
    {
        if (strcmp(level, head->vehicle->level) == 0)
        {
            return head;
        }
    }

    return NULL
}

node_t *node_find_lp(node_t *head, char *lp)
{
    for (; head != NULL; head = head->next)
    {
        if (strcmp(level, head->vehicle->licence_plate) == 0)
        {
            return head;
        }
    }

    return NULL
}

node_t *delete(node_t *head, char *level)
{
    node_t *previous = NULL;
    node_t *current = head;
    while(current != NULL)
    {
        if (strcmp(level, current->person->level) == 0)
        {
            // node_t *newhead = head;
            if(previous == NULL) // first item in list
            {
                head = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            return(head);
        }

        previous = current;
        current = current->next;
    }
}