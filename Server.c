#define MEMORY_SIZE 10
#define BLOCK_SIZE 2

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "MemoryManager.h"

shmemory_info info;
int server_sockfd;

struct client_thread_parms
{
	// socket identifier
	int client_sockid;
};

// thread that monitors console 
// when a char is typed, closes server
void* monitorThread(void* args) {
	char end;
	scanf("%c", &end);

	free_shmemory(info.shmid, info.shared_memory);	
	free_semaphore(info.semid, info.total_blocks);
	// close socket
	close(server_sockfd);

	exit(0);
	
	return NULL;
}


void* clientThread(void* args) {

	// convert parameter to correct type
	struct client_thread_parms* p = (struct client_thread_parms*) args;

	char command = 'b';
	int result;

	while(1){
		result = recv(p->client_sockid, &command, sizeof(char), MSG_WAITALL);
		if(result == -1) {
			perror("oops: ");
			break;
		}
		printf("command: %c \n", command);

		if(command == 'q'){
			break;
		}		
	
		int start_index;
		result = recv(p->client_sockid, &start_index, sizeof(int), MSG_WAITALL);

		if(result == -1) {
			perror("oops: ");
			break;
		}

		start_index = ntohl(start_index);
		printf("start_index: %d \n", start_index);
	
		int end_index;
		result = recv(p->client_sockid, &end_index, sizeof(int), MSG_WAITALL);

		if(result == -1) {
			perror("oops: ");
			break;
		}

		end_index = ntohl(end_index);
		printf("end_index: %d size %lu \n", end_index, sizeof(int));

		int data_size = end_index - start_index;
	
		char *data = malloc(data_size * sizeof(char));
		if(command == 'w') {
			printf("index %d \n", start_index);
		
			result = recv(p->client_sockid, data, data_size*sizeof(char), MSG_WAITALL);

			if(result == -1) {
				perror("oops: ");
				free(data);
				break;
			}

			printf("data to write: %s \n", data);
			

			// writes on shared memory
			// begin critical area
			write_shmemory(info, start_index, end_index, BLOCK_SIZE, data);
			// end critical area

		} else {
			// begin critical area
			read_shmemory(info, start_index, end_index, BLOCK_SIZE, data);
			// end critical area

			result = write(p->client_sockid, data, data_size * sizeof(char));

			if(result == -1) {
				perror("oops: ");
				free(data);
				break;
			}
		}
		
		free(data);
	}
	// close socket
	close(p->client_sockid);
	// free memory
	free(p);
	return NULL;
}

void memoryInitializer(int shmkey, int semkey){
	
	// SHARED MEMORY
	info = initialize_shmemory(MEMORY_SIZE, BLOCK_SIZE, shmkey, semkey, 1);

	if(info.result < 0){
		printf("shmid: %d \n", info.shmid);
		printf("semid: %d \n", info.semid);
		printf("error: %d \n", info.result);
		free_shmemory(info.shmid, info.shared_memory);	
		free_semaphore(info.semid, info.total_blocks);
		
		exit(EXIT_FAILURE);
	}

	printf("%s %d \n", "semaphore initialized, semid = ", info.semid);
	printf("%s %d \n", "memory initialized, shmid = ", info.shmid);
}

int main(int argc, const char *argv[]) {

	int port = 9734;
	int shmkey = 1238;
	int semkey = 1234; // for see value of semaphore: ipcs -s -i 1234

	if (argc >= 4) {
		port = atoi(argv[1]);
		shmkey = atoi(argv[2]);
		semkey = atoi(argv[3]);
	}

	memoryInitializer(shmkey, semkey);
	
	pthread_t idMonitor;

	if(pthread_create(&idMonitor, NULL, monitorThread, NULL) != 0) {
		// a error ocurred while creating the monitors thread
		exit(EXIT_FAILURE);
	}

	// now that memory is ready, the server can open the socket
	pthread_t idThread;
	int cont = 0;
	int server_len, client_len;
	struct client_thread_parms *client_params;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	// creating the server socket (domain: internet, type: stream, protocol: tcp)
	// the so returns the socket descriptor
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// TODO tem q ter hotn na familia??
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	server_len = sizeof(server_address);

	// atributes to socket internet address and port
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);

	// socket is ready to receive clients requests
	// size of queue is 30
	listen(server_sockfd, 30);
	printf("%s %d \n","server listen on port...", port);

	// ignore child exit details
	// when a client exit a connection, the server does not break
	signal(SIGCHLD, SIG_IGN);

	while(1) {

		client_len = sizeof(client_address);

		client_params = malloc(sizeof(struct client_thread_parms));
		// accept a client connection
		client_params->client_sockid = accept(server_sockfd,(struct sockaddr *)&client_address, (void*) &client_len);
		
		// creating a thread to attend the client
		if(pthread_create(&idThread, NULL, clientThread, client_params) != 0){
			// a error ocurred while creating the client thread
			// disconect client
			close(client_params->client_sockid);
			// free memory
			free(client_params);
			continue;
		}
		cont++;
		printf("Atendidos %d clientes \n", cont);
	}

}
