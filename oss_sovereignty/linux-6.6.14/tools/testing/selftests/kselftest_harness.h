 
 

 

#ifndef __KSELFTEST_HARNESS_H
#define __KSELFTEST_HARNESS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <asm/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <setjmp.h>

#include "kselftest.h"

#define TEST_TIMEOUT_DEFAULT 30

 
#ifndef TH_LOG_STREAM
#  define TH_LOG_STREAM stderr
#endif

#ifndef TH_LOG_ENABLED
#  define TH_LOG_ENABLED 1
#endif

 
#define TH_LOG(fmt, ...) do { \
	if (TH_LOG_ENABLED) \
		__TH_LOG(fmt, ##__VA_ARGS__); \
} while (0)

 
#define __TH_LOG(fmt, ...) \
		fprintf(TH_LOG_STREAM, "# %s:%d:%s:" fmt "\n", \
			__FILE__, __LINE__, _metadata->name, ##__VA_ARGS__)

 
#define SKIP(statement, fmt, ...) do { \
	snprintf(_metadata->results->reason, \
		 sizeof(_metadata->results->reason), fmt, ##__VA_ARGS__); \
	if (TH_LOG_ENABLED) { \
		fprintf(TH_LOG_STREAM, "#      SKIP      %s\n", \
			_metadata->results->reason); \
	} \
	_metadata->passed = 1; \
	_metadata->skip = 1; \
	_metadata->trigger = 0; \
	statement; \
} while (0)

 
#define TEST(test_name) __TEST_IMPL(test_name, -1)

 
#define TEST_SIGNAL(test_name, signal) __TEST_IMPL(test_name, signal)

#define __TEST_IMPL(test_name, _signal) \
	static void test_name(struct __test_metadata *_metadata); \
	static inline void wrapper_##test_name( \
		struct __test_metadata *_metadata, \
		struct __fixture_variant_metadata *variant) \
	{ \
		_metadata->setup_completed = true; \
		if (setjmp(_metadata->env) == 0) \
			test_name(_metadata); \
		__test_check_assert(_metadata); \
	} \
	static struct __test_metadata _##test_name##_object = \
		{ .name = #test_name, \
		  .fn = &wrapper_##test_name, \
		  .fixture = &_fixture_global, \
		  .termsig = _signal, \
		  .timeout = TEST_TIMEOUT_DEFAULT, }; \
	static void __attribute__((constructor)) _register_##test_name(void) \
	{ \
		__register_test(&_##test_name##_object); \
	} \
	static void test_name( \
		struct __test_metadata __attribute__((unused)) *_metadata)

 
#define FIXTURE_DATA(datatype_name) struct _test_data_##datatype_name

 
#define FIXTURE(fixture_name) \
	FIXTURE_VARIANT(fixture_name); \
	static struct __fixture_metadata _##fixture_name##_fixture_object = \
		{ .name =  #fixture_name, }; \
	static void __attribute__((constructor)) \
	_register_##fixture_name##_data(void) \
	{ \
		__register_fixture(&_##fixture_name##_fixture_object); \
	} \
	FIXTURE_DATA(fixture_name)

 
#define FIXTURE_SETUP(fixture_name) \
	void fixture_name##_setup( \
		struct __test_metadata __attribute__((unused)) *_metadata, \
		FIXTURE_DATA(fixture_name) __attribute__((unused)) *self, \
		const FIXTURE_VARIANT(fixture_name) \
			__attribute__((unused)) *variant)

 
#define FIXTURE_TEARDOWN(fixture_name) \
	void fixture_name##_teardown( \
		struct __test_metadata __attribute__((unused)) *_metadata, \
		FIXTURE_DATA(fixture_name) __attribute__((unused)) *self, \
		const FIXTURE_VARIANT(fixture_name) \
			__attribute__((unused)) *variant)

 
#define FIXTURE_VARIANT(fixture_name) struct _fixture_variant_##fixture_name

 
#define FIXTURE_VARIANT_ADD(fixture_name, variant_name) \
	extern FIXTURE_VARIANT(fixture_name) \
		_##fixture_name##_##variant_name##_variant; \
	static struct __fixture_variant_metadata \
		_##fixture_name##_##variant_name##_object = \
		{ .name = #variant_name, \
		  .data = &_##fixture_name##_##variant_name##_variant}; \
	static void __attribute__((constructor)) \
		_register_##fixture_name##_##variant_name(void) \
	{ \
		__register_fixture_variant(&_##fixture_name##_fixture_object, \
			&_##fixture_name##_##variant_name##_object);	\
	} \
	FIXTURE_VARIANT(fixture_name) \
		_##fixture_name##_##variant_name##_variant =

 
#define TEST_F(fixture_name, test_name) \
	__TEST_F_IMPL(fixture_name, test_name, -1, TEST_TIMEOUT_DEFAULT)

#define TEST_F_SIGNAL(fixture_name, test_name, signal) \
	__TEST_F_IMPL(fixture_name, test_name, signal, TEST_TIMEOUT_DEFAULT)

#define TEST_F_TIMEOUT(fixture_name, test_name, timeout) \
	__TEST_F_IMPL(fixture_name, test_name, -1, timeout)

#define __TEST_F_IMPL(fixture_name, test_name, signal, tmout) \
	static void fixture_name##_##test_name( \
		struct __test_metadata *_metadata, \
		FIXTURE_DATA(fixture_name) *self, \
		const FIXTURE_VARIANT(fixture_name) *variant); \
	static inline void wrapper_##fixture_name##_##test_name( \
		struct __test_metadata *_metadata, \
		struct __fixture_variant_metadata *variant) \
	{ \
		  \
		FIXTURE_DATA(fixture_name) self; \
		memset(&self, 0, sizeof(FIXTURE_DATA(fixture_name))); \
		if (setjmp(_metadata->env) == 0) { \
			fixture_name##_setup(_metadata, &self, variant->data); \
			  \
                       if (!_metadata->passed || _metadata->skip) \
				return; \
			_metadata->setup_completed = true; \
			fixture_name##_##test_name(_metadata, &self, variant->data); \
		} \
		if (_metadata->setup_completed) \
			fixture_name##_teardown(_metadata, &self, variant->data); \
		__test_check_assert(_metadata); \
	} \
	static struct __test_metadata \
		      _##fixture_name##_##test_name##_object = { \
		.name = #test_name, \
		.fn = &wrapper_##fixture_name##_##test_name, \
		.fixture = &_##fixture_name##_fixture_object, \
		.termsig = signal, \
		.timeout = tmout, \
	 }; \
	static void __attribute__((constructor)) \
			_register_##fixture_name##_##test_name(void) \
	{ \
		__register_test(&_##fixture_name##_##test_name##_object); \
	} \
	static void fixture_name##_##test_name( \
		struct __test_metadata __attribute__((unused)) *_metadata, \
		FIXTURE_DATA(fixture_name) __attribute__((unused)) *self, \
		const FIXTURE_VARIANT(fixture_name) \
			__attribute__((unused)) *variant)

 
