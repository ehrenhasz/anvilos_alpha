
 

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

 
#define USBDRD_UCTL_CTL				0x00
 
# define USBDRD_UCTL_CTL_CLEAR_BIST		BIT_ULL(63)
 
# define USBDRD_UCTL_CTL_START_BIST		BIT_ULL(62)
 
# define USBDRD_UCTL_CTL_REF_CLK_SEL		GENMASK_ULL(61, 60)
 
# define USBDRD_UCTL_CTL_SSC_EN			BIT_ULL(59)
 
# define USBDRD_UCTL_CTL_SSC_RANGE		GENMASK_ULL(58, 56)
 
# define USBDRD_UCTL_CTL_SSC_REF_CLK_SEL	GENMASK_ULL(55, 47)
 
# define USBDRD_UCTL_CTL_MPLL_MULTIPLIER	GENMASK_ULL(46, 40)
 
# define USBDRD_UCTL_CTL_REF_SSP_EN		BIT_ULL(39)
 
# define USBDRD_UCTL_CTL_REF_CLK_DIV2		BIT_ULL(38)
 
# define USBDRD_UCTL_CTL_REF_CLK_FSEL		GENMASK_ULL(37, 32)
 
# define USBDRD_UCTL_CTL_H_CLK_EN		BIT_ULL(30)
 
# define USBDRD_UCTL_CTL_H_CLK_BYP_SEL		BIT_ULL(29)
 
# define USBDRD_UCTL_CTL_H_CLKDIV_RST		BIT_ULL(28)
 
# define USBDRD_UCTL_CTL_H_CLKDIV_SEL		GENMASK_ULL(26, 24)
 
# define USBDRD_UCTL_CTL_USB3_PORT_PERM_ATTACH	BIT_ULL(21)
 
# define USBDRD_UCTL_CTL_USB2_PORT_PERM_ATTACH	BIT_ULL(20)
 
# define USBDRD_UCTL_CTL_USB3_PORT_DISABLE	BIT_ULL(18)
 
# define USBDRD_UCTL_CTL_USB2_PORT_DISABLE	BIT_ULL(16)
 
# define USBDRD_UCTL_CTL_SS_POWER_EN		BIT_ULL(14)
 
# define USBDRD_UCTL_CTL_HS_POWER_EN		BIT_ULL(12)
 
# define USBDRD_UCTL_CTL_CSCLK_EN		BIT_ULL(4)
 
# define USBDRD_UCTL_CTL_DRD_MODE		BIT_ULL(3)
 
# define USBDRD_UCTL_CTL_UPHY_RST		BIT_ULL(2)
 
# define USBDRD_UCTL_CTL_UAHC_RST		BIT_ULL(1)
 
# define USBDRD_UCTL_CTL_UCTL_RST		BIT_ULL(0)

#define USBDRD_UCTL_BIST_STATUS			0x08
#define USBDRD_UCTL_SPARE0			0x10
#define USBDRD_UCTL_INTSTAT			0x30
#define USBDRD_UCTL_PORT_CFG_HS(port)		(0x40 + (0x20 * port))
#define USBDRD_UCTL_PORT_CFG_SS(port)		(0x48 + (0x20 * port))
#define USBDRD_UCTL_PORT_CR_DBG_CFG(port)	(0x50 + (0x20 * port))
#define USBDRD_UCTL_PORT_CR_DBG_STATUS(port)	(0x58 + (0x20 * port))

 
#define USBDRD_UCTL_HOST_CFG			0xe0
 
# define USBDRD_UCTL_HOST_CFG_HOST_CURRENT_BELT	GENMASK_ULL(59, 48)
 
# define USBDRD_UCTL_HOST_CFG_FLA		GENMASK_ULL(37, 32)
 
# define USBDRD_UCTL_HOST_CFG_BME		BIT_ULL(28)
 
# define USBDRD_UCTL_HOST_OCI_EN		BIT_ULL(27)
 
# define USBDRD_UCTL_HOST_OCI_ACTIVE_HIGH_EN	BIT_ULL(26)
 
# define USBDRD_UCTL_HOST_PPC_EN		BIT_ULL(25)
 
# define USBDRD_UCTL_HOST_PPC_ACTIVE_HIGH_EN	BIT_ULL(24)

 
#define USBDRD_UCTL_SHIM_CFG			0xe8
 
# define USBDRD_UCTL_SHIM_CFG_XS_NCB_OOB_WRN	BIT_ULL(63)
 
