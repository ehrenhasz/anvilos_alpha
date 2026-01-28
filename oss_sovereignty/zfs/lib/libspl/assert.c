#include <assert.h>
static boolean_t libspl_assert_ok = B_FALSE;
void
libspl_set_assert_ok(boolean_t val)
{
	libspl_assert_ok = val;
}
void
libspl_assertf(const char *file, const char *func, int line,
    const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	fprintf(stderr, "ASSERT at %s:%d:%s()", file, line, func);
	va_end(args);
#if !__has_feature(attribute_analyzer_noreturn) && !defined(__COVERITY__)
	if (libspl_assert_ok) {
		return;
	}
#endif
	abort();
}
