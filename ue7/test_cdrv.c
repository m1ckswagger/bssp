#define _XOPEN_SOURCE 500


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "my_ioctl.h"
ssize_t got_bytes, resetted;
pthread_mutex_t fastmutex = PTHREAD_MUTEX_INITIALIZER;
char* ok = "\e[32m✓\e[0m";
char* err = "\e[31m✘\e[0m";
char new_buff[1024];

void* write_abc(void* FD){
	int fd = (int) FD;
	sleep(0.5);
	pthread_mutex_lock(&fastmutex);
	got_bytes = write(fd, "abc", 3);
	if(got_bytes != 3){
		printf("%s could not wirte abc to buffer %s\n", err, strerror(errno));
		pthread_mutex_unlock(&fastmutex);
		exit(62);
	}
	pthread_mutex_unlock(&fastmutex);
	printf("wrote 3 bytes\n");
	return NULL;
}
void* write_abcabc(void* FD){
	int fd = (int) FD;
	sleep(0.5);
	pthread_mutex_lock(&fastmutex);
	got_bytes = write(fd, "abc", 3);
	pthread_mutex_unlock(&fastmutex);
	sleep(0.1);
	pthread_mutex_lock(&fastmutex);
	got_bytes += write(fd, "abc", 3);
	pthread_mutex_unlock(&fastmutex);
	printf("wrote 6 bytes\n");
	return NULL;
}
void* write_reset_dev(void* FD){
	int fd = (int) FD;
	int res;
	sleep(1);
	printf("attempt to clear\n");
	pthread_mutex_lock(&fastmutex);
	res = ioctl(fd, IOC_CLEAR);
	resetted = 1;
	if(res != 0){
		printf("%s could not clear buffer %d\n", err, res);
		pthread_mutex_unlock(&fastmutex);
		exit(68);
	}
	pthread_mutex_unlock(&fastmutex);
	printf("cleared\n");
	return NULL;
}

int can_open_read(char* file){
	errno = 0;
	int fd = open(file, O_RDONLY|O_NONBLOCK|O_NDELAY);
	if(fd == -1)
		return -1;
	return fd;
}

int can_close(int fd){
	errno = 0;
	return close(fd);
}

int can_write_char(int fd, char c){
	errno = 0;
	return write(fd, &c, 1);
}

int can_open_write(char*file){
	errno = 0;
	int fd = open(file, O_RDWR|O_NONBLOCK|O_NDELAY);
	if(fd == -1)
		return -1;
	return fd;
}

int check_100_a(int fd){
	int i;
	char c;
	errno = 0;
	for(i = 0; i < 100; ++i){
		if(read(fd, &c, 1) != 1)
			return -1;
		if(c != 'a')
			return -2;
	}
	return 0;
}

int check_50_a(int fd){
	int i;
	char c;
	errno = 0;
	for(i = 0; i < 100; ++i){
		if(read(fd, &c, 1) != 1)
			return -1;
		if(i < 50){
			if(c != 'b')
				return -2;
		}else{
			if(c != 'a')
				return -2;
		}
	}
	return 0;
}

