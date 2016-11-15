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

typedef struct process_env {
	char pwd[MAXPATHNAME];
	int pwd_fd;
	int umask;
	char path[MAXPATHNAME];
} process_env_t;


void signal_handler(int sig);
void clean_up_child_process();

void *handle_client(void *args);
int handle_commands(char **argvec, int argc, time_t start_time, int background, char*     client_ip, FILE *socket_in, FILE *socket_out, process_env_t *process_env);
int handle_umask(char **argvec, const unsigned argc, FILE *socket_out, process_env_t *    process_env);
int handle_setpath(char **argvec, const unsigned argc, FILE *socket_out, process_env_t     *process_env);
int handle_info(time_t start_time, FILE *socket_out);
int handle_cd(char **argvec, const unsigned argc, process_env_t *process_env);
int handle_getprot(char **argvec, int argc, char *log_path, FILE *socket_out);

void show_prompt(FILE *socket_out, char *pwd);
void get_commands(char *cmdline, char **argvec, int *argc);
void read_input(FILE *socket_in, char *cmdline);
int is_background(char **argvec, int argc);
int get_ipv4(int fd, char *client_ip);


char user_name[MAXUSERNAME];
struct utsname uname_data;
time_t start_time;
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
		err=pthread_create(&thr_id, NULL, handle_client, (void *)clientfd);
		if (err)
			printf("Threaderzeugung: %s\n", strerror(err)); 
    
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
	char client_ip[20];
	char *path;
	int i; 
	int background;
	int fd;
	int prev_umask;
  time_t start_time;
	fd = (int) arg;
	FILE *socket_in = fdopen(fd, "r");
	FILE *socket_out = fdopen(fd, "w");
	process_env_t process_env;	
	struct utsname uname_data;


	// set path for current thread
	path = getenv("PATH");
	if (path != NULL && ((strlen(path)+1) < sizeof(process_env.path))) {
		strcpy(process_env.path, path);
	}
	uname(&uname_data);
  time(&start_time);
	getlogin_r(user_name, sizeof(user_name));
	// set umask for the current thread
	prev_umask = umask(0);
	process_env.umask = prev_umask;
	umask(prev_umask);

	// set working dir for the current thread
	getcwd(process_env.pwd, sizeof(process_env.pwd));
	if((process_env.pwd_fd = open(process_env.pwd, O_DIRECTORY)) == -1) {
		return NULL;
	}	
	
	// get the ipv4 address of the client
	if (get_ipv4(fd, client_ip) == -1) {
		fprintf(socket_out, "get_ipv4: %s\n", strerror(errno));
		return NULL;
	}

	for (;;) {
    show_prompt(socket_out, process_env.pwd);
    read_input(socket_in, cmdline);

    // show prompt when \n was pressed
    if (strlen(cmdline) < 1) {
      continue;
    }

    // Commandozeile in Worte Zerlegen und expandieren
    get_commands(cmdline, argvec, &i);
    background = is_background(argvec, i);

    if(handle_commands(argvec, i, start_time, background, client_ip, socket_in, socket_out, &process_env) == 1) {
      fclose(socket_in);
			fclose(socket_out);
			close(fd);
			break;
    }
  }
  return NULL;
}

