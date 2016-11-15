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
#include <semaphore.h>

#define MAXCMDLINE 2048
#define MAXPATHNAME 2048
#define MAXUSERNAME 1024
#define MAXARGS 200
#define BUFSZ 1024
#define PORT 5006
#define SEM_NAME "/is151002_sem"

void signal_handler(int sig);
void reset_signal();
int handle_umask(char **argvec, const unsigned argc);
int handle_setpath(char **argvec, const unsigned argc);
int handle_info(time_t start_time);
void clean_up_child_process();
int handle_cd(char **argvec, const unsigned argc);
void show_prompt();
int show_prompt_socket(int fd);
int handle_getprot(char **argvec, int argc, char *log_path); 
int handle_client(int fd);
int read_input_s(char *cmdline, int fd);
void read_input(char *cmdline);
void get_commands(char *cmdline, char **argvec, int *argc);
int is_background(char **argvec, int argc);
void start_shellserver();
int handle_commands(char **argvec, int argc, time_t start_time, int background, int client_fd, char *ipv4);

char pwd[MAXPATHNAME];
char *user_name;
struct utsname uname_data;
static sem_t *sem;
time_t start_time;

int main(int argc, char **argv) {

  time(&start_time);

  getcwd(pwd, sizeof(pwd));
  user_name = getlogin();
  uname(&uname_data);

  start_shellserver();

  return 0;
}

void start_shellserver() {
  int sock, clientfd;
  int pid;
  socklen_t s_addr_size;

  struct sockaddr s_addr;
  struct sockaddr_in ss_addr;

	if ((sem = sem_open(SEM_NAME, O_RDWR | O_CREAT, 0600, 1)) == SEM_FAILED) {
		perror(SEM_NAME);
		exit(1);
	}

  ss_addr.sin_addr.s_addr = INADDR_ANY;
  ss_addr.sin_port = htons(PORT);
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

    switch(pid = fork()) {
      case -1:
        perror("fork");
        break;
      case 0:
        dup2( clientfd, STDOUT_FILENO );  /* duplicate socket on stdout */
        dup2( clientfd, STDERR_FILENO );
        dup2( clientfd, STDIN_FILENO );
        handle_client(clientfd);
				close(sock);
				close(clientfd);
        exit(1);
        break;
      default:
        signal(SIGCHLD, clean_up_child_process);
        close(clientfd);
        break;
    }
  }
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

int handle_client(int fd) {
  char cmdline[MAXCMDLINE];
  char *argvec[MAXARGS];
  int i, background;
  time_t start_time;
	char client_ip[20];

  time(&start_time);

	if (get_ipv4(fd, client_ip) == -1) {
		perror("get_ipv4");
		return 1;
	}

  for (;;) {
    if(show_prompt_socket(fd) == -1) {
      return -1;
    }

    if(read_input_s(cmdline, fd) == -1) {
      return -1;
    }

    // show prompt when \n was pressed
    if (strlen(cmdline) < 1) {
      continue;
    }

    // Commandozeile in Worte Zerlegen und expandieren
    get_commands(cmdline, argvec, &i);

    background = is_background(argvec, i);

    if(handle_commands(argvec, i, start_time, background, fd, client_ip) == 1) {
      return 0;
    }

  }

  return 0;
}

