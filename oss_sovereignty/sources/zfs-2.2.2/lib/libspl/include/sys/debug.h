


#ifndef _LIBSPL_SYS_DEBUG_H
#define	_LIBSPL_SYS_DEBUG_H

#include <assert.h>

#ifndef	__printflike
#define	__printflike(x, y) __attribute__((__format__(__printf__, x, y)))
#endif

#ifndef __maybe_unused
#define	__maybe_unused __attribute__((unused))
#endif

#endif
