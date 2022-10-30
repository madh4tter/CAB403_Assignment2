/************** SHARED MEMORY METHODS **********************************************/  
bool get_shared_object( shm_t* shm, const char* share_name ) {
    // Get a file descriptor connected to shared memory object and save in 
    // shm->fd. If the operation fails, ensure that shm->data is 
    // NULL and return false.
    shm->fd = shm_open(share_name, O_RDWR, 0666);
    // Check if shm_open worked
    if(shm->fd == -1){
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    shm->data = mmap(NULL, sizeof(PARKING_t), PROT_READ | PROT_WRITE, MAP_SHARED, 
                    shm->fd, 0);
    if(shm->data == MAP_FAILED){
        return false; 
    }

    // Modify the remaining stub only if necessary.
    return true;
}

void destroy_shared_object( shm_t* shm ) {
    // Remove the shared memory object.
    munmap(shm, sizeof(shm_t));
    shm_unlink("PARKING");
    shm->fd = -1;
    shm->data = NULL;
}