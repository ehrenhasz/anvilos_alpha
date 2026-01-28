#ifndef _XTENSA_PLATFORM_H
#define _XTENSA_PLATFORM_H
#include <linux/types.h>
#include <asm/bootparam.h>
extern void platform_init(bp_tag_t*);
extern void platform_setup (char **);
extern void platform_idle (void);
extern void platform_calibrate_ccount (void);
void cpu_reset(void) __attribute__((noreturn));
#endif	 
