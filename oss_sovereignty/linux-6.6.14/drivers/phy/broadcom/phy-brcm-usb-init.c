
 

 

#include <linux/delay.h>
#include <linux/io.h>

#include <linux/soc/brcmstb/brcmstb.h>
#include "phy-brcm-usb-init.h"

#define PHY_PORTS 2
#define PHY_PORT_SELECT_0 0
#define PHY_PORT_SELECT_1 0x1000

 
#define USB_CTRL_SETUP			0x00
#define   USB_CTRL_SETUP_BABO_MASK			BIT(0)
#define   USB_CTRL_SETUP_FNHW_MASK			BIT(1)
#define   USB_CTRL_SETUP_FNBO_MASK			BIT(2)
#define   USB_CTRL_SETUP_WABO_MASK			BIT(3)
#define   USB_CTRL_SETUP_IOC_MASK			BIT(4)
#define   USB_CTRL_SETUP_IPP_MASK			BIT(5)
#define   USB_CTRL_SETUP_SCB_CLIENT_SWAP_MASK		BIT(13)  
#define   USB_CTRL_SETUP_SCB1_EN_MASK			BIT(14)  
#define   USB_CTRL_SETUP_SCB2_EN_MASK			BIT(15)  
#define   USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK		BIT(17)  
#define   USB_CTRL_SETUP_SS_EHCI64BIT_EN_VAR_MASK	BIT(16)  
#define   USB_CTRL_SETUP_STRAP_IPP_SEL_MASK		BIT(25)  
#define   USB_CTRL_SETUP_CC_DRD_MODE_ENABLE_MASK	BIT(26)  
#define   USB_CTRL_SETUP_STRAP_CC_DRD_MODE_ENABLE_SEL_MASK BIT(27)  
#define   USB_CTRL_SETUP_OC_DISABLE_PORT0_MASK		BIT(28)
#define   USB_CTRL_SETUP_OC_DISABLE_PORT1_MASK		BIT(29)
#define   USB_CTRL_SETUP_OC_DISABLE_MASK		GENMASK(29, 28)  
#define   USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK		BIT(30)
#define   USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK		BIT(31)
#define   USB_CTRL_SETUP_OC3_DISABLE_MASK		GENMASK(31, 30)  
#define USB_CTRL_PLL_CTL		0x04
#define   USB_CTRL_PLL_CTL_PLL_SUSPEND_EN_MASK		BIT(27)
#define   USB_CTRL_PLL_CTL_PLL_RESETB_MASK		BIT(30)
#define   USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_MASK		BIT(31)  
#define USB_CTRL_EBRIDGE		0x0c
#define   USB_CTRL_EBRIDGE_EBR_SCB_SIZE_MASK		GENMASK(11, 7)  
#define   USB_CTRL_EBRIDGE_ESTOP_SCB_REQ_MASK		BIT(17)  
#define USB_CTRL_OBRIDGE		0x10
#define   USB_CTRL_OBRIDGE_LS_KEEP_ALIVE_MASK		BIT(27)
#define USB_CTRL_MDIO			0x14
#define USB_CTRL_MDIO2			0x18
#define USB_CTRL_UTMI_CTL_1		0x2c
#define   USB_CTRL_UTMI_CTL_1_POWER_UP_FSM_EN_MASK	BIT(11)
#define   USB_CTRL_UTMI_CTL_1_POWER_UP_FSM_EN_P1_MASK	BIT(27)
#define USB_CTRL_USB_PM			0x34
#define   USB_CTRL_USB_PM_RMTWKUP_EN_MASK		BIT(0)
#define   USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK	GENMASK(21, 20)  
#define   USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK		BIT(22)  
#define   USB_CTRL_USB_PM_BDC_SOFT_RESETB_MASK		BIT(23)  
#define   USB_CTRL_USB_PM_USB20_HC_RESETB_MASK		GENMASK(29, 28)  
#define   USB_CTRL_USB_PM_XHC_SOFT_RESETB_VAR_MASK	BIT(30)  
#define   USB_CTRL_USB_PM_SOFT_RESET_MASK		BIT(30)  
#define   USB_CTRL_USB_PM_USB_PWRDN_MASK		BIT(31)  
#define USB_CTRL_USB_PM_STATUS		0x38
#define USB_CTRL_USB30_CTL1		0x60
#define   USB_CTRL_USB30_CTL1_PHY3_PLL_SEQ_START_MASK	BIT(4)
#define   USB_CTRL_USB30_CTL1_PHY3_RESETB_MASK		BIT(16)
#define   USB_CTRL_USB30_CTL1_XHC_SOFT_RESETB_MASK	BIT(17)  
#define   USB_CTRL_USB30_CTL1_USB3_IOC_MASK		BIT(28)  
#define   USB_CTRL_USB30_CTL1_USB3_IPP_MASK		BIT(29)  
#define USB_CTRL_USB30_PCTL		0x70
#define   USB_CTRL_USB30_PCTL_PHY3_SOFT_RESETB_MASK	BIT(1)
#define   USB_CTRL_USB30_PCTL_PHY3_IDDQ_OVERRIDE_MASK	BIT(15)
#define   USB_CTRL_USB30_PCTL_PHY3_SOFT_RESETB_P1_MASK	BIT(17)
#define USB_CTRL_USB_DEVICE_CTL1	0x90
#define   USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK	GENMASK(1, 0)  

 
#define USB_XHCI_EC_IRAADR 0x658
#define USB_XHCI_EC_IRADAT 0x65c

