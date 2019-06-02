#define CONFIG_SERVER_ADDR "127.0.0.1"
#define CONFIG_SERVER_PORT 9740
#define MEMORY_SERVER_SIZE 10
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
#include <string.h>
#include <time.h>
#include "MemoryManager.h"

shmemory_info info;
int is_global_logger, server_sockfd = -1, total_loggers;

int port = 9735;
int shmkey = 1238;
int semkey = 1234;

char *buffer;
char *checkpoint;
pthread_t idLocalLogger, idGlobalLogger;

typedef struct serv {
	char ip[15];
	int port;
	int start_position;
	int end_position;
	int is_connected;
	int sockfd;
}Logger;


Logger *loggers;

void getLoggersConfig(){
	int sockfd;
	struct sockaddr_in address;	

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(CONFIG_SERVER_ADDR);
	address.sin_port = htons(CONFIG_SERVER_PORT);
	int len = sizeof(address);

	int result = connect(sockfd, (struct sockaddr *)&address, len);
	if(result == -1) {
		perror("oops: ");
		exit(1);
	}
	
	int config_str_size;	
	int flag, i, j, k, l;
	char aux[6];

	// reads number of loggers
	recv(sockfd, &total_loggers, sizeof(int), MSG_WAITALL);	
	total_loggers = ntohl(total_loggers);

	// reads string config size
	recv(sockfd, &config_str_size, sizeof(int), MSG_WAITALL);
	config_str_size = ntohl(config_str_size);

	// reads loggers configuration
	char *loggers_config = malloc(config_str_size * sizeof(char));
	recv(sockfd, loggers_config, config_str_size * sizeof(char), MSG_WAITALL);

	printf("%s \n", loggers_config);
	
	// create a vector of server structs
	loggers = (Logger*)malloc(total_loggers * sizeof(Logger));

	i = 0;
	flag = 0;
	j = 0;

	while(i < config_str_size && j < total_loggers) {
		k = 0;
		l = 0;
		// getting ip address
		while(loggers_config[i] != ':') {
		    loggers[j].ip[k] = loggers_config[i];
		    k++; i++;
		}
		loggers[j].ip[k] = '\0';
		i++;

		// getting port number
		while(loggers_config[i] != ';'){
		    aux[l] = loggers_config[i];
		    l++; i++;
		}
		loggers[j].port = atoi(aux);
		// IMPORTANT: the logger port is ALWAYS = service_port + 1
		loggers[j].port++;
		
		loggers[j].is_connected = 0;
		loggers[j].start_position = j * MEMORY_SERVER_SIZE;
		loggers[j].end_position = loggers[j].start_position + MEMORY_SERVER_SIZE - 1;
		
		j++; i++;
	}

	printf("Loggers Configuration: \n");
	for(i=0; i < total_loggers; i++)
		printf("%s:%d : [%d: %d] \n", loggers[i].ip, loggers[i].port, loggers[i].start_position, loggers[i].end_position);

	free(loggers_config);
}



// thread that monitors console 
// when a char is typed, closes server
void* monitorThread(void* args) {
	char end;
	scanf("%c", &end);

	free_shmemory(info.shmid, info.shared_memory);	
	free_semaphore(info.semid, info.total_blocks);

	if(server_sockfd != -1) {
		close(server_sockfd);
		free(buffer);
	}
	for(int i = 0; i < total_loggers; i++) {
		close(loggers[i].sockfd);
	}
	pthread_cancel(idLocalLogger);
	pthread_cancel(idGlobalLogger);

	

	//free(checkpoint);
	//free(buffer);

	exit(0);
	
	return NULL;
}

void saveCheckPoint(char *global_mem) {

	int size, i, count = 0;
	FILE* log;

	time_t timer;
	char buffer_time[26];
	struct tm* tm_info;

	time(&timer);
	tm_info = localtime(&timer);

	strftime(buffer_time, 26, "%Y-%m-%d %H:%M:%S", tm_info);

	strcat(buffer_time, ".txt");        //concatenates file name with ".txt"

	log = fopen(buffer_time, "w");      //open file to write and creates a new one

	size = total_loggers * MEMORY_SERVER_SIZE;

	for(i = 0; i < size; i++)
		fprintf(log, "%d: %c\n", i, global_mem[i]);     //write to file

	fclose(log);                      			//close file
}

