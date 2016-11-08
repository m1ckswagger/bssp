#ifndef SHARE_H_
#define SHARE_H_

#define MAX_MSG_LEN 256
#define MAX_MSG_COUNT 10
#define SHM_NAME "/is151002_shm1"
#define SEM_FREE_FOR_WRITE_MSG "/is151002_for_write"
#define SEM_READY_TO_READ_MSG "/is151002_to_read"

struct shm_for_msg
{
	int maxcount;
	int akt;	
	char message[MAX_MSG_LEN];
};

#endif