enum brcm_family_type {
	BRCM_FAMILY_3390A0,
	BRCM_FAMILY_4908,
	BRCM_FAMILY_7250B0,
	BRCM_FAMILY_7271A0,
	BRCM_FAMILY_7364A0,
	BRCM_FAMILY_7366C0,
	BRCM_FAMILY_74371A0,
	BRCM_FAMILY_7439B0,
	BRCM_FAMILY_7445D0,
	BRCM_FAMILY_7260A0,
	BRCM_FAMILY_7278A0,
	BRCM_FAMILY_COUNT,
};

#define USB_BRCM_FAMILY(chip) \
	[BRCM_FAMILY_##chip] = __stringify(chip)

static const char *family_names[BRCM_FAMILY_COUNT] = {
	USB_BRCM_FAMILY(3390A0),
	USB_BRCM_FAMILY(4908),
	USB_BRCM_FAMILY(7250B0),
	USB_BRCM_FAMILY(7271A0),
	USB_BRCM_FAMILY(7364A0),
	USB_BRCM_FAMILY(7366C0),
	USB_BRCM_FAMILY(74371A0),
	USB_BRCM_FAMILY(7439B0),
	USB_BRCM_FAMILY(7445D0),
	USB_BRCM_FAMILY(7260A0),
	USB_BRCM_FAMILY(7278A0),
};

enum {
	USB_CTRL_SETUP_SCB1_EN_SELECTOR,
	USB_CTRL_SETUP_SCB2_EN_SELECTOR,
	USB_CTRL_SETUP_SS_EHCI64BIT_EN_SELECTOR,
	USB_CTRL_SETUP_STRAP_IPP_SEL_SELECTOR,
	USB_CTRL_SETUP_OC3_DISABLE_PORT0_SELECTOR,
	USB_CTRL_SETUP_OC3_DISABLE_PORT1_SELECTOR,
	USB_CTRL_SETUP_OC3_DISABLE_SELECTOR,
	USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_SELECTOR,
	USB_CTRL_USB_PM_BDC_SOFT_RESETB_SELECTOR,
	USB_CTRL_USB_PM_XHC_SOFT_RESETB_SELECTOR,
	USB_CTRL_USB_PM_USB_PWRDN_SELECTOR,
	USB_CTRL_USB30_CTL1_XHC_SOFT_RESETB_SELECTOR,
	USB_CTRL_USB30_CTL1_USB3_IOC_SELECTOR,
	USB_CTRL_USB30_CTL1_USB3_IPP_SELECTOR,
	USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_SELECTOR,
	USB_CTRL_USB_PM_SOFT_RESET_SELECTOR,
	USB_CTRL_SETUP_CC_DRD_MODE_ENABLE_SELECTOR,
	USB_CTRL_SETUP_STRAP_CC_DRD_MODE_ENABLE_SEL_SELECTOR,
	USB_CTRL_USB_PM_USB20_HC_RESETB_SELECTOR,
	USB_CTRL_SETUP_ENDIAN_SELECTOR,
	USB_CTRL_SELECTOR_COUNT,
};

#define USB_CTRL_MASK_FAMILY(params, reg, field)			\
	(params->usb_reg_bits_map[USB_CTRL_##reg##_##field##_SELECTOR])

#define USB_CTRL_SET_FAMILY(params, reg, field)	\
	usb_ctrl_set_family(params, USB_CTRL_##reg,	\
			USB_CTRL_##reg##_##field##_SELECTOR)
#define USB_CTRL_UNSET_FAMILY(params, reg, field)	\
	usb_ctrl_unset_family(params, USB_CTRL_##reg,	\
		USB_CTRL_##reg##_##field##_SELECTOR)

#define MDIO_USB2	0
#define MDIO_USB3	BIT(31)

#define USB_CTRL_SETUP_ENDIAN_BITS (	\
		USB_CTRL_MASK(SETUP, BABO) |	\
		USB_CTRL_MASK(SETUP, FNHW) |	\
		USB_CTRL_MASK(SETUP, FNBO) |	\
		USB_CTRL_MASK(SETUP, WABO))

#ifdef __LITTLE_ENDIAN
#define ENDIAN_SETTINGS (			\
		USB_CTRL_MASK(SETUP, BABO) |	\
		USB_CTRL_MASK(SETUP, FNHW))
#else
#define ENDIAN_SETTINGS (			\
		USB_CTRL_MASK(SETUP, FNHW) |	\
		USB_CTRL_MASK(SETUP, FNBO) |	\
		USB_CTRL_MASK(SETUP, WABO))
#endif

struct id_to_type {
	u32 id;
	int type;
};

static const struct id_to_type id_to_type_table[] = {
	{ 0x33900000, BRCM_FAMILY_3390A0 },
	{ 0x72500010, BRCM_FAMILY_7250B0 },
	{ 0x72600000, BRCM_FAMILY_7260A0 },
	{ 0x72550000, BRCM_FAMILY_7260A0 },
	{ 0x72680000, BRCM_FAMILY_7271A0 },
	{ 0x72710000, BRCM_FAMILY_7271A0 },
	{ 0x73640000, BRCM_FAMILY_7364A0 },
	{ 0x73660020, BRCM_FAMILY_7366C0 },
	{ 0x07437100, BRCM_FAMILY_74371A0 },
	{ 0x74390010, BRCM_FAMILY_7439B0 },
	{ 0x74450030, BRCM_FAMILY_7445D0 },
	{ 0x72780000, BRCM_FAMILY_7278A0 },
	{ 0, BRCM_FAMILY_7271A0 },  
};

static const u32
usb_reg_bits_map_table[BRCM_FAMILY_COUNT][USB_CTRL_SELECTOR_COUNT] = {
	 
	[BRCM_FAMILY_3390A0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		USB_CTRL_SETUP_STRAP_IPP_SEL_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		0,  
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_4908] = {
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
	},
	 
	[BRCM_FAMILY_7250B0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		0,  
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_MASK,
		0,  
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_VAR_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7271A0] = {
		0,  
		0,  
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		USB_CTRL_SETUP_STRAP_IPP_SEL_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		USB_CTRL_USB_PM_BDC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK,
		USB_CTRL_USB_PM_SOFT_RESET_MASK,
		USB_CTRL_SETUP_CC_DRD_MODE_ENABLE_MASK,
		USB_CTRL_SETUP_STRAP_CC_DRD_MODE_ENABLE_SEL_MASK,
		USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7364A0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		0,  
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_MASK,
		0,  
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_VAR_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7366C0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		0,  
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		0,  
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_VAR_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_74371A0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_VAR_MASK,
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB30_CTL1_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB30_CTL1_USB3_IOC_MASK,
		USB_CTRL_USB30_CTL1_USB3_IPP_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7439B0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		USB_CTRL_SETUP_STRAP_IPP_SEL_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		USB_CTRL_USB_PM_BDC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7445D0] = {
		USB_CTRL_SETUP_SCB1_EN_MASK,
		USB_CTRL_SETUP_SCB2_EN_MASK,
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_VAR_MASK,
		0,  
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		USB_CTRL_PLL_CTL_PLL_IDDQ_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB30_CTL1_XHC_SOFT_RESETB_MASK,
		0,  
		0,  
		0,  
		0,  
		0,  
		0,  
		USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7260A0] = {
		0,  
		0,  
		USB_CTRL_SETUP_SS_EHCI64BIT_EN_MASK,
		USB_CTRL_SETUP_STRAP_IPP_SEL_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		USB_CTRL_USB_PM_BDC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK,
		USB_CTRL_USB_PM_SOFT_RESET_MASK,
		USB_CTRL_SETUP_CC_DRD_MODE_ENABLE_MASK,
		USB_CTRL_SETUP_STRAP_CC_DRD_MODE_ENABLE_SEL_MASK,
		USB_CTRL_USB_PM_USB20_HC_RESETB_VAR_MASK,
		ENDIAN_SETTINGS,  
	},
	 
	[BRCM_FAMILY_7278A0] = {
		0,  
		0,  
		0,  
		USB_CTRL_SETUP_STRAP_IPP_SEL_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT0_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_PORT1_MASK,
		USB_CTRL_SETUP_OC3_DISABLE_MASK,
		0,  
		USB_CTRL_USB_PM_BDC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_XHC_SOFT_RESETB_MASK,
		USB_CTRL_USB_PM_USB_PWRDN_MASK,
		0,  
		0,  
		0,  
		USB_CTRL_USB_DEVICE_CTL1_PORT_MODE_MASK,
		USB_CTRL_USB_PM_SOFT_RESET_MASK,
		0,  
		0,  
		0,  
		0,  
	},
};

static inline
void usb_ctrl_unset_family(struct brcm_usb_init_params *params,
			   u32 reg_offset, u32 field)
{
	u32 mask;

	mask = params->usb_reg_bits_map[field];
	brcm_usb_ctrl_unset(params->regs[BRCM_REGS_CTRL] + reg_offset, mask);
};

static inline
void usb_ctrl_set_family(struct brcm_usb_init_params *params,
			 u32 reg_offset, u32 field)
{
	u32 mask;

	mask = params->usb_reg_bits_map[field];
	brcm_usb_ctrl_set(params->regs[BRCM_REGS_CTRL] + reg_offset, mask);
};

static u32 brcmusb_usb_mdio_read(void __iomem *ctrl_base, u32 reg, int mode)
{
	u32 data;

	data = (reg << 16) | mode;
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	data |= (1 << 24);
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	data &= ~(1 << 24);
	 
	usleep_range(10, 20);
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	 
	usleep_range(10, 20);

	return brcm_usb_readl(USB_CTRL_REG(ctrl_base, MDIO2)) & 0xffff;
}

static void brcmusb_usb_mdio_write(void __iomem *ctrl_base, u32 reg,
				   u32 val, int mode)
{
	u32 data;

	data = (reg << 16) | val | mode;
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	data |= (1 << 25);
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	data &= ~(1 << 25);

	 
	usleep_range(10, 20);
	brcm_usb_writel(data, USB_CTRL_REG(ctrl_base, MDIO));
	 
	usleep_range(10, 20);
}

static void brcmusb_usb_phy_ldo_fix(void __iomem *ctrl_base)
{
	 
	 
	USB_CTRL_UNSET(ctrl_base, UTMI_CTL_1, POWER_UP_FSM_EN);
	USB_CTRL_UNSET(ctrl_base, UTMI_CTL_1, POWER_UP_FSM_EN_P1);

	 
	USB_CTRL_UNSET(ctrl_base, PLL_CTL, PLL_RESETB);
	 
	udelay(1);
	USB_CTRL_SET(ctrl_base, PLL_CTL, PLL_RESETB);
	 
	usleep_range(1000, 2000);
}

static void brcmusb_usb2_eye_fix(void __iomem *ctrl_base)
{
	 
	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x80a0, MDIO_USB2);
	brcmusb_usb_mdio_write(ctrl_base, 0x0a, 0xc6a0, MDIO_USB2);
}

static void brcmusb_usb3_pll_fix(void __iomem *ctrl_base)
{
	 
	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x8000, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x07, 0x1503, MDIO_USB3);
}

