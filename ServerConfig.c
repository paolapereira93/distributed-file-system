#define MAX 1000
#define FILE_NAME "config.txt"

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

int server_sockfd;
int total_servers, size_str;
char servers[MAX];

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

	// close socket
	close(server_sockfd);

	exit(0);
	
	return NULL;
}

// reads servers config:
// number of servers
// ip:port of each one
void readServersConfig(){
	int i=0, cont = -1;
	char line[21];
	char aux[21];
	
	FILE* arq;

	arq = fopen(FILE_NAME, "r");

	while(fgets(line, 21, arq)){
	cont++;

	if(cont == 0){
	    for(i = 0; i < 21; i++){
		if(line[i]!='\n'){
		    aux[i]=line[i];
		}
	    }
	    total_servers = atoi(aux);
	}

	else if(cont==1)
	    strcpy(servers, line);

	else
	    strcat(servers, line);
	}

	size_str = strlen(servers);
	for(i = 0; i < size_str; i++) {
		if(servers[i] == '\n')
			servers[i] = ';';
	}
	servers[size_str] = '\0';

	printf("%s\n", servers);

	fclose(arq);

	size_str = htonl(size_str);
	total_servers = htonl(total_servers);
}

void* clientThread(void* args) {

	// convert parameter to correct type
	struct client_thread_parms* p = (struct client_thread_parms*) args;

	// send number of servers
	write(p->client_sockid, &total_servers, sizeof(int));
	
	// send the size of string config
	write(p->client_sockid, &size_str, sizeof(int));
	
	// send string config
	write(p->client_sockid, servers, size_str * sizeof(char));
	
	// close socket
	close(p->client_sockid);
	// free memory
	free(p);
	return NULL;
}

int main(){
	pthread_t idMonitor;

	if(pthread_create(&idMonitor, NULL, monitorThread, NULL) != 0) {
		// a error ocurred while creating the monitors thread
		exit(EXIT_FAILURE);
	}

	// reads config file and store into a string
	readServersConfig();
	
	pthread_t idThread;
	int cont = 0;
	int server_len, client_len;
	struct client_thread_parms *client_params;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	// creating the server socket (domain: internet, type: stream, protocol: tcp)
	// the so returns the socket descriptor
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(9740);
	server_len = sizeof(server_address);

	// atributes to socket internet address and port
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
	
	// socket is ready to receive clients requests
	// size of queue is 30
	listen(server_sockfd, 30);
	printf("%s \n","server listen...");

	// ignore sigpipe signal
	// when a client ends a connection, the server does not break
	signal(SIGPIPE, SIG_IGN);

	while(1) {

		client_len = sizeof(client_address);

		client_params = malloc(sizeof(struct client_thread_parms));
		// accept a client connection
		client_params->client_sockid = accept(server_sockfd, (struct sockaddr *)&client_address, (void*) &client_len);
		
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
		printf("Config.: Atendidos %d clientes \n", cont);
	}	

}
