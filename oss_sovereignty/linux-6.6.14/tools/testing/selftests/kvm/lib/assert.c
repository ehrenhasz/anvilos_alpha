
 

#define _GNU_SOURCE  

#include "test_util.h"

#include <execinfo.h>
#include <sys/syscall.h>

#include "kselftest.h"

 
static void __attribute__((noinline)) test_dump_stack(void);
static void test_dump_stack(void)
{
	 
	size_t i;
	size_t n = 20;
	void *stack[n];
	const char *addr2line = "addr2line -s -e /proc/$PPID/exe -fpai";
	const char *pipeline = "|cat -n 1>&2";
	char cmd[strlen(addr2line) + strlen(pipeline) +
		  
		 n * (((sizeof(void *)) * 2) + 1) +
		  
		 1];
	char *c = cmd;

	n = backtrace(stack, n);
	 
	if (n <= 2) {
		fputs("  (stack trace empty)\n", stderr);
		return;
	}

	c += sprintf(c, "%s", addr2line);
	for (i = 2; i < n; i++)
		c += sprintf(c, " %lx", ((unsigned long) stack[i]) - 1);

	c += sprintf(c, "%s", pipeline);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
	system(cmd);
#pragma GCC diagnostic pop
}

static pid_t _gettid(void)
{
	return syscall(SYS_gettid);
}

void __attribute__((noinline))
test_assert(bool exp, const char *exp_str,
	const char *file, unsigned int line, const char *fmt, ...)
{
	va_list ap;

	if (!(exp)) {
		va_start(ap, fmt);

		fprintf(stderr, "==== Test Assertion Failure ====\n"
			"  %s:%u: %s\n"
			"  pid=%d tid=%d errno=%d - %s\n",
			file, line, exp_str, getpid(), _gettid(),
			errno, strerror(errno));
		test_dump_stack();
		if (fmt) {
			fputs("  ", stderr);
			vfprintf(stderr, fmt, ap);
			fputs("\n", stderr);
		}
		va_end(ap);

		if (errno == EACCES) {
			print_skip("Access denied - Exiting");
			exit(KSFT_SKIP);
		}
		exit(254);
	}

	return;
}
