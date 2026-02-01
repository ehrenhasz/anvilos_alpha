
 

#include <stdint.h>
#include <elf.h>
#include <stdio.h>
#include <sys/auxv.h>
#include <sys/time.h>

#include "../kselftest.h"
#include "parse_vdso.h"

 
#if defined(__aarch64__)
const char *version = "LINUX_2.6.39";
const char *name = "__kernel_gettimeofday";
#elif defined(__riscv)
const char *version = "LINUX_4.15";
const char *name = "__vdso_gettimeofday";
#else
const char *version = "LINUX_2.6";
const char *name = "__vdso_gettimeofday";
#endif

int main(int argc, char **argv)
{
	unsigned long sysinfo_ehdr = getauxval(AT_SYSINFO_EHDR);
	if (!sysinfo_ehdr) {
		printf("AT_SYSINFO_EHDR is not present!\n");
		return KSFT_SKIP;
	}

	vdso_init_from_sysinfo_ehdr(getauxval(AT_SYSINFO_EHDR));

	 
	typedef long (*gtod_t)(struct timeval *tv, struct timezone *tz);
	gtod_t gtod = (gtod_t)vdso_sym(version, name);

	if (!gtod) {
		printf("Could not find %s\n", name);
		return KSFT_SKIP;
	}

	struct timeval tv;
	long ret = gtod(&tv, 0);

	if (ret == 0) {
		printf("The time is %lld.%06lld\n",
		       (long long)tv.tv_sec, (long long)tv.tv_usec);
	} else {
		printf("%s failed\n", name);
		return KSFT_FAIL;
	}

	return 0;
}
