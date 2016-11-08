#include <stdio.h>
#include <string.h>
#include "share.h"
#include <mqueue.h>
#include <sys/stat.h>

int main() {
	char line[MAX_MSG_LEN];
	mqd_t queue;

	queue = mq_open(QUEUE_NAME, O_RDONLY);
	if(queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}

	// TODO check attributes mq_getattr
	for(;;) {
		unsigned int prio = 7;		// init != 0 to check if it works :)
		ssize_t read_cnt = mq_receive(queue, line, MAX_MSG_LEN, &prio);
		if(read_cnt == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			// TODO	mq_unlink????
			return 1;
		}

		if(!read_cnt) {
			continue;
		}
		line[read_cnt - 1] = '\0';		// if not a '\0' overwrite it with a '\0'
		
		printf("priority %u, output: %s\n", prio, line);		
		if(!strcmp(line, "quit")) {
			break;
		}
	}
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	return 0;
}
