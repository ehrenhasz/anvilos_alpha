


#ifndef SIGNAL_H
#define SIGNAL_H

#include <linux/signal.h>

#include "system.h"

typedef __sighandler_t sighandler_t;

int sigemptyset(sigset_t *s);
int sigaddset(sigset_t *s, int n);
int sigaction(int n, struct sigaction *sa, const struct sigaction *old);
int sigprocmask(int how, const sigset_t *mask, sigset_t *old);

#endif 
