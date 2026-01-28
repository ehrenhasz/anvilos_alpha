


#ifndef _SOC_TI_PRUSS_H_
#define _SOC_TI_PRUSS_H_

#include <linux/bits.h>
#include <linux/regmap.h>


#define PRUSS_CFG_REVID         0x00
#define PRUSS_CFG_SYSCFG        0x04
#define PRUSS_CFG_GPCFG(x)      (0x08 + (x) * 4)
#define PRUSS_CFG_CGR           0x10
#define PRUSS_CFG_ISRP          0x14
#define PRUSS_CFG_ISP           0x18
#define PRUSS_CFG_IESP          0x1C
#define PRUSS_CFG_IECP          0x20
#define PRUSS_CFG_SCRP          0x24
#define PRUSS_CFG_PMAO          0x28
#define PRUSS_CFG_MII_RT        0x2C
#define PRUSS_CFG_IEPCLK        0x30
#define PRUSS_CFG_SPP           0x34
#define PRUSS_CFG_PIN_MX        0x40


#define PRUSS_GPCFG_PRU_GPI_MODE_MASK           GENMASK(1, 0)
#define PRUSS_GPCFG_PRU_GPI_MODE_SHIFT          0

#define PRUSS_GPCFG_PRU_MUX_SEL_SHIFT           26
#define PRUSS_GPCFG_PRU_MUX_SEL_MASK            GENMASK(29, 26)


#define PRUSS_MII_RT_EVENT_EN                   BIT(0)


#define PRUSS_SPP_XFER_SHIFT_EN                 BIT(1)
#define PRUSS_SPP_PRU1_PAD_HP_EN                BIT(0)
#define PRUSS_SPP_RTU_XFR_SHIFT_EN              BIT(3)


static int pruss_cfg_read(struct pruss *pruss, unsigned int reg, unsigned int *val)
{
	if (IS_ERR_OR_NULL(pruss))
		return -EINVAL;

	return regmap_read(pruss->cfg_regmap, reg, val);
}


static int pruss_cfg_update(struct pruss *pruss, unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	if (IS_ERR_OR_NULL(pruss))
		return -EINVAL;

	return regmap_update_bits(pruss->cfg_regmap, reg, mask, val);
}

#endif  
