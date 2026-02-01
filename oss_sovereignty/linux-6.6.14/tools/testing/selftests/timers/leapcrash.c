 



#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <string.h>
#include <signal.h>
#include "../kselftest.h"

 
int clear_time_state(void)
{
	struct timex tx;
	int ret;

	 
	tx.modes = ADJ_STATUS;
	tx.status = STA_PLL;
	ret = adjtimex(&tx);

	tx.modes = ADJ_STATUS;
	tx.status = 0;
	ret = adjtimex(&tx);

	return ret;
}

 
void handler(int unused)
{
	clear_time_state();
	exit(0);
}


int main(void)
{
	struct timex tx;
	struct timespec ts;
	time_t next_leap;
	int count = 0;

	setbuf(stdout, NULL);

	signal(SIGINT, handler);
	signal(SIGKILL, handler);
	printf("This runs for a few minutes. Press ctrl-c to stop\n");

	clear_time_state();


	 
	clock_gettime(CLOCK_REALTIME, &ts);

	 
	next_leap = ts.tv_sec;
	next_leap += 86400 - (next_leap % 86400);

	for (count = 0; count < 20; count++) {
		struct timeval tv;


		 
		tv.tv_sec = next_leap - 2;
		tv.tv_usec = 0;
		if (settimeofday(&tv, NULL)) {
			printf("Error: You're likely not running with proper (ie: root) permissions\n");
			return ksft_exit_fail();
		}
		tx.modes = 0;
		adjtimex(&tx);

		 
		while (tx.time.tv_sec < next_leap + 1) {
			 
			tx.modes = ADJ_STATUS;
			tx.status = STA_INS;
			adjtimex(&tx);
		}
		clear_time_state();
		printf(".");
		fflush(stdout);
	}
	printf("[OK]\n");
	return ksft_exit_pass();
}
