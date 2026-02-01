
 

 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <asm/mach-au1x00/au1000.h>

 
#define CNTR_OK (SYS_CNTRL_E0 | SYS_CNTRL_32S)

static int au1xtoy_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long t;

	t = alchemy_rdsys(AU1000_SYS_TOYREAD);

	rtc_time64_to_tm(t, tm);

	return 0;
}

static int au1xtoy_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long t;

	t = rtc_tm_to_time64(tm);

	alchemy_wrsys(t, AU1000_SYS_TOYWRITE);

	 
	while (alchemy_rdsys(AU1000_SYS_CNTRCTRL) & SYS_CNTRL_C0S)
		msleep(1);

	return 0;
}

static const struct rtc_class_ops au1xtoy_rtc_ops = {
	.read_time	= au1xtoy_rtc_read_time,
	.set_time	= au1xtoy_rtc_set_time,
};

static int au1xtoy_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtcdev;
	unsigned long t;

	t = alchemy_rdsys(AU1000_SYS_CNTRCTRL);
	if (!(t & CNTR_OK)) {
		dev_err(&pdev->dev, "counters not working; aborting.\n");
		return -ENODEV;
	}

	 
	if (alchemy_rdsys(AU1000_SYS_TOYTRIM) != 32767) {
		 
		t = 0x00100000;
		while ((alchemy_rdsys(AU1000_SYS_CNTRCTRL) & SYS_CNTRL_T0S) && --t)
			msleep(1);

		if (!t) {
			 
			dev_err(&pdev->dev, "timeout waiting for access\n");
			return -ETIMEDOUT;
		}

		 
		alchemy_wrsys(32767, AU1000_SYS_TOYTRIM);
	}

	 
	while (alchemy_rdsys(AU1000_SYS_CNTRCTRL) & SYS_CNTRL_C0S)
		msleep(1);

	rtcdev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtcdev))
		return PTR_ERR(rtcdev);

	rtcdev->ops = &au1xtoy_rtc_ops;
	rtcdev->range_max = U32_MAX;

	platform_set_drvdata(pdev, rtcdev);

	return devm_rtc_register_device(rtcdev);
}

static struct platform_driver au1xrtc_driver = {
	.driver		= {
		.name	= "rtc-au1xxx",
	},
};

module_platform_driver_probe(au1xrtc_driver, au1xtoy_rtc_probe);

MODULE_DESCRIPTION("Au1xxx TOY-counter-based RTC driver");
MODULE_AUTHOR("Manuel Lauss <manuel.lauss@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc-au1xxx");
