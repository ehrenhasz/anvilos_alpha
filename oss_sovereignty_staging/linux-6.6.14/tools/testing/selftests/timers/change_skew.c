 


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <time.h>
#include "../kselftest.h"

#define NSEC_PER_SEC 1000000000LL


int change_skew_test(int ppm)
{
	struct timex tx;
	int ret;

	tx.modes = ADJ_FREQUENCY;
	tx.freq = ppm << 16;

	ret = adjtimex(&tx);
	if (ret < 0) {
		printf("Error adjusting freq\n");
		return ret;
	}

	ret = system("./raw_skew");
	ret |= system("./inconsistency-check");
	ret |= system("./nanosleep");

	return ret;
}


int main(int argc, char **argv)
{
	struct timex tx;
	int i, ret;

	int ppm[5] = {0, 250, 500, -250, -500};

	 
	ret = system("killall -9 ntpd");

	 
	tx.modes = ADJ_OFFSET;
	tx.offset = 0;
	ret = adjtimex(&tx);

	if (ret < 0) {
		printf("Maybe you're not running as root?\n");
		return -1;
	}

	for (i = 0; i < 5; i++) {
		printf("Using %i ppm adjustment\n", ppm[i]);
		ret = change_skew_test(ppm[i]);
		if (ret)
			break;
	}

	 
	tx.modes = ADJ_FREQUENCY;
	tx.offset = 0;
	adjtimex(&tx);

	if (ret) {
		printf("[FAIL]");
		return ksft_exit_fail();
	}
	printf("[OK]");
	return ksft_exit_pass();
}
