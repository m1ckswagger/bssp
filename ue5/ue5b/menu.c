#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#include "share.h"

int main() {
	char line[MAX_MSG_LEN];
	int fd;
	struct shm_for_msg *shm;
	sem_t *sem_free_for_write_msg;
	sem_t *sem_ready_to_read_msg;


	// init shared mem --- begin	
	unlink(SHM_NAME);

	fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0600);
	if(fd == -1) {
		perror(SHM_NAME);
		return 1;
	}	
	if(ftruncate(fd, sizeof(struct shm_for_msg)) == -1) {
		perror(SHM_NAME);
		return 1;
	}	

	shm = (struct shm_for_msg *)mmap(NULL, sizeof(struct shm_for_msg), PROT_WRITE, MAP_SHARED, fd, 0);

	if(shm == MAP_FAILED) {
		perror(SHM_NAME);
		close(fd);
		unlink(SHM_NAME);
		return 1;	
	}
	// init shared mem --- end

	sem_unlink(SEM_FREE_FOR_WRITE_MSG);
	sem_unlink(SEM_READY_TO_READ_MSG);

	// habe gelesen (display) --> menu darf wieder rein schreiben
	sem_free_for_write_msg = sem_open(SEM_FREE_FOR_WRITE_MSG, O_RDWR | O_CREAT, 0600, 1);
	if(sem_free_for_write_msg == SEM_FAILED) {
		perror(SHM_NAME);
	}	
	
	// menu hat geschrieben --> display darf lesen (ready to read)
	sem_ready_to_read_msg = sem_open(SEM_READY_TO_READ_MSG, O_RDWR | O_CREAT, 0600, 0);
	if(sem_ready_to_read_msg == SEM_FAILED) {
		perror(SHM_NAME);
	}	


	for(;;) {
		int len;
		printf("> ");
		fgets(line, MAX_MSG_LEN, stdin);
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}
		sem_wait(sem_free_for_write_msg);
		memcpy(shm->message, line, MAX_MSG_LEN);
		sem_post(sem_ready_to_read_msg);	
		if(!strcmp(line, "quit")) {
			break;
		}
	}
	munmap(shm, sizeof(struct shm_for_msg));
	close(fd);
	return 0;
}
