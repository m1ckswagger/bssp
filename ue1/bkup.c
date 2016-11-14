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

void backup_node(const char *src_name, const struct stat *target_inode, int target_fd);
int process_dir(const char *work, const struct stat *target_inode, int tfd);
int write_inode_and_name(const struct stat *src_inode, int tfd, const char *src_file_name);
int write_content(const char *src_file_name, int tfd);
int create_signature(int target_fd);

int main(int argc, char **argv)
{
	// Archivnamen aus Environmentvariable "BACKUPTARGET" holen
	char *target = getenv("BACKUPTARGET");
	int target_fd; // fd = file descriptor
	struct stat target_inode;

	if (!target)
		fprintf(stderr, "BACKUPTARGET Environment missing\n"), exit(2);

	// Archiv oeffnen (creat)
	target_fd = creat(target, 0660);
	create_signature(target_fd);
	if (target_fd == -1) {
		fprintf(stderr, "Can't create archiv file\n");
		exit(3);
	}

	// Inode des Archivs lesen (zum Uebergeben fuer Ueberpruefungen)
	if (fstat(target_fd, &target_inode) == -1) {
		//fprintf(stderr, "Cannot stat Backup Target %s\n", target);
		perror(target);
		exit(4);
	}
	
	// Parametercheck
	if (argc == 1) {
		backup_node(".", &target_inode, target_fd);
	}
   // Abarbeiten dir uebergebenen parameter
	else {
		int i;
		for (i = 1; i < argc; i++) {
			backup_node(argv[i], &target_inode, target_fd);
		}
	}
	close(target_fd);
	return 0;
}

int create_signature(int target_fd) {
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

	if(write(target_fd, lastname, sizeof(lastname)) != strlen(lastname)) {
		return -1;
	}
	if(write(target_fd, &bday, 4) != 4) {
		printf("error!\n");
		return -1;
	}
	//printf("Bytes written: %d\nBirthday: %0x\n", wbytes, bday);
	return 0;
}


void backup_node(const char *src_name, const struct stat *target_inode, int target_fd)
{
	struct stat src_inode;
	if (lstat(src_name, &src_inode) == -1) {
		perror(src_name);
		return;
	}

	// ueberpruefen, ob es sich um das archiv file selbst handelt
	if (src_inode.st_dev == target_inode->st_dev &&
			src_inode.st_ino == target_inode->st_ino) {
		printf("Ignore archivfile!\n");
		return;
	}
   
   // ignorieren falls es sich um sym-Link handelt
	if (S_ISLNK(src_inode.st_mode)) {
		printf("%s: Sym links currently not supported!!!\n", src_name);
	}
   // Verarbeitung eines Verzeichnisses
	else if (S_ISDIR(src_inode.st_mode)) {
		printf("Archiv dir: %s\n", src_name); 
		if (write_inode_and_name(&src_inode, target_fd, src_name) == -1) {
			fprintf(stderr, "%s: Can't write inode and name\n", src_name);
			return;
		}
		else {
			process_dir(src_name, target_inode, target_fd);
		}
	}
   // Verarbeitung eines regulaeren Files
	else if (S_ISREG(src_inode.st_mode)) {
		printf("Archiv reg. file: %s\n", src_name); 
		if (write_inode_and_name(&src_inode, target_fd, src_name) == -1) {
			fprintf(stderr, "%s: Can't write inode and name\n", src_name);
		}
		else if (write_content(src_name, target_fd) == -1) {
			fprintf(stderr, "%s: Can't write content\n", src_name);
		}
	}
	else {
		printf("%s: Special inode are currently not supported!\n", src_name);
	}
}

int process_dir(const char *work, const struct stat *target_inode, int tfd)
{
	DIR *dir;
	struct dirent *dentry;
	char src_file_name[MAXPATHNAME];
	//struct stat src_inode;

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
			strcpy(src_file_name,work);
			strcat(src_file_name, "/");
			strcat(src_file_name, dentry->d_name);
			
			backup_node(src_file_name, target_inode, tfd);
		}
	}
	return 0;
}

int write_inode_and_name(const struct stat *src_inode, int tfd, const char *src_file_name)
{
	// inode schreiben
	if (write(tfd, src_inode, sizeof(struct stat)) != sizeof(struct stat)) {
		return -1;
	}
	// schreibt filenamen
	if (write(tfd, src_file_name, strlen(src_file_name) + 1) != strlen(src_file_name) + 1) {
		return -1;
	}
	return 0;
}

int write_content(const char *src_file_name, int tfd)
{
	int fd;
	char buf[BUFSZ];
	ssize_t rdbytes;

	fd = open(src_file_name, O_RDONLY);
	if (fd == -1) {
		perror(src_file_name);
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

