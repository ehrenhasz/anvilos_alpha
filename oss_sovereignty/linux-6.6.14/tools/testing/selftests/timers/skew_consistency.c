 


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include "../kselftest.h"

#define NSEC_PER_SEC 1000000000LL

int main(int argc, char **argv)
{
	struct timex tx;
	int ret, ppm;
	pid_t pid;


	printf("Running Asynchronous Frequency Changing Tests...\n");

	pid = fork();
	if (!pid)
		return system("./inconsistency-check -c 1 -t 600");

	ppm = 500;
	ret = 0;

	while (pid != waitpid(pid, &ret, WNOHANG)) {
		ppm = -ppm;
		tx.modes = ADJ_FREQUENCY;
		tx.freq = ppm << 16;
		adjtimex(&tx);
		usleep(500000);
	}

	 
	tx.modes = ADJ_FREQUENCY;
	tx.offset = 0;
	adjtimex(&tx);


	if (ret) {
		printf("[FAILED]\n");
		return ksft_exit_fail();
	}
	printf("[OK]\n");
	return ksft_exit_pass();
}
