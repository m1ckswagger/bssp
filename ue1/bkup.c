#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#define BUFSZ 1024
#define MAXPATHNAME 1024

void BackupNode(const char *srcName, const struct stat *targetInode, int targetfd);
int process_dir(const char *work, const struct stat *target_inode, int tfd);
int write_inode_and_name(const struct stat *srcInode, int tfd, const char *srcFName);
int write_content(const char *srcFName, int tfd);
int create_signature(int targetfd);

int main(int argc, char **argv)
{
	// Archivnamen aus Environmentvariable "BACKUPTARGET" holen
	char *target = getenv("BACKUPTARGET");
	int targetfd; // fd = file descriptor
	struct stat target_inode;

	if (!target)
		fprintf(stderr, "BACKUPTARGET Environment missing\n"), exit(2);

	// Archiv oeffnen (creat)
	targetfd = creat(target, 0660);
	create_signature(targetfd);
	if (targetfd == -1) {
		fprintf(stderr, "Can't create archiv file\n");
		exit(3);
	}

	// Inode des Archivs lesen (zum Uebergeben fuer Ueberpruefungen)
	if (fstat(targetfd, &target_inode) == -1) {
		//fprintf(stderr, "Cannot stat Backup Target %s\n", target);
		perror(target);
		exit(4);
	}
	
	// Parametercheck
	if (argc==1) {
		BackupNode(".", &target_inode, targetfd);
	}
   // Abarbeiten dir uebergebenen parameter
	else {
		int i;
		for (i = 1; i < argc; i++) {
			BackupNode(argv[i], &target_inode, targetfd);
		}
	}
	close(targetfd);
	return 0;
}

int create_signature(int targetfd) {
	char lastname[] = {'B', 'e', 'r', 'g', 'e', 'r'};
	struct tm birthday;
	time_t bday;

	birthday.tm_year = 1995 - 1900;
	birthday.tm_mon = 12 - 1;
	birthday.tm_mday = 22;
	birthday.tm_hour = 15;
	birthday.tm_min = 17;
	birthday.tm_sec = 0;
	birthday.tm_isdst = -1;

	bday = mktime(&birthday);

	if(bday == -1)
		return -1;	

	if(write(targetfd, lastname, sizeof(lastname)) != strlen(lastname)) {
		return -1;
	}
	if(write(targetfd, &bday, 4) != 4) {
		printf("error!\n");
		return -1;
	}
	//printf("Bytes written: %d\nBirthday: %0x\n", wbytes, bday);
	return 0;
}


void BackupNode(const char *srcName, const struct stat *targetInode, int targetfd)
{
	struct stat srcInode;
	if (lstat(srcName, &srcInode) == -1) {
		perror(srcName);
		return;
	}

	// ueberpruefen, ob es sich um das archiv file selbst handelt
	if (srcInode.st_dev == targetInode->st_dev &&
			srcInode.st_ino == targetInode->st_ino) {
		printf("Ignore archivfile!\n");
		return;
	}
   
   // ignorieren falls es sich um sym-Link handelt
	if (S_ISLNK(srcInode.st_mode)) {
		printf("%s: Sym links currently not supported!!!\n", srcName);
	}
   // Verarbeitung eines Verzeichnisses
	else if (S_ISDIR(srcInode.st_mode)) {
		printf("Archiv dir: %s\n", srcName); 
		if (write_inode_and_name(&srcInode, targetfd, srcName) == -1) {
			fprintf(stderr, "%s: Can't write inode and name\n", srcName);
			return;
		}
		else {
			process_dir(srcName, targetInode, targetfd);
		}
	}
   // Verarbeitung eines regulaeren Files
	else if (S_ISREG(srcInode.st_mode)) {
		printf("Archiv reg. file: %s\n", srcName); 
		if (write_inode_and_name(&srcInode, targetfd, srcName) == -1) {
			fprintf(stderr, "%s: Can't write inode and name\n", srcName);
		}
		else if (write_content(srcName, targetfd) == -1) {
			fprintf(stderr, "%s: Can't write content\n", srcName);
		}
	}
	else {
		printf("%s: Special inode are currently not supported!\n", srcName);
	}
}

int process_dir(const char *work, const struct stat *target_inode, int tfd)
{
	DIR *dir;
	struct dirent *dentry;
	char srcFName[MAXPATHNAME];
	//struct stat srcInode;

	dir = opendir(work);
	if (dir == NULL) {
		fprintf(stderr, "Can't open dir\n");
		return -1;
	}
	
	while ((dentry = readdir(dir)) != NULL) {
		if(!strcmp(dentry->d_name, ".")) {
			//printf("Skip current dir\n");
		}
		else if (!strcmp(dentry->d_name,"..")) {
			//printf("Skip parent dir\n");
		}
		else {
			// TODO laengencheck
			if((strlen(work) + strlen(dentry->d_name) + 1) > MAXPATHNAME) {
				fprintf(stderr, "Path too long!\n");
				return -1;
			}
			strcpy(srcFName,work);
			strcat(srcFName, "/");
			strcat(srcFName, dentry->d_name);
			
			BackupNode(srcFName, target_inode, tfd);
		}
	}
	return 0;
}

int write_inode_and_name(const struct stat *srcInode, int tfd, const char *srcFName)
{
	// inode schreiben
	if (write(tfd, srcInode, sizeof(struct stat)) != sizeof(struct stat)) {
		return -1;
	}
	// schreibt filenamen
	if (write(tfd, srcFName, strlen(srcFName) + 1) != strlen(srcFName) + 1) {
		return -1;
	}
	return 0;
}

int write_content(const char *srcFName, int tfd)
{
	int fd;
	char buf[BUFSZ];
	ssize_t rdbytes;

	fd = open(srcFName, O_RDONLY);
	if (fd == -1) {
		perror(srcFName);
		return -1;
	}
	while ((rdbytes = read(fd, buf, BUFSZ)) > 0) {
		if (write(tfd, buf, rdbytes) != rdbytes) {
			perror("archivfile");
			return -1;
		}
	}
	close(fd);
	return 0;
}