# define USBDRD_UCTL_SHIM_CFG_XS_NCB_OOB_OSRC	GENMASK_ULL(59, 48)
 
# define USBDRD_UCTL_SHIM_CFG_XM_BAD_DMA_WRN	BIT_ULL(47)
 
# define USBDRD_UCTL_SHIM_CFG_XM_BAD_DMA_TYPE	GENMASK_ULL(43, 40)
 
# define USBDRD_UCTL_SHIM_CFG_DMA_READ_CMD	BIT_ULL(12)
 
# define USBDRD_UCTL_SHIM_CFG_DMA_ENDIAN_MODE	GENMASK_ULL(9, 8)
 
# define USBDRD_UCTL_SHIM_CFG_CSR_ENDIAN_MODE	GENMASK_ULL(1, 0)

#define USBDRD_UCTL_ECC				0xf0
#define USBDRD_UCTL_SPARE1			0xf8

struct dwc3_octeon {
	struct device *dev;
	void __iomem *base;
};

#define DWC3_GPIO_POWER_NONE	(-1)

#ifdef CONFIG_CAVIUM_OCTEON_SOC
#include <asm/octeon/octeon.h>
static inline uint64_t dwc3_octeon_readq(void __iomem *addr)
{
	return cvmx_readq_csr(addr);
}

static inline void dwc3_octeon_writeq(void __iomem *base, uint64_t val)
{
	cvmx_writeq_csr(base, val);
}

static void dwc3_octeon_config_gpio(int index, int gpio)
{
	union cvmx_gpio_bit_cfgx gpio_bit;

	if ((OCTEON_IS_MODEL(OCTEON_CN73XX) ||
	    OCTEON_IS_MODEL(OCTEON_CNF75XX))
	    && gpio <= 31) {
		gpio_bit.u64 = cvmx_read_csr(CVMX_GPIO_BIT_CFGX(gpio));
		gpio_bit.s.tx_oe = 1;
		gpio_bit.s.output_sel = (index == 0 ? 0x14 : 0x15);
		cvmx_write_csr(CVMX_GPIO_BIT_CFGX(gpio), gpio_bit.u64);
	} else if (gpio <= 15) {
		gpio_bit.u64 = cvmx_read_csr(CVMX_GPIO_BIT_CFGX(gpio));
		gpio_bit.s.tx_oe = 1;
		gpio_bit.s.output_sel = (index == 0 ? 0x14 : 0x19);
		cvmx_write_csr(CVMX_GPIO_BIT_CFGX(gpio), gpio_bit.u64);
	} else {
		gpio_bit.u64 = cvmx_read_csr(CVMX_GPIO_XBIT_CFGX(gpio));
		gpio_bit.s.tx_oe = 1;
		gpio_bit.s.output_sel = (index == 0 ? 0x14 : 0x19);
		cvmx_write_csr(CVMX_GPIO_XBIT_CFGX(gpio), gpio_bit.u64);
	}
}
#else
static inline uint64_t dwc3_octeon_readq(void __iomem *addr)
{
	return 0;
}

static inline void dwc3_octeon_writeq(void __iomem *base, uint64_t val) { }

static inline void dwc3_octeon_config_gpio(int index, int gpio) { }

static uint64_t octeon_get_io_clock_rate(void)
{
	return 150000000;
}
#endif

static int dwc3_octeon_get_divider(void)
{
	static const uint8_t clk_div[] = { 1, 2, 4, 6, 8, 16, 24, 32 };
	int div = 0;

	while (div < ARRAY_SIZE(clk_div)) {
		uint64_t rate = octeon_get_io_clock_rate() / clk_div[div];
		if (rate <= 300000000 && rate >= 150000000)
			return div;
		div++;
	}

	return -EINVAL;
}

