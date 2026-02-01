 

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-sunxi.h"

static const struct sunxi_desc_pin sun8i_a23_r_pins[] = {
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 0),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_rsb"),		 
		  SUNXI_FUNCTION(0x3, "s_i2c"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 0)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 1),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_rsb"),		 
		  SUNXI_FUNCTION(0x3, "s_i2c"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 1)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 2),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_uart"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 2)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 3),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_uart"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 3)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 4),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 4)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 5),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 5)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 6),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 6)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 7),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x3, "s_jtag"),	 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 7)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 8),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_twi"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 8)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 9),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_twi"),		 
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 9)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 10),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION(0x2, "s_pwm"),
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 10)),	 
	SUNXI_PIN(SUNXI_PINCTRL_PIN(L, 11),
		  SUNXI_FUNCTION(0x0, "gpio_in"),
		  SUNXI_FUNCTION(0x1, "gpio_out"),
		  SUNXI_FUNCTION_IRQ_BANK(0x4, 0, 11)),	 
};

static const struct sunxi_pinctrl_desc sun8i_a23_r_pinctrl_data = {
	.pins = sun8i_a23_r_pins,
	.npins = ARRAY_SIZE(sun8i_a23_r_pins),
	.pin_base = PL_BASE,
	.irq_banks = 1,
	.disable_strict_mode = true,
};

static int sun8i_a23_r_pinctrl_probe(struct platform_device *pdev)
{
	return sunxi_pinctrl_init(pdev, &sun8i_a23_r_pinctrl_data);
}

static const struct of_device_id sun8i_a23_r_pinctrl_match[] = {
	{ .compatible = "allwinner,sun8i-a23-r-pinctrl", },
	{}
};

static struct platform_driver sun8i_a23_r_pinctrl_driver = {
	.probe	= sun8i_a23_r_pinctrl_probe,
	.driver	= {
		.name		= "sun8i-a23-r-pinctrl",
		.of_match_table	= sun8i_a23_r_pinctrl_match,
	},
};
builtin_platform_driver(sun8i_a23_r_pinctrl_driver);
