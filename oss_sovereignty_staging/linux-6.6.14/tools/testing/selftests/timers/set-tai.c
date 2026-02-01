 


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "../kselftest.h"

int set_tai(int offset)
{
	struct timex tx;

	memset(&tx, 0, sizeof(tx));

	tx.modes = ADJ_TAI;
	tx.constant = offset;

	return adjtimex(&tx);
}

int get_tai(void)
{
	struct timex tx;

	memset(&tx, 0, sizeof(tx));

	adjtimex(&tx);
	return tx.tai;
}

int main(int argc, char **argv)
{
	int i, ret;

	ret = get_tai();
	printf("tai offset started at %i\n", ret);

	printf("Checking tai offsets can be properly set: ");
	fflush(stdout);
	for (i = 1; i <= 60; i++) {
		ret = set_tai(i);
		ret = get_tai();
		if (ret != i) {
			printf("[FAILED] expected: %i got %i\n", i, ret);
			return ksft_exit_fail();
		}
	}
	printf("[OK]\n");
	return ksft_exit_pass();
}