#define TEST_HARNESS_MAIN \
	static void __attribute__((constructor)) \
	__constructor_order_last(void) \
	{ \
		if (!__constructor_order) \
			__constructor_order = _CONSTRUCTOR_ORDER_BACKWARD; \
	} \
	int main(int argc, char **argv) { \
		return test_harness_run(argc, argv); \
	}

 

 
#define ASSERT_EQ(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, ==, 1)

 
#define ASSERT_NE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, !=, 1)

 
#define ASSERT_LT(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, <, 1)

 
#define ASSERT_LE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, <=, 1)

 
#define ASSERT_GT(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, >, 1)

 
#define ASSERT_GE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, >=, 1)

 
#define ASSERT_NULL(seen) \
	__EXPECT(NULL, "NULL", seen, #seen, ==, 1)

 
#define ASSERT_TRUE(seen) \
	__EXPECT(0, "0", seen, #seen, !=, 1)

 
#define ASSERT_FALSE(seen) \
	__EXPECT(0, "0", seen, #seen, ==, 1)

 
#define ASSERT_STREQ(expected, seen) \
	__EXPECT_STR(expected, seen, ==, 1)

 
#define ASSERT_STRNE(expected, seen) \
	__EXPECT_STR(expected, seen, !=, 1)

 
#define EXPECT_EQ(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, ==, 0)

 
#define EXPECT_NE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, !=, 0)

 
#define EXPECT_LT(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, <, 0)

 
#define EXPECT_LE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, <=, 0)

 
#define EXPECT_GT(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, >, 0)

 
#define EXPECT_GE(expected, seen) \
	__EXPECT(expected, #expected, seen, #seen, >=, 0)

 
#define EXPECT_NULL(seen) \
	__EXPECT(NULL, "NULL", seen, #seen, ==, 0)

 
#define EXPECT_TRUE(seen) \
	__EXPECT(0, "0", seen, #seen, !=, 0)

 
#define EXPECT_FALSE(seen) \
	__EXPECT(0, "0", seen, #seen, ==, 0)

 
#define EXPECT_STREQ(expected, seen) \
	__EXPECT_STR(expected, seen, ==, 0)

 
#define EXPECT_STRNE(expected, seen) \
	__EXPECT_STR(expected, seen, !=, 0)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))
#endif

 
#define OPTIONAL_HANDLER(_assert) \
	for (; _metadata->trigger; _metadata->trigger = \
			__bail(_assert, _metadata))

#define __INC_STEP(_metadata) \
	 	\
	if (_metadata->passed && _metadata->step < 253) \
		_metadata->step++;

#define is_signed_type(var)       (!!(((__typeof__(var))(-1)) < (__typeof__(var))1))

#define __EXPECT(_expected, _expected_str, _seen, _seen_str, _t, _assert) do { \
	  \
	__typeof__(_expected) __exp = (_expected); \
	__typeof__(_seen) __seen = (_seen); \
	if (_assert) __INC_STEP(_metadata); \
	if (!(__exp _t __seen)) { \
		  \
		switch (is_signed_type(__exp) * 2 + is_signed_type(__seen)) { \
		case 0: { \
			unsigned long long __exp_print = (uintptr_t)__exp; \
			unsigned long long __seen_print = (uintptr_t)__seen; \
			__TH_LOG("Expected %s (%llu) %s %s (%llu)", \
				 _expected_str, __exp_print, #_t, \
				 _seen_str, __seen_print); \
			break; \
			} \
		case 1: { \
			unsigned long long __exp_print = (uintptr_t)__exp; \
			long long __seen_print = (intptr_t)__seen; \
			__TH_LOG("Expected %s (%llu) %s %s (%lld)", \
				 _expected_str, __exp_print, #_t, \
				 _seen_str, __seen_print); \
			break; \
			} \
		case 2: { \
			long long __exp_print = (intptr_t)__exp; \
			unsigned long long __seen_print = (uintptr_t)__seen; \
			__TH_LOG("Expected %s (%lld) %s %s (%llu)", \
				 _expected_str, __exp_print, #_t, \
				 _seen_str, __seen_print); \
			break; \
			} \
		case 3: { \
			long long __exp_print = (intptr_t)__exp; \
			long long __seen_print = (intptr_t)__seen; \
			__TH_LOG("Expected %s (%lld) %s %s (%lld)", \
				 _expected_str, __exp_print, #_t, \
				 _seen_str, __seen_print); \
			break; \
			} \
		} \
		_metadata->passed = 0; \
		  \
		_metadata->trigger = 1; \
	} \
} while (0); OPTIONAL_HANDLER(_assert)

#define __EXPECT_STR(_expected, _seen, _t, _assert) do { \
	const char *__exp = (_expected); \
	const char *__seen = (_seen); \
	if (_assert) __INC_STEP(_metadata); \
	if (!(strcmp(__exp, __seen) _t 0))  { \
		__TH_LOG("Expected '%s' %s '%s'.", __exp, #_t, __seen); \
		_metadata->passed = 0; \
		_metadata->trigger = 1; \
	} \
} while (0); OPTIONAL_HANDLER(_assert)

 
#define __LIST_APPEND(head, item) \
{ \
	  \
	if (head == NULL) { \
		head = item; \
		item->next = NULL; \
		item->prev = item; \
		return;	\
	} \
	if (__constructor_order == _CONSTRUCTOR_ORDER_FORWARD) { \
		item->next = NULL; \
		item->prev = head->prev; \
		item->prev->next = item; \
		head->prev = item; \
	} else { \
		item->next = head; \
		item->next->prev = item; \
		item->prev = item; \
		head = item; \
	} \
}

struct __test_results {
	char reason[1024];	 
};

struct __test_metadata;
struct __fixture_variant_metadata;

 
struct __fixture_metadata {
	const char *name;
	struct __test_metadata *tests;
	struct __fixture_variant_metadata *variant;
	struct __fixture_metadata *prev, *next;
} _fixture_global __attribute__((unused)) = {
	.name = "global",
	.prev = &_fixture_global,
};

static struct __fixture_metadata *__fixture_list = &_fixture_global;
static int __constructor_order;

#define _CONSTRUCTOR_ORDER_FORWARD   1
#define _CONSTRUCTOR_ORDER_BACKWARD -1

static inline void __register_fixture(struct __fixture_metadata *f)
{
	__LIST_APPEND(__fixture_list, f);
}

struct __fixture_variant_metadata {
	const char *name;
	const void *data;
	struct __fixture_variant_metadata *prev, *next;
};

static inline void
__register_fixture_variant(struct __fixture_metadata *f,
			   struct __fixture_variant_metadata *variant)
{
	__LIST_APPEND(f->variant, variant);
}

 
struct __test_metadata {
	const char *name;
	void (*fn)(struct __test_metadata *,
		   struct __fixture_variant_metadata *);
	pid_t pid;	 
	struct __fixture_metadata *fixture;
	int termsig;
	int passed;
	int skip;	 
	int trigger;  
	int timeout;	 
	bool timed_out;	 
	__u8 step;
	bool no_print;  
	bool aborted;	 
	bool setup_completed;  
	jmp_buf env;	 
	struct __test_results *results;
	struct __test_metadata *prev, *next;
};

 
static inline void __register_test(struct __test_metadata *t)
{
	__LIST_APPEND(t->fixture->tests, t);
}

static inline int __bail(int for_realz, struct __test_metadata *t)
{
	 
	if (for_realz) {
		t->aborted = true;
		longjmp(t->env, 1);
	}
	 
	return 0;
}

static inline void __test_check_assert(struct __test_metadata *t)
{
	if (t->aborted) {
		if (t->no_print)
			_exit(t->step);
		abort();
	}
}

struct __test_metadata *__active_test;
static void __timeout_handler(int sig, siginfo_t *info, void *ucontext)
{
	struct __test_metadata *t = __active_test;

	 
	if (!t) {
		fprintf(TH_LOG_STREAM,
			"# no active test in SIGALRM handler!?\n");
		abort();
	}
	if (sig != SIGALRM || sig != info->si_signo) {
		fprintf(TH_LOG_STREAM,
			"# %s: SIGALRM handler caught signal %d!?\n",
			t->name, sig != SIGALRM ? sig : info->si_signo);
		abort();
	}

	t->timed_out = true;
	
	kill(-(t->pid), SIGKILL);
}

void __wait_for_test(struct __test_metadata *t)
{
	struct sigaction action = {
		.sa_sigaction = __timeout_handler,
		.sa_flags = SA_SIGINFO,
	};
	struct sigaction saved_action;
	int status;

	if (sigaction(SIGALRM, &action, &saved_action)) {
		t->passed = 0;
		fprintf(TH_LOG_STREAM,
			"# %s: unable to install SIGALRM handler\n",
			t->name);
		return;
	}
	__active_test = t;
	t->timed_out = false;
	alarm(t->timeout);
	waitpid(t->pid, &status, 0);
	alarm(0);
	if (sigaction(SIGALRM, &saved_action, NULL)) {
		t->passed = 0;
		fprintf(TH_LOG_STREAM,
			"# %s: unable to uninstall SIGALRM handler\n",
			t->name);
		return;
	}
	__active_test = NULL;

	if (t->timed_out) {
		t->passed = 0;
		fprintf(TH_LOG_STREAM,
			"# %s: Test terminated by timeout\n", t->name);
	} else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 255) {
			 
			t->passed = 1;
			t->skip = 1;
		} else if (t->termsig != -1) {
			t->passed = 0;
			fprintf(TH_LOG_STREAM,
				"# %s: Test exited normally instead of by signal (code: %d)\n",
				t->name,
				WEXITSTATUS(status));
		} else {
			switch (WEXITSTATUS(status)) {
			 
			case 0:
				t->passed = 1;
				break;
			 
			default:
				t->passed = 0;
				fprintf(TH_LOG_STREAM,
					"# %s: Test failed at step #%d\n",
					t->name,
					WEXITSTATUS(status));
			}
		}
	} else if (WIFSIGNALED(status)) {
		t->passed = 0;
		if (WTERMSIG(status) == SIGABRT) {
			fprintf(TH_LOG_STREAM,
				"# %s: Test terminated by assertion\n",
				t->name);
		} else if (WTERMSIG(status) == t->termsig) {
			t->passed = 1;
		} else {
			fprintf(TH_LOG_STREAM,
				"# %s: Test terminated unexpectedly by signal %d\n",
				t->name,
				WTERMSIG(status));
		}
	} else {
		fprintf(TH_LOG_STREAM,
			"# %s: Test ended in some other way [%u]\n",
			t->name,
			status);
	}
}

static void test_harness_list_tests(void)
{
	struct __fixture_variant_metadata *v;
	struct __fixture_metadata *f;
	struct __test_metadata *t;

	for (f = __fixture_list; f; f = f->next) {
		v = f->variant;
		t = f->tests;

		if (f == __fixture_list)
			fprintf(stderr, "%-20s %-25s %s\n",
				"# FIXTURE", "VARIANT", "TEST");
		else
			fprintf(stderr, "--------------------------------------------------------------------------------\n");

		do {
			fprintf(stderr, "%-20s %-25s %s\n",
				t == f->tests ? f->name : "",
				v ? v->name : "",
				t ? t->name : "");

			v = v ? v->next : NULL;
			t = t ? t->next : NULL;
		} while (v || t);
	}
}

static int test_harness_argv_check(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "hlF:f:V:v:t:T:r:")) != -1) {
		switch (opt) {
		case 'f':
		case 'F':
		case 'v':
		case 'V':
		case 't':
		case 'T':
		case 'r':
			break;
		case 'l':
			test_harness_list_tests();
			return KSFT_SKIP;
		case 'h':
		default:
			fprintf(stderr,
				"Usage: %s [-h|-l] [-t|-T|-v|-V|-f|-F|-r name]\n"
				"\t-h       print help\n"
				"\t-l       list all tests\n"
				"\n"
				"\t-t name  include test\n"
				"\t-T name  exclude test\n"
				"\t-v name  include variant\n"
				"\t-V name  exclude variant\n"
				"\t-f name  include fixture\n"
				"\t-F name  exclude fixture\n"
				"\t-r name  run specified test\n"
				"\n"
				"Test filter options can be specified "
				"multiple times. The filtering stops\n"
				"at the first match. For example to "
				"include all tests from variant 'bla'\n"
				"but not test 'foo' specify '-T foo -v bla'.\n"
				"", argv[0]);
			return opt == 'h' ? KSFT_SKIP : KSFT_FAIL;
		}
	}

	return KSFT_PASS;
}

