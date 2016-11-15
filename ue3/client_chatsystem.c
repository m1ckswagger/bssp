#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include "mychat.h"
#include "rawio.h"

#define BUFFER_SIZE 1024
#define MAX_USR_NAME 32

pthread_mutex_t screen = PTHREAD_MUTEX_INITIALIZER;

int term_lines;
int term_cols;

typedef struct data {
	int fd;
	int current_term_lines;
} data_t;

void *recieve_message(void *arg) {
	char buf[BUFFER_SIZE];
	data_t *data;
	data  = (data_t*) arg;
	int rdbytes;
	int line=1;

	
	while ((rdbytes = read(data->fd, buf, BUFFER_SIZE-1)) > 0) {
		//memset(buf, 0, BUFFER_SIZE);
		buf[rdbytes] = 0;
		pthread_mutex_lock(&screen);
		writestr_raw(buf, 0, line);
		if (line == (term_lines-1)) {
			scroll_up(0, data->current_term_lines);
			line--;
		}
		else {
			line++;
		}
		pthread_mutex_unlock(&screen);
	}
	return NULL;
}

int send_message(int fd, char *message) {
	size_t len = strlen(message) + 1;
	if (write(fd, message, len) != len) {
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int sock_fd;
	int err;
	pthread_t thr_id;
	char *srv_addr;
	char username[MAX_USR_NAME+1];
	char buf[BUFFER_SIZE];
	struct sockaddr_in client_sock;

	if(argc != 3) {
		printf("usage: client <ip address> <username>\n");
		exit(1);
	}
	// setzen der Server adresse
	srv_addr = argv[1];
	if(strlen(argv[2]) > MAX_USR_NAME) {
		fprintf(stderr, "Max. username length is %d.\n", MAX_USR_NAME);
		exit(1);
	}
	// kopieren des username und setzen von \0
	strcpy(username, argv[2]);
	username[strlen(argv[2])] = '\0';

	if ( (sock_fd=socket(AF_INET,SOCK_STREAM, 0)) == -1)
	{
		perror("socket"); 
		exit(1);
	}
	printf("Socket created!\n");

	memset(&client_sock, 0, sizeof(client_sock));

	client_sock.sin_family = AF_INET;	

	// konvertieren von adresse zu binary ipv4
	err = inet_aton(srv_addr, &client_sock.sin_addr);	
	if(err < 0) {
		printf("No valid IPv4 address!\n");
		exit(1);
	}

	// PORT zuweisen
	client_sock.sin_port = htons(SRV_PORT);

	if ( err = connect(sock_fd, (struct sockaddr *)&client_sock, sizeof(client_sock )) < 0)
	{
		perror("connect"); 
		exit(1);
	}
	write(sock_fd, username, strlen(username)+1); 	
	printf("connected to %s:%d as %s\n", srv_addr, SRV_PORT, username);

	memset(buf, 0, BUFFER_SIZE);
	err = pthread_create(&thr_id, NULL, recieve_message, (void*) &sock_fd);
	if (err) {
		printf("Threaderzeugung: %s\n", strerror(err));
	}

	// Send
	printf("Current Term lines: %d\n", term_lines);
	//sleep(5);
	clearscr();
	term_cols = get_cols();
	term_lines = get_lines();
	for (;;) {
		int i;
		pthread_mutex_lock(&screen);
		writestr_raw("Say something: ", 0, term_lines-1);
		pthread_mutex_unlock(&screen);
		
		gets_raw(buf, BUFFER_SIZE, strlen("Say something: "), term_lines);
		
		pthread_mutex_lock(&screen);
		writestr_raw("Say something: ", 0, term_lines-1);
		pthread_mutex_unlock(&screen);
		
		for (i = 0; i < term_cols; i++) {
			writestr_raw(" ", i, term_lines-1);
		}

		if (!strcmp(buf, "/quit")) {
			break;
		}
		if (strlen(buf) > 0 && send_message(sock_fd, buf) == -1) {
			fprintf(stderr, "could not end message \n");
			break;
		}
	}
	close(sock_fd);
	pthread_exit(NULL);

	return 0;
}
