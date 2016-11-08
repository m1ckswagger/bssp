// Socketserr

// socketheader
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>


#define BUFSZ 1024

void *client_handler(void *arg) {
	int fd;
	int anzbytes;
	char buf[BUFSZ];
	
	fd = (int)arg;

	// TODO client in liste eintragen

	while ((anzbytes = read(fd, buf, BUFSZ-1)) > 0 ) {
		buf[anzbytes] = 0;
		printf("Client sagt: %s\n", buf);
		write(fd, "Danke\n", 6);
		// TODO meldung an alle clients	
	}
	// TODO clients aus liste löschen
	return NULL;
}

int main() {

	int sock, client_sock_fd;
	unsigned sock_addr_size;
	int err;
	pthread_t thr_id;
	
	struct sockaddr sock_addr;
	struct sockaddr_in srv_sock_addr;

	srv_sock_addr.sin_addr.s_addr = INADDR_ANY;
	srv_sock_addr.sin_port = htons(4002);
	srv_sock_addr.sin_family = AF_INET;

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
		perror("socket"), exit(1);
	
	// cast weil type sockaddr_in
	if (bind(sock, (struct sockaddr*)&srv_sock_addr, sizeof(srv_sock_addr)) == -1)	
		perror("bind"), exit(1);

	if (listen(sock,6) == -1) 
		perror("listen"), exit(1);

	printf("Server listening on 4002\n");
	
	for(;;) {
		sock_addr_size = sizeof(sock_addr);
		if((client_sock_fd = accept(sock, &sock_addr, &sock_addr_size) == -1))
			perror("accept"), exit(1);
		
		printf("Client connected!\n");
		
		err = pthread_create(&thr_id, NULL, client_handler, (void *)client_sock_fd);
		if (err)
			printf("Thread creation failed: %s\n", strerror(err)), exit(1);
	}
	return 0;
}