static bool test_enabled(int argc, char **argv,
			 struct __fixture_metadata *f,
			 struct __fixture_variant_metadata *v,
			 struct __test_metadata *t)
{
	unsigned int flen = 0, vlen = 0, tlen = 0;
	bool has_positive = false;
	int opt;

	optind = 1;
	while ((opt = getopt(argc, argv, "F:f:V:v:t:T:r:")) != -1) {
		has_positive |= islower(opt);

		switch (tolower(opt)) {
		case 't':
			if (!strcmp(t->name, optarg))
				return islower(opt);
			break;
		case 'f':
			if (!strcmp(f->name, optarg))
				return islower(opt);
			break;
		case 'v':
			if (!strcmp(v->name, optarg))
				return islower(opt);
			break;
		case 'r':
			if (!tlen) {
				flen = strlen(f->name);
				vlen = strlen(v->name);
				tlen = strlen(t->name);
			}
			if (strlen(optarg) == flen + 1 + vlen + !!vlen + tlen &&
			    !strncmp(f->name, &optarg[0], flen) &&
			    !strncmp(v->name, &optarg[flen + 1], vlen) &&
			    !strncmp(t->name, &optarg[flen + 1 + vlen + !!vlen], tlen))
				return true;
			break;
		}
	}

	 
	return !has_positive;
}

