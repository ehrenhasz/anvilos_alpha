


#include_next <assert.h>

#ifndef _LIBSPL_ASSERT_H
#define	_LIBSPL_ASSERT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>


#ifndef __has_feature
#define	__has_feature(x) 0
#endif


#if __has_feature(attribute_analyzer_noreturn) || defined(__COVERITY__)
#define	NORETURN	__attribute__((__noreturn__))
#else
#define	NORETURN
#endif


extern void libspl_set_assert_ok(boolean_t val);


extern void libspl_assertf(const char *file, const char *func, int line,
    const char *format, ...) NORETURN __attribute__((format(printf, 4, 5)));

static inline int
libspl_assert(const char *buf, const char *file, const char *func, int line)
{
	libspl_assertf(file, func, line, "%s", buf);
	return (0);
}

#ifdef verify
#undef verify
#endif

#define	VERIFY(cond)							\
	(void) ((!(cond)) &&						\
	    libspl_assert(#cond, __FILE__, __FUNCTION__, __LINE__))
#define	verify(cond)							\
	(void) ((!(cond)) &&						\
	    libspl_assert(#cond, __FILE__, __FUNCTION__, __LINE__))

#define	VERIFY3B(LEFT, OP, RIGHT)					\
do {									\
	const boolean_t __left = (boolean_t)(LEFT);			\
	const boolean_t __right = (boolean_t)(RIGHT);			\
	if (!(__left OP __right))					\
		libspl_assertf(__FILE__, __FUNCTION__, __LINE__,	\
		    "%s %s %s (0x%llx %s 0x%llx)", #LEFT, #OP, #RIGHT,	\
		    (u_longlong_t)__left, #OP, (u_longlong_t)__right);	\
} while (0)

#define	VERIFY3S(LEFT, OP, RIGHT)					\
do {									\
	const int64_t __left = (int64_t)(LEFT);				\
	const int64_t __right = (int64_t)(RIGHT);			\
	if (!(__left OP __right))					\
		libspl_assertf(__FILE__, __FUNCTION__, __LINE__,	\
		    "%s %s %s (0x%llx %s 0x%llx)", #LEFT, #OP, #RIGHT,	\
		    (u_longlong_t)__left, #OP, (u_longlong_t)__right);	\
} while (0)

#define	VERIFY3U(LEFT, OP, RIGHT)					\
do {									\
	const uint64_t __left = (uint64_t)(LEFT);			\
	const uint64_t __right = (uint64_t)(RIGHT);			\
	if (!(__left OP __right))					\
		libspl_assertf(__FILE__, __FUNCTION__, __LINE__,	\
		    "%s %s %s (0x%llx %s 0x%llx)", #LEFT, #OP, #RIGHT,	\
		    (u_longlong_t)__left, #OP, (u_longlong_t)__right);	\
} while (0)

#define	VERIFY3P(LEFT, OP, RIGHT)					\
do {									\
	const uintptr_t __left = (uintptr_t)(LEFT);			\
	const uintptr_t __right = (uintptr_t)(RIGHT);			\
	if (!(__left OP __right))					\
		libspl_assertf(__FILE__, __FUNCTION__, __LINE__,	\
		    "%s %s %s (0x%llx %s 0x%llx)", #LEFT, #OP, #RIGHT,	\
		    (u_longlong_t)__left, #OP, (u_longlong_t)__right);	\
} while (0)

#define	VERIFY0(LEFT)							\
do {									\
	const uint64_t __left = (uint64_t)(LEFT);			\
	if (!(__left == 0))						\
		libspl_assertf(__FILE__, __FUNCTION__, __LINE__,	\
		    "%s == 0 (0x%llx == 0)", #LEFT,			\
		    (u_longlong_t)__left);				\
} while (0)

#ifdef assert
#undef assert
#endif

#ifdef NDEBUG
#define	ASSERT3B(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3S(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3U(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3P(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT0(x)		((void) sizeof ((uintptr_t)(x)))
#define	ASSERT(x)		((void) sizeof ((uintptr_t)(x)))
#define	assert(x)		((void) sizeof ((uintptr_t)(x)))
#define	IMPLY(A, B)							\
	((void) sizeof ((uintptr_t)(A)), (void) sizeof ((uintptr_t)(B)))
#define	EQUIV(A, B)							\
	((void) sizeof ((uintptr_t)(A)), (void) sizeof ((uintptr_t)(B)))
#else
#define	ASSERT3B	VERIFY3B
#define	ASSERT3S	VERIFY3S
#define	ASSERT3U	VERIFY3U
#define	ASSERT3P	VERIFY3P
#define	ASSERT0		VERIFY0
#define	ASSERT		VERIFY
#define	assert		VERIFY
#define	IMPLY(A, B) \
	((void)(((!(A)) || (B)) || \
	    libspl_assert("(" #A ") implies (" #B ")", \
	    __FILE__, __FUNCTION__, __LINE__)))
#define	EQUIV(A, B) \
	((void)((!!(A) == !!(B)) || \
	    libspl_assert("(" #A ") is equivalent to (" #B ")", \
	    __FILE__, __FUNCTION__, __LINE__)))

#endif  

#endif  
