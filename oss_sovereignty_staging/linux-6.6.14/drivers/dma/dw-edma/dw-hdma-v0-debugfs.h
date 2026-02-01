 
 

#ifndef _DW_HDMA_V0_DEBUG_FS_H
#define _DW_HDMA_V0_DEBUG_FS_H

#include <linux/dma/edma.h>

#ifdef CONFIG_DEBUG_FS
void dw_hdma_v0_debugfs_on(struct dw_edma *dw);
#else
static inline void dw_hdma_v0_debugfs_on(struct dw_edma *dw)
{
}
#endif  

#endif  
