#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "share.h"
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/types.h>

void convert_to_upper(char *str) {
	int i;
	for(i = 0; str[i] != '\0'; i++) {
		if(islower(str[i]))
			str[i] = toupper(str[i]);
	}
}

void convert_to_lower(char *str) {
	int i;
	for(i = 0; str[i] != '\0'; i++) {
		if(isupper(str[i]))
			str[i] = tolower(str[i]);
	}
}

int main() {
	char line[MAX_MSG_LEN];
	mqd_t queue;
	int pipe_fd_lower;
	int pipe_fd_upper;	

	queue = mq_open(QUEUE_NAME, O_RDONLY);
	if(queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}
	
	
	// erstellen von Pipes --- start

	if(mkfifo(PIPE_NAME_LOWER, 0600) == -1) {
		perror(PIPE_NAME_LOWER);
		return 1;
	}
	else {
		if((pipe_fd_lower = open(PIPE_NAME_LOWER, O_RDWR)) == -1) {
			perror("open");
			return 1;
		}
	}

	if(mkfifo(PIPE_NAME_UPPER, 0600) == -1) {
		perror(PIPE_NAME_UPPER);
		return 1;
	}
	else {
		if((pipe_fd_upper = open(PIPE_NAME_UPPER, O_RDWR)) == -1) {
			perror("open");
			return 1;
		}
	}

	for(;;) {
		unsigned int prio = 7;		// init != 0 to check if it works :)
		ssize_t read_cnt = mq_receive(queue, line, MAX_MSG_LEN, &prio);
		if(read_cnt == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			mq_unlink(QUEUE_NAME);
			return 1;
		}

		if(!read_cnt) {
			continue;
		}
		line[read_cnt - 1] = '\0';		// if not a '\0' overwrite it with a '\0'
		
		printf("Read from Message Queue: %s\n", line);
	
		// PIPE_UPPER	
		convert_to_upper(line);
		write(pipe_fd_upper, line, strlen(line)+1);
		write(pipe_fd_upper, "\n", 1);
		printf("Wrote to Pipe (%s): %s\n", PIPE_NAME_UPPER,  line);		
		
		// PIPE_LOWER
		convert_to_lower(line);
		write(pipe_fd_lower, line, strlen(line)+1);
		write(pipe_fd_lower, "\n", 1);
		printf("Wrote to Pipe (%s): %s\n\n", PIPE_NAME_LOWER,  line);
		if(!strcmp(line, "quit")) {
			break;
		}
	}
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	return 0;
}