// LOCAL LOGGER
void memoryInitializer(int shmkey, int semkey){
	
	// SHARED MEMORY
	info = initialize_shmemory(MEMORY_SERVER_SIZE, BLOCK_SIZE, shmkey, semkey, 0);

	if(info.result == -1){
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

void* globalLoggerWorker(void* args) {

	int result;

	printf("%s \n", "Global Logger: will connect on config service");
	getLoggersConfig();
	printf("%s \n", "Global Logger: start connect locals loggers");
	for(int i = 0; i < total_loggers; i++) {
		struct sockaddr_in address;

		loggers[i].sockfd = socket(AF_INET, SOCK_STREAM, 0);
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr(loggers[i].ip);
		address.sin_port = htons(loggers[i].port);
		int len = sizeof(address);
		printf("%s %s %d \n", "trying connect on ", loggers[i].ip, loggers[i].port);

		result = connect(loggers[i].sockfd, (struct sockaddr *)&address, len);
		if(result == -1) {
			perror("error on connect to server: ");
			exit(1);
		}
		
		loggers[i].is_connected = 1;
	}
	printf("%s \n", "Global Logger: end connect locals loggers");
	checkpoint = malloc(total_loggers * MEMORY_SERVER_SIZE * sizeof(char));
	int cont = 0;
	while(1) {

		printf("%s %d \n", "Global Logger: start require locals loggers ", cont);
		for(int i = 0; i < total_loggers; i++) {

			result = write(loggers[i].sockfd, &i, sizeof(int));
			if(result == -1) {
				perror("Global Logger 1: \n");
				break;
			}
			result = recv(loggers[i].sockfd, &checkpoint[i * MEMORY_SERVER_SIZE], MEMORY_SERVER_SIZE * sizeof(char), MSG_WAITALL);

			if(result == -1) {
				perror("Global Logger 2: \n");
				break;
			}
		}

		saveCheckPoint(checkpoint);
		cont++;
		printf("%s %d \n", "Global Logger: end require locals loggers ", cont);
		sleep(100);
	}
	return NULL;
}

void* localLoggerWorker(void* args) {

	memoryInitializer(shmkey, semkey);

	int server_len, client_len, result;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	// creating the server socket (domain: internet, type: stream, protocol: tcp)
	// the so returns the socket descriptor
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	server_len = sizeof(server_address);

	// atributes to socket internet address and port
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
	
	// socket is ready to receive clients requests
	// size of queue is 5
	listen(server_sockfd, 5);
	printf("%s \n","Local logger listen...");

	// ignore sigpipe signal
	// when a client ends a connection, the server does not break
	signal(SIGPIPE, SIG_IGN);

	client_len = sizeof(client_address);

	// accept a client connection
	int client_sockid = accept(server_sockfd, (struct sockaddr *)&client_address, (void*) &client_len);
	
	printf("%s \n", "Local Logger: Global Logger Connected");
	
	buffer = malloc(MEMORY_SERVER_SIZE * sizeof(int));
	int cont = 0;
	while(1) {
		int command;

		// local logger waits until global logger requires the memory state
		result = recv(client_sockid, &command, sizeof(int), MSG_WAITALL);

		printf("%s %d \n", "Local Logger: Global Logger require checkpoint ", command);

		if(result == -1) {
			perror("Local Logger 1: \n");
			break;
		}
		printf("%d \n", command);
		
		// ** BEGIN CRITICAL AREA **
		result = read_shmemory(info, 0, MEMORY_SERVER_SIZE, BLOCK_SIZE, buffer);
		// ** END CRITICAL AREA **

		if(!result) {
			perror("Local Logger 2: \n");
			break;
		}
		result = write(client_sockid, buffer, MEMORY_SERVER_SIZE * sizeof(char));
		printf("%s \n", "Local Logger: checkpoint sended");

		if(result == -1) {
			perror("Local Logger 3: \n");
			break;
		}
		cont++;	
	}
	free(buffer);
	close(client_sockid);
	
	return NULL;
}


int main(int argc, const char *argv[]){

	if(argc < 2) {
		printf("The parameter is_global_logger is required! \n");
		exit(1);
	}

	is_global_logger = atoi(argv[1]);

	if (argc >= 5) {
		port = atoi(argv[2]);
		shmkey = atoi(argv[3]);
		semkey = atoi(argv[4]);
	}
	pthread_t idMonitor;

	if(pthread_create(&idMonitor, NULL, monitorThread, NULL) != 0) {
		// a error ocurred while creating the monitors thread
		exit(EXIT_FAILURE);
	}

	// creates the local logger thread
	
	if(pthread_create(&idLocalLogger, NULL, localLoggerWorker, NULL) != 0) {
		// a error ocurred while creating the local logger thread
		exit(EXIT_FAILURE);
	}


	if(is_global_logger == 1){
		// creates the global logger thread

		if(pthread_create(&idGlobalLogger, NULL, globalLoggerWorker, NULL) != 0) {
			// a error ocurred while creating the global logger thread
			exit(EXIT_FAILURE);
		}

	}


	sleep(20);
	pthread_join(idLocalLogger, NULL);
	pthread_join(idGlobalLogger, NULL);
}