static void brcmusb_usb3_enable_pipe_reset(void __iomem *ctrl_base)
{
	u32 val;

	 
	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x8000, MDIO_USB3);
	val = brcmusb_usb_mdio_read(ctrl_base, 0x0f, MDIO_USB3) | 0x200;
	brcmusb_usb_mdio_write(ctrl_base, 0x0f, val, MDIO_USB3);
}

static void brcmusb_usb3_enable_sigdet(void __iomem *ctrl_base)
{
	u32 val, ofs;
	int ii;

	ofs = 0;
	for (ii = 0; ii < PHY_PORTS; ++ii) {
		 
		brcmusb_usb_mdio_write(ctrl_base, 0x1f, (0x8080 + ofs),
				       MDIO_USB3);
		val = brcmusb_usb_mdio_read(ctrl_base, 0x05, MDIO_USB3);
		val = (val & ~0x800f) | 0x800d;
		brcmusb_usb_mdio_write(ctrl_base, 0x05, val, MDIO_USB3);
		ofs = PHY_PORT_SELECT_1;
	}
}

static void brcmusb_usb3_enable_skip_align(void __iomem *ctrl_base)
{
	u32 val, ofs;
	int ii;

	ofs = 0;
	for (ii = 0; ii < PHY_PORTS; ++ii) {
		 
		brcmusb_usb_mdio_write(ctrl_base, 0x1f, (0x8060 + ofs),
				       MDIO_USB3);
		val = brcmusb_usb_mdio_read(ctrl_base, 0x01, MDIO_USB3) | 0x200;
		brcmusb_usb_mdio_write(ctrl_base, 0x01, val, MDIO_USB3);
		ofs = PHY_PORT_SELECT_1;
	}
}

