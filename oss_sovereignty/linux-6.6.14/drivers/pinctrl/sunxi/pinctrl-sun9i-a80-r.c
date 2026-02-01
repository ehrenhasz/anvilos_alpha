 

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const struct sunxi_desc_pin sun9i_a80_r_pins[] = {
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 0),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_uart"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 0)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_uart"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 1)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 2),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 2)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 3),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 3)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 4),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 4)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 5),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 5)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 6),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_cir_rx"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 6)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 7),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "1wire"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 7)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 8),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_ps2"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 8)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 9),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_ps2"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 0, 9)),	 

	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 0),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 0)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 1)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 2),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 2)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 3),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 3)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 4),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_i2s1"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 4)),	 

	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 8),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_i2c1"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 8)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 9),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_i2c1"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 9)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 10),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2s0"),	 
		  SUNXI_FUNCTION(0x3, "s_i2s1")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 11),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2s0"),	 
		  SUNXI_FUNCTION(0x3, "s_i2s1")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 12),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2s0"),	 
		  SUNXI_FUNCTION(0x3, "s_i2s1")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 13),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2s0"),	 
		  SUNXI_FUNCTION(0x3, "s_i2s1")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 14),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2s0"),	 
		  SUNXI_FUNCTION(0x3, "s_i2s1")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(M, 15),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x6, 1, 15)),	 

	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(N, 0),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2c0"),	 
		  SUNXI_FUNCTION(0x3, "s_rsb")),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(N, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_i2c0"),	 
		  SUNXI_FUNCTION(0x3, "s_rsb")),	 
};

static const struct sunxi_pinctrl_desc sun9i_a80_r_pinctrl_data = {
	.pins = sun9i_a80_r_pins,
	.npins = ARRAY_SIZE(sun9i_a80_r_pins),
	.pin_base = PL_BASE,
	.irq_banks = 2,
	.disable_strict_mode = true,
	.io_bias_cfg_variant = BIAS_VOLTAGE_GRP_CONFIG,
};

static int sun9i_a80_r_pinctrl_probe(struct platform_device *pdev)
{
	return sunxi_pinctrl_init(pdev,
				  &sun9i_a80_r_pinctrl_data);
}

static const struct of_device_id sun9i_a80_r_pinctrl_match[] = {
	{ .compatible = "allwinner,sun9i-a80-r-pinctrl", },
	{}
};

static struct platform_driver sun9i_a80_r_pinctrl_driver = {
	.probe	= sun9i_a80_r_pinctrl_probe,
	.driver	= {
		.name		= "sun9i-a80-r-pinctrl",
		.owner		= THIS_MODULE,
		.of_match_table	= sun9i_a80_r_pinctrl_match,
	},
};
builtin_platform_driver(sun9i_a80_r_pinctrl_driver);
