// Socketserver

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include "mychat.h"

#define BUFFER_SIZE 1024
#define MAX_USR_NAME 32
#define MAX_USRS 5

typedef struct chat_client {
	char username[MAX_USR_NAME];
	int fd;
	pid_t tid;
} chat_client_t;

chat_client_t clients[MAX_USRS];
int usercount = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_handler_params {
	int fd;		// socket file descriptor
	char username[MAX_USR_NAME];
};

void show_client_list() {
	int i;
	pthread_mutex_lock(&mutex);
	for (i = 0; i<MAX_USRS; i++) {
		if (clients[i].fd != 0)
			printf("Username: %s FD: %d TID: %d\n", clients[i].username, clients[i].fd, clients[i].tid);
	}
	pthread_mutex_unlock(&mutex);
}

int contains_user(chat_client_t *client) {
	int i;
	pthread_mutex_lock(&mutex);
	for(i = 0; i < MAX_USRS; i++) {
		// wenn user enthalten return 1
		if(!strcmp(client->username, clients[i].username) && clients[i].fd!=0) {
			pthread_mutex_unlock(&mutex);
			return 1;
		}
	}
	pthread_mutex_unlock(&mutex);
	return 0;
}


// Liefert bei Erfolg den Index an dem der Benutzer
// eingefügt wurde
int add_user(chat_client_t *client) {
	int i;
	pthread_mutex_lock(&mutex);
	for(i = 0; i < MAX_USRS; i++) {
		if (clients[i].fd == 0) {
			clients[i].fd = client->fd;
			clients[i].tid = client->tid;
			strcpy(clients[i].username, client->username);
			usercount++;
			pthread_mutex_unlock(&mutex);
			return i;
		}
	}
	pthread_mutex_unlock(&mutex);
	return -1;
}

int receive_until_null_or_linefeed(int target_fd, char *buffer, int buffer_size) {

  int rdbytes = 0;
  char currentByte;

  // weil -1 ist fehler und 0 ist EOF z.B. oder connection terminiert.
  while ((recv(target_fd, &currentByte, 1, MSG_WAITALL) > 0) && (rdbytes < buffer_size)) {
    if (currentByte == '\0') {
      buffer[rdbytes] = '\0';
      return rdbytes;
    } else if (currentByte == '\n') {
      buffer[rdbytes] = '\0';
      return rdbytes + 1;
    } else {
      buffer[rdbytes] = currentByte;
      rdbytes++;
    }
  }
  return rdbytes;
}

void message_all_clients(char *message, chat_client_t *client) {
	int i;
	show_client_list();
	pthread_mutex_lock(&mutex);
	
	for (i = 0; i < MAX_USRS; i++) {
		// wenn fd nicht verwendet
		if (clients[i].fd != 0) {
			//printf("User %s writing %s\n", client->username, clients[i].username);
			write(clients[i].fd, client->username, strlen(client->username));
			write(clients[i].fd, ": ", 2);
			write(clients[i].fd, message, strlen(message)+1);
		}
	}
	pthread_mutex_unlock(&mutex);
}

void *client_handler(void *args)
{
	struct client_handler_params *params = args;
	int rdbytes;
	int index = 0;			// Index an dem der Benutzer gespeichert ist
	chat_client_t client;
	char message[BUFFER_SIZE];
	char user_taken[] = "Username already in use!";
	char user_limit_exceeded[] = "User limit exceeded! Try again later.";	
	pid_t tid = syscall(__NR_gettid);

	client.fd = params->fd;
	client.tid = tid;
	strcpy(client.username, params->username);
	
	// Client in Liste hinzufügen
	if(usercount) {
		// wenn username bereits verwendet -> Fehlermeldung
		if(contains_user(&client)) {
			write(client.fd, user_taken, sizeof(user_taken));
			return NULL;
		}

		if(usercount > MAX_USRS) {
			write(client.fd, user_limit_exceeded, sizeof(user_limit_exceeded));
			return NULL;
		}
	}
	if((index = add_user(&client)) == -1) {
		printf("Fehler beim Speichern des Benutzers in der Liste!\n");
	}
	else {
		printf("Added user %s (fd=%d) on index %d\n", clients[index].username, clients[index].fd, index);
	}
	pthread_mutex_unlock(&mutex);
	
	show_client_list();
	// Client Nachrichten verwalten
	/*while ( (rdbytes = read(client.fd, message, BUFFER_SIZE)) > 0 )
	{*/
	for (;;) {
		rdbytes = receive_until_null_or_linefeed(client.fd, message, BUFFER_SIZE);
		if (rdbytes == -1) {
			fprintf(stderr, "Error in Receive from Client!\n");
			break;
		}
		if (rdbytes == 0) {
			printf("SERVER-Info: Client %s closed the connection!\n", client.username);
			break;
		}	
		printf("Read %d bytes from Client %s\n", rdbytes,  client.username);
		
		// alle Clients benachrichtigen
		pthread_mutex_lock(&mutex);
		printf("Thread working: %d\n", client.tid);
		printf("Client (%s) sagt: %s\n", client.username, message);
		pthread_mutex_unlock(&mutex);
		message_all_clients(message, &client);
			
	}
	// Client aus der Liste Löschen
	pthread_mutex_lock(&mutex);
	printf("Removing Client %s from the list\n", client.username);
	close(client.fd);
	clients[index].fd = 0;
	usercount--;
	pthread_mutex_unlock(&mutex);
	
	return NULL;
}

void init_clients() {
	int i;
	for (i = 0; i<MAX_USRS; i++) {
		clients[i].fd = 0;
	}
}

int main()
{
	int sock, client_sock_fd;
	unsigned sock_addr_size;
	int err;
	char username[MAX_USR_NAME];
	int rdbytes;
	pthread_t thr_id;

	struct sockaddr sock_addr;
	struct sockaddr_in srv_sock_addr;

	struct client_handler_params params;

	srv_sock_addr.sin_addr.s_addr = INADDR_ANY;
	srv_sock_addr.sin_port = htons(SRV_PORT);
	srv_sock_addr.sin_family = AF_INET;

	if ( (sock=socket(PF_INET,SOCK_STREAM, 0)) == -1)
	{
		perror("socket"); 
		exit(1);
	}
	if ( bind(sock, (struct sockaddr*)&srv_sock_addr, sizeof(srv_sock_addr) ) == -1)
	{
		perror("bind"); 
		exit(1);
	}
	if ( listen(sock,6) ==-1 )
	{
		perror("listen"); 
		exit(1);
	}
	init_clients();
	printf("Server running on %d\n", SRV_PORT);
	for (;;)
	{
		sock_addr_size=sizeof(sock_addr);
		if ( (client_sock_fd=accept(sock,&sock_addr,&sock_addr_size)) == -1)
		{
			perror("accept"); 
			exit(1);
		}
		if((rdbytes = read(client_sock_fd, username, sizeof(username))) == -1) {
			fprintf(stderr, "Cannot read username");
			exit(1);
		}	
		printf ("%s hat sich verbunden\n", username);

		// setzen der parameter
		params.fd = client_sock_fd;
		strcpy(params.username, username);
		err=pthread_create(&thr_id, NULL, client_handler, &params);
		if (err)
			printf("Threaderzeugung: %s\n", strerror(err));
	}
}
