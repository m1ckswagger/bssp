#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MY_DEVICE "/dev/mydev1"
#define MY_DEVICE_2 "/dev/mydev3"
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[]) {
  FILE *mydevice;
  char buf[BUFFER_SIZE];

  if((mydevice = fopen(MY_DEVICE, "w")) == NULL) {
    perror("fopen");
    return 1;
  }
	printf("Opened %s for write...\n", MY_DEVICE);

  printf("I will now sleep for 5 seconds...\n");
  sleep(5);
  printf("I am up again. Time for work!\n\n");

	printf("Writing \"This is a test\" into %s\n", MY_DEVICE);
  fprintf(mydevice, "This is a test\n");
  if(fclose(mydevice)) {
		perror("fprintf");
	}
	else {
	//	printf("closed device %s\n", MY_DEVICE);
	}

  // it should print out an error, when device was not opened with write
  // mode before.
  
	/*
	if((mydevice = fopen(MY_DEVICE_2, "r")) == NULL) {
    perror("fopen");
    return 1;
  }
	*/

  if((mydevice = fopen(MY_DEVICE, "r")) == NULL) {
    perror("fopen");
    return 1;
  }
	printf("opened device %s for read\n", MY_DEVICE);

  printf("Read from %s\n\n", MY_DEVICE);
  while(fgets(buf, BUFFER_SIZE, mydevice) != NULL) {
    printf("%s", buf);
  }

  fclose(mydevice);

  return 0;
}
