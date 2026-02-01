
 

#define _GNU_SOURCE
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "../kselftest.h"

static long syscall_clock_getres(clockid_t _clkid, struct timespec *_ts)
{
	long ret;

	ret = syscall(SYS_clock_getres, _clkid, _ts);

	return ret;
}

const char *vdso_clock_name[12] = {
	"CLOCK_REALTIME",
	"CLOCK_MONOTONIC",
	"CLOCK_PROCESS_CPUTIME_ID",
	"CLOCK_THREAD_CPUTIME_ID",
	"CLOCK_MONOTONIC_RAW",
	"CLOCK_REALTIME_COARSE",
	"CLOCK_MONOTONIC_COARSE",
	"CLOCK_BOOTTIME",
	"CLOCK_REALTIME_ALARM",
	"CLOCK_BOOTTIME_ALARM",
	"CLOCK_SGI_CYCLE",
	"CLOCK_TAI",
};

 
static inline int vdso_test_clock(unsigned int clock_id)
{
	struct timespec x, y;

	printf("clock_id: %s", vdso_clock_name[clock_id]);
	clock_getres(clock_id, &x);
	syscall_clock_getres(clock_id, &y);

	if ((x.tv_sec != y.tv_sec) || (x.tv_nsec != y.tv_nsec)) {
		printf(" [FAIL]\n");
		return KSFT_FAIL;
	}

	printf(" [PASS]\n");
	return KSFT_PASS;
}

int main(int argc, char **argv)
{
	int ret = 0;

#if _POSIX_TIMERS > 0

#ifdef CLOCK_REALTIME
	ret += vdso_test_clock(CLOCK_REALTIME);
#endif

#ifdef CLOCK_BOOTTIME
	ret += vdso_test_clock(CLOCK_BOOTTIME);
#endif

#ifdef CLOCK_TAI
	ret += vdso_test_clock(CLOCK_TAI);
#endif

#ifdef CLOCK_REALTIME_COARSE
	ret += vdso_test_clock(CLOCK_REALTIME_COARSE);
#endif

#ifdef CLOCK_MONOTONIC
	ret += vdso_test_clock(CLOCK_MONOTONIC);
#endif

#ifdef CLOCK_MONOTONIC_RAW
	ret += vdso_test_clock(CLOCK_MONOTONIC_RAW);
#endif

#ifdef CLOCK_MONOTONIC_COARSE
	ret += vdso_test_clock(CLOCK_MONOTONIC_COARSE);
#endif

#endif
	if (ret > 0)
		return KSFT_FAIL;

	return KSFT_PASS;
}
