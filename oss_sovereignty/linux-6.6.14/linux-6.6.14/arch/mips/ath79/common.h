#ifndef __ATH79_COMMON_H
#define __ATH79_COMMON_H
#include <linux/types.h>
#define ATH79_MEM_SIZE_MIN	(2 * 1024 * 1024)
#define ATH79_MEM_SIZE_MAX	(256 * 1024 * 1024)
void ath79_ddr_ctrl_init(void);
#endif  
