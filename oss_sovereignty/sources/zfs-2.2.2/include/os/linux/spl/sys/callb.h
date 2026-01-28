

#ifndef _SPL_CALLB_H
#define	_SPL_CALLB_H

#include <linux/module.h>
#include <sys/mutex.h>

#define	CALLB_CPR_ASSERT(cp)		ASSERT(MUTEX_HELD((cp)->cc_lockp));

typedef struct callb_cpr {
	kmutex_t	*cc_lockp;
} callb_cpr_t;

#define	CALLB_CPR_INIT(cp, lockp, func, name)   {               \
	(cp)->cc_lockp = lockp;                                 \
}

#define	CALLB_CPR_SAFE_BEGIN(cp) {                              \
	CALLB_CPR_ASSERT(cp);					\
}

#define	CALLB_CPR_SAFE_END(cp, lockp) {                         \
	CALLB_CPR_ASSERT(cp);					\
}

#define	CALLB_CPR_EXIT(cp) {                                    \
	ASSERT(MUTEX_HELD((cp)->cc_lockp));                     \
	mutex_exit((cp)->cc_lockp);                             \
}

#endif  