static void brcmusb_usb3_unfreeze_aeq(void __iomem *ctrl_base)
{
	u32 val, ofs;
	int ii;

	ofs = 0;
	for (ii = 0; ii < PHY_PORTS; ++ii) {
		 
		brcmusb_usb_mdio_write(ctrl_base, 0x1f, (0x80e0 + ofs),
				       MDIO_USB3);
		val = brcmusb_usb_mdio_read(ctrl_base, 0x01, MDIO_USB3);
		val &= ~0x0008;
		brcmusb_usb_mdio_write(ctrl_base, 0x01, val, MDIO_USB3);
		ofs = PHY_PORT_SELECT_1;
	}
}

static void brcmusb_usb3_pll_54mhz(struct brcm_usb_init_params *params)
{
	u32 ofs;
	int ii;
	void __iomem *ctrl_base = params->regs[BRCM_REGS_CTRL];

	 
	switch (params->selected_family) {
	case BRCM_FAMILY_3390A0:
	case BRCM_FAMILY_4908:
	case BRCM_FAMILY_7250B0:
	case BRCM_FAMILY_7366C0:
	case BRCM_FAMILY_74371A0:
	case BRCM_FAMILY_7439B0:
	case BRCM_FAMILY_7445D0:
	case BRCM_FAMILY_7260A0:
		return;
	case BRCM_FAMILY_7364A0:
		if (BRCM_REV(params->family_id) < 0x20)
			return;
		break;
	}

	 
	USB_CTRL_UNSET(ctrl_base, USB30_CTL1, PHY3_PLL_SEQ_START);

	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x8000, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x10, 0x5784, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x11, 0x01d0, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x12, 0x1DE8, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x13, 0xAA80, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x14, 0x8826, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x15, 0x0044, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x16, 0x8000, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x17, 0x0851, MDIO_USB3);
	brcmusb_usb_mdio_write(ctrl_base, 0x18, 0x0000, MDIO_USB3);

	 
	ofs = 0;
	for (ii = 0; ii < PHY_PORTS; ++ii) {
		brcmusb_usb_mdio_write(ctrl_base, 0x1f, (0x8040 + ofs),
				       MDIO_USB3);
		brcmusb_usb_mdio_write(ctrl_base, 0x03, 0x0090, MDIO_USB3);
		brcmusb_usb_mdio_write(ctrl_base, 0x04, 0x0134, MDIO_USB3);
		brcmusb_usb_mdio_write(ctrl_base, 0x1f, (0x8020 + ofs),
				       MDIO_USB3);
		brcmusb_usb_mdio_write(ctrl_base, 0x01, 0x00e2, MDIO_USB3);
		ofs = PHY_PORT_SELECT_1;
	}

	 
	USB_CTRL_SET(ctrl_base, USB30_CTL1, PHY3_PLL_SEQ_START);
	 
	usleep_range(1000, 2000);
}

