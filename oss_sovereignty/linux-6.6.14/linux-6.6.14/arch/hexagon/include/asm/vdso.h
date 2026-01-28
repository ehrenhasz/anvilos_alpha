#ifndef __ASM_VDSO_H
#define __ASM_VDSO_H
#include <linux/types.h>
struct hexagon_vdso {
	u32 rt_signal_trampoline[2];
};
#endif  