int handle_commands(char **argvec, int argc, time_t start_time, int background, int client_fd, char *ipv4) {
  int pid, status;
	int i;
	int log_fd;
	char log_path[] = "/var/log/is151002";
	int success = 0;
	
  if (!strcmp(argvec[0], "exit")) {
    printf("bye, bye\n");
    return 1;
  } 
	else if (!strcmp(argvec[0], "cd")) {
    if (handle_cd(argvec, argc) == -1) {
			perror("cd");
		}
		else {
			success = 1;
		}
  } 
	else if(!strcmp(argvec[0], "umask")) {
    handle_umask(argvec, argc);
		success = 1;
  } 
	else if(!strcmp(argvec[0], "setpath")) {
    if (handle_setpath(argvec, argc) == -1) {
			perror("setpath");
		}
		else {
			success = 1;
		}
  } 
	else if(!strcmp(argvec[0], "info")) {
    handle_info(start_time);
		success = 1;
  } 
	else if(!strcmp(argvec[0], "getprot")) {
    handle_getprot(argvec, argc, log_path);
  } 
	else { // externe commandos

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
        if (background == 1) {
          signal(SIGCHLD, clean_up_child_process);
          printf("running %s in background [%d]\n", argvec[0], pid);
					fflush(stdout);
        } 
				else {
          signal(SIGCHLD, SIG_DFL);
          waitpid(pid,&status,0);
					if (!status) {
						success = 1;
					}
        }
    }
	}

	if (success) {
		sem_wait(sem);
		if((log_fd = open(log_path, O_RDWR|O_APPEND)) == -1) {
			perror("open log");
			fflush(stdout);
			exit(1);
		}

		for(i = 0; i < argc; i++) {
			write(log_fd, ipv4, strlen(ipv4));
			write(log_fd, ":", 1);
			write(log_fd, argvec[i], strlen(argvec[i]));
			write(log_fd, " ", 1);
		}
		write(log_fd, "\n", 1);
		sem_post(sem);
		close(log_fd);
	}
  return 0;
}

void get_commands(char *cmdline, char **argvec, int *argc) {
  for (*argc=0,argvec[*argc] = strtok(cmdline, " \t"); argvec[*argc] != NULL;  argvec[++(*argc)] = strtok(NULL, " \t"))
    ;
}

int read_input_s(char *cmdline, int fd) {
  int rdbytes;
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

void show_prompt() {
  printf("[%s@%s] in %s -> ", user_name == NULL ? "user" : user_name, uname_data.nodename, pwd);
}

int show_prompt_socket(int fd) {
  char prompt[1024];
  bzero(prompt, sizeof(prompt));
  sprintf(prompt, "[%s@%s] in %s -> ", user_name == NULL ? "user" : user_name, uname_data.nodename, pwd);
  if(write(fd, prompt, sizeof(prompt)) != sizeof(prompt)) {
    return -1;
  }
  return 0;
}

int handle_getprot(char **argvec, int argc, char *log_path) {
  FILE *log;
  char line[256];
  int i = 0, n;
  int skip;

  if(argc<2 || argc>2) {
    fprintf(stderr, "getprot: wrong number of arguments!\n");
    return -1;
  }

	sem_wait(sem);
  n = strtol(argvec[1], NULL, 10);
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
	fflush(stdout);
	sem_post(sem);
	return 0;
}

int handle_cd(char **argvec, const unsigned argc) {
  if(argc >= 2) {
    if (chdir(argvec[1]) == -1) {
      perror("cd");
			return -1;
    }
  } 
	else {
    if (chdir(getenv("HOME")) == -1) {
      perror("cd");
			return -1;
    }
  }
  getcwd(pwd, sizeof(pwd));
	return 0;
}

int handle_umask(char **argvec, const unsigned argc) {
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
	return 0;
}

int handle_setpath(char **argvec, const unsigned argc) {
  char *path;
  if(argc >= 2) {
    path = getenv("PATH");
    if(path != NULL) {
      strcat(path, ":");
      strcat(path, argvec[1]);
      if (setenv("PATH", path, 1) == -1) {
        perror("setenv");
				return -1;
      }
    } else {
      if (setenv("PATH", argvec[1], 1) == -1) {
        perror("setenv");
				return -1;
      }
    }
  } else {
    printf("%s\n", getenv("PATH"));
		fflush(stdout);
  }
	return 0;
}

int handle_info(time_t start_time) {
  struct tm *timeinfo;
  timeinfo = localtime( &start_time );

  printf("Shell von: Berger, is151002\nPID: %d\nLäuft seit: %s\n", getpid(), asctime(timeinfo));
  fflush(stdout);
	return 0;
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
