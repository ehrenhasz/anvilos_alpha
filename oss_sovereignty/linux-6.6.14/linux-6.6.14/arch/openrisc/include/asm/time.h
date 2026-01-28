#ifndef __ASM_OR1K_TIME_H
#define __ASM_OR1K_TIME_H
extern void openrisc_clockevent_init(void);
extern void openrisc_timer_set(unsigned long count);
extern void openrisc_timer_set_next(unsigned long delta);
#ifdef CONFIG_SMP
extern void synchronise_count_master(int cpu);
extern void synchronise_count_slave(int cpu);
#endif
#endif  
