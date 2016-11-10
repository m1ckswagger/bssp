// Meine Shell - is151002

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <signal.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>

#define MAXCMDLINE 2048
#define MAXPATHNAME 2048
#define MAXUSERNAME 1024
#define MAXARGS 200
#define BUFSZ 1024

void signal_handler(int sig);
void reset_signal();
void handle_umask(char **argvec, const unsigned argc);
void handle_setpath(char **argvec, const unsigned argc);
void handle_info(time_t start_time);
void clean_up_child_process();
void handle_cd(char **argvec, const unsigned argc, char *workdir); 
void show_prompt();
int show_prompt_socket(int fd, struct utsname uname_data, char *pwd);
void handle_getprot(char **argvec, int argc, char *log_path); 
void* handle_client(void* fd);
int read_input_s(char *cmdline, int fd);
void read_input(char *cmdline);
void get_commands(char *cmdline, char **argvec, int *argc);
int is_background(char **argvec, int argc);
void start_shellserver();
int handle_commands(char **argvec, int argc, time_t start_time, int background, int client_fd, char *pwd);


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main() {
  int sock, clientfd;
	int err;
	pthread_t thr_id;
  socklen_t s_addr_size;

	struct sockaddr s_addr;
  struct sockaddr_in ss_addr;

  ss_addr.sin_addr.s_addr = INADDR_ANY;
  ss_addr.sin_port = htons(6000);
  ss_addr.sin_family = AF_INET;

  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  if (bind(sock, (struct sockaddr*) &ss_addr, sizeof(ss_addr)) == -1) {
    perror("bind");
    exit(1);
  }

  if (listen(sock,6) == -1) {
    perror("listen");
    exit(1);
  }

  s_addr_size = sizeof(s_addr);
  for (;;) {
    if ((clientfd = accept(sock, &s_addr, &s_addr_size)) == -1) {
      perror("accept");
      exit(1);
    }
		//printf("FD is: %d\n", clientfd);	
		dup2( clientfd, STDOUT_FILENO );  
		dup2( clientfd, STDERR_FILENO );
		dup2( clientfd, STDIN_FILENO );
		err=pthread_create(&thr_id, NULL, handle_client, (void *)clientfd);
		if(err)
			printf("Threaderzeugung: %s\n", strerror(err)); 
    /*switch(pid = fork()) {
      case -1:
        perror("fork");
        break;
      case 0:
        dup2( clientfd, STDOUT_FILENO );  
        dup2( clientfd, STDERR_FILENO );
        handle_client(clientfd);
        exit(1);
        break;
      default:
        signal(SIGCHLD, clean_up_ehild_process);
        close(clientfd);
        break;
    }
		*/
		//signal(SIGCHLD, clean_up_child_process);
   // close(clientfd);
  }
	return 0;
}

int is_background(char **argvec, int argc) {
  char *last_arg = argvec[argc-1];
  if(!strcmp(last_arg, "&")) {
    argvec[argc-1] = 0;
    return 1;
  } else if(last_arg[strlen(last_arg) - 1] == '&') {
    last_arg[strlen(last_arg)-1] = '\0';
    return 1;
  }

  return 0;
}

int get_ipv4(int fd, char *client_ip) {
  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);

  if(getpeername(fd, (struct sockaddr *)&addr, &addr_size) == -1) {
    return -1;
  }

  strcpy(client_ip, inet_ntoa(addr.sin_addr));

  return 0;
}

void *handle_client(void *arg) {
  char cmdline[MAXCMDLINE];
  char *argvec[MAXARGS];
  int i, background;
  time_t start_time;
	int fd;

	//char pwd[MAXPATHNAME];
	char *pwd;
	struct utsname uname_data;

	fd = (int)arg;
	printf("FD: %d\n", fd);
	pwd = getcwd(pwd, MAXPATHNAME);	
	uname(&uname_data);
  time(&start_time);
		
	dup2( fd, STDOUT_FILENO );  
	dup2( fd, STDERR_FILENO );
	dup2( fd, STDIN_FILENO );
	
	fflush(stdout);
  for (;;) {
    if(show_prompt_socket(fd, uname_data, pwd) == -1) {
			printf("Error showing prompt!\n");
			fflush(stdout);
      return NULL;
    }

    if(read_input_s(cmdline, fd) == -1) {
			printf("Error reading line!\n");
			fflush(stdout);
      return NULL;
    }

    // show prompt when \n was pressed
    if (strlen(cmdline) < 1) {
      continue;
    }

    // Commandozeile in Worte Zerlegen und expandieren
    get_commands(cmdline, argvec, &i);
		/*
		for(j=0; j < i; j++) {
			printf("%d:%s\n", j, argvec[j]);
		}
		fflush(stdout);
		*/
    background = is_background(argvec, i);

    if(handle_commands(argvec, i, start_time, background, fd, pwd) == 1) {
      return 0;
    }

  }
	printf("Thread completed!\n");
  return NULL;
}

