#ifndef _ASM_SPARC_CLOCKSOURCE_H
#define _ASM_SPARC_CLOCKSOURCE_H
#define VCLOCK_NONE   0   
#define VCLOCK_TICK   1   
#define VCLOCK_STICK  2   
struct arch_clocksource_data {
	int vclock_mode;
};
#endif  