static void brcmusb_usb3_ssc_enable(void __iomem *ctrl_base)
{
	u32 val;

	 
	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x8040, MDIO_USB3);
	val = brcmusb_usb_mdio_read(ctrl_base, 0x01, MDIO_USB3) | 0xf;
	brcmusb_usb_mdio_write(ctrl_base, 0x01, val, MDIO_USB3);

	 
	brcmusb_usb_mdio_write(ctrl_base, 0x1f, 0x9040, MDIO_USB3);
	val = brcmusb_usb_mdio_read(ctrl_base, 0x01, MDIO_USB3) | 0xf;
	brcmusb_usb_mdio_write(ctrl_base, 0x01, val, MDIO_USB3);
}

static void brcmusb_usb3_phy_workarounds(struct brcm_usb_init_params *params)
{
	void __iomem *ctrl_base = params->regs[BRCM_REGS_CTRL];

	brcmusb_usb3_pll_fix(ctrl_base);
	brcmusb_usb3_pll_54mhz(params);
	brcmusb_usb3_ssc_enable(ctrl_base);
	brcmusb_usb3_enable_pipe_reset(ctrl_base);
	brcmusb_usb3_enable_sigdet(ctrl_base);
	brcmusb_usb3_enable_skip_align(ctrl_base);
	brcmusb_usb3_unfreeze_aeq(ctrl_base);
}

static void brcmusb_memc_fix(struct brcm_usb_init_params *params)
{
	u32 prid;

	if (params->selected_family != BRCM_FAMILY_7445D0)
		return;
	 

	prid = params->product_id & 0xfffff000;
	switch (prid) {
	case 0x72520000:
	case 0x74480000:
	case 0x74490000:
	case 0x07252000:
	case 0x07448000:
	case 0x07449000:
		USB_CTRL_UNSET_FAMILY(params, SETUP, SCB2_EN);
	}
}

