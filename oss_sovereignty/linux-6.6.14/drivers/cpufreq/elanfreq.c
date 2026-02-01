
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/delay.h>
#include <linux/cpufreq.h>

#include <asm/cpu_device_id.h>
#include <asm/msr.h>
#include <linux/timex.h>
#include <linux/io.h>

#define REG_CSCIR 0x22		 
#define REG_CSCDR 0x23		 

 
static int max_freq;

struct s_elan_multiplier {
	int clock;		 
	int val40h;		 
	int val80h;		 
};

 
static struct s_elan_multiplier elan_multiplier[] = {
	{1000,	0x02,	0x18},
	{2000,	0x02,	0x10},
	{4000,	0x02,	0x08},
	{8000,	0x00,	0x00},
	{16000,	0x00,	0x02},
	{33000,	0x00,	0x04},
	{66000,	0x01,	0x04},
	{99000,	0x01,	0x05}
};

static struct cpufreq_frequency_table elanfreq_table[] = {
	{0, 0,	1000},
	{0, 1,	2000},
	{0, 2,	4000},
	{0, 3,	8000},
	{0, 4,	16000},
	{0, 5,	33000},
	{0, 6,	66000},
	{0, 7,	99000},
	{0, 0,	CPUFREQ_TABLE_END},
};


 

static unsigned int elanfreq_get_cpu_frequency(unsigned int cpu)
{
	u8 clockspeed_reg;     

	local_irq_disable();
	outb_p(0x80, REG_CSCIR);
	clockspeed_reg = inb_p(REG_CSCDR);
	local_irq_enable();

	if ((clockspeed_reg & 0xE0) == 0xE0)
		return 0;

	 
	if ((clockspeed_reg & 0xE0) == 0xC0) {
		if ((clockspeed_reg & 0x01) == 0)
			return 66000;
		else
			return 99000;
	}

	 
	if ((clockspeed_reg & 0xE0) == 0xA0)
		return 33000;

	return (1<<((clockspeed_reg & 0xE0) >> 5)) * 1000;
}


static int elanfreq_target(struct cpufreq_policy *policy,
			    unsigned int state)
{
	 

	 

	local_irq_disable();
	outb_p(0x40, REG_CSCIR);		 
	outb_p(0x00, REG_CSCDR);
	local_irq_enable();		 
	udelay(1000);			 

	local_irq_disable();

	 
	outb_p(0x80, REG_CSCIR);
	outb_p(elan_multiplier[state].val80h, REG_CSCDR);

	 
	outb_p(0x40, REG_CSCIR);
	outb_p(elan_multiplier[state].val40h, REG_CSCDR);
	udelay(10000);
	local_irq_enable();

	return 0;
}
 

static int elanfreq_cpu_init(struct cpufreq_policy *policy)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	struct cpufreq_frequency_table *pos;

	 
	if ((c->x86_vendor != X86_VENDOR_AMD) ||
	    (c->x86 != 4) || (c->x86_model != 10))
		return -ENODEV;

	 
	if (!max_freq)
		max_freq = elanfreq_get_cpu_frequency(0);

	 
	cpufreq_for_each_entry(pos, elanfreq_table)
		if (pos->frequency > max_freq)
			pos->frequency = CPUFREQ_ENTRY_INVALID;

	policy->freq_table = elanfreq_table;
	return 0;
}


#ifndef MODULE
 
static int __init elanfreq_setup(char *str)
{
	max_freq = simple_strtoul(str, &str, 0);
	pr_warn("You're using the deprecated elanfreq command line option. Use elanfreq.max_freq instead, please!\n");
	return 1;
}
__setup("elanfreq=", elanfreq_setup);
#endif


static struct cpufreq_driver elanfreq_driver = {
	.get		= elanfreq_get_cpu_frequency,
	.flags		= CPUFREQ_NO_AUTO_DYNAMIC_SWITCHING,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= elanfreq_target,
	.init		= elanfreq_cpu_init,
	.name		= "elanfreq",
	.attr		= cpufreq_generic_attr,
};

static const struct x86_cpu_id elan_id[] = {
	X86_MATCH_VENDOR_FAM_MODEL(AMD, 4, 10, NULL),
	{}
};
MODULE_DEVICE_TABLE(x86cpu, elan_id);

static int __init elanfreq_init(void)
{
	if (!x86_match_cpu(elan_id))
		return -ENODEV;
	return cpufreq_register_driver(&elanfreq_driver);
}


static void __exit elanfreq_exit(void)
{
	cpufreq_unregister_driver(&elanfreq_driver);
}


module_param(max_freq, int, 0444);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Schwebel <r.schwebel@pengutronix.de>, "
		"Sven Geggus <sven@geggus.net>");
MODULE_DESCRIPTION("cpufreq driver for AMD's Elan CPUs");

module_init(elanfreq_init);
module_exit(elanfreq_exit);
