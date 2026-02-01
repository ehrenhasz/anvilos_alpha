 

#include <signal.h>

 
#ifndef SIG2STR_MAX

# include "intprops.h"

 
# define SIG2STR_MAX (sizeof "SIGRTMAX" + INT_STRLEN_BOUND (int) - 1)

#ifdef __cplusplus
extern "C" {
#endif

int sig2str (int, char *);
int str2sig (char const *, int *);

#ifdef __cplusplus
}
#endif

#endif

 

#if defined _sys_nsig
# define SIGNUM_BOUND (_sys_nsig - 1)
#elif defined _SIG_MAXSIG
# define SIGNUM_BOUND (_SIG_MAXSIG - 2)  
#elif defined NSIG
# define SIGNUM_BOUND (NSIG - 1)
#else
# define SIGNUM_BOUND 64
#endif
