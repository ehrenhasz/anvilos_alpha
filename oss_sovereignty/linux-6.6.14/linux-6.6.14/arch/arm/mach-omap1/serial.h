#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H
#include <linux/init.h>
#define OMAP_UART_INFO_OFS	0x3ffc
#define OMAP_PORT_SHIFT		2
#define OMAP7XX_PORT_SHIFT	0
#define OMAP1510_BASE_BAUD	(12000000/16)
#define OMAP16XX_BASE_BAUD	(48000000/16)
#define OMAP1UART1		11
#define OMAP1UART2		12
#define OMAP1UART3		13
#ifndef __ASSEMBLER__
extern void omap_serial_init(void);
#endif
#endif
