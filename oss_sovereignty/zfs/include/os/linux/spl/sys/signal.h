#ifndef _SPL_SIGNAL_H
#define	_SPL_SIGNAL_H
#include <linux/sched.h>
#ifdef HAVE_SCHED_SIGNAL_HEADER
#include <linux/sched/signal.h>
#endif
#define	FORREAL		0	 
#define	JUSTLOOKING	1	 
extern int issig(int why);
#endif  