static void brcmusb_usb3_otp_fix(struct brcm_usb_init_params *params)
{
	void __iomem *xhci_ec_base = params->regs[BRCM_REGS_XHCI_EC];
	u32 val;

	if (params->family_id != 0x74371000 || !xhci_ec_base)
		return;
	brcm_usb_writel(0xa20c, USB_XHCI_EC_REG(xhci_ec_base, IRAADR));
	val = brcm_usb_readl(USB_XHCI_EC_REG(xhci_ec_base, IRADAT));

	 
	val |= (1 << 27);
	brcm_usb_writel(val, USB_XHCI_EC_REG(xhci_ec_base, IRADAT));

	 
	USB_CTRL_UNSET(params->regs[BRCM_REGS_CTRL], USB30_CTL1, PHY3_RESETB);
	USB_CTRL_SET(params->regs[BRCM_REGS_CTRL], USB30_CTL1, PHY3_RESETB);
}

static void brcmusb_xhci_soft_reset(struct brcm_usb_init_params *params,
				    int on_off)
{
	 
	if (on_off) {
		if (USB_CTRL_MASK_FAMILY(params, USB_PM, XHC_SOFT_RESETB))
			USB_CTRL_UNSET_FAMILY(params, USB_PM, XHC_SOFT_RESETB);
		else
			USB_CTRL_UNSET_FAMILY(params,
					      USB30_CTL1, XHC_SOFT_RESETB);
	} else {  
		if (USB_CTRL_MASK_FAMILY(params, USB_PM, XHC_SOFT_RESETB))
			USB_CTRL_SET_FAMILY(params, USB_PM, XHC_SOFT_RESETB);
		else
			USB_CTRL_SET_FAMILY(params, USB30_CTL1,
					    XHC_SOFT_RESETB);
	}
}

 
static enum brcm_family_type get_family_type(
	struct brcm_usb_init_params *params)
{
	int last_type = -1;
	u32 last_family = 0;
	u32 family_no_major;
	unsigned int x;
	u32 family;

	family = params->family_id & 0xfffffff0;
	family_no_major = params->family_id & 0xffffff00;
	for (x = 0; id_to_type_table[x].id; x++) {
		if (family == id_to_type_table[x].id)
			return id_to_type_table[x].type;
		if (family_no_major == (id_to_type_table[x].id & 0xffffff00))
			if (family > id_to_type_table[x].id &&
			    last_family < id_to_type_table[x].id) {
				last_family = id_to_type_table[x].id;
				last_type = id_to_type_table[x].type;
			}
	}

	 
	if (last_type == -1)
		return id_to_type_table[x].type;
	return last_type;
}

static void usb_init_ipp(struct brcm_usb_init_params *params)
{
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];
	u32 reg;
	u32 orig_reg;

	 
	if (USB_CTRL_MASK_FAMILY(params, USB30_CTL1, USB3_IOC)) {
		if (params->ioc)
			USB_CTRL_SET_FAMILY(params, USB30_CTL1, USB3_IOC);
		if (params->ipp == 1)
			USB_CTRL_SET_FAMILY(params, USB30_CTL1, USB3_IPP);
	}

	reg = brcm_usb_readl(USB_CTRL_REG(ctrl, SETUP));
	orig_reg = reg;
	if (USB_CTRL_MASK_FAMILY(params, SETUP, STRAP_CC_DRD_MODE_ENABLE_SEL))
		 
		reg &= ~(USB_CTRL_MASK_FAMILY(params,
					      SETUP,
					      STRAP_CC_DRD_MODE_ENABLE_SEL));
	if (USB_CTRL_MASK_FAMILY(params, SETUP, STRAP_IPP_SEL))
		 
		if (params->ipp != 2)
			reg &= ~(USB_CTRL_MASK_FAMILY(params, SETUP,
						      STRAP_IPP_SEL));

	 
	reg &= ~(USB_CTRL_MASK(SETUP, IPP) | USB_CTRL_MASK(SETUP, IOC));
	if (params->ioc)
		reg |= USB_CTRL_MASK(SETUP, IOC);
	if (params->ipp == 1)
		reg |= USB_CTRL_MASK(SETUP, IPP);
	brcm_usb_writel(reg, USB_CTRL_REG(ctrl, SETUP));

	 
	if ((reg ^ orig_reg) & USB_CTRL_MASK(SETUP, IPP))
		msleep(50);
}

static void usb_wake_enable(struct brcm_usb_init_params *params,
			  bool enable)
{
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];

	if (enable)
		USB_CTRL_SET(ctrl, USB_PM, RMTWKUP_EN);
	else
		USB_CTRL_UNSET(ctrl, USB_PM, RMTWKUP_EN);
}

