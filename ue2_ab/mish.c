// Meine eigene Shell

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define PATH_MAX 1024
#define MAXCMDLINE 2048
#define MAXARGS 200

int handle_lastarg(char *lastarg);
void cmd_umask(const unsigned argc, char **argvec);
void cmd_setpath(const unsigned argc, char **argvec);
void cmd_info(time_t starttime);
void cmd_cd(const unsigned argc, char **argvec, char *workdir);  
void signal_handler(int sig); 
void clean_up_childprocess(); 
void reset_signal();

int main()
{
	char cmdline[MAXCMDLINE];
	char * argvec[MAXARGS];
	int i, pid;
	int status;
	int bg;							// Legt fest, ob es sich um einen Hintergrundprozess handelt	
	struct utsname promptname;
	char *workdir;
	char *lastarg;
	time_t start;
	
	time(&start);
	if(uname(&promptname) == -1)
		exit(1);
	workdir = getcwd(NULL, PATH_MAX);

	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
		
	for(;;)
	{
		// Eingabeprompt
		printf("%s:%s $ ", promptname.nodename, workdir);
		
		// Einlesen von stdin
		if (fgets(cmdline, MAXCMDLINE, stdin)) {
			if(strlen(cmdline) > 1)
				cmdline[strlen(cmdline)-1] = '\0';		// entfernt das \n am Ende der Eingabe
		}

		if (!strcmp(cmdline, "\n" )) {
			continue;
		}

		// Commandozeile in Worte Zerlegen und expandieren
		for(i = 0, argvec[i] = strtok(cmdline, " \t"); argvec[i] != NULL; argvec[++i]=strtok(NULL, " \t"))
			;
		
		// Hintergrundprozess?
		lastarg = argvec[i-1];
		bg = handle_lastarg(lastarg);				

		// interne Kommandos behandeln und durchfueheren
		if( !strcmp(argvec[0], "exit"))
		{
			printf("Bye bye ...!\n");
			exit(0);
		}
		/*
		Weitere shellinterne Kommandos
		*/
		if( !strcmp(argvec[0], "cd")) {
			cmd_cd(i, argvec, workdir);
			continue;	
		}
		if( !strcmp(argvec[0], "umask")) {
			cmd_umask(i, argvec);
			continue;
		}		
		if( !strcmp(argvec[0], "setpath")) {
			cmd_setpath(i, argvec);
			continue;
		}
		if( !strcmp(argvec[0], "info")) {
			cmd_info(start);
			continue;
		}
	

		// externe Kommandos	
		switch(pid = fork())
		{
		case -1: perror("fork");
			break;
		case 0:
			reset_signal();
			execvp(argvec[0], argvec);
			perror(argvec[0]);
			exit(1);
		default:
			//printf("Ich bin die Mish und hab einen Prozess zum Arbeiten abgesetzt.\n Der macht %s und hat die PID %d\n", argvec[0], pid);
			
			if(!bg) 
			{
				signal(SIGCHLD, SIG_DFL);	
				waitpid(pid, &status,0);
			}
			else 
			{
				signal(SIGCHLD, clean_up_childprocess);
				printf("Running %s in background [%d]\n", argvec[0], pid);
			}
		}
	}
	return 0;
}


// Return 1 wenn Hintergrundprozess, sonst 0
int handle_lastarg(char *lastarg) {
	if(!strcmp(lastarg, "&")) {
		lastarg = 0;
		return 1;
	}
	if(lastarg[strlen(lastarg)-1] == '&') {
		lastarg[strlen(lastarg)-1] = '\0';
		return 1;
	}
	return 0;
}

void cmd_umask(const unsigned argc, char **argvec) {
	int umask_val;
	mode_t new_umask;

	if(argc > 1) {
		umask_val = strtol(argvec[1], NULL, 8);
		umask(umask_val);
		new_umask = umask(0);
	}
	else {
		new_umask = umask(0);
	}
	printf("%04o\n", new_umask);		
	umask(new_umask);
}

void cmd_setpath(const unsigned argc, char **argvec) {
	char *path;

	if(argc > 1) {
		path = getenv("PATH");
		if(path != NULL) {
			strcat(path, ":");
			strcat(path, argvec[1]);
			if(setenv("PATH", path, 1) == -1) {
				perror("setenv");
			}
		}
		else {
			if(setenv("PATH", argvec[1], 1) == -1) {
				perror("setevn");
			}
		}
	}
	else {
		printf("%s\n", getenv("PATH"));
	}
}

void cmd_info(time_t starttime) {
	struct tm *info;
	info = localtime(&starttime);

	printf("Shell von  : Berger is151002\n");
	printf("PID        : %d\n", getpid());
	printf("Laeuft seit: %s\n", asctime(info)); 			
}

void cmd_cd(const unsigned argc, char **argvec, char *workdir) {
	if(argc > 1) {
		if(chdir(argvec[1]) == -1) {
			perror("cd");
		}
	}
	else {
		if(chdir(getenv("HOME")) == -1) {
			perror("cd");
		}
	}
	getcwd(workdir, PATH_MAX);
}  

void signal_handler(int sig) {
	switch(sig) {
		case SIGINT:
		case SIGQUIT:
			signal(sig, SIG_IGN);
			break;
		default:
			return;
	}
}

void clean_up_childprocess() {
	int status;
	int pid;
	
	while ((pid = waitpid(-1, &status, 0)) != -1)
		;
	printf("\n Prozess %d hat sich mit Status %d beendet!\n", pid, status);	
}

void reset_signal() {
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
}
