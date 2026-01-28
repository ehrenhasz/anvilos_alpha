#ifndef __PLAT_DEVS_H
#define __PLAT_DEVS_H __FILE__
#include <linux/platform_device.h>
struct s3c24xx_uart_resources {
	struct resource		*resources;
	unsigned long		 nr_resources;
};
extern struct s3c24xx_uart_resources s3c2410_uart_resources[];
extern struct s3c24xx_uart_resources s3c64xx_uart_resources[];
extern struct platform_device *s3c24xx_uart_devs[];
extern struct platform_device *s3c24xx_uart_src[];
extern struct platform_device s3c64xx_device_iis0;
extern struct platform_device s3c64xx_device_iis1;
extern struct platform_device s3c64xx_device_spi0;
extern struct platform_device s3c_device_fb;
extern struct platform_device s3c_device_hsmmc0;
extern struct platform_device s3c_device_hsmmc1;
extern struct platform_device s3c_device_hsmmc2;
extern struct platform_device s3c_device_hsmmc3;
extern struct platform_device s3c_device_i2c0;
extern struct platform_device s3c_device_i2c1;
extern struct platform_device s3c_device_ohci;
extern struct platform_device s3c_device_usb_hsotg;
extern struct platform_device samsung_device_keypad;
extern struct platform_device samsung_device_pwm;
extern void *s3c_set_platdata(void *pd, size_t pdsize,
			      struct platform_device *pdev);
#endif  
