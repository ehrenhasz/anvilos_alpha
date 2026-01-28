#ifndef _SPL_DISP_H
#define	_SPL_DISP_H
#include <linux/preempt.h>
#define	KPREEMPT_SYNC		(-1)
#define	kpreempt(unused)	cond_resched()
#define	kpreempt_disable()	preempt_disable()
#define	kpreempt_enable()	preempt_enable()
#endif  
