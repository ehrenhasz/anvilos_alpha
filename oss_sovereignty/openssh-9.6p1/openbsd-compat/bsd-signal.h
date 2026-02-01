 

#ifndef _BSD_SIGNAL_H
#define _BSD_SIGNAL_H

#include "includes.h"

#include <signal.h>

#ifndef _NSIG
# ifdef NSIG
#  define _NSIG NSIG
# else
#  define _NSIG 128
# endif
#endif

#if !defined(HAVE_STRSIGNAL)
char *strsignal(int);
#endif

#endif  
