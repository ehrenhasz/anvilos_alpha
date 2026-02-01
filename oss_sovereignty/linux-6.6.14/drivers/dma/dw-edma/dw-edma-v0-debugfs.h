 
 

#ifndef _DW_EDMA_V0_DEBUG_FS_H
#define _DW_EDMA_V0_DEBUG_FS_H

#include <linux/dma/edma.h>

#ifdef CONFIG_DEBUG_FS
void dw_edma_v0_debugfs_on(struct dw_edma *dw);
#else
static inline void dw_edma_v0_debugfs_on(struct dw_edma *dw)
{
}
#endif  

#endif  
