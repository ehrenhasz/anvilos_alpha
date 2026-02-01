 

#include "bcma_private.h"
#include <linux/bcma/bcma.h>

void bcma_core_gmac_cmn_init(struct bcma_drv_gmac_cmn *gc)
{
	mutex_init(&gc->phy_mutex);
}