void __run_test(struct __fixture_metadata *f,
		struct __fixture_variant_metadata *variant,
		struct __test_metadata *t)
{
	 
	t->passed = 1;
	t->skip = 0;
	t->trigger = 0;
	t->step = 1;
	t->no_print = 0;
	memset(t->results->reason, 0, sizeof(t->results->reason));

	ksft_print_msg(" RUN           %s%s%s.%s ...\n",
	       f->name, variant->name[0] ? "." : "", variant->name, t->name);

	 
	fflush(stdout);
	fflush(stderr);

	t->pid = fork();
	if (t->pid < 0) {
		ksft_print_msg("ERROR SPAWNING TEST CHILD\n");
		t->passed = 0;
	} else if (t->pid == 0) {
		setpgrp();
		t->fn(t, variant);
		if (t->skip)
			_exit(255);
		 
		if (t->passed)
			_exit(0);
		 
		_exit(t->step);
	} else {
		__wait_for_test(t);
	}
	ksft_print_msg("         %4s  %s%s%s.%s\n", t->passed ? "OK" : "FAIL",
	       f->name, variant->name[0] ? "." : "", variant->name, t->name);

	if (t->skip)
		ksft_test_result_skip("%s\n", t->results->reason[0] ?
					t->results->reason : "unknown");
	else
		ksft_test_result(t->passed, "%s%s%s.%s\n",
			f->name, variant->name[0] ? "." : "", variant->name, t->name);
}

