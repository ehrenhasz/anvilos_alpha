

#ifndef _SPL_CMN_ERR_H
#define	_SPL_CMN_ERR_H

#if defined(_KERNEL) && defined(HAVE_STANDALONE_LINUX_STDARG)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#include <sys/atomic.h>

#define	CE_CONT		0 
#define	CE_NOTE		1 
#define	CE_WARN		2 
#define	CE_PANIC	3 
#define	CE_IGNORE	4 

extern void cmn_err(int, const char *, ...)
    __attribute__((format(printf, 2, 3)));
extern void vcmn_err(int, const char *, va_list)
    __attribute__((format(printf, 2, 0)));
extern void vpanic(const char *, va_list)
    __attribute__((format(printf, 1, 0), __noreturn__));

#define	fm_panic	panic

#define	cmn_err_once(ce, ...)				\
do {							\
	static volatile uint32_t printed = 0;		\
	if (atomic_cas_32(&printed, 0, 1) == 0) {	\
		cmn_err(ce, __VA_ARGS__);		\
	}						\
} while (0)

#define	vcmn_err_once(ce, fmt, ap)			\
do {							\
	static volatile uint32_t printed = 0;		\
	if (atomic_cas_32(&printed, 0, 1) == 0) {	\
		vcmn_err(ce, fmt, ap);			\
	}						\
} while (0)

#endif 
