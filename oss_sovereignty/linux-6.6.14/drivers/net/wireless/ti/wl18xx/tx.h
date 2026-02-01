 
 

#ifndef __WL18XX_TX_H__
#define __WL18XX_TX_H__

#include "../wlcore/wlcore.h"

#define WL18XX_TX_HW_BLOCK_SPARE        1
 
#define WL18XX_TX_HW_EXTRA_BLOCK_SPARE  2
#define WL18XX_TX_HW_BLOCK_SIZE         268

#define WL18XX_TX_STATUS_DESC_ID_MASK    0x7F
#define WL18XX_TX_STATUS_STAT_BIT_IDX    7

 
#define WL18XX_TX_CTRL_NOT_PADDED	BIT(7)

 
#define CONF_TX_RATE_USE_WIDE_CHAN BIT(31)

void wl18xx_tx_immediate_complete(struct wl1271 *wl);

#endif  
