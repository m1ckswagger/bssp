// Socketserver

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

#define BUFSZ 1024
#define SRV_PORT 4002
#define MAX_USR_NAME 32

void *recieve_message(void *socket) {
	int sock_fd, anzbytes;
	char buf[BUFSZ];
	int *tmp = (int*) socket;

	sock_fd = *tmp;

	memset(buf, 0, BUFSZ);
	
	for(;;) {
		anzbytes = read(sock_fd, buf, BUFSZ);
		write(1, buf, anzbytes);
	}
}

int main(int argc, char **argv)
{
	int sock_fd;
	int err;
	pthread_t thr_id;
	char *srv_addr;
	char username[MAX_USR_NAME+1];
	char buf[BUFSZ];
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

	memset(buf, 0, BUFSZ);
	printf("Enter message:\n");

	err = pthread_create(&thr_id, NULL, recieve_message, (void*) &sock_fd);
	if (err) {
		printf("Threaderzeugung: %s\n", strerror(err));
	}
	while(fgets(buf, BUFSZ, stdin) != NULL)
		write(sock_fd, buf, BUFSZ-1);

	close(sock_fd);
	pthread_exit(NULL);

	return 0;
}
