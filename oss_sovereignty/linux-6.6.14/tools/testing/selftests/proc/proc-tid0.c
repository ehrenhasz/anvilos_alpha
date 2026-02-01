 

#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static pid_t pid = -1;

static void atexit_hook(void)
{
	if (pid > 0) {
		kill(pid, SIGKILL);
	}
}

static void *f(void *_)
{
	return NULL;
}

static void sigalrm(int _)
{
	exit(0);
}

int main(void)
{
	pid = fork();
	if (pid == 0) {
		 
		while (1) {
			pthread_t pth;
			pthread_create(&pth, NULL, f, NULL);
			pthread_join(pth, NULL);
		}
	} else if (pid > 0) {
		 
		atexit(atexit_hook);

		char buf[64];
		snprintf(buf, sizeof(buf), "/proc/%u/task", pid);

		signal(SIGALRM, sigalrm);
		alarm(1);

		while (1) {
			DIR *d = opendir(buf);
			struct dirent *de;
			while ((de = readdir(d))) {
				if (strcmp(de->d_name, "0") == 0) {
					exit(1);
				}
			}
			closedir(d);
		}

		return 0;
	} else {
		perror("fork");
		return 1;
	}
}
