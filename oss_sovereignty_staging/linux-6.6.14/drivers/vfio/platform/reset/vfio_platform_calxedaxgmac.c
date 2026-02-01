
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include "../vfio_platform_private.h"

#define DRIVER_VERSION  "0.1"
#define DRIVER_AUTHOR   "Eric Auger <eric.auger@linaro.org>"
#define DRIVER_DESC     "Reset support for Calxeda xgmac vfio platform device"

 
#define XGMAC_CONTROL           0x00000000       

 
#define XGMAC_DMA_CONTROL       0x00000f18       
#define XGMAC_DMA_INTR_ENA      0x00000f1c       

 
#define DMA_CONTROL_ST          0x00002000       
#define DMA_CONTROL_SR          0x00000002       

 
#define MAC_ENABLE_TX           0x00000008       
#define MAC_ENABLE_RX           0x00000004       

static inline void xgmac_mac_disable(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + XGMAC_DMA_CONTROL);

	value &= ~(DMA_CONTROL_ST | DMA_CONTROL_SR);
	writel(value, ioaddr + XGMAC_DMA_CONTROL);

	value = readl(ioaddr + XGMAC_CONTROL);
	value &= ~(MAC_ENABLE_TX | MAC_ENABLE_RX);
	writel(value, ioaddr + XGMAC_CONTROL);
}

static int vfio_platform_calxedaxgmac_reset(struct vfio_platform_device *vdev)
{
	struct vfio_platform_region *reg = &vdev->regions[0];

	if (!reg->ioaddr) {
		reg->ioaddr =
			ioremap(reg->addr, reg->size);
		if (!reg->ioaddr)
			return -ENOMEM;
	}

	 
	writel(0, reg->ioaddr + XGMAC_DMA_INTR_ENA);

	 
	xgmac_mac_disable(reg->ioaddr);

	return 0;
}

module_vfio_reset_handler("calxeda,hb-xgmac", vfio_platform_calxedaxgmac_reset);

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
