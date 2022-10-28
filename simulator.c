/* Include shared memory struct definitions */
#include "PARKING.h"
#include "simulator.h"
#include "manager.h"
#include "shm_methods.h"
#include "firealarm.h"
#include "display_shm.h"

#define ACC_CAR_AMT 100
#define PLATE_LENGTH 6


mstimer_t runtime;

qnode_t *eqlist_head;
pthread_mutex_t eq_lock;
pthread_cond_t eq_cond; 

qnode_t *exqlist_head;
pthread_mutex_t exq_lock;
pthread_cond_t exq_cond;

pthread_mutex_t rand_lock;

node_t *inside_list;
pthread_mutex_t inlist_lock;

shm_t shm;

/* Create linked list for cars existing in simulation */
node_t *sim_cars_head = NULL;
pthread_mutex_t sim_cars_lock;

/*************** LINKED LIST METHODS ********************************************/
typedef struct node node_t;

struct node {
    car_t *car;
    node_t *next;
};

node_t *node_push(node_t *head, car_t *car){
    /* create new node to add to list */
    node_t *new = (node_t *)malloc(sizeof(node_t));
    if (new == NULL)
    {
        printf("Memory allocation failed\n");
        fflush(stdout);
        return NULL;
    }
    new->car = car;
    if(head == NULL){
        new->next = NULL;
    } else {
        new->next = head;
    }

    return new;
}

node_t *node_pop(node_t *head)
{
    /* Remove and return the last car of a linked list */
    node_t *ret = (node_t *)malloc(sizeof(node_t));
    node_t *temp_head = head;
    ret->next = head;

    if (head == NULL){
        /* return NULL if list is empty */
        return NULL;
    } else if(head->next->car == NULL) {
        /* case where only one car in queue */
        ret->car = head->car;
        ret->next->car = NULL;
        return ret;
    } else {
        /* 2 or more cars */
        for(; temp_head != NULL; temp_head = temp_head->next){
            /* Find second last node */
            if(temp_head->next->next->car == NULL){
                /* Remove reference to end node */
                ret->car = temp_head->next->car;
                temp_head->next->car = NULL;
                ret->next = head;

                /* return popped node */
                return ret;
            }
        }
    }

    return NULL;
}

node_t *node_find_LP(node_t *head, char *plate){
    for (; head != NULL; head = head->next)
    {
        if (strcmp(plate, head->car->plate) == 0)
        {
            return head;
        }
    }
    return NULL;
}

