#ifndef _M68K_BLINKEN_H
#define _M68K_BLINKEN_H
#include <asm/setup.h>
#include <asm/io.h>
#define HP300_LEDS		0xf001ffff
extern unsigned char hp300_ledstate;
static __inline__ void blinken_leds(int on, int off)
{
	if (MACH_IS_HP300)
	{
		hp300_ledstate |= on;
		hp300_ledstate &= ~off;
		out_8(HP300_LEDS, ~hp300_ledstate);
	}
}
#endif