static void usb_init_common(struct brcm_usb_init_params *params)
{
	u32 reg;
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];

	 
	usb_wake_enable(params, false);
	reg = brcm_usb_readl(USB_CTRL_REG(ctrl, USB_PM_STATUS));
	brcm_usb_writel(reg, USB_CTRL_REG(ctrl, USB_PM_STATUS));

	 
	if (USB_CTRL_MASK_FAMILY(params, PLL_CTL, PLL_IDDQ_PWRDN)) {
		USB_CTRL_UNSET_FAMILY(params, PLL_CTL, PLL_IDDQ_PWRDN);
		 
		usleep_range(1000, 2000);
	}

	if (USB_CTRL_MASK_FAMILY(params, USB_PM, USB_PWRDN)) {
		USB_CTRL_UNSET_FAMILY(params, USB_PM, USB_PWRDN);
		 
		usleep_range(1000, 2000);
	}

	if (params->selected_family != BRCM_FAMILY_74371A0 &&
	    (BRCM_ID(params->family_id) != 0x7364))
		 
		USB_CTRL_SET_FAMILY(params, SETUP, SS_EHCI64BIT_EN);

	 
	USB_CTRL_SET(ctrl, PLL_CTL, PLL_SUSPEND_EN);

	reg = brcm_usb_readl(USB_CTRL_REG(ctrl, SETUP));
	if (params->selected_family == BRCM_FAMILY_7364A0)
		 
		reg |= USB_CTRL_MASK_FAMILY(params, SETUP, OC3_DISABLE);

	brcmusb_usb_phy_ldo_fix(ctrl);
	brcmusb_usb2_eye_fix(ctrl);

	 
	if (USB_CTRL_MASK_FAMILY(params, SETUP, SCB1_EN))
		reg |= USB_CTRL_MASK_FAMILY(params, SETUP, SCB1_EN);
	if (USB_CTRL_MASK_FAMILY(params, SETUP, SCB2_EN))
		reg |= USB_CTRL_MASK_FAMILY(params, SETUP, SCB2_EN);
	brcm_usb_writel(reg, USB_CTRL_REG(ctrl, SETUP));

	brcmusb_memc_fix(params);

	 
	if ((params->family_id == 0x74390012) &&
	    (params->supported_port_modes != USB_CTLR_MODE_HOST)) {
		USB_CTRL_SET(ctrl, SETUP, OC_DISABLE_PORT1);
		USB_CTRL_SET_FAMILY(params, SETUP, OC3_DISABLE_PORT1);
	}

	if (USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1, PORT_MODE)) {
		reg = brcm_usb_readl(USB_CTRL_REG(ctrl, USB_DEVICE_CTL1));
		reg &= ~USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1,
					PORT_MODE);
		reg |= params->port_mode;
		brcm_usb_writel(reg, USB_CTRL_REG(ctrl, USB_DEVICE_CTL1));
	}
	if (USB_CTRL_MASK_FAMILY(params, USB_PM, BDC_SOFT_RESETB)) {
		switch (params->supported_port_modes) {
		case USB_CTLR_MODE_HOST:
			USB_CTRL_UNSET_FAMILY(params, USB_PM, BDC_SOFT_RESETB);
			break;
		default:
			USB_CTRL_UNSET_FAMILY(params, USB_PM, BDC_SOFT_RESETB);
			USB_CTRL_SET_FAMILY(params, USB_PM, BDC_SOFT_RESETB);
		break;
		}
	}
	if (USB_CTRL_MASK_FAMILY(params, SETUP, CC_DRD_MODE_ENABLE)) {
		if (params->supported_port_modes == USB_CTLR_MODE_TYPEC_PD)
			USB_CTRL_SET_FAMILY(params, SETUP, CC_DRD_MODE_ENABLE);
		else
			USB_CTRL_UNSET_FAMILY(params, SETUP,
					      CC_DRD_MODE_ENABLE);
	}
}

static void usb_init_eohci(struct brcm_usb_init_params *params)
{
	u32 reg;
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];

	if (USB_CTRL_MASK_FAMILY(params, USB_PM, USB20_HC_RESETB))
		USB_CTRL_SET_FAMILY(params, USB_PM, USB20_HC_RESETB);

	if (params->selected_family == BRCM_FAMILY_7366C0)
		 
		USB_CTRL_SET(ctrl, EBRIDGE, ESTOP_SCB_REQ);

	 
	reg = brcm_usb_readl(USB_CTRL_REG(ctrl, SETUP));
	reg &= ~USB_CTRL_SETUP_ENDIAN_BITS;
	reg |= USB_CTRL_MASK_FAMILY(params, SETUP, ENDIAN);
	brcm_usb_writel(reg, USB_CTRL_REG(ctrl, SETUP));

	if (params->selected_family == BRCM_FAMILY_7271A0)
		 
		USB_CTRL_SET(ctrl, OBRIDGE, LS_KEEP_ALIVE);

	if (params->family_id == 0x72550000) {
		 
		reg = brcm_usb_readl(USB_CTRL_REG(ctrl, EBRIDGE));
		reg &= ~USB_CTRL_MASK(EBRIDGE, EBR_SCB_SIZE);
		reg |= 0x800;
		brcm_usb_writel(reg, USB_CTRL_REG(ctrl, EBRIDGE));
	}
}