static int test_harness_run(int argc, char **argv)
{
	struct __fixture_variant_metadata no_variant = { .name = "", };
	struct __fixture_variant_metadata *v;
	struct __fixture_metadata *f;
	struct __test_results *results;
	struct __test_metadata *t;
	int ret;
	unsigned int case_count = 0, test_count = 0;
	unsigned int count = 0;
	unsigned int pass_count = 0;

	ret = test_harness_argv_check(argc, argv);
	if (ret != KSFT_PASS)
		return ret;

	for (f = __fixture_list; f; f = f->next) {
		for (v = f->variant ?: &no_variant; v; v = v->next) {
			unsigned int old_tests = test_count;

			for (t = f->tests; t; t = t->next)
				if (test_enabled(argc, argv, f, v, t))
					test_count++;

			if (old_tests != test_count)
				case_count++;
		}
	}

	results = mmap(NULL, sizeof(*results), PROT_READ | PROT_WRITE,
		       MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	ksft_print_header();
	ksft_set_plan(test_count);
	ksft_print_msg("Starting %u tests from %u test cases.\n",
	       test_count, case_count);
	for (f = __fixture_list; f; f = f->next) {
		for (v = f->variant ?: &no_variant; v; v = v->next) {
			for (t = f->tests; t; t = t->next) {
				if (!test_enabled(argc, argv, f, v, t))
					continue;
				count++;
				t->results = results;
				__run_test(f, v, t);
				t->results = NULL;
				if (t->passed)
					pass_count++;
				else
					ret = 1;
			}
		}
	}
	munmap(results, sizeof(*results));

	ksft_print_msg("%s: %u / %u tests passed.\n", ret ? "FAILED" : "PASSED",
			pass_count, count);
	ksft_exit(ret == 0);

	 
	return KSFT_FAIL;
}

static void __attribute__((constructor)) __constructor_order_first(void)
{
	if (!__constructor_order)
		__constructor_order = _CONSTRUCTOR_ORDER_FORWARD;
}

#endif   
