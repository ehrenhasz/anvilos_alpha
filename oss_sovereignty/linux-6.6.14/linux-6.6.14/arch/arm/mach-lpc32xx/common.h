#ifndef __LPC32XX_COMMON_H
#define __LPC32XX_COMMON_H
#include <linux/init.h>
extern void __init lpc32xx_map_io(void);
extern void __init lpc32xx_serial_init(void);
extern void lpc32xx_get_uid(u32 devid[4]);
extern int lpc32xx_sys_suspend(void);
extern int lpc32xx_sys_suspend_sz;
#endif
