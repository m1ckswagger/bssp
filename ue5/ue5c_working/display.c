#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "share.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

int main(int argc, char **argv) {
	char line[MAX_MSG_LEN];
	int fd;
	int id;
	struct shm_for_msg *shm;
	sem_t *sem_free_for_write_msg;
	sem_t *sem_ready_to_read_msg;

	// oeffnen des fd fuer das shared memory
	fd = shm_open(SHM_NAME, O_RDWR ,0);
	if(fd == -1) {
		perror(SHM_NAME);
		return 1;
	}
	
	// groesse des shared memory setzen	
	if(ftruncate(fd, sizeof(struct shm_for_msg)) == -1) {
		perror(SHM_NAME);
		return 1;
	}	

	// mappen des shared memory
	shm = (struct shm_for_msg *)mmap(NULL, sizeof(struct shm_for_msg), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(shm == MAP_FAILED) {
		perror(SHM_NAME);
		close(fd);
		unlink(SHM_NAME);
		return 1;	
	}
	
	// setzen der display id und erhoehen von maxcount
	id = shm->maxcount;
	shm->maxcount += 1;	
	printf("Display ID: %d\n", id);
	
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
		// ausgabe wird nur getaetigt wenn die akt_id im shared memory
		// mit der id des displays uebereinstimmt
		if(shm->akt == id) {	
			sem_wait(sem_ready_to_read_msg);
			memcpy(line, shm->message, MAX_MSG_LEN);
			printf("output: %s\n", line);
			
			// wenn letztes display gelesen hat wird akt auf 0 gesetzt
			// und das menu kann wieder schreiben
			if(shm->akt == shm->maxcount-1) {
				shm->akt = 0;
				sem_post(sem_free_for_write_msg);
			}
			// sonst wird die id fuer das zu lesende display im shared 
			// memory erhoeht und das display kann erneut lesen
			else {
				shm->akt += 1;
				sem_post(sem_ready_to_read_msg);
			}
			if(!strcmp(line, "quit")) {
				break;
			}
		}
	}
	munmap(shm, sizeof(struct shm_for_msg));
	close(fd);	
	return 0;
}
