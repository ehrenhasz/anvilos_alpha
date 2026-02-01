 

 

#ifndef _POSIXJMP_H_
#define _POSIXJMP_H_

#include <setjmp.h>

 

#if defined (HAVE_POSIX_SIGSETJMP)
#  define procenv_t	sigjmp_buf

#  define setjmp_nosigs(x)	sigsetjmp((x), 0)
#  define setjmp_sigs(x)	sigsetjmp((x), 1)

#  define _rl_longjmp(x, n)	siglongjmp((x), (n))
#  define sh_longjmp(x, n)	siglongjmp((x), (n))
#else
#  define procenv_t	jmp_buf

#  define setjmp_nosigs		setjmp
#  define setjmp_sigs		setjmp

#  define _rl_longjmp(x, n)	longjmp((x), (n))
#  define sh_longjmp(x, n)	longjmp((x), (n))
#endif

#endif  
