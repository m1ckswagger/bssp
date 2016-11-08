#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "share.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int main() {
	char line[MAX_MSG_LEN];
	int pipe_fd;
	FILE *pipe;
	if((pipe_fd = open(PIPE_NAME, O_RDONLY)) == -1) {
		perror("open");
		return 1;
	}
	pipe = fdopen(pipe_fd, "r");
	for(;;) {
			
		if(fgets(line, MAX_MSG_LEN, pipe) == NULL) {
			fprintf(stderr, "Error reading from pipe!\n");
			return 1;
		}
		printf("Read from Pipe: %s\n", line);
		
		if(!strcmp(line, "QUIT")) {
			break;
		}
	}
	unlink(PIPE_NAME);	
	return 0;
}
