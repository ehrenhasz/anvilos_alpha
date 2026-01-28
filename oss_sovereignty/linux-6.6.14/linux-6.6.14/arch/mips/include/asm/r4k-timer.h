#ifndef __ASM_R4K_TIMER_H
#define __ASM_R4K_TIMER_H
#include <linux/compiler.h>
#ifdef CONFIG_SYNC_R4K
extern void synchronise_count_master(int cpu);
extern void synchronise_count_slave(int cpu);
#else
static inline void synchronise_count_master(int cpu)
{
}
static inline void synchronise_count_slave(int cpu)
{
}
#endif
#endif  
