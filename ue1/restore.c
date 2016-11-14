#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define BUFSZ 1024
#define MAXPATHNAME 1024
#define MAXNAME 25

// Prototypes
int read_inode_and_name(struct stat *inode, int sfd, char *src_file_name);
int restore_file(struct stat *node, const char *name, int fd);
int process_dir(const char *work, const struct stat *src_inode, int sfd);
int read_signature(int fd);
int birthday_matches(struct tm *birthday);


int main(int argc, char **argv) {
	char *src_file_path;			// file path of source archive
	char src_file_name[BUFSZ];	// file name
	int src_fd;					// file descriptor of source archive
	int sig_code;				// code returned from signature
	struct stat inode;
	if(argc != 2) {
		printf("Wrong arguments!\n");
	}
   src_file_path = argv[1];

	// Lesen des uebergebenen archivs
   src_fd = open(src_file_path, O_RDWR);
	
	// ueberpruefen der signatur

	if((sig_code = read_signature(src_fd)) == -1) {
		fprintf(stderr, "Error reading signature!\n");
		return -1;
	}
	else if(!sig_code) {
		fprintf(stderr, "Data not matching!\n");
	}


	// Abarbeiten aller Inodes im archiv
	while(read_inode_and_name(&inode, src_fd, src_file_name) != -1) {
		if(S_ISDIR(inode.st_mode)) {
			printf("Restoreing directory: %s \n", src_file_name);
			process_dir(src_file_name, &inode, src_fd);
		}
		// Wiederherstellung eines files
		else if(S_ISREG(inode.st_mode)) {
			if(inode.st_nlink > 1) {
				printf("%s: File has multiple hardlinks. Files will be seperately restored\n", src_file_name);
			}
			if(restore_file(&inode, src_file_name, src_fd) != -1) 
				printf("%s: File restored.\n", src_file_name);
		}
		else {
			printf("%s: File type not supported!\n", src_file_name);
		}
	}
	close(src_fd);
   return 0;
}

int read_signature(int fd) {
	char buf;
	char name[7];
	int i;
	time_t bday;
	struct tm *birthday;
	size_t rdbytes;

	for(i = 0; i < 6; i++) {
		if(read(fd, &buf, 1) == -1) {
			return -1;
		}
		if(buf-'0' < 0 && buf-'0' > 9) {
			lseek(fd, -1, SEEK_CUR);
			break;
		}
		name[i] = buf;
	}
	name[6] = '\0';

	if((rdbytes = read(fd, &bday, 4)) != 4) {
		fprintf(stderr, "Error reading signature");
		return -1;
	}

	birthday = localtime(&bday);
	printf("Name: %s\n", name);
	printf("Geb.: %s\n", asctime(localtime(&bday)));

	
// && birthday_matches(birthday));
	return (!strcmp("Berger", name)); 
}

int birthday_matches(struct tm *birthday) {
	if(birthday->tm_mday != 21)
		return 0;
	if(birthday->tm_mon != 11)
		return 0;
	if(birthday->tm_year != (1995-1900))
		return 0;
	if(birthday->tm_hour != 15)
		return 0;
	if(birthday->tm_min != 17)
		return 0;
	return 1;
}

// Wiederherstellung eines files aus dem Archiv
int restore_file(struct stat *node, const char *name, int fd) {
	int refd;		// file descriptor for restored file
   char content[node->st_size];
	int rdbytes;
	

	printf("RESTORING %s with size %ld\n", name, node->st_size);
	// Auslesen des Contents
	if((rdbytes = read(fd, content, node->st_size)) != node->st_size) {
		return -1;
	}
	// erzeugen des restorten file 
	refd = creat(name, node->st_mode);
	if(write(refd, content, node->st_size) == -1) {
		return -1;
	}
	close(refd);
	return 1;
}


// Auslesen von inode und name aus dem Archiv
int read_inode_and_name(struct stat *inode, int sfd, char *src_file_name) {
	char buf[BUFSZ];	
	int slen, rdbytes;
   if(read(sfd, inode, sizeof(struct stat)) != sizeof(struct stat)) {
		return -1;
   }
   
	rdbytes = read(sfd, buf, BUFSZ);
	// feststellen der string laenge
	for(slen = 0; buf[slen] != '\0'; slen++);
   strncpy(src_file_name, buf, slen+1);	
	
	// zuruecksetzen des FD
	lseek(sfd, rdbytes*-1, SEEK_CUR);
	lseek(sfd, slen+1, SEEK_CUR); 
   return 1;
}

int process_dir(const char *work, const struct stat *src_inode, int sfd)
{
  char src_file_name[MAXPATHNAME];
	char new_file_name[MAXPATHNAME];
	struct stat new_node;
	int i = 0;
	
	if (mkdir(work, src_inode->st_mode) == -1) {
		fprintf(stderr, "%s: Cannot create dir!\n", work);
		perror("mkdir");
		return -1;
	}
	
	while (read_inode_and_name(&new_node, sfd, new_file_name) != -1) {
		if (S_ISDIR(new_node.st_mode)) {
			printf("RESTORING %s with size %ld\n", new_file_name, new_node.st_size);	
			process_dir(new_file_name, &new_node, sfd);
		}
		else if(S_ISREG(new_node.st_mode)) {
			restore_file(&new_node, new_file_name, sfd);
		}
	}
   return 0;
}
