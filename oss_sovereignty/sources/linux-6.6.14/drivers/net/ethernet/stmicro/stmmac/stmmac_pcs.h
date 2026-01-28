


#ifndef __STMMAC_PCS_H__
#define __STMMAC_PCS_H__

#include <linux/slab.h>
#include <linux/io.h>
#include "common.h"


#define GMAC_AN_CTRL(x)		(x)		
#define GMAC_AN_STATUS(x)	(x + 0x4)	
#define GMAC_ANE_ADV(x)		(x + 0x8)	
#define GMAC_ANE_LPA(x)		(x + 0xc)	
#define GMAC_ANE_EXP(x)		(x + 0x10)	
#define GMAC_TBI(x)		(x + 0x14)	


#define GMAC_AN_CTRL_RAN	BIT(9)	
#define GMAC_AN_CTRL_ANE	BIT(12)	
#define GMAC_AN_CTRL_ELE	BIT(14)	
#define GMAC_AN_CTRL_ECD	BIT(16)	
#define GMAC_AN_CTRL_LR		BIT(17)	
#define GMAC_AN_CTRL_SGMRAL	BIT(18)	


#define GMAC_AN_STATUS_LS	BIT(2)	
#define GMAC_AN_STATUS_ANA	BIT(3)	
#define GMAC_AN_STATUS_ANC	BIT(5)	
#define GMAC_AN_STATUS_ES	BIT(8)	


#define GMAC_ANE_FD		BIT(5)
#define GMAC_ANE_HD		BIT(6)
#define GMAC_ANE_PSE		GENMASK(8, 7)
#define GMAC_ANE_PSE_SHIFT	7
#define GMAC_ANE_RFE		GENMASK(13, 12)
#define GMAC_ANE_RFE_SHIFT	12
#define GMAC_ANE_ACK		BIT(14)


static inline void dwmac_pcs_isr(void __iomem *ioaddr, u32 reg,
				 unsigned int intr_status,
				 struct stmmac_extra_stats *x)
{
	u32 val = readl(ioaddr + GMAC_AN_STATUS(reg));

	if (intr_status & PCS_ANE_IRQ) {
		x->irq_pcs_ane_n++;
		if (val & GMAC_AN_STATUS_ANC)
			pr_info("stmmac_pcs: ANE process completed\n");
	}

	if (intr_status & PCS_LINK_IRQ) {
		x->irq_pcs_link_n++;
		if (val & GMAC_AN_STATUS_LS)
			pr_info("stmmac_pcs: Link Up\n");
		else
			pr_info("stmmac_pcs: Link Down\n");
	}
}


static inline void dwmac_rane(void __iomem *ioaddr, u32 reg, bool restart)
{
	u32 value = readl(ioaddr + GMAC_AN_CTRL(reg));

	if (restart)
		value |= GMAC_AN_CTRL_RAN;

	writel(value, ioaddr + GMAC_AN_CTRL(reg));
}


static inline void dwmac_ctrl_ane(void __iomem *ioaddr, u32 reg, bool ane,
				  bool srgmi_ral, bool loopback)
{
	u32 value = readl(ioaddr + GMAC_AN_CTRL(reg));

	
	if (ane)
		value |= GMAC_AN_CTRL_ANE | GMAC_AN_CTRL_RAN;

	
	if (srgmi_ral)
		value |= GMAC_AN_CTRL_SGMRAL;

	if (loopback)
		value |= GMAC_AN_CTRL_ELE;

	writel(value, ioaddr + GMAC_AN_CTRL(reg));
}


static inline void dwmac_get_adv_lp(void __iomem *ioaddr, u32 reg,
				    struct rgmii_adv *adv_lp)
{
	u32 value = readl(ioaddr + GMAC_ANE_ADV(reg));

	if (value & GMAC_ANE_FD)
		adv_lp->duplex = DUPLEX_FULL;
	if (value & GMAC_ANE_HD)
		adv_lp->duplex |= DUPLEX_HALF;

	adv_lp->pause = (value & GMAC_ANE_PSE) >> GMAC_ANE_PSE_SHIFT;

	value = readl(ioaddr + GMAC_ANE_LPA(reg));

	if (value & GMAC_ANE_FD)
		adv_lp->lp_duplex = DUPLEX_FULL;
	if (value & GMAC_ANE_HD)
		adv_lp->lp_duplex = DUPLEX_HALF;

	adv_lp->lp_pause = (value & GMAC_ANE_PSE) >> GMAC_ANE_PSE_SHIFT;
}
#endif 
