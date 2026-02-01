 

#ifndef GETHRXTIME_H_
#define GETHRXTIME_H_ 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include "xtime.h"

_GL_INLINE_HEADER_BEGIN
#ifndef GETHRXTIME_INLINE
# define GETHRXTIME_INLINE _GL_INLINE
#endif

#ifdef  __cplusplus
extern "C" {
#endif

 

#if HAVE_ARITHMETIC_HRTIME_T && HAVE_DECL_GETHRTIME
# include <time.h>
GETHRXTIME_INLINE xtime_t gethrxtime (void) { return gethrtime (); }
# else
xtime_t gethrxtime (void);
#endif

_GL_INLINE_HEADER_END

#ifdef  __cplusplus
}
#endif

#endif
