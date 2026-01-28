#ifndef __DRV_TYPES_SDIO_H__
#define __DRV_TYPES_SDIO_H__
	#include <linux/mmc/sdio_func.h>
	#include <linux/mmc/sdio_ids.h>
struct sdio_data {
	u8  func_number;
	u8  tx_block_mode;
	u8  rx_block_mode;
	u32 block_transfer_len;
	struct sdio_func	 *func;
	void *sys_sdio_irq_thd;
};
#endif
