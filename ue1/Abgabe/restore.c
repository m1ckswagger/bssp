#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFSZ 1024

// Prototypes
int read_inode_and_name(struct stat *inode, int sfd, char *srcFName);
int restore_file(struct stat *node, const char *name, int fd);


int main(int argc, char **argv) {
	char *srcFPath;			// file path of source archive
   //char *srcFPathCpy;		 // copy of file path
	char srcFName[BUFSZ];	// file name
	int srcfd;					// file descriptor of source archive

	struct stat inode;

	if(argc != 2) {
		printf("//Wrong arguments!\n");
	}
   srcFPath = argv[1];

	// Lesen des uebergebenen archivs
   srcfd = open(srcFPath, O_RDWR);
	
	// Abarbeiten aller Inodes im archiv
	while(read_inode_and_name(&inode, srcfd, srcFName) != -1) {
		if(S_ISDIR(inode.st_mode)) {
			printf("%s: Support for Directories not yet implemented.\n", srcFName);
		}
		// Wiederherstellung eines files
		else if(S_ISREG(inode.st_mode)) {
			printf("Inode read: %s!\n", srcFName);
			printf("Size: %ld\n", inode.st_size);
			if(restore_file(&inode, srcFName, srcfd) != -1) 
				printf("%s: File recovered.\n", srcFName);
		}
		else {
			printf("%s: File type not supported!\n", srcFName);
		}
	}

   return 0;
}

// Wiederherstellung eines files aus dem Archiv
int restore_file(struct stat *node, const char *name, int fd) {
	int refd;		// file descriptor for restored file
   char content[node->st_size];
	int rdbytes;
	
	// Auslesen des Contents
	if((rdbytes = read(fd, content, node->st_size)) != node->st_size) {
		return -1;
	}
	// erzeugen des restorten file 
	refd = creat(name, node->st_mode);
	if(write(refd, content, node->st_size) == -1) {
		return -1;
	}

	return 1;
}


// Auslesen von inode und name aus dem Archiv
int read_inode_and_name(struct stat *inode, int sfd, char *srcFName) {
	char buf[BUFSZ];	
	int slen, rdbytes;
   if(read(sfd, inode, sizeof(struct stat)) != sizeof(struct stat)) {
		return -1;
   }
   
	rdbytes = read(sfd, buf, BUFSZ);
	// feststellen der string laenge
	for(slen = 0; buf[slen] != '\0'; slen++);
   strncpy(srcFName, buf, slen+1);	
	
	// zuruecksetzen des FD
	lseek(sfd, rdbytes*-1, SEEK_CUR);
	lseek(sfd, slen+1, SEEK_CUR); 
   return 1;
}
