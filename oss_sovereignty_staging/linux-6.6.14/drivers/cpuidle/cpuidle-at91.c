
 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/cpuidle.h>
#include <linux/io.h>
#include <linux/export.h>
#include <asm/cpuidle.h>

#define AT91_MAX_STATES	2

static void (*at91_standby)(void);

 
static int at91_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			       int index)
{
	at91_standby();
	return index;
}

static struct cpuidle_driver at91_idle_driver = {
	.name			= "at91_idle",
	.owner			= THIS_MODULE,
	.states[0]		= ARM_CPUIDLE_WFI_STATE,
	.states[1]		= {
		.enter			= at91_enter_idle,
		.exit_latency		= 10,
		.target_residency	= 10000,
		.name			= "RAM_SR",
		.desc			= "WFI and DDR Self Refresh",
	},
	.state_count = AT91_MAX_STATES,
};

 
static int at91_cpuidle_probe(struct platform_device *dev)
{
	at91_standby = (void *)(dev->dev.platform_data);
	
	return cpuidle_register(&at91_idle_driver, NULL);
}

static struct platform_driver at91_cpuidle_driver = {
	.driver = {
		.name = "cpuidle-at91",
	},
	.probe = at91_cpuidle_probe,
};
builtin_platform_driver(at91_cpuidle_driver);