int handle_commands(char **argvec, int argc, time_t start_time, int background, 
										char *ipv4, FILE *socket_in, FILE *socket_out, process_env_t *process_env) {
  int pid, status;
	int i, success;
	int log_fd;
	char log_path[] = "/var/log/is151002_threads";
	
  if (!strcmp(argvec[0], "exit")) {
    printf("bye, bye\n");
    return 1;
  } 
	else if (!strcmp(argvec[0], "cd")) {
    if (handle_cd(argvec, argc, process_env) == -1) {
			perror("cd");
		}
		else {
			success = 1;
		}
  } 
	else if(!strcmp(argvec[0], "umask")) {
    handle_umask(argvec, argc, socket_out, process_env);
		success = 1;
  } 
	else if(!strcmp(argvec[0], "setpath")) {
    if (handle_setpath(argvec, argc, socket_out, process_env) == -1) {
			perror("setpath");
		}
		else {
			success = 1;
		}
  } 
	else if(!strcmp(argvec[0], "info")) {
    handle_info(start_time, socket_out);
		success = 1;
  } 
	else if(!strcmp(argvec[0], "getprot")) {
    handle_getprot(argvec, argc, log_path, socket_out);
		success = 1;
  } 
	else { // externe commandos

    switch(pid = fork()) {
      case -1:
        fprintf(socket_out, "fork %s\n", strerror(errno));
        break;
      case 0:
				/* duplicate socket on stdout and stderr */
        dup2( fileno(socket_in), STDIN_FILENO );
        dup2( fileno(socket_out), STDOUT_FILENO );
        dup2( fileno(socket_out), STDERR_FILENO );
        close(fileno(socket_out));

        // set the umask with the current value in the thread
        umask(process_env->umask);

        // set the current working dir with the current value in the thread
        if(fchdir(process_env->pwd_fd) == -1) {
          fprintf(socket_out, "fchdir %s\n", strerror(errno));
          exit(1);
        }

        // set the PATH variable with the current value in the thread
        if(setenv("PATH", process_env->path, 1) == -1) {
          fprintf(socket_out, "setenv %s\n", strerror(errno));
          exit(1);
        }

        execvp(argvec[0], argvec);
        fprintf(socket_out, "%s %s\n", argvec[0], strerror(errno));
        fflush(socket_out);
        exit(1);
        break;
      default:
        if (background == 1) {
          signal(SIGCHLD, clean_up_child_process);
          fprintf(socket_out, "running %s in background [%d]\n", argvec[0], pid);
					fflush(socket_out);
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
		pthread_mutex_lock(&mutex);
		if((log_fd = open(log_path, O_RDWR|O_APPEND)) == -1) {
			//perror("open log");
			//fflush(stdout);
			exit(1);
		}

		
		for(i = 0; i < argc; i++) {
			write(log_fd, ipv4, strlen(ipv4));
			write(log_fd, ":", 1);
			write(log_fd, argvec[i], strlen(argvec[i]));
			write(log_fd, " ", 1);
		}
		write(log_fd, "\n", 1);
		close(log_fd);
		pthread_mutex_unlock(&mutex);
	}
  return 0;
}

void get_commands(char *cmdline, char **argvec, int *argc) {
  for (*argc=0,argvec[*argc] = strtok(cmdline, " \t"); argvec[*argc] != NULL;  argvec[++(*argc)] = strtok(NULL, " \t"))
    ;
}

void read_input(FILE *socket_in, char *cmdline) {
	if (fgets(cmdline, MAXCMDLINE, socket_in)) {
		if (strlen(cmdline) >= 2) {
			if (cmdline[strlen(cmdline)-2] == '\r') {
				cmdline[strlen(cmdline)-2] = '\0';
			}
		}
		else if (strlen(cmdline) > 1) {
			cmdline[strlen(cmdline)-1] = '\0';
		}
	}
}

void show_prompt(FILE *socket_out, char *pwd) {
	fprintf(socket_out, "[%s@%s] in %s -> ", user_name == NULL ? "user" : user_name, uname_data.nodename, pwd);
	fflush(socket_out);
}

int handle_getprot(char **argvec, int argc, char *log_path, FILE *socket_out) {
	FILE *log;
	char line[256];
	int i = 0, n;
	int skip;	

	if(argc<2 || argc>2) {
		fprintf(socket_out, "getprot: wrong number of arguments!\n");
		return -1;
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
	return 0;
} 

int handle_cd(char **argvec, const unsigned argc, process_env_t *process_env) {
	int tmp_fd;
	int MAXSIZE = 0xFFF;
	char proclnk[0xFFF];
	char filename[0xFFF];
	ssize_t r;

	if (argc >= 2) {
		if ((tmp_fd = openat(process_env->pwd_fd, argvec[1], O_DIRECTORY)) == -1) {
			return -1;
		}
		close(process_env->pwd_fd);
		process_env->pwd_fd = tmp_fd;
	}
	else {
		if((tmp_fd = openat(process_env->pwd_fd, getenv("HOME"), O_DIRECTORY)) == -1) {
			return -1;
		}
		strcpy(process_env->pwd, getenv("HOME"));
		close(process_env->pwd_fd);
		process_env->pwd_fd = tmp_fd;
	}

	// get filename of fd
	sprintf(proclnk, "/proc/self/fd/%d", process_env->pwd_fd);
	r = readlink(proclnk, filename, MAXSIZE);
	if (r < 0) {
		printf("failed to readlink\n");
		return -1;
	}
	filename[r] = '\0';
	strcpy(process_env->pwd, filename);
	return 0;
}

int handle_umask(char **argvec, const unsigned argc, FILE *socket_out, process_env_t *process_env) {

  if(argc >= 2) {
		process_env->umask = strtol(argvec[1], NULL, 8); 
  }
	fprintf(socket_out, "%04o\n", process_env->umask);
	fflush(socket_out);
	return 0;
}

int handle_setpath(char **argvec, const unsigned argc, FILE *socket_out, process_env_t *process_env) {
  if (argc >= 2) {
		if (process_env->path != NULL && (sizeof(process_env->path) > (strlen(argvec[1])+2))) {
			strcat(process_env->path, ":");
			strcat(process_env->path, argvec[1]);
		}
  }
	else {
		fprintf(socket_out, "%s\n", process_env->path);
		fflush(socket_out);
	}
	return 0;
}

int handle_info(time_t start_time, FILE *socket_out) {
  struct tm *timeinfo;
  timeinfo = localtime( &start_time );

  fprintf(socket_out, "Shell von: Berger, is151002\nPID: %d\nLäuft seit: %s\n", getpid(), asctime(timeinfo));
  fflush(socket_out);
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
