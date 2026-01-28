






#ifndef _SYS_CMN_ERR_H
#define	_SYS_CMN_ERR_H

#if !defined(_ASM)
#include <sys/_stdarg.h>
#include <sys/atomic.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif



#define	CE_CONT		0	
#define	CE_NOTE		1	
#define	CE_WARN		2	
#define	CE_PANIC	3	
#define	CE_IGNORE	4	

#ifndef _ASM

extern void cmn_err(int, const char *, ...)
    __attribute__((format(printf, 2, 3)));

extern void vzcmn_err(zoneid_t, int, const char *, __va_list)
    __attribute__((format(printf, 3, 0)));

extern void vcmn_err(int, const char *, __va_list)
    __attribute__((format(printf, 2, 0)));

extern void zcmn_err(zoneid_t, int, const char *, ...)
    __attribute__((format(printf, 3, 4)));

extern void vzprintf(zoneid_t, const char *, __va_list)
    __attribute__((format(printf, 2, 0)));

extern void zprintf(zoneid_t, const char *, ...)
    __attribute__((format(printf, 2, 3)));

extern void vuprintf(const char *, __va_list)
    __attribute__((format(printf, 1, 0)));

extern void panic(const char *, ...)
    __attribute__((format(printf, 1, 2), __noreturn__));

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

#define	zcmn_err_once(zone, ce, ...)			\
do {							\
	static volatile uint32_t printed = 0;		\
	if (atomic_cas_32(&printed, 0, 1) == 0) {	\
		zcmn_err(zone, ce, __VA_ARGS__);	\
	}						\
} while (0)

#define	vzcmn_err_once(zone, ce, fmt, ap)		\
do {							\
	static volatile uint32_t printed = 0;		\
	if (atomic_cas_32(&printed, 0, 1) == 0) {	\
		vzcmn_err(zone, ce, fmt, ap);		\
	}						\
} while (0)

#endif 

#ifdef	__cplusplus
}
#endif

#endif	