int main()
{
	char* files[] = {
		"/dev/mydev0",
		"/dev/mydev1",
		"/dev/mydev2",
		"/dev/mydev3",
		"/dev/mydev4"
	};
	int dev_count = 4;
	int i, x, error, fd;
	for(i = 0, error = 0; i < dev_count; ++i){
		int fd = can_open_read(files[i]);
		if(fd >= 0){
			error = 1;
			break;
		}
	}
	if(error)
		printf("%s open readonly (without write %s) %s\n", err, files[i], strerror(errno));	
	else
		printf("%s open readonly\n", ok);	

	for(i = 0, error = 0; i < dev_count; ++i){
		fd = can_open_write(files[i]);
		if ( fd <= 0 ){
			error = 1;
			break;
		}	

		for(x = 0; x < 100; ++x)		
			if (can_write_char(fd, 'a') != 1){
				error = 2;
				break;
			}
		if(error)
			break;

		if( can_close(fd) != 0 ){
			error = 3;
			break;

		}
		fd = can_open_read(files[i]);
		if(fd <= 0){
			error = 4;
			break;
		}

		if( check_100_a(fd) != 0 ){
			error = 5;
			break;
		}

		if(can_close(fd) != 0){
			error = 6;
			break;
		}

		fd = can_open_write(files[i]);

		if( fd <= 0 ){
			error = 7;
			break;
		}

		for(x = 0; x < 50; ++x)		
			if (can_write_char(fd, 'b') != 1){
				error = 8;
				break;
			}
		if(error)
			break;

		if(can_close(fd) != 0){
			error = 9;
			break;
		}

		fd = can_open_read(files[i]);
		if(fd <= 0){
			error = 10;
			break;
		}

		if(check_50_a(fd) != 0){
			error = 11;
			break;
		}

		close(fd);
	}
	if(error == 1){
		printf("%s open write/read (%s) %s\n", err, files[i], strerror(errno));	
		return error;
	}else
		printf("%s open write/read\n",ok);	

	if(error == 2){
		printf("%s could not write char (#%d) to %s: %s\n", err, x, files[i], strerror(errno));
		return error;
	}else
		printf("%s wrote 100 a's\n", ok);

	if(error == 3){
		printf("%s close write/read (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}else
		printf("%s could close file\n", ok);


	if(error == 4){
		printf("%s open read (%s) %s\n", err, files[i], strerror(errno));	
		return error;
	}else
		printf("%s opening for read after a write worked\n", ok);

	if(error == 5){
		printf("%s checked 100 a's (%s) %s\n", err, files[i], strerror(errno));	
		return error;
	}else{
		printf("%s checked 100 a's\n", ok);
	}

	if(error == 6){
		printf("%s close read (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}

	if(error == 7){
		printf("%s re-open write/read (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}else{
		printf("%s re-opened write/read\n", ok);
	}

	if(error == 8){
		printf("%s could not write char (#%d) to %s: %s\n", err, x, files[i], strerror(errno));
		return error;
	}

	if(error == 9){
		printf("%s close write/read again (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}

	if(error == 10){
		printf("%s open read (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}

	if(error == 11){
		printf("%s checked 50 b's (%s) %s\n", err, files[i], strerror(errno));
		return error;
	}else{
		printf("%s checked 50 a's and 50 b's\n", ok);
	}

	errno = 0;
	int fd_open_1 = open(files[0], O_WRONLY|O_NONBLOCK|O_NDELAY);
	if(fd_open_1 <= 0){
		printf("%s could not open 1st fd %s\n", err, strerror(errno));
		return 12;
	}
	errno = 0;
	int fd_open_2 = open(files[0], O_RDWR|O_NONBLOCK|O_NDELAY);
	if(fd_open_2 > 0){
		printf("%s should not be able to open a file more than once for writing\n", err);
		return 13;
	}else{
		printf("%s could not open 2 times for writing\n", ok);
	}
	fd_open_2 = open(files[0], O_RDONLY|O_NONBLOCK|O_NDELAY);
	if(fd_open_2 <= 0){
		printf("%s could not open a 2nd fd (1ts for write, 2nd for read) %s\n", err, strerror(errno));
		return 14;
	}else{
		printf("%s could open for read and wirte in 2 diffrent fd's\n", ok);
	}

	char shitload_of_c[1025];
	memset(shitload_of_c, 'c', 1024);
	shitload_of_c[1024] = '\0';
	errno = 0;
	got_bytes = write(fd_open_1, shitload_of_c, 1024);
	if(got_bytes != 1024){
		printf("%s wrote 1024 'c' to %s (actually: %zd) %s\n", err, files[0], got_bytes, strerror(errno));
		return 15;
	}else{
		printf("%s overwrite with 1024 c's worked\n", ok);
	}

	if(close(fd_open_1) != 0){
		printf("%s could not close fd1 properly %s\n", err, strerror(errno));
		return 16;
	}
	if(close(fd_open_2) != 0){
		printf("%s could not close fd2 properly %s\n", err, strerror(errno));
		return 17;
	}
	char shitload_compare[1025];
	shitload_compare[1024] = '\0';

	fd_open_1 = open(files[0], O_RDONLY|O_NONBLOCK|O_NDELAY);
	if(fd_open_1 <= 0){
		printf("%s could not open fd1 for read %s\n", err, strerror(errno));
		return 18;
	}
	got_bytes = read(fd_open_1, shitload_compare, 1024);
	if(got_bytes != 1024){
		printf("%s did not get the requested 1024 byte from %s (%s)\n", err, files[0], strerror(errno));
		return 19;
	}
	if(memcmp(shitload_of_c, shitload_compare, 1024) != 0){
		printf("%s 1024 c's did not match file %s\n", err, files[0]);
		return 20;
	}else{
		printf("%s 1024 c's confirmed\n", ok);
	}

	if(close(fd_open_1) != 0){
		printf("%s could not close fd %s\n", err, strerror(errno));
		return 21;
	}
	fd_open_1 = open(files[0], O_RDONLY|O_NONBLOCK|O_NDELAY);
	if(fd_open_1 <= 0){
		printf("%s could not open fd1 for read %s\n", err, strerror(errno));
		return 22;
	}
	got_bytes = read(fd_open_1, shitload_compare, 0);
	if(got_bytes != 0){
		printf("%s requested 0 bytes from %s (should be 0 accorunding to man actually: %zd) %s\n", err, files[0], got_bytes, strerror(errno));	
		return 23;
	}else{
		printf("%s behaviour for reading 0 bytes ok\n", ok);
	}
	got_bytes = read(fd_open_1, NULL, 1024);
	if(got_bytes > 0){
		printf("%s requested 1024 bytes from %s to NULL (got: %zd) %s\n", err, files[0], got_bytes, strerror(errno));	
		return 24;
	}else{
		printf("%s reading into NULL buffer\n", ok);
	}
	errno = 0;

	if(close(fd_open_1) != 0){
		printf("%s could not close fd %s\n", err, strerror(errno));
		return 25;
	}
	fd_open_1 = open(files[0], O_WRONLY|O_NONBLOCK|O_NDELAY);
	if(fd_open_1 <= 0){
		printf("%s could not open fd1 for read %s\n", err, strerror(errno));
		return 26;
	}

	got_bytes = write(fd_open_1, NULL, 1024);
	if(got_bytes > 0){
		printf("%s write 1024 bytes from %s to NULL (got: %zd) %s\n", err, files[0], got_bytes, strerror(errno));	
		return 27;
	}else{
		printf("%s behaviour for write from NULL buffer ok\n", ok);
	}
	errno = 0;

	got_bytes = write(fd_open_1, shitload_of_c, 13374223);
	if(got_bytes > 0 && got_bytes != 1024){
		printf("%s write 13374223 bytes to %s from 1k buffer (got: %zd) %s\n", err, files[0], got_bytes, strerror(errno));
		return 28;
	}else{
		printf("%s handled writing of 13374223 bytes correct\n", ok);
	}
	errno = 0;

	if(close(fd_open_1) != 0){
		printf("%s could not close fd %s\n", err, strerror(errno));
		return 29;
	}
	errno = 0;
	fd_open_1 = open(files[4], O_RDWR|O_NONBLOCK|O_NDELAY);
	if(fd_open_1 <= 0){
		printf("%s could not open fd1 for read for %s %s\n", err, files[4], strerror(errno));
		return 30;
	}
	errno = 0;
	got_bytes = read(fd_open_1, shitload_of_c, 1024);
	if(got_bytes > 0){
		printf("%s read 1024 bytes from %s (not written to it yet) (got: %zd) %s\n", err, files[0], got_bytes, strerror(errno));	
		errno = 0;
	}	

	close(fd_open_1);
	if(fd_open_1 <= 0){
		printf("%s could not open fd1 for read for %s %s\n", err, files[4], strerror(errno));
		return 31;
	}

	int fd_arr[10];
	int fd_arr_len = 10;

	for(i = 0; i < fd_arr_len; ++i){
		fd_arr[i] = open(files[0], O_RDONLY|O_NONBLOCK|O_NDELAY);
		if(fd_arr[i] == -1){
			error = 32;
			break;
		}
	}
	if(error){
		printf("%s could not open fd#%i for read for %s\n", err,i, strerror(errno));
		return error;
	}
	int res = ioctl(fd_arr[0], IOC_OPENREADCNT);
	if( res != fd_arr_len ){
		printf("%s ioctl read-count: open-fd-count did not match, should be %d but was %d\n", err, fd_arr_len, res);
		return 33;
	}else{
		printf("%s ioctl read-count was ok\n", ok);
	}

	for(i = 1; i < fd_arr_len; ++i){
		if(close(fd_arr[i]) < 0){
			error = 34;
			break;
		}
		fd_arr[i] = 0;
	}
	if(error){
		printf("%s could not close fd\n", err);
		return error;
	}

	res = ioctl(fd_arr[0], IOC_OPENREADCNT);
	if(res != 1){
		printf("%s closed all but one fd, fd count was %d instead of 1\n", err, res);
		return 35;
	}else{
		printf("%s ioctl: reader-count correct after closing some\n", ok);
	}

	fd_arr[1] = open(files[0], O_RDWR|O_NONBLOCK|O_NDELAY);
	res = ioctl(fd_arr[0], IOC_OPENREADCNT);
	if(res != 2){
		printf("%s opened file for read/write, read count was: %d should be 2\n", err, res);
		return 36;
	}else{
		printf("%s ioctl: reader-count correct after opening read/write\n", ok);
	}
	res = ioctl(fd_arr[0], IOC_OPENWRITECNT);
	if(res != 1){
		printf("%s ioctl: writer-count should be 1 but was %d\n", err, res);
		return 37;
	}else{
		if(close(fd_arr[1]) < 0){
			printf("%s close failed %s\n", err, strerror(errno));
			return 38;
		}
		fd_arr[1] = 0;
		res = ioctl(fd_arr[0], IOC_OPENWRITECNT);
		if(res != 0){
			printf("%s ioctl: closed writer, but writercout was %d\n", err, res);
			return 39;
		}
		printf("%s ioctl: writer-count was correct\n", ok);
	}

	memset(shitload_compare, '\0', 1025);
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if(res != 0){
		printf("%s ioctl: clear should return 0 according to manpage, result was %d\n", err, res);
		return 40;
	}else{
		printf("%s ioctl: clear return value was according to manpage\n", ok);
	}

	got_bytes = read(fd_arr[0], shitload_compare, 1024);
	if(got_bytes > 0){
		printf("%s ioctl: issued clear, but was able to read %zd bytes\n", err, got_bytes);
		return 41;
	}else{
		if(strlen(shitload_compare) != 0){
			printf("%s ioctl: after clear, return value was correct, but buffer seems to be %zd bytes large\n", err, strlen(shitload_compare));
			return 42;
		}
		printf("%s ioctl: after clear returnvalue of read correct\n", ok);
	}

	fd_arr[1] = open(files[0], O_RDWR|O_NONBLOCK|O_NDELAY);
	res = ioctl(fd_arr[0], IOC_OPENWRITECNT);
	if(fd_arr[1] < 0){
		printf("%s open read/write failed %s\n", err, strerror(errno));
		return 43;
	}
	if(res != 1){
		printf("%s ioctl: got wrong writer-count was %d\n", err, res);
		return 44;
	}
	char* umpalumpa = "umpalumpa";
	got_bytes = write(fd_arr[1], umpalumpa, strlen(umpalumpa));
	if( got_bytes != strlen(umpalumpa) ){
		printf("%s could not write '%s', only wrote %zd\n", err, umpalumpa, got_bytes);
		return 45;
	}
	got_bytes = read(fd_arr[0], shitload_compare, 5);
	if(got_bytes != 5){
		printf("%s could not read requested bytes, got %zd of 5\n", err, got_bytes);
		return 46;
	}
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if(res != 0){
		printf("%s return of clear was not conforming to manpage was %d\n", err, res);
		return 47;
	}
	got_bytes = read(fd_arr[0], shitload_compare+5, 1);
	if(got_bytes > 0){
		printf("%s requested 1 byte after clear, got %zd istead of 0\n", err, got_bytes);
		return 48;
	}
	if (close(fd_arr[1]) <  0) {
		printf("%s could not close fd %s\n", err , strerror(errno));
		return 49;
	}
	fd_arr[1] = open(files[0], O_RDWR|O_NONBLOCK|O_NDELAY);
	if(fd_arr[1] < 0){
		printf("%s could not open fd %s\n", err, strerror(errno));
		return 50;
	}
	got_bytes = write(fd_arr[1], umpalumpa, strlen(umpalumpa));
	if( got_bytes != strlen(umpalumpa) ){
		printf("%s could not write '%s', only wrote %zd\n", err, umpalumpa, got_bytes);
		return 51;
	}
	got_bytes = read(fd_arr[0], shitload_compare+5, 10);
	if(got_bytes+5 != strlen(umpalumpa)){
		printf("%s got wrong amount of bytes, wanted %zd, but only got %zd %s\n", err, strlen(umpalumpa), got_bytes+5, strerror(errno));
		return 52;
	}
	if(strcmp(shitload_compare, umpalumpa) > 0){
		printf("%s did not read '%s' got '%s' instead\n", err, umpalumpa, shitload_compare);
		return 53;
	}else{
		printf("%s continue reading after clear and write worked\n", ok);
	}
	if(close(fd_arr[0]) < 0){
		printf("%s could not close read-fd %s\n", err, strerror(errno));
		return 54;
	}else
		fd_arr[0] = 0;
	if(close(fd_arr[1]) < 0){
		printf("%s could not close write-fd %s\n", err, strerror(errno));
		return 55;
	}

	printf("\n");
	fd_arr[0] = open(files[0], O_RDONLY);
	if(fd_arr[0] < 0){
		printf("%s could not opening file blocking %s\n", err, strerror(errno));
		return 56;
	}
	fd_arr[1] = open(files[0], O_WRONLY | O_NONBLOCK);
	if(fd_arr[1] < 0){
		printf("%s could not opening file %s\n", err, strerror(errno));
		return 57;
	}
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if(res != 0){
		printf("%s could not reset device. got %d %s\n", err, res, strerror(errno));
		return 58;
	}
	memset(shitload_compare, '\0', 1025);
	ssize_t read_bytes;
	pthread_mutex_lock(&fastmutex);
	got_bytes = 0;
	pthread_mutex_unlock(&fastmutex);
	pthread_t thread_id;
	int errornumber_thread = pthread_create(&thread_id, NULL, write_abc, (void*) fd_arr[1]);
	read_bytes = read(fd_arr[0], shitload_compare, 3);

	pthread_mutex_lock(&fastmutex);
	if(got_bytes == 0){
		printf("%s read finished before bytes got written!\n", err);
		return 60;
	}
	if(read_bytes != 3){
		printf("%s got wrong amount of bytes %zd instead of 3\n", err, read_bytes);
		return 61;
	}else{
		printf("%s read wokeup correctly\n", ok);
	}
	pthread_mutex_unlock(&fastmutex);
	got_bytes = 0;
	errornumber_thread = pthread_create(&thread_id, NULL, write_abcabc, (void*) fd_arr[1]);
	read_bytes = read(fd_arr[0], shitload_compare, 6);
	sleep(0.1);

	pthread_mutex_lock(&fastmutex);
	if(got_bytes < 6){
		printf("%s read finished before bytes (written %zd!)\n", err, got_bytes);
		return 62;
	}
	if(read_bytes != 6){
		printf("%s got wrong amount of bytes %zd instead of 6\n", err, read_bytes);
		return 63;
	}else{
		printf("%s read handled with 2 small writes correct\n", ok);
	}
	pthread_mutex_unlock(&fastmutex);

	if(close(fd_arr[1]) < 0){
		printf("%s could not close nonblocking fd %s\n", err, strerror(errno));
		return 64;
	}

	fd_arr[1] = open(files[0], O_WRONLY);
	if(fd_arr[1] < 0){
		printf("%s could not open fd for writing blocking %s\n", err, strerror(errno));
		 return 65;
	}

	memset(shitload_of_c, 'c', 1024);

	res = ioctl(fd_arr[1], IOC_CLEAR);
	if(res != 0){
		printf("%s could not clear buffer %d\n", err, res);
		return 66;
	}
	char* yay = "yay";
	got_bytes = write(fd_arr[1], yay, 3);
	if(got_bytes != 3){
		printf("%s could not write 3 bytes got %zd\n", err, got_bytes);
		return 67;
	}

	resetted = 0;
	errornumber_thread = pthread_create(&thread_id, NULL, write_reset_dev, (void*) fd_arr[1]);
	got_bytes += write(fd_arr[1], shitload_of_c, 1024);
	pthread_mutex_lock(&fastmutex);
	if(resetted != 1){
		printf("%s write was not blocked! wrote %zd/1027 bytes\n", err, got_bytes);
		return 69;
	}
	pthread_mutex_unlock(&fastmutex);
	if(got_bytes != 1027){
		printf("%s could not write all bytes %zd/1027 %s\n", err, got_bytes, strerror(errno));
		return 70;
	}else{
		printf("%s write was blocked corectly\n", ok);
	}
	close(fd_arr[1]);
	close(fd_arr[0]);
	fd_arr[0]=open(files[0],O_RDONLY);
	if(fd_arr[0]<0)
	{
		printf("fehler in open\n");
		return 71;
	}
	memset(shitload_compare, '\0', 1025);
	read_bytes=read(fd_arr[0],shitload_compare,3);
	if(read_bytes != 3){
		printf("%s got wrong amount of bytes %zd instead of 6\n", err, read_bytes);
		return 72;
	}else{
		printf("%s wanted 3 c got %s\n",ok,shitload_compare);
	}
	close(fd_arr[0]);
	ssize_t write_bytes;
	
	fd_arr[1]=open(files[4],O_WRONLY);
	fd_arr[0]=open(files[4],O_RDONLY);

	if(fd_arr[1]<0)
	{
		printf("%s fehler in open write %s\n", err, strerror(errno));
		return 73;
	}
	if(fd_arr[0]<0)
	{
		printf("%s fehler in open read %s\n", err, strerror(errno));
		return 74;
	}
	memset(shitload_compare, '\0', 1025);
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if(res != 0){
		printf("%s ioctl: clear should return 0 according to manpage, result was %d\n", err, res);
		return 75;
	}else{
		printf("%s ioctl: clear return value was according to manpage\n", ok);
	}
	res = ioctl(fd_arr[1], IOC_CLEAR);
	if(res != 0){
		printf("%s ioctl: clear should return 0 according to manpage, result was %d\n", err, res);
		return 76;
	}else{
		printf("%s ioctl: clear return value was according to manpage\n", ok);
	}
	for(i=0; i<100;i++)
	{
		shitload_compare[i]='a';
	}
	write_bytes=write(fd_arr[1],shitload_compare,100);
	write_bytes=write(fd_arr[1],"blah",4);

	int sleep_time = 2;
	switch(fork())
	{
		case 0:
			sleep(sleep_time);
			write_bytes=write(fd_arr[1],"blubb",5);
			exit(1);
			break;

		default:
			read_bytes=read(fd_arr[0],new_buff,109);
			break;
	}
	if( read_bytes != 109 ) {
		printf("%s readbytes should be 109 %d\n",err, read_bytes);
		return 76;
	}else{
		printf("%s readbytes was ok\n", ok);
	}

	printf("\n\n\e[1m\e[33mYOUR PROC OUTPUT:\e[0m\n");
	system("cat /proc/is*/info");

	printf("\n\e[1m\e[33mSHOULD BE (please compare it yourself):\e[0m\n");
	printf("DEVICE-NAME ╻ CURRENT LENGTH ╻ READ ╻ WRITE ╻ OPEN ╻ CLOSE ╻ CLEAR\n━━━━━━━━━━━━╇━━━━━━━━━━━━━━━━╇━━━━━━╇━━━━━━━╇━━━━━━╇━━━━━━━╇━━━━━━\nmydev0      │              3 │  210 │   160 │   28 │    26 │     5\nmydev1      │            100 │  200 │   150 │    5 │     4 │     0\nmydev2      │            100 │  200 │   150 │    5 │     4 │     0\nmydev3      │            100 │  200 │   150 │    5 │     4 │     0\nmydev4      │            109 │    2 │     3 │    3 │     1 │     2\n\n");
	printf("\e[1minfo: dev1 write should be at least 1 sec; dev4 read time should be at least %d sec\n", sleep_time);
	printf("\e[2mnote: 2 fd's of dev4 are currently open!\e[0m\n\n");
	
	res = ioctl(fd_arr[0], IOC_SET_READERS, 3);
	if(res != 0){
		printf("%s ioctl: setting new max amount of readers should return old value, should be 0, was %d\n", err, res);
		return 77;
	}else{
		printf("%s ioctl: setting new max returned correct value\n", ok);
	}
	res = ioctl(fd_arr[0], IOC_SET_READERS, 5);
	if(res != 3){
		printf("%s ioctl: setting new max amount of readers should return old value, should be 3, was %d\n", err, res);
		return 78;
	}
	// arr[0] = R
	// arr[1] = W
	//open(files[4],O_RDONLY);
	for(i = 2; i < 6; ++i){
		fd_arr[i] = open(files[4],O_RDONLY);
		if(fd_arr[i] < 0){
			printf("%s could not open reader %d, limit sould be 5 (%s)\n", err, i, strerror(errno));
			return 79;
		}
	}
	for(i=7;i < 10; ++i){
		fd_arr[i] = open(files[4],O_RDONLY);
		if(fd_arr[i] >= 0){
			printf("%s should not be able to create another reader!\n");
			return 80;
		}
	}
	printf("%s max-reader count worked\n", ok);

	// arr[0] = R
	// arr[1] = W
	// arr[2 - 6]=  R
	// arr[7 - 9]= -1
	got_bytes = read(fd_arr[0], shitload_compare, 0);
	if(got_bytes < 0){
		printf("%s could not read from fd 0 (%s)\n", err, got_bytes);
		return 81;
	}
	for(i = 0; i < 7; ++i){
		if(close(fd_arr[i])< 0){
			printf("%s could not close fd corect (%s)\n", err, strerror(errno));
			return 82;
		}
	}
	for(i = 0; i < 10; ++i){
		fd_arr[i] = open(files[0],O_RDONLY);
		if(fd_arr[i] < 0){
			printf("%s could not open fd on dev0 (limit is on dev4) [%s]\n", err, strerror(errno));
			return 83;
		}
		if(close(fd_arr[i]) < 0){
			printf("%s could not close fd on dev0 [%s]\n", err, strerror(errno));
			return 84;
		}
	}
	for(i = 5; i < 5; ++i){
		fd_arr[i] = open(files[4], O_RDONLY);
		if(fd_arr[i] < 0){
			printf("%s could not open fd on dev0 [%s]\n", err, strerror(errno));
			return 85;
		}
		got_bytes = read(fd_arr[i], shitload_compare, 1);
		if(got_bytes != 1){
			printf("%s could not read from fd %d [%s]\n", err, i, strerror(errno));
			return 86;
		}
		if(close(fd_arr[i])<0){
			printf("%s could not close fd corectly [%s]\n", err, strerror(errno));
			return 87;
		}
	}
	printf("%s could read from all open fd's\n", ok);
	
	fd_arr[0] =  open(files[0], O_RDONLY);
	res = 0;
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if( res != 0 ){
		printf("%s could not clear dev0 got %d istead of 0\n", err, res);
		return 88;
	}
	res = ioctl(fd_arr[0], IOC_CLEAR);
	if( res != 0 ){
		printf("%s could not clear dev0 got %d istead of 0\n", err, res);
		return 89;
	}
	printf("%s your programm survied a doble-clear\n", ok);
	return 0;
}

