 

 

#if !defined (_BASHTYPES_H_)
#  define _BASHTYPES_H_

#if defined (CRAY)
#  define word __word
#endif

#include <sys/types.h>

#if defined (CRAY)
#  undef word
#endif

#if defined (HAVE_INTTYPES_H)
#  include <inttypes.h>
#endif

#if HAVE_STDINT_H
#  include <stdint.h>
#endif

#endif  
