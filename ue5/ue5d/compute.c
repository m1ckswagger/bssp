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

int main() {
	char line[MAX_MSG_LEN];
	mqd_t queue;
	int pipe_fd;

	queue = mq_open(QUEUE_NAME, O_RDONLY);
	if(queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}

	if(mkfifo(PIPE_NAME, 0600) == -1) {
		perror(PIPE_NAME);
		return 1;
	}
	else {
		if((pipe_fd = open(PIPE_NAME, O_RDWR)) == -1) {
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
		
		convert_to_upper(line);
		write(pipe_fd, line, strlen(line)+1);
		write(pipe_fd, "\n", 1);
		printf("Wrote to Pipe: %s\n", line);				
		if(!strcmp(line, "QUIT")) {
			break;
		}
	}
	//unlink(PIPE_NAME);	
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	return 0;
}