node_t *snode_delete(node_t *head, car_t *car)
{
    node_t *previous = NULL;
    node_t *current = head;
    while (current != NULL)
    {
        if (strcmp(car->plate, current->car->plate) == 0)
        {
            node_t *newhead = head;
            if (previous == NULL) {// first item in list
                newhead = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return newhead;
        }
        previous = current;
        current = current->next;
    }
    return head;
}


typedef struct queue_node qnode_t;

struct queue_node {
    node_t *queue;
    qnode_t *qnext;
    uint8_t ID;
};

qnode_t *qnode_push(qnode_t *head, node_t *queue){
    /* create new node to add to list */
    qnode_t *new = (qnode_t *)malloc(sizeof(qnode_t));
    if (new == NULL)
    {
        printf("Memory allocation failed\n");
        fflush(stdout);
        return NULL;
    }

    new->queue = queue;
    new->qnext = head;

    return new;
}



/*************************** SHARED MEMORY METHODS *********************************/

void init_conds(shm_t* shm){
    pthread_condattr_t cta;
    pthread_condattr_init(&cta);
    pthread_condattr_setpshared(&cta, PTHREAD_PROCESS_SHARED);
    for(int i =0; i<ENTRANCES; i++){
        pthread_cond_init(&(shm->data->entrances[i].LPR.cond), &cta);
        pthread_cond_init(&(shm->data->entrances[i].gate.cond), &cta);
        pthread_cond_init(&(shm->data->entrances[i].screen.cond), &cta);
    }

    for(int i =0; i<EXITS; i++){
        pthread_cond_init(&(shm->data->exits[i].LPR.cond), &cta);
        pthread_cond_init(&(shm->data->exits[i].gate.cond), &cta);
    }    

    for(int i =0; i<LEVELS; i++){
        pthread_cond_init(&(shm->data->levels[i].LPR.cond), &cta);
    }   

    pthread_cond_init(&runtime.cond, NULL);
    pthread_cond_init(&eq_cond, NULL);

    
}

void init_mutexes(shm_t* shm){
    pthread_mutexattr_t mta;
    pthread_mutexattr_init(&mta);
    pthread_mutexattr_setpshared(&mta, PTHREAD_PROCESS_SHARED);

    for(int i =0; i<ENTRANCES; i++){
        pthread_mutex_init(&(shm->data->entrances[i].LPR.lock), &mta);
        pthread_mutex_init(&(shm->data->entrances[i].gate.lock), &mta);
        pthread_mutex_init(&(shm->data->entrances[i].screen.lock), &mta);
    }

    for(int i =0; i<EXITS; i++){
        pthread_mutex_init(&(shm->data->exits[i].LPR.lock), &mta);
        pthread_mutex_init(&(shm->data->exits[i].gate.lock), &mta);
    }    

    for(int i =0; i<LEVELS; i++){
        pthread_mutex_init(&(shm->data->levels[i].LPR.lock), &mta);
    }   

    pthread_mutex_init(&runtime.lock, NULL);
    pthread_mutex_init(&eq_lock, NULL);
    pthread_mutex_init(&sim_cars_lock, NULL);
    pthread_mutex_init(&inlist_lock, NULL);
    pthread_mutex_init(&rand_lock, NULL);


    
}

bool create_shared_object( shm_t* shared_mem, const char* share_name ) {
    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(share_name);

    // Assign share name to shm->name.
    shared_mem->name = share_name;

    // Create the shared memory object, allowing read-write access, and saving the
    // resulting file descriptor in shm->fd. If creation failed, ensure 
    // that shm->data is NULL and return false.
    shared_mem->fd = shm_open(share_name, O_RDWR | O_CREAT, 0666);

    if(shared_mem->fd == -1){
        shared_mem->data = NULL;
        return false;
    }
    

    // Set the capacity of the shared memory object via ftruncate. If the 
    // operation fails, ensure that shm->data is NULL and return false. 
    if(ftruncate(shared_mem->fd, sizeof(PARKING_t)) == -1){
        shared_mem->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    shared_mem->data = mmap(NULL, sizeof(PARKING_t), PROT_READ | PROT_WRITE, MAP_SHARED, 
                    shared_mem->fd, 0);
    if(shared_mem->data == (void *)-1){
        return false; 
    }

    // Initialise mutexes
    init_mutexes(shared_mem);

    // Initialise conditional variables
    init_conds(shared_mem);

    // If we reach this point we should return true.
    return true;
}

void trig_LPR(LPR_t *LPR, car_t *car){
    /* Lock mutex */
    pthread_mutex_lock(&LPR->lock);

    /* Store plate in LPR */
    for(int i=0; i<PLATE_LENGTH; i++){
        LPR->plate[i] = car->plate[i];
    }

    /* Unlock mutex */
    pthread_mutex_unlock(&LPR->lock);

    /* Signal conditional variable */
    pthread_cond_broadcast(&LPR->cond);

}



/***************************** FIRE ALARM TEMPS *****************************************/

void temp_start()
{
    shm_t *shm_ptr = &shm;

    int start_temp = 25;
    int base_temp = 10;

    pthread_mutex_lock(&rand_lock);
    for (int i = 0; i < LEVELS; i++)
    {
        shm_ptr->data->levels[i].temp = (rand() % start_temp) + base_temp;
    }
    pthread_mutex_unlock(&rand_lock);
}

void *thf_temp(void *ptr)
{
    shm_t *shm_ptr = &shm;

    temp_start();
    int change[3] = {-1, 0, 1};
    int random_index;
    int wait;

    while(1){
        pthread_mutex_lock(&runtime.lock);
        random_index = runtime.elapsed % 4;
        wait = (runtime.elapsed % 4) + 1;
        pthread_mutex_unlock(&runtime.lock);
        usleep(wait *10000);
        //usleep(100000);

        int random_change = change[random_index];
        for (int i = 0; i < LEVELS; i++)
        {
            if(shm_ptr->data->levels[i].temp < 11)
            {
                shm_ptr->data->levels[i].temp += change[2];
            }
            else if(shm_ptr->data->levels[i].temp > 39)
            {
                shm_ptr->data->levels[i].temp += change[0];
            }
            else
            {
                shm_ptr->data->levels[i].temp += random_change; 
            }
        }
        printf("%d\n", shm_ptr->data->levels[0].temp);
        fflush(stdout);
    }
    return ptr;

}

void rate_of_rise()
{
    shm_t *shm_ptr = &shm;

    int change = 1;
    pthread_mutex_lock(&rand_lock);
    int choose = rand() % LEVELS;
    pthread_mutex_unlock(&rand_lock);
    
    for(int i = 0; i < LEVELS; i++)
    {
        if (choose == i)
        {
            shm_ptr->data->levels[i].temp += change; 
        }
    }
}

void fixed_temp()
{
    shm_t *shm_ptr = &shm;

    int large_temp = 100;
    pthread_mutex_lock(&rand_lock);
    int choose = rand() % LEVELS;
    pthread_mutex_unlock(&rand_lock);
    
    for(int i = 0; i < LEVELS; i++)
    {
        if (choose == i)
        {
            shm_ptr->data->levels[i].temp = large_temp; 
        }
    }
}


char check_user_input(pthread_t temp_th)
{
    char user_input = getchar();
    char ret;
    char alarm_check = '0';
    shm_t *shm_ptr = &shm;

    switch(user_input){
        case 'r':
            printf("Running Rate of Rise Simulation\n"); 
            fflush(stdout);
            pthread_cancel(temp_th);            
            rate_of_rise(shm_ptr);
            while(alarm_check != '1'){
                for(int i = 0; i < LEVELS; i++){
                    if(shm_ptr->data->levels[i].alarm == '1'){
                        alarm_check = '1';
                    }
                }
            }

            ret = 'a';
            break;
        case 'f':
            printf("Running Fixed Temperature Simulation\n"); 
            fflush(stdout);
            pthread_cancel(temp_th);
            fixed_temp(shm_ptr);
            printf("Fixed temp reached: %d\n", shm_ptr->data->levels[0].temp);
            fflush(stdout);
            while(alarm_check != '1'){
                for(int i = 0; i < LEVELS; i++){
                    if(shm_ptr->data->levels[i].alarm == '1'){
                        alarm_check = '1';
                    }
                }
            }
            printf("Alarm sounding!");
            fflush(stdout);            
            ret = 'a';

            break;
        case 'e':
            ret = 'e';
            printf("Simulator ending\n");
            fflush(stdout);
            break;
        default:
            printf("Default Alarm Sounding\n");
            fflush(stdout);
            fixed_temp(shm_ptr);
            ret = 'a';
            break;
    }
    return ret;     
}


/********************** MISC FUNCTIONS ********************************************/
void init_shmvals(shm_t *shared_mem){
        /* Initialise status of hardware no other threads awake so no need to lock*/
    for(int i=0; i<ENTRANCES; i++){
        shared_mem->data->entrances[i].gate.status = 'C';
        shared_mem->data->entrances[i].LPR.plate[0] = '\0';
        shared_mem->data->entrances[i].screen.display = '\0';
    }

    for(int i=0; i<EXITS; i++){
        shared_mem->data->exits[i].gate.status = 'C';
        shared_mem->data->exits[i].LPR.plate[0] = '\0';
    }

    for(int i=0; i<LEVELS; i++){
        shared_mem->data->levels[i].LPR.plate[0] = '\0';
        shared_mem->data->levels[i].alarm = '0';
    }
}

node_t *read_file(char *file, node_t *head) {
    FILE* text = fopen(file, "r");
    if(text == NULL){
        printf("Failed to open plates file\n");
        fflush(stdout);
    }

    int i= 0;
    char str[PLATE_LENGTH+1];
    while(fgets(str, PLATE_LENGTH+1, text)) {
        if(!(i%2)){
            car_t *newcar = (car_t *)malloc(sizeof(car_t));
            newcar->plate = strdup(str);
            head = node_push(head, newcar);
            memset(str, 0, 7);
        }

        i++;
    }

    fclose(text);
    return head;
}

void destroy_car(car_t *car){
    pthread_mutex_lock(&sim_cars_lock);
    sim_cars_head = snode_delete(sim_cars_head, car);
    pthread_mutex_unlock(&sim_cars_lock);
}

char sim_gates(gate_t *gate){
   pthread_mutex_lock(&gate->lock); 
   while(gate->status == 'O' || gate->status == 'C'){
      pthread_cond_wait(&gate->cond, &gate->lock);
   }
   if(gate->status == 'L'){
      usleep(10000);
      gate->status = 'C';
   } else {
      usleep(10000);
      gate->status = 'O';
   }
   char ret_val = gate->status;
   pthread_mutex_unlock(&gate->lock);
   pthread_cond_broadcast(&gate->cond);
   return ret_val;
}

car_t *comp_times(node_t *head, uint64_t elap_time){
    for (; head != NULL; head = head->next)
    {
        if (head->car->exit_time <= elap_time)
        {
            return head->car;
        }
    }
    return NULL;
}
/*************************** THREAD FUNCTIONS ******************************/

/* Thread function to keep track of time in ms*/
void *thf_time(void *ptr){
    struct timespec start, end;
    /* determine start time of thread */
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while(1)
    {
        /* sleep for one millisecond */
        usleep(1000);

        /* Read time on arbitrary clock */
        clock_gettime(CLOCK_MONOTONIC, &end);

        /* Acquire mutex */
        pthread_mutex_lock(&runtime.lock);

        /* Compare time to start time of thread */
        runtime.elapsed = (end.tv_sec - start.tv_sec) * 1000 
        + (end.tv_nsec - start.tv_nsec) / 1000000;

        /* Signal that change has occured and unlock the mutex */
        pthread_mutex_unlock(&runtime.lock);
        pthread_cond_signal(&runtime.cond);
    }

    return ptr;
}

void *thf_creator(void *ptr){
    sleep(1);
    /* Create linked list for accepted car number plates */
    node_t *acc_cars_head = NULL;
    acc_cars_head = read_file(PLATES_FILE, acc_cars_head);

    /* Initialise local loop variables */
    bool exists;
    int car_loc;
    int count;
    qnode_t *eq_temp;
    node_t *head_temp;
    char str[6];

    while(1){
        /* Wait between 1-100ms before generating another car */
        pthread_mutex_lock(&rand_lock);
        int wait = (rand() % 99) + 1;
        usleep(wait * 1000);
        pthread_mutex_unlock(&rand_lock);
        //sleep(1);

        /* create car */
        car_t *new_car = malloc(sizeof(car_t));

        /* Initialise other values of the car */
        new_car->exit_time = 0;
        new_car->lvl = 0;

        /* assign random number plate */
        count = 0;
        do {
            exists = false;
            pthread_mutex_lock(&rand_lock);
            if(rand()%2){
                /* select from approved list */
                car_loc = rand() % ACC_CAR_AMT;
                head_temp = acc_cars_head;
                for (; head_temp != NULL; head_temp = head_temp->next){
                    if (count >= car_loc){
                        new_car->plate = head_temp->car->plate;
                        break;
                    } else {
                        count++;
                    }
                }
            } else {
                /* Generate random */
                for (int i=0; i<3;i++){
                str[i] = '0' + rand() % 9;
                str[i+3] = 'A' + rand() % 25;
                new_car->plate = strdup(str);
                }
            }
            pthread_mutex_unlock(&rand_lock);
            

            /* Check every value in existing cars list to see if it matches. 
            If it matches, regenerate an LP*/
            pthread_mutex_lock(&sim_cars_lock);
            head_temp = sim_cars_head;
            pthread_mutex_unlock(&sim_cars_lock); 
            for (; head_temp != NULL; head_temp = head_temp->next){
                if(!strcmp(head_temp->car->plate, new_car->plate)){
                    exists = true;
                }
            }

        } while (exists == true);

        /* Add onto list of existing cars*/
        pthread_mutex_lock(&sim_cars_lock);
        sim_cars_head = node_push(sim_cars_head, new_car);
        pthread_mutex_unlock(&sim_cars_lock);

        /* Randomly choose an entrance queue to add to */
        pthread_mutex_lock(&rand_lock);
        int entranceID = rand() % ENTRANCES;
        pthread_mutex_unlock(&rand_lock);

        /* Lock mutex of global simulator-shared variable */
        pthread_mutex_lock(&eq_lock);

        /* Find car queue in list of entrances*/
        eq_temp = eqlist_head;
        for(; eq_temp != NULL; eq_temp = eq_temp->qnext){
            if(eq_temp->ID == entranceID){
                break;
            }
        }

        /* Push new car onto start of queue */
        eq_temp->queue = node_push(eq_temp->queue, new_car);

        /* Unlock mutex and signal that a new car has arrived */
        pthread_mutex_unlock(&eq_lock);
        pthread_cond_signal(&eq_cond);
    }
    return ptr;
}

void *thf_entr(void *data){
    /* Correction of variable type */
    int entranceID = *((char *)data);

    /* Get shared memory data */
    shm_t *shm_ptr = &shm;

    /* Lock mutex of entrance queue list */
    pthread_mutex_lock(&eq_lock);

    /* Identify head of queue relevant to this thread */
    qnode_t *qhead = eqlist_head;
    for (; qhead != NULL; qhead = qhead->qnext){
        if(qhead->ID == entranceID){
            break;
        }
    }

    /* Acquire the entrance simulated hardware */ 
    entrance_t *entrance = &shm_ptr->data->entrances[entranceID];
    level_t *level;

    /* Unlock the mutex */
    pthread_mutex_unlock(&eq_lock);

    /* Initialise loop local variables */
    node_t *popped_node;
    car_t *popped_car;
    char char_lvl;
    int assigned_lvl;
    int leave_time;

    while(1){
        /* Lock mutex of entrance queue list */
        pthread_mutex_lock(&eq_lock);
        /* Check if queue is empty. If so, wait for a car to be added */
        while(qhead->queue->car == NULL){
            pthread_cond_wait(&eq_cond, &eq_lock);
        }

        popped_node = NULL;
        while(popped_node == NULL){
            popped_node = node_pop(qhead->queue);
        }
        
        /* Popped node holds the popped car and head of the queue with car removed */
        popped_car = popped_node->car;
        qhead->queue = popped_node->next;

        /* Unlock mutex */
        pthread_mutex_unlock(&eq_lock);
        
        /* Wait 2ms */
        usleep(2000);

        /* Trigger LPR */
        trig_LPR(&entrance->LPR, popped_car);

        /* Watch Info Screen for assigned level */
        pthread_mutex_lock(&entrance->screen.lock);
        pthread_cond_wait(&entrance->screen.cond, &entrance->screen.lock);
        char_lvl = entrance->screen.display;
        pthread_mutex_unlock(&entrance->screen.lock);
        assigned_lvl = (int)char_lvl - 48;
        
        if(!(assigned_lvl >= 0 && assigned_lvl <= (ENTRANCES-1))){
            /* Remove car from simulation */
            destroy_car(popped_car);
        } else {
            popped_car->lvl = assigned_lvl;
            level = &shm_ptr->data->levels[assigned_lvl];
        
            /* Raise boom gate */
            while( sim_gates(&entrance->gate) != 'O'){}
        
            /* Assign leaving time to car */
            pthread_mutex_lock(&rand_lock);
            leave_time = (rand() % 9900) + 100;
            pthread_mutex_unlock(&rand_lock);

            pthread_mutex_lock(&runtime.lock);
            popped_car->exit_time = runtime.elapsed + leave_time;
            pthread_mutex_unlock(&runtime.lock);
        
            /* Wait for car to drive to spot */
            usleep(10000);

            /* Signal Level LPR */
            trig_LPR(&level->LPR, popped_car);

            /* Add car to inside-car-park list */
            pthread_mutex_lock(&inlist_lock);
            inside_list = node_push(inside_list, popped_car);
            pthread_mutex_unlock(&inlist_lock);

            /* Close boom gate */
            while( sim_gates(&entrance->gate) != 'C');
        }
        /* Repeat with next cars in queue */
    }
    return NULL;
}

void *thf_inside(void *ptr){
    shm_t *shm_ptr = &shm;

    uint64_t curr_time;
    car_t *leave_car;
    level_t *level;
    int lvlID;
    qnode_t *exq_temp;

    while(1){
        /* Acquire the time currently */
        pthread_mutex_lock(&runtime.lock);
        curr_time = runtime.elapsed;
        pthread_mutex_unlock(&runtime.lock);

        /* Find if any car needs to leave */
        pthread_mutex_lock(&inlist_lock);
        leave_car = comp_times(inside_list, curr_time);

        if(leave_car != NULL){
            /* Remove car from inside-carpark list */
            inside_list = snode_delete(inside_list, leave_car);
            pthread_mutex_unlock(&inlist_lock);

            /* Set off LPR of leaving car */
            lvlID = leave_car->lvl;
            level = &shm_ptr->data->levels[lvlID];
            trig_LPR(&level->LPR, leave_car);

            /* Add to a random exit queue */
            /* Randomly choose an exit queue to add to */
            pthread_mutex_lock(&rand_lock);
            int exitID = rand() % (EXITS);
            pthread_mutex_unlock(&rand_lock);

            /* Lock mutex of global simulator-shared variable */
            pthread_mutex_lock(&exq_lock);

            /* Find car queue in list of exits*/
            exq_temp = exqlist_head;
            for(; exq_temp != NULL; exq_temp = exq_temp->qnext){
                if(exq_temp->ID == exitID){
                    break;
                }
            }

            /* Push new car onto start of queue */
            exq_temp->queue = node_push(exq_temp->queue, leave_car);

            /* Unlock mutex and signal that a new car has arrived */
            pthread_mutex_unlock(&exq_lock);
            pthread_cond_signal(&exq_cond);

        } else {
            pthread_mutex_unlock(&inlist_lock);
        }     
        /* Wait one ms before checking again */  
        usleep(1000);
    }

    return ptr;
}

void *thf_exit(void *data){
    /* Correction of variable type */
    int exitID = *((char *)data);

    shm_t *shm_ptr = &shm;

    /* Lock mutex of exit queue list */
    pthread_mutex_lock(&exq_lock);

    /* Identify head of queue relevant to this thread */
    qnode_t *qhead = exqlist_head;
    for (; qhead != NULL; qhead = qhead->qnext){
        if(qhead->ID == exitID){
            break;
        }
    }

    /* Acquire the entrance simulated hardware */ 
    exit_t *exit = &shm_ptr->data->exits[exitID];

    /* Unlock the mutex */
    pthread_mutex_unlock(&exq_lock);

    /* Initialise loop local variables */
    node_t *popped_node;
    car_t *popped_car;

    while(1){
        /* Lock mutex of entrance queue list */
        pthread_mutex_lock(&exq_lock);

        /* Check if queue is empty. If so, wait for a car to be added */
        while(qhead->queue->car == NULL){
            pthread_cond_wait(&exq_cond, &exq_lock);
        }

        /* Remove car from end of queue */
        popped_node = node_pop(qhead->queue);
        

        /* Popped node holds the popped car and head of the queue with car removed */
        popped_car = popped_node->car;
        qhead->queue = popped_node->next;

        /* Unlock mutex */
        pthread_mutex_unlock(&exq_lock);
        
        /* Wait 2ms */
        usleep(2000);

        /* Trigger LPR */
        trig_LPR(&exit->LPR, popped_car);
    
        /* Raise boom gate */
        while( sim_gates(&exit->gate) != 'O'){}

        /* Remove car from simulation */
        destroy_car(popped_car);

        /* Close boom gate */
        while( sim_gates(&exit->gate) != 'C');

        /* Repeat with next cars in queue */
    }
        return NULL;
}

void *thf_entrAlarm(void *data){
    /* Correction of variable type */
    int entranceID = *((char *)data);
    shm_t *shm_ptr = &shm;
    entrance_t *entrance = &shm_ptr->data->entrances[entranceID];

    

    pthread_mutex_lock(&entrance->gate.lock);
    pthread_cond_wait(&entrance->gate.cond, &entrance->gate.lock);
    usleep(10*1000);
    entrance->gate.status = 'O';
    pthread_mutex_unlock(&entrance->gate.lock);
    pthread_cond_broadcast(&entrance->gate.cond);

    return NULL;
}

void *thf_exitAlarm(void *data){
    /* Correction of variable type */
    int exitID = *((char *)data);
    shm_t *shm_ptr = &shm;
    exit_t *exit = &shm_ptr->data->exits[exitID];

    

    pthread_mutex_lock(&exit->gate.lock);
    pthread_cond_wait(&exit->gate.cond, &exit->gate.lock);
    usleep(10*1000);
    exit->gate.status = 'O';
    pthread_mutex_unlock(&exit->gate.lock);
    pthread_cond_broadcast(&exit->gate.cond);

    return NULL;
}

 
/************************** MAIN ***********************************************/
int main(void){
    /* Create shared object before any other process begins */
    
    if(create_shared_object(&shm, SHARE_NAME) == false){
        printf("Shared memory creation failed\n");
        return EXIT_FAILURE;
    }
    shm_t *shm_ptr = &shm;
 	printf("shm_t addr %p", &shm_ptr->data);

    init_shmvals(shm_ptr);
    /* Create threads for simulator-based functions */
    pthread_t time_th;
    pthread_t creator_th;
    pthread_t inside_th;
    pthread_t temp_th;

    pthread_t entr_threads[ENTRANCES];
    int th_entrID[ENTRANCES];

    pthread_t exit_threads[ENTRANCES];
    int th_exitID[ENTRANCES];
    
    /* Create linked list that holds linked lists for entrance queues */
    node_t *queue;
    for(int i=0; i<ENTRANCES; i++){
        queue = (node_t *)malloc(sizeof(node_t));
        eqlist_head = qnode_push(eqlist_head, queue);
        eqlist_head->ID = i;
    }

    /* Create linked list that holds linked lists for exit queues */
    for(int i=0; i<EXITS; i++){
        queue = (node_t *)malloc(sizeof(node_t));
        exqlist_head = qnode_push(exqlist_head, queue);
        exqlist_head->ID = i;
    }
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    /* Create threads for simulator-based functions */
    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&creator_th, NULL, thf_creator, NULL);
    pthread_create(&inside_th, NULL, thf_inside, NULL);
    pthread_create(&temp_th, NULL, thf_temp, NULL);

    for (int i = 0; i < ENTRANCES; i++)
    {
        th_entrID[i] = i;
        pthread_create(&entr_threads[i], NULL, thf_entr, (void *)&th_entrID[i]);
    }

    for (int i = 0; i < EXITS; i++)
    {
        th_exitID[i] = i;
        pthread_create(&exit_threads[i], NULL, thf_exit, (void *)&th_exitID[i]);
    }
    
    
    char local_input = check_user_input(temp_th);


    switch (local_input) {
        case 'a':
            pthread_cancel(time_th);
            pthread_cancel(creator_th);
            pthread_cancel(inside_th);
            pthread_cancel(temp_th);

            for (int i = 0; i < ENTRANCES; i++)
            {
                pthread_cancel(entr_threads[i]);
            }

            for (int i = 0; i < EXITS; i++)
            {
                pthread_cancel(exit_threads[i]);
            }
            for (int i = 0; i < ENTRANCES; i++)
            {
                th_entrID[i] = i;
                pthread_create(&entr_threads[i], NULL, thf_entrAlarm, (void *)&th_entrID[i]);
            }
            
            for (int i = 0; i < EXITS; i++)
            {
                th_exitID[i] = i;
                pthread_create(&exit_threads[i], NULL, thf_exitAlarm, (void *)&th_exitID[i]);
            }
            
            break;
        case 'e':
            pthread_cancel(time_th);
            pthread_cancel(creator_th);
            pthread_cancel(inside_th);
            pthread_cancel(temp_th);
            for (int i = 0; i < ENTRANCES; i++)
            {
                pthread_cancel(entr_threads[i]);
            }

            for (int i = 0; i < EXITS; i++)
            {
                pthread_cancel(exit_threads[i]);
            }

            // free(&shm);
            
            break;
        default:
            break;  
    }


    /* Join all threads back to main */
    for (int i = 0; i < ENTRANCES; i++)
    {
        th_entrID[i] = i;
        pthread_join(entr_threads[i], NULL);
    }

        for (int i = 0; i < EXITS; i++)
    {
        th_entrID[i] = i;
        pthread_join(exit_threads[i], NULL);
    }
    
    pthread_join(time_th, NULL);
    pthread_join(creator_th, NULL);
    pthread_join(inside_th, NULL);

    printf("All threads rejoined, simulator is done\nOver to you firealarm!\n");
    fflush(stdout);




    return EXIT_SUCCESS;
    
}


