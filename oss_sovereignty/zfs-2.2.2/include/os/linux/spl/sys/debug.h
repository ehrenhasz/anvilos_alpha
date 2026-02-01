 

 

#ifndef _SPL_DEBUG_H
#define	_SPL_DEBUG_H

 
#define	__printflike(a, b)	__printf(a, b)

#ifndef __maybe_unused
#define	__maybe_unused __attribute__((unused))
#endif

 
#if defined(__COVERITY__) || defined(__clang_analyzer__)
__attribute__((__noreturn__))
#endif
extern void spl_panic(const char *file, const char *func, int line,
    const char *fmt, ...);
extern void spl_dumpstack(void);

static inline int
spl_assert(const char *buf, const char *file, const char *func, int line)
{
	spl_panic(file, func, line, "%s", buf);
	return (0);
}

#define	PANIC(fmt, a...)						\
	spl_panic(__FILE__, __FUNCTION__, __LINE__, fmt, ## a)

#define	VERIFY(cond)							\
	(void) (unlikely(!(cond)) &&					\
	    spl_assert("VERIFY(" #cond ") failed\n",			\
	    __FILE__, __FUNCTION__, __LINE__))

#define	VERIFY3B(LEFT, OP, RIGHT)	do {				\
		const boolean_t _verify3_left = (boolean_t)(LEFT);	\
		const boolean_t _verify3_right = (boolean_t)(RIGHT);	\
		if (unlikely(!(_verify3_left OP _verify3_right)))	\
		    spl_panic(__FILE__, __FUNCTION__, __LINE__,		\
		    "VERIFY3(" #LEFT " "  #OP " "  #RIGHT ") "		\
		    "failed (%d " #OP " %d)\n",				\
		    (boolean_t)(_verify3_left),				\
		    (boolean_t)(_verify3_right));			\
	} while (0)

#define	VERIFY3S(LEFT, OP, RIGHT)	do {				\
		const int64_t _verify3_left = (int64_t)(LEFT);		\
		const int64_t _verify3_right = (int64_t)(RIGHT);	\
		if (unlikely(!(_verify3_left OP _verify3_right)))	\
		    spl_panic(__FILE__, __FUNCTION__, __LINE__,		\
		    "VERIFY3(" #LEFT " "  #OP " "  #RIGHT ") "		\
		    "failed (%lld " #OP " %lld)\n",			\
		    (long long)(_verify3_left),				\
		    (long long)(_verify3_right));			\
	} while (0)

#define	VERIFY3U(LEFT, OP, RIGHT)	do {				\
		const uint64_t _verify3_left = (uint64_t)(LEFT);	\
		const uint64_t _verify3_right = (uint64_t)(RIGHT);	\
		if (unlikely(!(_verify3_left OP _verify3_right)))	\
		    spl_panic(__FILE__, __FUNCTION__, __LINE__,		\
		    "VERIFY3(" #LEFT " "  #OP " "  #RIGHT ") "		\
		    "failed (%llu " #OP " %llu)\n",			\
		    (unsigned long long)(_verify3_left),		\
		    (unsigned long long)(_verify3_right));		\
	} while (0)

#define	VERIFY3P(LEFT, OP, RIGHT)	do {				\
		const uintptr_t _verify3_left = (uintptr_t)(LEFT);	\
		const uintptr_t _verify3_right = (uintptr_t)(RIGHT);	\
		if (unlikely(!(_verify3_left OP _verify3_right)))	\
		    spl_panic(__FILE__, __FUNCTION__, __LINE__,		\
		    "VERIFY3(" #LEFT " "  #OP " "  #RIGHT ") "		\
		    "failed (%px " #OP " %px)\n",			\
		    (void *) (_verify3_left),				\
		    (void *) (_verify3_right));				\
	} while (0)

#define	VERIFY0(RIGHT)	do {						\
		const int64_t _verify3_left = (int64_t)(0);		\
		const int64_t _verify3_right = (int64_t)(RIGHT);	\
		if (unlikely(!(_verify3_left == _verify3_right)))	\
		    spl_panic(__FILE__, __FUNCTION__, __LINE__,		\
		    "VERIFY0(0 == " #RIGHT ") "				\
		    "failed (0 == %lld)\n",				\
		    (long long) (_verify3_right));			\
	} while (0)

#define	VERIFY_IMPLY(A, B) \
	((void)(likely((!(A)) || (B)) ||				\
	    spl_assert("(" #A ") implies (" #B ")",			\
	    __FILE__, __FUNCTION__, __LINE__)))

#define	VERIFY_EQUIV(A, B) \
	((void)(likely(!!(A) == !!(B)) || 				\
	    spl_assert("(" #A ") is equivalent to (" #B ")",		\
	    __FILE__, __FUNCTION__, __LINE__)))

 
#ifdef NDEBUG

#define	ASSERT(x)		((void) sizeof ((uintptr_t)(x)))
#define	ASSERT3B(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3S(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3U(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT3P(x, y, z)						\
	((void) sizeof ((uintptr_t)(x)), (void) sizeof ((uintptr_t)(z)))
#define	ASSERT0(x)		((void) sizeof ((uintptr_t)(x)))
#define	IMPLY(A, B)							\
	((void) sizeof ((uintptr_t)(A)), (void) sizeof ((uintptr_t)(B)))
#define	EQUIV(A, B)		\
	((void) sizeof ((uintptr_t)(A)), (void) sizeof ((uintptr_t)(B)))

 
#else

#define	ASSERT3B	VERIFY3B
#define	ASSERT3S	VERIFY3S
#define	ASSERT3U	VERIFY3U
#define	ASSERT3P	VERIFY3P
#define	ASSERT0		VERIFY0
#define	ASSERT		VERIFY
#define	IMPLY		VERIFY_IMPLY
#define	EQUIV		VERIFY_EQUIV

#endif  

#endif  
