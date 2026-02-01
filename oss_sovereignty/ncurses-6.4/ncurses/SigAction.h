 

 

 

#ifndef _SIGACTION_H
#define _SIGACTION_H

#ifndef HAVE_SIGACTION
#define HAVE_SIGACTION 0
#endif

#ifndef HAVE_SIGVEC
#define HAVE_SIGVEC 0
#endif

#if HAVE_SIGACTION

#if !HAVE_TYPE_SIGACTION
typedef struct sigaction sigaction_t;
#endif

#else	 

#if HAVE_SIGVEC

#undef  SIG_BLOCK
#define SIG_BLOCK       00

#undef  SIG_UNBLOCK
#define SIG_UNBLOCK     01

#undef  SIG_SETMASK
#define SIG_SETMASK     02

 	 
#if HAVE_BSD_SIGNAL_H
#include <bsd/signal.h>
#endif

typedef struct sigvec sigaction_t;

#define sigset_t _nc_sigset_t
typedef unsigned long sigset_t;

#undef  sa_mask
#define sa_mask sv_mask
#undef  sa_handler
#define sa_handler sv_handler
#undef  sa_flags
#define sa_flags sv_flags

#undef  sigaction
#define sigaction   _nc_sigaction
#undef  sigprocmask
#define sigprocmask _nc_sigprocmask
#undef  sigemptyset
#define sigemptyset _nc_sigemptyset
#undef  sigsuspend
#define sigsuspend  _nc_sigsuspend
#undef  sigdelset
#define sigdelset   _nc_sigdelset
#undef  sigaddset
#define sigaddset   _nc_sigaddset

 
#include <base/sigaction.c>

#endif  
#endif  
#endif  
