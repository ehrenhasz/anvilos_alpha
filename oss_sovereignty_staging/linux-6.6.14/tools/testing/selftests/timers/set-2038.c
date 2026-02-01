 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "../kselftest.h"

#define NSEC_PER_SEC 1000000000LL

#define KTIME_MAX	((long long)~((unsigned long long)1 << 63))
#define KTIME_SEC_MAX	(KTIME_MAX / NSEC_PER_SEC)

#define YEAR_1901 (-0x7fffffffL)
#define YEAR_1970 1
#define YEAR_2038 0x7fffffffL			 
#define YEAR_2262 KTIME_SEC_MAX			 
#define YEAR_MAX  ((long long)((1ULL<<63)-1))	 

int is32bits(void)
{
	return (sizeof(long) == 4);
}

int settime(long long time)
{
	struct timeval now;
	int ret;

	now.tv_sec = (time_t)time;
	now.tv_usec  = 0;

	ret = settimeofday(&now, NULL);

	printf("Setting time to 0x%lx: %d\n", (long)time, ret);
	return ret;
}

int do_tests(void)
{
	int ret;

	ret = system("date");
	ret = system("./inconsistency-check -c 0 -t 20");
	ret |= system("./nanosleep");
	ret |= system("./nsleep-lat");
	return ret;

}

int main(int argc, char *argv[])
{
	int ret = 0;
	int opt, dangerous = 0;
	time_t start;

	 
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
		case 'd':
			dangerous = 1;
		}
	}

	start = time(0);

	 
	if (!settime(YEAR_1901)) {
		ret = -1;
		goto out;
	}
	if (!settime(YEAR_MAX)) {
		ret = -1;
		goto out;
	}
	if (!is32bits() && !settime(YEAR_2262)) {
		ret = -1;
		goto out;
	}

	 
	settime(YEAR_1970);
	ret = do_tests();
	if (ret)
		goto out;

	settime(YEAR_2038 - 600);
	ret = do_tests();
	if (ret)
		goto out;

	 
	if (is32bits() && !dangerous)
		goto out;
	 
	settime(YEAR_2038 - 10);
	ret = do_tests();
	if (ret)
		goto out;

	settime(YEAR_2262 - 600);
	ret = do_tests();

out:
	 
	settime(start);
	if (ret)
		return ksft_exit_fail();
	return ksft_exit_pass();
}