static void usb_init_xhci(struct brcm_usb_init_params *params)
{
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];

	USB_CTRL_UNSET(ctrl, USB30_PCTL, PHY3_IDDQ_OVERRIDE);
	 
	usleep_range(1000, 2000);

	if (BRCM_ID(params->family_id) == 0x7366) {
		 
		USB_CTRL_SET(ctrl, USB30_PCTL, PHY3_SOFT_RESETB);
		USB_CTRL_SET(ctrl, USB30_PCTL, PHY3_SOFT_RESETB_P1);
	}

	 
	USB_CTRL_UNSET(ctrl, USB30_CTL1, PHY3_PLL_SEQ_START);
	USB_CTRL_SET(ctrl, USB30_CTL1, PHY3_PLL_SEQ_START);

	brcmusb_usb3_phy_workarounds(params);
	brcmusb_xhci_soft_reset(params, 0);
	brcmusb_usb3_otp_fix(params);
}

static void usb_uninit_common(struct brcm_usb_init_params *params)
{
	if (USB_CTRL_MASK_FAMILY(params, USB_PM, USB_PWRDN))
		USB_CTRL_SET_FAMILY(params, USB_PM, USB_PWRDN);

	if (USB_CTRL_MASK_FAMILY(params, PLL_CTL, PLL_IDDQ_PWRDN))
		USB_CTRL_SET_FAMILY(params, PLL_CTL, PLL_IDDQ_PWRDN);
	if (params->wake_enabled)
		usb_wake_enable(params, true);
}

static void usb_uninit_eohci(struct brcm_usb_init_params *params)
{
}

static void usb_uninit_xhci(struct brcm_usb_init_params *params)
{
	brcmusb_xhci_soft_reset(params, 1);
	USB_CTRL_SET(params->regs[BRCM_REGS_CTRL], USB30_PCTL,
		     PHY3_IDDQ_OVERRIDE);
}

static int usb_get_dual_select(struct brcm_usb_init_params *params)
{
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];
	u32 reg = 0;

	pr_debug("%s\n", __func__);
	if (USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1, PORT_MODE)) {
		reg = brcm_usb_readl(USB_CTRL_REG(ctrl, USB_DEVICE_CTL1));
		reg &= USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1,
					PORT_MODE);
	}
	return reg;
}

static void usb_set_dual_select(struct brcm_usb_init_params *params)
{
	void __iomem *ctrl = params->regs[BRCM_REGS_CTRL];
	u32 reg;

	pr_debug("%s\n", __func__);

	if (USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1, PORT_MODE)) {
		reg = brcm_usb_readl(USB_CTRL_REG(ctrl, USB_DEVICE_CTL1));
		reg &= ~USB_CTRL_MASK_FAMILY(params, USB_DEVICE_CTL1,
					PORT_MODE);
		reg |= params->port_mode;
		brcm_usb_writel(reg, USB_CTRL_REG(ctrl, USB_DEVICE_CTL1));
	}
}

static const struct brcm_usb_init_ops bcm7445_ops = {
	.init_ipp = usb_init_ipp,
	.init_common = usb_init_common,
	.init_eohci = usb_init_eohci,
	.init_xhci = usb_init_xhci,
	.uninit_common = usb_uninit_common,
	.uninit_eohci = usb_uninit_eohci,
	.uninit_xhci = usb_uninit_xhci,
	.get_dual_select = usb_get_dual_select,
	.set_dual_select = usb_set_dual_select,
};

void brcm_usb_dvr_init_4908(struct brcm_usb_init_params *params)
{
	int fam;

	fam = BRCM_FAMILY_4908;
	params->selected_family = fam;
	params->usb_reg_bits_map =
		&usb_reg_bits_map_table[fam][0];
	params->family_name = family_names[fam];
	params->ops = &bcm7445_ops;
}

void brcm_usb_dvr_init_7445(struct brcm_usb_init_params *params)
{
	int fam;

	pr_debug("%s\n", __func__);

	fam = get_family_type(params);
	params->selected_family = fam;
	params->usb_reg_bits_map =
		&usb_reg_bits_map_table[fam][0];
	params->family_name = family_names[fam];
	params->ops = &bcm7445_ops;
}
