


#ifndef _LIBSPL_SYS_CMN_ERR_H
#define	_LIBSPL_SYS_CMN_ERR_H

#include <atomic.h>

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
