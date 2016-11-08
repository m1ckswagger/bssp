#include <stdio.h>
#include <string.h>
#include "share.h"
#include <mqueue.h>
#include <sys/stat.h>

int main() {
	char line[MAX_MSG_LEN];
	struct mq_attr attr = {0, MAX_MSG_COUNT, MAX_MSG_LEN, 0 };
	mqd_t queue;

	queue = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0600, &attr);
	if(queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}

	// TODO check attributes mq_getattr
	for(;;) {
		int len;
		printf("> ");
		fgets(line, MAX_MSG_LEN, stdin);
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}
		// send message to queue
		if(mq_send(queue, line, strlen(line) + 1, 0 /*priority*/) == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			return 1;
		}	
		if(!strcmp(line, "quit")) {
			break;
		}
	}
	mq_close(queue);
	return 0;
}