static int dwc3_octeon_setup(struct dwc3_octeon *octeon,
			     int ref_clk_sel, int ref_clk_fsel, int mpll_mul,
			     int power_gpio, int power_active_low)
{
	u64 val;
	int div;
	struct device *dev = octeon->dev;
	void __iomem *uctl_ctl_reg = octeon->base + USBDRD_UCTL_CTL;
	void __iomem *uctl_host_cfg_reg = octeon->base + USBDRD_UCTL_HOST_CFG;

	 

	 

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val |= USBDRD_UCTL_CTL_UPHY_RST |
	       USBDRD_UCTL_CTL_UAHC_RST |
	       USBDRD_UCTL_CTL_UCTL_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val |= USBDRD_UCTL_CTL_H_CLKDIV_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	div = dwc3_octeon_get_divider();
	if (div < 0) {
		dev_err(dev, "clock divider invalid\n");
		return div;
	}
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_H_CLKDIV_SEL;
	val |= FIELD_PREP(USBDRD_UCTL_CTL_H_CLKDIV_SEL, div);
	val |= USBDRD_UCTL_CTL_H_CLK_EN;
	dwc3_octeon_writeq(uctl_ctl_reg, val);
	val = dwc3_octeon_readq(uctl_ctl_reg);
	if ((div != FIELD_GET(USBDRD_UCTL_CTL_H_CLKDIV_SEL, val)) ||
	    (!(FIELD_GET(USBDRD_UCTL_CTL_H_CLK_EN, val)))) {
		dev_err(dev, "clock init failure (UCTL_CTL=%016llx)\n", val);
		return -EINVAL;
	}

	 
	val &= ~USBDRD_UCTL_CTL_H_CLKDIV_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_REF_CLK_DIV2;
	val &= ~USBDRD_UCTL_CTL_REF_CLK_SEL;
	val |= FIELD_PREP(USBDRD_UCTL_CTL_REF_CLK_SEL, ref_clk_sel);

	val &= ~USBDRD_UCTL_CTL_REF_CLK_FSEL;
	val |= FIELD_PREP(USBDRD_UCTL_CTL_REF_CLK_FSEL, ref_clk_fsel);

	val &= ~USBDRD_UCTL_CTL_MPLL_MULTIPLIER;
	val |= FIELD_PREP(USBDRD_UCTL_CTL_MPLL_MULTIPLIER, mpll_mul);

	 
	val |= USBDRD_UCTL_CTL_SSC_EN;

	 
	val |= USBDRD_UCTL_CTL_REF_SSP_EN;

	 

	 
	val |= USBDRD_UCTL_CTL_HS_POWER_EN;
	val |= USBDRD_UCTL_CTL_SS_POWER_EN;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	udelay(10);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_UCTL_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	udelay(10);

	 
	val = dwc3_octeon_readq(uctl_host_cfg_reg);
	val |= USBDRD_UCTL_HOST_PPC_EN;
	if (power_gpio == DWC3_GPIO_POWER_NONE) {
		val &= ~USBDRD_UCTL_HOST_PPC_EN;
	} else {
		val |= USBDRD_UCTL_HOST_PPC_EN;
		dwc3_octeon_config_gpio(((__force uintptr_t)octeon->base >> 24) & 1,
					power_gpio);
		dev_dbg(dev, "power control is using gpio%d\n", power_gpio);
	}
	if (power_active_low)
		val &= ~USBDRD_UCTL_HOST_PPC_ACTIVE_HIGH_EN;
	else
		val |= USBDRD_UCTL_HOST_PPC_ACTIVE_HIGH_EN;
	dwc3_octeon_writeq(uctl_host_cfg_reg, val);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_UAHC_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	udelay(10);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val |= USBDRD_UCTL_CTL_CSCLK_EN;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	 
	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_DRD_MODE;
	dwc3_octeon_writeq(uctl_ctl_reg, val);

	return 0;
}

static void dwc3_octeon_set_endian_mode(struct dwc3_octeon *octeon)
{
	u64 val;
	void __iomem *uctl_shim_cfg_reg = octeon->base + USBDRD_UCTL_SHIM_CFG;

	val = dwc3_octeon_readq(uctl_shim_cfg_reg);
	val &= ~USBDRD_UCTL_SHIM_CFG_DMA_ENDIAN_MODE;
	val &= ~USBDRD_UCTL_SHIM_CFG_CSR_ENDIAN_MODE;
#ifdef __BIG_ENDIAN
	val |= FIELD_PREP(USBDRD_UCTL_SHIM_CFG_DMA_ENDIAN_MODE, 1);
	val |= FIELD_PREP(USBDRD_UCTL_SHIM_CFG_CSR_ENDIAN_MODE, 1);
#endif
	dwc3_octeon_writeq(uctl_shim_cfg_reg, val);
}

static void dwc3_octeon_phy_reset(struct dwc3_octeon *octeon)
{
	u64 val;
	void __iomem *uctl_ctl_reg = octeon->base + USBDRD_UCTL_CTL;

	val = dwc3_octeon_readq(uctl_ctl_reg);
	val &= ~USBDRD_UCTL_CTL_UPHY_RST;
	dwc3_octeon_writeq(uctl_ctl_reg, val);
}

