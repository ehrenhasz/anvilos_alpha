 

 

 
 

MODULE_ID("$Id: sigaction.c,v 1.15 2020/02/02 23:34:34 tom Exp $")

static int
_nc_sigaction(int sig, sigaction_t * sigact, sigaction_t * osigact)
{
    return sigvec(sig, sigact, osigact);
}

static int
_nc_sigemptyset(sigset_t * mask)
{
    *mask = 0;
    return 0;
}

static int
_nc_sigprocmask(int mode, sigset_t * mask, sigset_t * omask)
{
    sigset_t current = sigsetmask(0);

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

static int
_nc_sigaddset(sigset_t * mask, int sig)
{
    *mask |= sigmask(sig);
    return 0;
}

 
#if 0
static int
_nc_sigsuspend(sigset_t * mask)
{
    return sigpause(*mask);
}

static int
_nc_sigdelset(sigset_t * mask, int sig)
{
    *mask &= ~sigmask(sig);
    return 0;
}

static int
_nc_sigismember(sigset_t * mask, int sig)
{
    return (*mask & sigmask(sig)) != 0;
}
#endif
