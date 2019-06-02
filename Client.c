#define CONFIG_SERVER_ADDR "127.0.0.1"
#define CONFIG_SERVER_PORT 9740
#define MEMORY_SERVER_SIZE 10

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct serv {
	char ip[15];
	int port;
	int start_position;
	int end_position;
	int is_connected;
	int sockfd;
}Server;

int total_servers;
Server *servers;

void getServersConfig(){
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

	// reads number os servers
	recv(sockfd, &total_servers, sizeof(int), MSG_WAITALL);	
	total_servers = ntohl(total_servers);

	// reads string config size
	recv(sockfd, &config_str_size, sizeof(int), MSG_WAITALL);
	config_str_size = ntohl(config_str_size);

	// reads servers configuration
	char *servers_config = malloc(config_str_size * sizeof(char));
	recv(sockfd, servers_config, config_str_size * sizeof(char), MSG_WAITALL);

	printf("%s \n", servers_config);
	
	// create a vector of server structs
	servers = (Server*)malloc(total_servers * sizeof(Server));

	i = 0;
	flag = 0;
	j = 0;

	while(i < config_str_size && j < total_servers) {
		k = 0;
		l = 0;
		// getting ip address
		while(servers_config[i] != ':') {
		    servers[j].ip[k] = servers_config[i];
		    k++; i++;
		}
		servers[j].ip[k] = '\0';
		i++;

		// getting port number
		while(servers_config[i] != ';'){
		    aux[l] = servers_config[i];
		    l++; i++;
		}
		servers[j].port = atoi(aux);
		
		servers[j].is_connected = 0;
		servers[j].start_position = j * MEMORY_SERVER_SIZE;
		servers[j].end_position = servers[j].start_position + MEMORY_SERVER_SIZE - 1;
		
		j++; i++;
	}

	printf("Servers Configuration: \n");
	for(i=0; i < total_servers; i++)
		printf("%s:%d : [%d: %d] \n", servers[i].ip, servers[i].port, servers[i].start_position, servers[i].end_position);

	free(servers_config);
}