int handle_commands(char **argvec, int argc, time_t start_time, int background, int client_fd, char *pwd) {
  int pid, status;
	int i;
	int log_fd;
	char log_path[] = "/var/log/is151002_threads";
	char ipv4[20];
	
  if (!strcmp(argvec[0], "exit")) {
    printf("bye, bye\n");
    return 1;
  } else if (!strcmp(argvec[0], "cd")) {
    handle_cd(argvec, argc, pwd);
  } else if(!strcmp(argvec[0], "umask")) {
    handle_umask(argvec, argc);
  } else if(!strcmp(argvec[0], "setpath")) {
    handle_setpath(argvec, argc);
  } else if(!strcmp(argvec[0], "info")) {
    handle_info(start_time);
  } else if(!strcmp(argvec[0], "getprot")) {
		handle_getprot(argvec, argc, log_path);
	}	else { // externe commandos

    switch(pid = fork()) {
      case -1:
        perror("fork");
        break;
      case 0:
        reset_signal();
        execvp(argvec[0], argvec);
        perror(argvec[0]);
        exit(1);
        break;
      default:
        if(background == 1) {
          signal(SIGCHLD, clean_up_child_process);
          printf("running %s in background [%d]\n", argvec[0], pid);
        } else {
          signal(SIGCHLD, SIG_DFL);
          waitpid(pid,&status,0);
        }
    }
	}
	
	pthread_mutex_lock(&mutex);
	if((log_fd = open(log_path, O_RDWR|O_APPEND)) == -1) {
		perror("open log");
		fflush(stdout);
		exit(1);
	}

	/*if(get_ipv4(client_fd, ipv4) == -1) {
		printf("Cannot convert IPv4 address!\n");
		fflush(stdout);
	}
	*/
	for(i = 0; i < argc; i++) {
		//write(log_fd, ipv4, strlen(ipv4));
		//write(log_fd, ":", 1);
		write(log_fd, argvec[i], strlen(argvec[i]));
		write(log_fd, " ", 1);
	}
	write(log_fd, "\n", 1);
	close(log_fd);
	pthread_mutex_unlock(&mutex);
  return 0;
}

void get_commands(char *cmdline, char **argvec, int *argc) {
  for (*argc=0,argvec[*argc] = strtok(cmdline, " \t"); argvec[*argc] != NULL;  argvec[++(*argc)] = strtok(NULL, " \t"))
    ;
}

int read_input_s(char *cmdline, int fd) {
  int rdbytes;
	//printf("entered read: fd %d!\n", fd);
	//printf("cmdline sz: %d\n", strlen(cmdline));
  if ((rdbytes = read(fd, cmdline, MAXCMDLINE-1)) > 0) { // lest eine Zeile
    cmdline[rdbytes] = 0;
    if (strlen(cmdline) >= 2) {
      if(cmdline[strlen(cmdline)-2] == '\r') {
        cmdline[strlen(cmdline)-2] = '\0';
      }
    } else if (strlen(cmdline) > 1) {
      cmdline[strlen(cmdline)-1] = '\0'; // das \n am Ende entfernen
    }
    return 0;
  }

  return -1;
}

int show_prompt_socket(int fd, struct utsname uname_data, char *pwd) {
  char prompt[1024];
  bzero(prompt, sizeof(prompt));
  //sprintf(prompt, "%s:%s $ ", uname_data.nodename, pwd);
	printf("%s:%s $ ", uname_data.nodename, pwd);
	fflush(stdout);
  //if(write(fd, prompt, sizeof(prompt)) != sizeof(prompt)) {
    //return -1;
  //}
  return 0;
}

void handle_getprot(char **argvec, int argc, char *log_path) {
	FILE *log;
	char line[256];
	int i = 0, n;
	int skip;	

	if(argc<2 || argc>2) {
		fprintf(stderr, "getprot: wrong number of arguments!\n");
		return;
	}

	n = strtol(argvec[1], NULL, 10);
	pthread_mutex_lock(&mutex);	
	log = fopen(log_path, "r");
	while (fgets(line, sizeof(line), log)) {
		i++; 
	}
	if(i>n) {
		skip = i-n;
	}
	fclose(log);

	log = fopen(log_path, "r");
	i = 0;
	while (fgets(line, sizeof(line), log)) {
		i++;
		if(i > skip) {
			printf("%s", line);
		} 
	}
	pthread_mutex_unlock(&mutex);
} 

void handle_cd(char **argvec, const unsigned argc, char *workdir) {
  if(argc >= 2) {
    if (chdir(argvec[1]) == -1) {
      perror("cd");
    }
  } else {
    if (chdir(getenv("HOME")) == -1) {
      perror("cd");
    }
  }
  getcwd(workdir, sizeof(MAXPATHNAME));
}

void handle_umask(char **argvec, const unsigned argc) {
  int umask_value;
  mode_t prev_umask;

  if(argc >= 2) {
    umask_value = strtol(argvec[1], NULL, 8);
    umask(umask_value);
    prev_umask = umask(0);
    printf("%04o\n", prev_umask);
  } else {
    prev_umask = umask(0);
    printf("%04o\n", prev_umask);
  }

  umask(prev_umask);
}

void handle_setpath(char **argvec, const unsigned argc) {
  char *path;
  if(argc >= 2) {
    path = getenv("PATH");
    if(path != NULL) {
      strcat(path, ":");
      strcat(path, argvec[1]);
      if (setenv("PATH", path, 1) == -1) {
        perror("setenv");
      }
    } else {
      if (setenv("PATH", argvec[1], 1) == -1) {
        perror("setenv");
      }
    }
  } else {
    printf("%s\n", getenv("PATH"));
  }
}

void handle_info(time_t start_time) {
  struct tm *timeinfo;
  timeinfo = localtime( &start_time );

  printf("Shell von: Berger, is151002\nPID: %d\nLäuft seit: %s\n", getpid(), asctime(timeinfo));
  fflush(stdout);
}

void signal_handler(int sig) {
    switch (sig) {
      case SIGINT:
      case SIGQUIT:
        signal(sig, SIG_IGN);
        break;
      default:
        return;
    }
}

void clean_up_child_process() {
  int status;
  int pid;

  while ((pid = waitpid(-1, &status, 0)) != -1)
    ;
}

void reset_signal() {
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
}
