 

 

#ifndef _POSIXSELECT_H_
#define _POSIXSELECT_H_

#if defined (FD_SET) && !defined (HAVE_SELECT)
#  define HAVE_SELECT 1
#endif

#if defined (HAVE_SELECT)
#  if !defined (HAVE_SYS_SELECT_H) || !defined (M_UNIX)
#    include <sys/time.h>
#  endif
#endif  
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#ifndef USEC_PER_SEC
#  define USEC_PER_SEC 1000000
#endif

#define USEC_TO_TIMEVAL(us, tv) \
do { \
  (tv).tv_sec = (us) / USEC_PER_SEC; \
  (tv).tv_usec = (us) % USEC_PER_SEC; \
} while (0)

#endif  
