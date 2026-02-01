
 

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/ipaq-micro.h>
#include <linux/leds.h>

#define LED_YELLOW	0x00
#define LED_GREEN	0x01

#define LED_EN       (1 << 4)  
#define LED_AUTOSTOP (1 << 5)  
#define LED_ALWAYS   (1 << 6)  

static int micro_leds_brightness_set(struct led_classdev *led_cdev,
				      enum led_brightness value)
{
	struct ipaq_micro *micro = dev_get_drvdata(led_cdev->dev->parent->parent);
	 
	struct ipaq_micro_msg msg = {
		.id = MSG_NOTIFY_LED,
		.tx_len = 4,
	};

	msg.tx_data[0] = LED_GREEN;
	msg.tx_data[1] = 0;
	if (value) {
		msg.tx_data[2] = 0;  
		msg.tx_data[3] = 1;
	} else {
		msg.tx_data[2] = 1;
		msg.tx_data[3] = 0;  
	}
	return ipaq_micro_tx_msg_sync(micro, &msg);
}

 
#define IPAQ_LED_MAX_DUTY 25600

static int micro_leds_blink_set(struct led_classdev *led_cdev,
				unsigned long *delay_on,
				unsigned long *delay_off)
{
	struct ipaq_micro *micro = dev_get_drvdata(led_cdev->dev->parent->parent);
	 
	struct ipaq_micro_msg msg = {
		.id = MSG_NOTIFY_LED,
		.tx_len = 4,
	};

	msg.tx_data[0] = LED_GREEN;
	if (*delay_on > IPAQ_LED_MAX_DUTY ||
	    *delay_off > IPAQ_LED_MAX_DUTY)
		return -EINVAL;

	if (*delay_on == 0 && *delay_off == 0) {
		*delay_on = 100;
		*delay_off = 100;
	}

	msg.tx_data[1] = 0;
	if (*delay_on >= IPAQ_LED_MAX_DUTY)
		msg.tx_data[2] = 0;
	else
		msg.tx_data[2] = (u8) DIV_ROUND_CLOSEST(*delay_on, 100);
	if (*delay_off >= IPAQ_LED_MAX_DUTY)
		msg.tx_data[3] = 0;
	else
		msg.tx_data[3] = (u8) DIV_ROUND_CLOSEST(*delay_off, 100);
	return ipaq_micro_tx_msg_sync(micro, &msg);
}

static struct led_classdev micro_led = {
	.name			= "led-ipaq-micro",
	.brightness_set_blocking = micro_leds_brightness_set,
	.blink_set		= micro_leds_blink_set,
	.flags			= LED_CORE_SUSPENDRESUME,
};

static int micro_leds_probe(struct platform_device *pdev)
{
	int ret;

	ret = devm_led_classdev_register(&pdev->dev, &micro_led);
	if (ret) {
		dev_err(&pdev->dev, "registering led failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "iPAQ micro notification LED driver\n");

	return 0;
}

static struct platform_driver micro_leds_device_driver = {
	.driver = {
		.name    = "ipaq-micro-leds",
	},
	.probe   = micro_leds_probe,
};
module_platform_driver(micro_leds_device_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("driver for iPAQ Atmel micro leds");
MODULE_ALIAS("platform:ipaq-micro-leds");
