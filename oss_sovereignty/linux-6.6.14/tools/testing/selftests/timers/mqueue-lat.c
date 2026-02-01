 

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <mqueue.h>
#include "../kselftest.h"

#define NSEC_PER_SEC 1000000000ULL

#define TARGET_TIMEOUT		100000000	 
#define UNRESONABLE_LATENCY	40000000	 


long long timespec_sub(struct timespec a, struct timespec b)
{
	long long ret = NSEC_PER_SEC * b.tv_sec + b.tv_nsec;

	ret -= NSEC_PER_SEC * a.tv_sec + a.tv_nsec;
	return ret;
}

struct timespec timespec_add(struct timespec ts, unsigned long long ns)
{
	ts.tv_nsec += ns;
	while (ts.tv_nsec >= NSEC_PER_SEC) {
		ts.tv_nsec -= NSEC_PER_SEC;
		ts.tv_sec++;
	}
	return ts;
}

int mqueue_lat_test(void)
{

	mqd_t q;
	struct mq_attr attr;
	struct timespec start, end, now, target;
	int i, count, ret;

	q = mq_open("/foo", O_CREAT | O_RDONLY, 0666, NULL);
	if (q < 0) {
		perror("mq_open");
		return -1;
	}
	mq_getattr(q, &attr);


	count = 100;
	clock_gettime(CLOCK_MONOTONIC, &start);

	for (i = 0; i < count; i++) {
		char buf[attr.mq_msgsize];

		clock_gettime(CLOCK_REALTIME, &now);
		target = now;
		target = timespec_add(now, TARGET_TIMEOUT);  

		ret = mq_timedreceive(q, buf, sizeof(buf), NULL, &target);
		if (ret < 0 && errno != ETIMEDOUT) {
			perror("mq_timedreceive");
			return -1;
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	mq_close(q);

	if ((timespec_sub(start, end)/count) > TARGET_TIMEOUT + UNRESONABLE_LATENCY)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	printf("Mqueue latency :                          ");
	fflush(stdout);

	ret = mqueue_lat_test();
	if (ret < 0) {
		printf("[FAILED]\n");
		return ksft_exit_fail();
	}
	printf("[OK]\n");
	return ksft_exit_pass();
}