int sendCommand(int server, char command, int server_start_index, int server_end_index, char *data) {
	int result;
	// if the client is not connected to this server yet, than connection is created
	if(!servers[server].is_connected) {
		
		struct sockaddr_in address;

		servers[server].sockfd = socket(AF_INET, SOCK_STREAM, 0);
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr(servers[server].ip);
		address.sin_port = htons(servers[server].port);
		int len = sizeof(address);

		result = connect(servers[server].sockfd, (struct sockaddr *)&address, len);
		if(result == -1) {
			perror("error on connect to server: ");
			return -1;
		}
		
		servers[server].is_connected = 1;
	}
	
	// ** Client is connected with the server **
	
	// writing on socket the command chosen
	result = write(servers[server].sockfd, &command, sizeof(char));

	if(result == -1) {
		perror("An error occurred writing on socket: ");
		return -1;
	}	

	// writing on socket the start index
	int server_start_index_to_network = htonl(server_start_index);
	result = write(servers[server].sockfd, &server_start_index_to_network, sizeof(int));

	if(result == -1) {
		perror("An error occurred writing on socket: ");
		return -1;
	}

	// writing on socket the end index
	int server_end_index_to_network = htonl(server_end_index);
	result = write(servers[server].sockfd, &server_end_index_to_network, sizeof(int));

	if(result == -1) {
		perror("An error occurred writing on socket: ");
		return -1;
	}

	int size = server_end_index - server_start_index;
	if(command == 'w') {

		// writing on socket the data to write
		result = write(servers[server].sockfd, data, size * sizeof(char));

		if(result == -1) {
			perror("An error occurred writing on socket: ");
			return -1;
		}

	} else {
		// reading data required
		result = recv(servers[server].sockfd, data, size * sizeof(char), MSG_WAITALL);

		if(result == -1) {
			perror("An error occurred reading on socket: ");
			return -1;
		}
	}

	return 1;
}
int main() {
	
	// TODO get the system config 
	getServersConfig();
	
	int total_size = total_servers * MEMORY_SERVER_SIZE;

	// TODO calculate N (total space in bytes)
	printf("Your memory have %d bytes!! Enjoy! \n", total_size);fflush(stdout);
	
	char command;
	int start_index, end_index, command_result;

	setlinebuf(stdout); 
	setbuf(stdout, NULL);
	printf("Commands: \n");fflush(stdout);
	printf("r: Read \n");fflush(stdout);
	printf("w: Write \n");fflush(stdout);
	printf("q: Quit \n");fflush(stdout);

	while(1){

		printf("Enter a command: \n");fflush(stdout);
		// reading client command
		command = '\n';
		while(1){
			scanf("%c", &command);
			if(command != '\n'){
				break;
			}
		}

		if(command == 'q') {
			break;
		}
		
		if(command != 'w' && command != 'r') {
			printf("Invalid command! \n");
			continue;
		}
		
		// if command is read or write, get memory positions
		printf("Start position:\n");fflush(stdout);

		// reading start position
		scanf("%d", &start_index);

		printf("End position:\n");fflush(stdout);

		// reading end position
		scanf("%d", &end_index);

		// Validating entrys..
		if (start_index > end_index) {
			printf("Invalid Command: The start position must be smaller then end position\n");
			continue;
		} else if (start_index < 0 || end_index < 0) {
			printf("Invalid Command: Position(s) < 0\n");
			continue;
		} else if (start_index >= total_size || end_index > total_size) {
			printf("Invalid Command: Invalid Position \n");
			continue;
		}
		
		int size = end_index - start_index;
		char *data = malloc(size * sizeof(char));

		// if client wanna write
		// gets data that will be stored
		if(command == 'w'){
			printf("Data that you wanna save (%d chars): \n", size);fflush(stdout);

			// reading client data
			scanf("%s", data);
			
			if(strlen(data) != size) {
				printf("Invalid Command: Informed data has the wrong size. \n");
				free(data);
				continue;
			}
		}

		
		// decide witch servers are required for the operation
		// ex.: if each server have 10 positions:
		// server[0] : [0..9]
		// server[1] : [10..19]
		// server[2] : [20..29]
		// if start_position = 8 and end_position = 20
		// then start_server = 0 and end_server = 1
		int start_server = start_index / MEMORY_SERVER_SIZE;
		int end_server = (end_index - 1) / MEMORY_SERVER_SIZE;
		printf("servers requireds: %d : %d \n", start_server, end_server);
		for (int i = start_server; i <= end_server; i++) {
			
			// if the server positions are included on required band 
			// but the start position does not belongs to it, the first position for that server is 0
			int server_start_index = 0;
			int data_position = servers[i].start_position - start_index;

			if(start_index >= servers[i].start_position && start_index <= servers[i].end_position) {
				server_start_index = start_index - servers[i].start_position;
				data_position = 0;
			}

			// if the server positions are included on required band 
			// but the end position does not belongs to it, the last position for that server is his size
			int server_end_index = MEMORY_SERVER_SIZE;

			if(end_index - 1 >= servers[i].start_position && end_index - 1 <= servers[i].end_position) {
				server_end_index = end_index - servers[i].start_position; 
			}

			int sub_data_size = server_end_index - server_start_index;
			if(command == 'w') {

				// calculating the part of data for this server
				char sub_data[sub_data_size];
				memcpy(sub_data, &data[data_position], sub_data_size);
				command_result = sendCommand(i, command, server_start_index, server_end_index, sub_data);

				if(command_result < 0) {
					// TODO tratar o erro
					break;
				}
				

			} else {

				char *readed = malloc(sub_data_size * sizeof(char));
				command_result = sendCommand(i, command, server_start_index, server_end_index, readed);

				
				if(command_result == 1) {
					for(int j = 0; j < sub_data_size; j++) {
						data[data_position + j] = readed[j];
					}
				} else {
					// TODO tratar o erro
					break;
				}
				free(readed);
			}
			
		}

		if(command == 'r' && command_result == 1) {
			printf("%s \n", "Data readed:");
			for(int i = 0; i < size; i++) {
				printf("%c", data[i]);
			}
			printf("\n");
		}

		free(data);

	}

	// command = quit
	// close all connections that are open
	for(int i = 0; i < total_servers; i++) {
		if(servers[i].is_connected){
			// writing on open sockets the command chosen quit
			write(servers[i].sockfd, &command, sizeof(char));
			close(servers[i].sockfd);
		}
	}

	// free allocated memory
	free(servers);	

	exit(0);
}
