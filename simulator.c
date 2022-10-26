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

node_t *inside_list;
pthread_mutex_t inlist_lock;

bool ROR_fire = false;
bool FT_fire = false;

bool running = true;
pthread_mutex_t run_lock;

char user_input = 'r';
//pthread_mutex_t input_lock;

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
        printf("Memory allocation failure");
        fflush(stdout);
        return NULL;
    }

    new->car = car;
    new->next = head;

    return new;
}

node_t *node_pop(node_t *head)
{
    /* Remove and return the last car of a linked list */
    node_t *ret = (node_t *)malloc(sizeof(node_t));
    node_t *temp_head = head;

    if (head == NULL){
        /* return NULL if list is empty */
        return NULL;
    } else if(head->next->car == NULL) {
        /* case where only one car in queue */
        ret->car = head->car;
        ret->next = NULL;
        return ret;
    } else {
        /* 2 or more cars */
        for(; temp_head != NULL; temp_head = temp_head->next){
            /* Find second last node */
            if(temp_head->next->next == NULL){
                /* Remove reference to end node */
                ret->car = temp_head->next->car;
                temp_head->next = NULL;
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
        printf("Memory allocation failure");
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
    pthread_mutex_init(&run_lock, NULL);
    
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
    pthread_cond_signal(&LPR->cond);

}



/***************************** FIRE ALARM TEMPS *****************************************/

void temp_start(shm_t *shm)
{
    int start_temp = 25;
    int base_temp = 10;

    for (int i = 0; i < LEVELS; i++)
    {
        shm->data->levels[i].temp = (rand() % start_temp) + base_temp;
    }
}

void temp_control(shm_t *shm)
{
    int change[3] = {-1, 0, 1};

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

void fixed_temp(shm_t *shm)
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

void temp_update(shm_t *shm)
{
    temp_start(shm);
    if(shm->data->levels[0].temp == 0) {
        printf("Temp start did not work");
        fflush(stdout);
        return;
    }
    
    while(running == true)
    {

        if (ROR_fire)
        {
            rate_of_rise(shm);
            usleep(10);
        }
        else if (FT_fire)
        {
            fixed_temp(shm);
            usleep(10);
        }
        else
        {
            temp_control(shm);
            usleep(10);
        }
    }
}

void check_user_input(void)
{
    /* Gain mutex lock of running variable */
    pthread_mutex_lock(&run_lock);
    while(running == true)
    {
        /* Release lock of running bool variable */
        pthread_mutex_unlock(&run_lock);
        user_input = getchar();

        switch (user_input) {
            case 'r':
                ROR_fire = true;
                printf("rateofrise sim\n");
                break;
            case 'f':
                FT_fire = true;
                printf("fixedtempture sim\n");
                break;
            case 'd':
                pthread_mutex_lock(&run_lock);
                running = false;
                pthread_mutex_lock(&run_lock);
                printf("You have finished the program\n");
                break;
            default:
                break;               
        }
        /* Gain mutex lock of running variable */
        pthread_mutex_lock(&run_lock);
    }
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
    }
}

node_t *read_file(char *file, node_t *head) {
    FILE* text = fopen(file, "r");
    if(text == NULL){
        printf("Could not open plates file\n");
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
   return ret_val;
}

car_t *comp_times(node_t *head, uint64_t elap_time){
    for (; head != NULL; head = head->next)
    {
        if (head->car->exit_time >= elap_time)
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
    
    /* Gain mutex lock of running variable */
    pthread_mutex_lock(&run_lock);
    while(running == true){
        /* Release lock of running bool variable */
        pthread_mutex_unlock(&run_lock);

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

        /* Gain mutex lock of running variable */
        pthread_mutex_lock(&run_lock);
    }

    return ptr;
}

void *thf_creator(void *ptr){
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
   
    /* Gain mutex lock of running variable */
    pthread_mutex_lock(&run_lock);

    int count_a = 0;

    while(running == true && count_a < 4){
        /* Release lock of running bool variable */
        pthread_mutex_unlock(&run_lock);

        /* Wait between 1-100ms before generating another car */
        /* supposed to protect this with a mutex? seems to work fine without */
        //int wait = (rand() % 99) + 1;
        //usleep(wait * 1000);
        sleep(2);

        /* create car */
        car_t *new_car = malloc(sizeof(car_t));

        /* Initialise other values of the car */
        new_car->exit_time = 0;
        new_car->lvl = 0;

        /* assign random number plate */
        count = 0;
        do {
            exists = false;
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

            printf("Car made: %s\n", new_car->plate);fflush(stdout);

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
        int entranceID = rand() % (ENTRANCES);

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

        /* Gain mutex lock of running variable */
        pthread_mutex_lock(&run_lock);

        count_a++;
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

    while(1){
        /* Lock mutex of entrance queue list */
        pthread_mutex_lock(&eq_lock);

        /* Check if queue is empty. If so, wait for a car to be added */
        while(qhead->queue->car == NULL){
            pthread_cond_wait(&eq_cond, &eq_lock);
        }

        /* Remove car from end of queue */
        popped_node = node_pop(qhead->queue);

        /* Popped node holds the popped car and head of the queue with car removed */
        popped_car = popped_node->car;
        qhead->queue = popped_node->next;

        /* Unlock mutex */
        pthread_mutex_unlock(&eq_lock);
        
        /* Wait 2ms */
        usleep(2000);

        /* Trigger LPR */
        trig_LPR(&entrance->LPR, popped_car);
        printf("Car in LPR: %s\n", popped_car->plate);fflush(stdout);
        
        /* Watch Info Screen for assigned level */
        pthread_mutex_lock(&entrance->screen.lock);
        pthread_cond_wait(&entrance->screen.cond, &entrance->screen.lock);
        char_lvl = entrance->screen.display;
        pthread_mutex_unlock(&entrance->screen.lock);
        assigned_lvl = (int)char_lvl - 48;
        printf("Screen said: %c\n", char_lvl);fflush(stdout);
        
        if(!(assigned_lvl >= 0 && assigned_lvl <= (0 + ENTRANCES-1))){
            /* Remove car from simulation */
            destroy_car(popped_car);
        } else {
            popped_car->lvl = assigned_lvl;
            level = &shm_ptr->data->levels[assigned_lvl];
        
            printf("Opening boom gate\n");fflush(stdout);
            /* Raise boom gate */
            while( sim_gates(&entrance->gate) != 'O'){}
            printf("Boom gate open\n");fflush(stdout);
        
            /* Assign leaving time to car */
            pthread_cond_wait(&runtime.cond, &runtime.lock);
            popped_car->exit_time = runtime.elapsed + (rand() % 9900) + 100;
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

            trig_LPR(&level->LPR, '\0');            
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

    /* Gain mutex lock of running variable */
    pthread_mutex_lock(&run_lock);
    while(running == true){
        /* Release lock of running bool variable */
        pthread_mutex_unlock(&run_lock);    

        /* Acquire the time currently */
        pthread_mutex_lock(&runtime.lock);
        curr_time = runtime.elapsed;
        pthread_mutex_unlock(&runtime.lock);

        /* Find if any car needs to leave */
        pthread_mutex_lock(&inlist_lock);
        leave_car = comp_times(inside_list, curr_time);

        if(leave_car != NULL){
            /* Remove car from inside-carpark list */
            inside_list = snode_delete(inside_list, leave_car)->next;
            pthread_mutex_unlock(&inlist_lock);

            /* Set off LPR of leaving car */
            lvlID = leave_car->lvl - '0';
            level = &shm_ptr->data->levels[lvlID];
            trig_LPR(&level->LPR, leave_car);

            /* Add to a random exit queue */
            /* Randomly choose an exit queue to add to */
            int exitID = rand() % (EXITS);

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
        /* Gain mutex lock of running variable */
        pthread_mutex_lock(&run_lock);
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

    /* Gain mutex lock of running variable */
    pthread_mutex_lock(&run_lock);
    while(running == true){
        /* Release lock of running bool variable */
        pthread_mutex_unlock(&run_lock); 
    
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
        /* Gain mutex lock of running variable */
        pthread_mutex_lock(&run_lock);
    }
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

    init_shmvals(shm_ptr);


    /* Create threads for simulator-based functions */
    pthread_t time_th;
    pthread_t creator_th;
    // pthread_t inside_th;

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
    
    /* Create threads for simulator-based functions */
    pthread_create(&time_th, NULL, thf_time, NULL);
    pthread_create(&creator_th, NULL, thf_creator, NULL);
    // pthread_create(&inside_th, NULL, thf_inside, NULL);

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
    
    // // /* Gain mutex lock of running variable */
    // // pthread_mutex_lock(&run_lock);
    // // while(running == true){
    // //     /* Release lock of running bool variable */
    // //     pthread_mutex_unlock(&run_lock); 
    // //     check_user_input();
    // //     /* Gain mutex lock of running variable */
    // //     pthread_mutex_lock(&run_lock);
    // // }




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
    // pthread_join(inside_th, NULL);




    return EXIT_SUCCESS;
    
}


