 

 

#include "config.h"

#if defined(HAVE_FEATURES_H) && defined(_FORTIFY_SOURCE)
# include <features.h>
# if defined(__GNU_LIBRARY__) && defined(__GLIBC_PREREQ)
#  if __GLIBC_PREREQ(2, 15) && (_FORTIFY_SOURCE > 0)
#   undef _FORTIFY_SOURCE
#   undef __USE_FORTIFY_LEVEL
#   include <sys/socket.h>
void kludge_FD_SET(int n, fd_set *set) {
	FD_SET(n, set);
}
int kludge_FD_ISSET(int n, fd_set *set) {
	return FD_ISSET(n, set);
}
#  endif  
# endif  
#endif  

