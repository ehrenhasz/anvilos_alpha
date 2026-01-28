


#ifndef _COMEDI_DRIVERS_TESTS_UNITTEST_H
#define _COMEDI_DRIVERS_TESTS_UNITTEST_H

static struct unittest_results {
	int passed;
	int failed;
} unittest_results;

typedef void (*unittest_fptr)(void);

#define unittest(result, fmt, ...) ({ \
	bool failed = !(result); \
	if (failed) { \
		++unittest_results.failed; \
		pr_err("FAIL %s():%i " fmt, __func__, __LINE__, \
		       ##__VA_ARGS__); \
	} else { \
		++unittest_results.passed; \
		pr_debug("pass %s():%i " fmt, __func__, __LINE__, \
			 ##__VA_ARGS__); \
	} \
	failed; \
})


static inline void exec_unittests(const char *name,
				  const unittest_fptr *unit_tests)
{
	pr_info("begin comedi:\"%s\" unittests\n", name);

	for (; (*unit_tests) != NULL; ++unit_tests)
		(*unit_tests)();

	pr_info("end of comedi:\"%s\" unittests - %i passed, %i failed\n", name,
		unittest_results.passed, unittest_results.failed);
}

#endif 
