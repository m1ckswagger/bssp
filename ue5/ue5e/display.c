#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "share.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int main(int argc, char **argv) {

	int id;		// bestimmt welche Pipe verwendet wird
	char line[MAX_MSG_LEN];
	int pipe_fd;
	FILE *pipe;	
	
	if(argc != 2) {
		fprintf(stderr, "usage: ./display <id>\n");	
		return 1;
	}
	id = strtol(argv[1], NULL, 10);
	
	// bestimmt ob id 0 oder 1
	if(id < 0 || id > 1) {	
		fprintf(stderr, "error: id has to be 1 or 0\n");
		return 1;
	}

	// Bestimmung ob lower- oder upper-pipe
	if(id) {
		if((pipe_fd = open(PIPE_NAME_LOWER, O_RDONLY)) == -1) {
			perror("open");
			return 1;
		}
	}
	else {
		if((pipe_fd = open(PIPE_NAME_UPPER, O_RDONLY)) == -1) {
			perror("open");
			return 1;
		}
	}
	pipe = fdopen(pipe_fd, "r");
	for(;;) {
			
		if(fgets(line, MAX_MSG_LEN, pipe) == NULL) {
			fprintf(stderr, "Error reading from pipe!\n");
			return 1;
		}
		printf("Read from Pipe: %s\n", line);
		
		if((!strcmp(line, "QUIT") && id==0) || (!strcmp(line, "quit") && id==1)) {
			break;
		}
	}

	if(id) {
		unlink(PIPE_NAME_LOWER);	
	}
	else {
		unlink(PIPE_NAME_UPPER);	
	}
	return 0;
}