static int dwc3_octeon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct dwc3_octeon *octeon;
	const char *hs_clock_type, *ss_clock_type;
	int ref_clk_sel, ref_clk_fsel, mpll_mul;
	int power_active_low, power_gpio;
	int err, len;
	u32 clock_rate;

	if (of_property_read_u32(node, "refclk-frequency", &clock_rate)) {
		dev_err(dev, "No UCTL \"refclk-frequency\"\n");
		return -EINVAL;
	}
	if (of_property_read_string(node, "refclk-type-ss", &ss_clock_type)) {
		dev_err(dev, "No UCTL \"refclk-type-ss\"\n");
		return -EINVAL;
	}
	if (of_property_read_string(node, "refclk-type-hs", &hs_clock_type)) {
		dev_err(dev, "No UCTL \"refclk-type-hs\"\n");
		return -EINVAL;
	}

	ref_clk_sel = 2;
	if (strcmp("dlmc_ref_clk0", ss_clock_type) == 0) {
		if (strcmp(hs_clock_type, "dlmc_ref_clk0") == 0)
			ref_clk_sel = 0;
		else if (strcmp(hs_clock_type, "pll_ref_clk"))
			dev_warn(dev, "Invalid HS clock type %s, using pll_ref_clk instead\n",
				 hs_clock_type);
	} else if (strcmp(ss_clock_type, "dlmc_ref_clk1") == 0) {
		if (strcmp(hs_clock_type, "dlmc_ref_clk1") == 0) {
			ref_clk_sel = 1;
		} else {
			ref_clk_sel = 3;
			if (strcmp(hs_clock_type, "pll_ref_clk"))
				dev_warn(dev, "Invalid HS clock type %s, using pll_ref_clk instead\n",
					 hs_clock_type);
		}
	} else {
		dev_warn(dev, "Invalid SS clock type %s, using dlmc_ref_clk0 instead\n",
			 ss_clock_type);
	}

	ref_clk_fsel = 0x07;
	switch (clock_rate) {
	default:
		dev_warn(dev, "Invalid ref_clk %u, using 100000000 instead\n",
			 clock_rate);
		fallthrough;
	case 100000000:
		mpll_mul = 0x19;
		if (ref_clk_sel < 2)
			ref_clk_fsel = 0x27;
		break;
	case 50000000:
		mpll_mul = 0x32;
		break;
	case 125000000:
		mpll_mul = 0x28;
		break;
	}

	power_gpio = DWC3_GPIO_POWER_NONE;
	power_active_low = 0;
	if (of_find_property(node, "power", &len)) {
		u32 gpio_pwr[3];

		switch (len) {
		case 8:
			of_property_read_u32_array(node, "power", gpio_pwr, 2);
			break;
		case 12:
			of_property_read_u32_array(node, "power", gpio_pwr, 3);
			power_active_low = gpio_pwr[2] & 0x01;
			break;
		default:
			dev_err(dev, "invalid power configuration\n");
			return -EINVAL;
		}
		power_gpio = gpio_pwr[1];
	}

	octeon = devm_kzalloc(dev, sizeof(*octeon), GFP_KERNEL);
	if (!octeon)
		return -ENOMEM;

	octeon->dev = dev;
	octeon->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(octeon->base))
		return PTR_ERR(octeon->base);

	err = dwc3_octeon_setup(octeon, ref_clk_sel, ref_clk_fsel, mpll_mul,
				power_gpio, power_active_low);
	if (err)
		return err;

	dwc3_octeon_set_endian_mode(octeon);
	dwc3_octeon_phy_reset(octeon);

	platform_set_drvdata(pdev, octeon);

	return of_platform_populate(node, NULL, NULL, dev);
}

static void dwc3_octeon_remove(struct platform_device *pdev)
{
	struct dwc3_octeon *octeon = platform_get_drvdata(pdev);

	of_platform_depopulate(octeon->dev);
}

static const struct of_device_id dwc3_octeon_of_match[] = {
	{ .compatible = "cavium,octeon-7130-usb-uctl" },
	{ },
};
MODULE_DEVICE_TABLE(of, dwc3_octeon_of_match);

static struct platform_driver dwc3_octeon_driver = {
	.probe		= dwc3_octeon_probe,
	.remove_new	= dwc3_octeon_remove,
	.driver		= {
		.name	= "dwc3-octeon",
		.of_match_table = dwc3_octeon_of_match,
	},
};
module_platform_driver(dwc3_octeon_driver);

MODULE_ALIAS("platform:dwc3-octeon");
MODULE_AUTHOR("Ladislav Michl <ladis@linux-mips.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 OCTEON III Glue Layer");
