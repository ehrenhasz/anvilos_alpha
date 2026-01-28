

#ifndef __KSELFTEST_H
#define __KSELFTEST_H

#ifndef NOLIBC
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


#ifndef __cpuid_count
#define __cpuid_count(level, count, a, b, c, d)				\
	__asm__ __volatile__ ("cpuid\n\t"				\
			      : "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			      : "0" (level), "2" (count))
#endif


#define KSFT_PASS  0
#define KSFT_FAIL  1
#define KSFT_XFAIL 2
#define KSFT_XPASS 3
#define KSFT_SKIP  4


struct ksft_count {
	unsigned int ksft_pass;
	unsigned int ksft_fail;
	unsigned int ksft_xfail;
	unsigned int ksft_xpass;
	unsigned int ksft_xskip;
	unsigned int ksft_error;
};

static struct ksft_count ksft_cnt;
static unsigned int ksft_plan;

static inline unsigned int ksft_test_num(void)
{
	return ksft_cnt.ksft_pass + ksft_cnt.ksft_fail +
		ksft_cnt.ksft_xfail + ksft_cnt.ksft_xpass +
		ksft_cnt.ksft_xskip + ksft_cnt.ksft_error;
}

static inline void ksft_inc_pass_cnt(void) { ksft_cnt.ksft_pass++; }
static inline void ksft_inc_fail_cnt(void) { ksft_cnt.ksft_fail++; }
static inline void ksft_inc_xfail_cnt(void) { ksft_cnt.ksft_xfail++; }
static inline void ksft_inc_xpass_cnt(void) { ksft_cnt.ksft_xpass++; }
static inline void ksft_inc_xskip_cnt(void) { ksft_cnt.ksft_xskip++; }
static inline void ksft_inc_error_cnt(void) { ksft_cnt.ksft_error++; }

static inline int ksft_get_pass_cnt(void) { return ksft_cnt.ksft_pass; }
static inline int ksft_get_fail_cnt(void) { return ksft_cnt.ksft_fail; }
static inline int ksft_get_xfail_cnt(void) { return ksft_cnt.ksft_xfail; }
static inline int ksft_get_xpass_cnt(void) { return ksft_cnt.ksft_xpass; }
static inline int ksft_get_xskip_cnt(void) { return ksft_cnt.ksft_xskip; }
static inline int ksft_get_error_cnt(void) { return ksft_cnt.ksft_error; }

static inline void ksft_print_header(void)
{
	
	setvbuf(stdout, NULL, _IOLBF, 0);

	if (!(getenv("KSFT_TAP_LEVEL")))
		printf("TAP version 13\n");
}

static inline void ksft_set_plan(unsigned int plan)
{
	ksft_plan = plan;
	printf("1..%d\n", ksft_plan);
}

static inline void ksft_print_cnts(void)
{
	if (ksft_plan != ksft_test_num())
		printf("# Planned tests != run tests (%u != %u)\n",
			ksft_plan, ksft_test_num());
	printf("# Totals: pass:%d fail:%d xfail:%d xpass:%d skip:%d error:%d\n",
		ksft_cnt.ksft_pass, ksft_cnt.ksft_fail,
		ksft_cnt.ksft_xfail, ksft_cnt.ksft_xpass,
		ksft_cnt.ksft_xskip, ksft_cnt.ksft_error);
}

static inline void ksft_print_msg(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	va_start(args, msg);
	printf("# ");
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline void ksft_test_result_pass(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_pass++;

	va_start(args, msg);
	printf("ok %d ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline void ksft_test_result_fail(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_fail++;

	va_start(args, msg);
	printf("not ok %d ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}


#define ksft_test_result(condition, fmt, ...) do {	\
	if (!!(condition))				\
		ksft_test_result_pass(fmt, ##__VA_ARGS__);\
	else						\
		ksft_test_result_fail(fmt, ##__VA_ARGS__);\
	} while (0)

static inline void ksft_test_result_xfail(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_xfail++;

	va_start(args, msg);
	printf("ok %d # XFAIL ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline void ksft_test_result_skip(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_xskip++;

	va_start(args, msg);
	printf("ok %d # SKIP ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}


static inline void ksft_test_result_error(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	ksft_cnt.ksft_error++;

	va_start(args, msg);
	printf("not ok %d # error ", ksft_test_num());
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);
}

static inline int ksft_exit_pass(void)
{
	ksft_print_cnts();
	exit(KSFT_PASS);
}

static inline int ksft_exit_fail(void)
{
	ksft_print_cnts();
	exit(KSFT_FAIL);
}


#define ksft_exit(condition) do {	\
	if (!!(condition))		\
		ksft_exit_pass();	\
	else				\
		ksft_exit_fail();	\
	} while (0)


#define ksft_finished()			\
	ksft_exit(ksft_plan ==		\
		  ksft_cnt.ksft_pass +	\
		  ksft_cnt.ksft_xfail +	\
		  ksft_cnt.ksft_xskip)

static inline int ksft_exit_fail_msg(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	va_start(args, msg);
	printf("Bail out! ");
	errno = saved_errno;
	vprintf(msg, args);
	va_end(args);

	ksft_print_cnts();
	exit(KSFT_FAIL);
}

static inline int ksft_exit_xfail(void)
{
	ksft_print_cnts();
	exit(KSFT_XFAIL);
}

static inline int ksft_exit_xpass(void)
{
	ksft_print_cnts();
	exit(KSFT_XPASS);
}

static inline int ksft_exit_skip(const char *msg, ...)
{
	int saved_errno = errno;
	va_list args;

	va_start(args, msg);

	
	if (ksft_plan || ksft_test_num()) {
		ksft_cnt.ksft_xskip++;
		printf("ok %d # SKIP ", 1 + ksft_test_num());
	} else {
		printf("1..0 # SKIP ");
	}
	if (msg) {
		errno = saved_errno;
		vprintf(msg, args);
		va_end(args);
	}
	if (ksft_test_num())
		ksft_print_cnts();
	exit(KSFT_SKIP);
}

#endif 
