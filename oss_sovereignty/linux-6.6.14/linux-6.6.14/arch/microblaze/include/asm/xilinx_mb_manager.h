#ifndef _XILINX_MB_MANAGER_H
#define _XILINX_MB_MANAGER_H
# ifndef __ASSEMBLY__
#include <linux/of_address.h>
void xmb_manager_register(uintptr_t phys_baseaddr, u32 cr_val,
			  void (*callback)(void *data),
			  void *priv, void (*reset_callback)(void *data));
asmlinkage void xmb_inject_err(void);
# endif  
#define XMB_INJECT_ERR_OFFSET	0x200
#endif  
