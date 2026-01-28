#ifndef _SELINUX_IMA_H_
#define _SELINUX_IMA_H_
#include "security.h"
#ifdef CONFIG_IMA
extern void selinux_ima_measure_state(void);
extern void selinux_ima_measure_state_locked(void);
#else
static inline void selinux_ima_measure_state(void)
{
}
static inline void selinux_ima_measure_state_locked(void)
{
}
#endif
#endif	 
