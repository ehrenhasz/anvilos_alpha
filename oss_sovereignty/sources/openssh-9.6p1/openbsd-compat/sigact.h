









#ifndef _SIGACTION_H
#define _SIGACTION_H

#if !defined(HAVE_SIGACTION) && defined(HAVE_SIGVEC)

#undef  SIG_BLOCK
#define SIG_BLOCK       00

#undef  SIG_UNBLOCK
#define SIG_UNBLOCK     01

#undef  SIG_SETMASK
#define SIG_SETMASK     02


#if HAVE_BSD_SIGNAL_H
# include <bsd/signal.h>
#endif

struct sigaction
{
	struct sigvec sv;
};

typedef unsigned long sigset_t;

#undef  sa_mask
#define sa_mask sv.sv_mask
#undef  sa_handler
#define sa_handler sv.sv_handler
#undef  sa_flags
#define sa_flags sv.sv_flags

int sigaction(int sig, struct sigaction *sigact, struct sigaction *osigact);
int sigprocmask (int how, sigset_t *mask, sigset_t *omask);
int sigemptyset (sigset_t *mask);
int sigsuspend (sigset_t *mask);
int sigdelset (sigset_t *mask, int sig);
int sigaddset (sigset_t *mask, int sig);

#endif 

#endif 
