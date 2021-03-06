#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "share.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

int main() {
	char line[MAX_MSG_LEN];
	int fd;
	struct shm_for_msg *shm;
	sem_t *sem_free_for_write_msg;
	sem_t *sem_ready_to_read_msg;
	
	fd = shm_open(SHM_NAME, O_RDWR ,0);
	if(fd == -1) {
		perror(SHM_NAME);
		return 1;
	}	
	if(ftruncate(fd, sizeof(struct shm_for_msg)) == -1) {
		perror(SHM_NAME);
		return 1;
	}	

	shm = (struct shm_for_msg *)mmap(NULL, sizeof(struct shm_for_msg), PROT_READ, MAP_SHARED, fd, 0);
	if(shm == MAP_FAILED) {
		perror(SHM_NAME);
		close(fd);
		unlink(SHM_NAME);
		return 1;	
	}
	// habe gelesen (display) --> menu darf wieder rein schreiben
	sem_free_for_write_msg = sem_open(SEM_FREE_FOR_WRITE_MSG, O_RDWR, 0, 0 );
	if(sem_free_for_write_msg == SEM_FAILED) {
		perror(SHM_NAME);
	}	
	// menu hat geschrieben --> display darf lesen (ready to read)
	sem_ready_to_read_msg = sem_open(SEM_READY_TO_READ_MSG, O_RDWR, 0, 0);
	if(sem_ready_to_read_msg == SEM_FAILED) {
		perror(SHM_NAME);
	}	

	for(;;) {
		sem_wait(sem_ready_to_read_msg);
		memcpy(line, shm->message, MAX_MSG_LEN);
		sem_post(sem_free_for_write_msg);
		printf("output: %s\n", line);
		if(!strcmp(line, "quit")) {
			break;
		}
	}
	munmap(shm, sizeof(struct shm_for_msg));
	close(fd);	
	unlink(SHM_NAME);
	sem_unlink(SEM_FREE_FOR_WRITE_MSG);
	sem_unlinkk(SEM_READY_TO_READ_MSG);
	return 0;
}
