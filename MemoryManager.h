typedef struct shmemory_info{
	int shmid;		// shared memory id
	int semid;		// semaphore interprocess id
	int result;		// returns -1 if an error ocourred
	char *data;		// pointer to data
	void *shared_memory;	// pointer to shared memory
	int total_blocks;	// number of blocks
}shmemory_info;

typedef struct array_int{
	int *values;
	int size;
}array_int;

// these parameters controls in how many parts the memory will be breaking
// shmid is the id of the shared memory where data is allocated
// size_memory : size of memory, in bytes
// size_blocks : size of each block, in bytes
shmemory_info initialize_shmemory(int size_memory, int size_blocks, int shmkey, int semkey, int is_to_initialize);

int free_shmemory(int shmid, void *shared_memory);

int free_semaphore(int semid, int total_blocks);

static int set_semvalue(int semid, int total_blocks);

static int semaphore_p(int sem_id, int *sem_nums, int size_sem_nums);

static int semaphores_v(int sem_id, int *sem_nums, int size_sem_nums);

static int semaphore_v(int sem_id, int sem_num);

array_int calculate_blocks(int start_index, int end_index, int block_size);

// this method writes the buffer on the memory 
// and controls critical area
int write_shmemory(shmemory_info info, int start_index, int end_index, int block_size, char *buffer);

// this method reads the shared memory and copys on buffer pointer 
// and controls critical area
int read_shmemory(shmemory_info info, int start_index, int end_index, int block_size, char *buffer);

