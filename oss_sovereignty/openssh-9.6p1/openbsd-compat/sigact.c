 

 

 

 

#include "includes.h"
#include <errno.h>
#include <signal.h>
#include "sigact.h"

 
 

#if !HAVE_SIGACTION && HAVE_SIGVEC

int
sigaction(int sig, struct sigaction *sigact, struct sigaction *osigact)
{
	return sigvec(sig, sigact ? &sigact->sv : NULL,
	    osigact ? &osigact->sv : NULL);
}

int
sigemptyset (sigset_t *mask)
{
	if (!mask) {
		errno = EINVAL;
		return -1;
	}
	*mask = 0;
	return 0;
}

int
sigprocmask (int mode, sigset_t *mask, sigset_t *omask)
{
	sigset_t current = sigsetmask(0);

	if (!mask) {
		errno = EINVAL;
		return -1;
	}

	if (omask)
		*omask = current;

	if (mode == SIG_BLOCK)
		current |= *mask;
	else if (mode == SIG_UNBLOCK)
		current &= ~*mask;
	else if (mode == SIG_SETMASK)
	current = *mask;

	sigsetmask(current);
	return 0;
}

int
sigsuspend (sigset_t *mask)
{
	if (!mask) {
		errno = EINVAL;
		return -1;
	}
	return sigpause(*mask);
}

int
sigdelset (sigset_t *mask, int sig)
{
	if (!mask) {
		errno = EINVAL;
		return -1;
	}
	*mask &= ~sigmask(sig);
	return 0;
}

int
sigaddset (sigset_t *mask, int sig)
{
	if (!mask) {
		errno = EINVAL;
		return -1;
	}
	*mask |= sigmask(sig);
	return 0;
}

int
sigismember (sigset_t *mask, int sig)
{
	if (!mask) {
		errno = EINVAL;
		return -1;
	}
	return (*mask & sigmask(sig)) != 0;
}

#endif
