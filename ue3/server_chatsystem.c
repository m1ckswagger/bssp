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

#define BUFSZ 1024
#define SRV_PORT 4002
#define MAX_USR_NAME 32
#define MAX_USRS 20

char users[MAX_USRS][MAX_USR_NAME];
int users_fd[MAX_USRS];
pid_t thr_ids[MAX_USRS];
int usercount = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct client_handler_params {
	int fd;		// socket file descriptor
	char username[MAX_USR_NAME];
};

int contains_user(char *username) {
	int i;
	for(i = 0; i < MAX_USRS; i++) {
		// wenn user enthalten return 1
		if(!strcmp(users[i], username))
			return 1;
	}
	return 0;
}


// Liefert bei Erfolg den Index an dem der Benutzer
// eingefügt wurde
int add_user(char *username, int fd, pid_t tid) {
	int i;
	for(i = 0; i < MAX_USRS; i++) {
		if(!strcmp(users[i], "\0")) {
			strcpy(users[i], username);
			//users[i] = username;
			users_fd[i] = fd;
			thr_ids[i] = tid;
			return i;
		}
	}
	return -1;
}

void message_all_clients(char *message, char* username) {
	int i;
	for(i = 0; i < MAX_USRS; i++) {
		// wenn benutzer nicht \0
		if(strcmp(users[i], "\0")) {
			write(users_fd[i], username, strlen(username));
			write(users_fd[i], ": ", 2);
			write(users_fd[i], message, strlen(message));
		}
	}
}

char *get_username_by_tid(pid_t tid, char *username) {
	int i;

	for(i = 0; i < MAX_USRS; i++) {
		if(tid == thr_ids[i]) {
			strcpy(username, users[i]);
			return users[i];
		}
	}
	return "";
}

void *client_handler(void *args)
{
	struct client_handler_params *params = args;
	int anzbytes;
	int index = 0;			// Index an dem der Benutzer gespeichert ist
	char usrn[MAX_USR_NAME];
	char buf[BUFSZ];
	char user_taken[] = "Username already in use!";
	char user_limit_exceeded[] = "User limit exceeded! Try again later.";	
	pid_t tid = syscall(__NR_gettid);

	// Client in Liste hinzufügen
	pthread_mutex_lock(&mutex);
	if(usercount) {
		// wenn username bereits verwendet -> Fehlermeldung
		if(contains_user(params->username)) {
			write(params->fd, user_taken, sizeof(user_taken));
			return NULL;
		}

		if(usercount > MAX_USRS) {
			write(params->fd, user_limit_exceeded, sizeof(user_limit_exceeded));
		}
	}
	else {
		memset(users, 0, MAX_USRS*MAX_USR_NAME);
	}
	tid = syscall(__NR_gettid);
	if((index = add_user(params->username, params->fd, tid)) == -1) {
		printf("Fehler beim Speichern des Benutzers in der Liste!\n");
	}
	else {
		printf("Added user %s on index %d\n", params->username, index);
		usercount++;
	}
	pthread_mutex_unlock(&mutex);
	
	// Client Nachrichten verwalten
	while ( (anzbytes=read(params->fd,buf,BUFSZ-1)) > 0 )
	{
		buf[anzbytes] = 0;
		
		// alle Clients benachrichtigen
		pthread_mutex_lock(&mutex);
		tid = syscall(__NR_gettid);
		get_username_by_tid(tid, usrn);
		printf("Thread working: %d\n", tid);
		printf("Client (%s) sagt: %s\n", usrn, buf);
		message_all_clients(buf, usrn);
		pthread_mutex_unlock(&mutex);
			
	}
	// Client aus der Liste Löschen
	strcpy(users[index], "\0");
	return NULL;
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
