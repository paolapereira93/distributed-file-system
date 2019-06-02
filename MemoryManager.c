#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <string.h>

#include "semun.h"
#include "MemoryManager.h"

// returns result = -1 : error 
shmemory_info initialize_shmemory(int size_memory, int size_blocks, int shmkey, int semkey, int is_to_initialize) {
	
	shmemory_info info;
	info.result = 1;

	// calculating the number of blocks
	info.total_blocks = size_memory / size_blocks;

	// breaking the memory in blocks to control access
	// creating mutex to control shared memory access
	// one mutex for each block of memory
	info.semid = semget((key_t)semkey, info.total_blocks, 0666 | IPC_CREAT);

	if(info.semid < 0){
		perror("semaphore error: ");
		info.result = -1;
		return info;		
	}
	
	// initializing semaphores
	if(is_to_initialize) {
		if (set_semvalue(info.semid, info.total_blocks) == -1) {
			perror("semaphore error: ");
			info.result = -2;
			return info;
		}
	}

	// creates the shared memory
	// allocate space for data
	info.shmid = shmget((key_t)shmkey, size_memory, 0666 | IPC_CREAT);
	
	if(info.shmid == -1){
		perror("shmemory error: ");
		info.result = -3;
		return info;
	}
	
	// attach the shared memory to the process and gets the location of 'malloc'
	info.shared_memory = shmat(info.shmid, (void*)0, 0);	
	
	info.data = (char *)info.shared_memory;

	if (info.shared_memory == (void *)-1) {
		// error to attach
		info.result = -4; 
		return info;
	}	
	
	if(is_to_initialize) {
		// initializing all positions empty
		for (int i = 0; i < size_memory; i++)
		{
			info.data[i] = '-';
		}		
	}

	return info;
}

// this method writes the buffer on the memory 
// and controls critical area
int write_shmemory(shmemory_info info, int start_index, int end_index, int block_size, char *data) {
	
	// calculating requeired memory blocks
	array_int blocks = calculate_blocks(start_index, end_index, block_size);

	// ** START CRITICAL AREA **

	// locks all required blocks
	int result = semaphore_p(info.semid, blocks.values, blocks.size);
	
	printf("sempahore_p \n");
	sleep(10);

	if(!result) {
		return result;
	}
	memcpy(&info.data[start_index], data, end_index - start_index);

	sleep(5);

	printf("sempahore_v \n");

	// UNLOCK blocks already used
	result = semaphores_v(info.semid, blocks.values, blocks.size);
	
	// ** END CRITICAL AREA **

	if(!result) {
		return result;
	}

	printf("ends \n");
	return 1;
}

// this method reads the shared memory and copys on buffer pointer 
// and controls critical area
int read_shmemory(shmemory_info info, int start_index, int end_index, int block_size, char *data) {
	
	array_int blocks = calculate_blocks(start_index, end_index, block_size);

	// ** START CRITICAL AREA **

	// locks all required blocks
	int result = semaphore_p(info.semid, blocks.values, blocks.size);
	
	printf("sempahore_p for reading.. \n");
	sleep(5);

	if(!result) {
		return result;
	}
	memcpy(data, &info.data[start_index], end_index - start_index);

	sleep(5);

	printf("sempahore_v for reading.. \n");

	// UNLOCK blocks already used
	result = semaphores_v(info.semid, blocks.values, blocks.size);

	// ** END CRITICAL AREA **
	if(!result) {
		return result;
	}
	return 1;
}


/*

calculate witch blocks have to control access
to lock the respectives mutexes

*/
array_int calculate_blocks(int start_index, int end_index, int block_size) {

	array_int blocks_indexes;
	
	int start_block = start_index / block_size;
	printf("start block %d \n", start_block);

	int end_block = (end_index - 1) / block_size;
	printf("end block %d \n\n", end_block);
	
	blocks_indexes.size = end_block - start_block + 1;
	blocks_indexes.values = malloc(blocks_indexes.size * sizeof(int));


	for(int i = start_block; i <= end_block; i++) {
		blocks_indexes.values[i-start_block] = i;
	}

	return blocks_indexes;
}

/* 
set_semvalue inicializa os semaforos usando o comando SETVAL em uma chamada semctl.  
*/
static int set_semvalue(int semid, int total_blocks)
{
	union semun sem_union;
	sem_union.val = 1;
	for(int i = 0; i < total_blocks; i++) {
		if (semctl(semid, i, SETVAL, sem_union) == -1) 
			return -1;
		
	}
	return 1;
}

// LOCK CRITICAL AREA
// int sem_id: semaphore identifier (returned by semget)
// int *sem_nums: array of indexes (blocks that must be locked)
// int size_sem_nums: total of blocks that must be locked
static int semaphore_p(int sem_id, int *sem_nums, int size_sem_nums)
{
	struct sembuf sem_b[size_sem_nums];

	for(int i = 0; i < size_sem_nums; i++) {
		sem_b[i].sem_num = sem_nums[i];
		sem_b[i].sem_op = -1; /* P() */
		sem_b[i].sem_flg = SEM_UNDO;		
	}

	if (semop(sem_id, sem_b, size_sem_nums) == -1) {
		perror("semaphore_p failed\n");
		return(0);
	}
	return(1);
}

static int semaphore_v(int sem_id, int sem_num) {
	
	struct sembuf sem_b;
	
	sem_b.sem_num = sem_num;
	sem_b.sem_op = 1; /* V() */
	sem_b.sem_flg = SEM_UNDO;

	if (semop(sem_id, &sem_b, 1) == -1) {
		perror("semaphore_v failed\n");
		return(0);
	}
}


// UNLOCK CRITICAL AREA
// int sem_id: semaphore identifier (returned by semget)
// int *sem_nums: array of indexes (blocks that must be unlocked)
// int size_sem_nums: total of blocks that must be unlocked
static int semaphores_v(int sem_id, int *sem_nums, int size_sem_nums)
{
	struct sembuf sem_b[size_sem_nums];

	for(int i = 0; i < size_sem_nums; i++) {
		sem_b[i].sem_num = sem_nums[i];
		sem_b[i].sem_op = 1; /* V() */
		sem_b[i].sem_flg = SEM_UNDO;
	}
	if (semop(sem_id, sem_b, size_sem_nums) == -1) {
		perror("semaphore_v failed\n");
		return(0);
	}
	return(1);
}

int free_shmemory(int shmid, void *shared_memory){
	
	if (shmdt(shared_memory) == -1) {
		return -1;
	}

	if (shmctl(shmid, IPC_RMID, 0) == -1) {
		return -2;
	}
		
	return 1;
}

int free_semaphore(int semid, int total_blocks){
	
	union semun sem_union;
	if (semctl(semid, total_blocks, IPC_RMID, sem_union) == -1)
		return -1;
		
	return 1;
}
