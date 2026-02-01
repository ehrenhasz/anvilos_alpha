
 
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netlink.h>
#include <net/Space.h>

 
struct netdev_boot_setup {
	char name[IFNAMSIZ];
	struct ifmap map;
};
#define NETDEV_BOOT_SETUP_MAX 8


 

 
static struct netdev_boot_setup dev_boot_setup[NETDEV_BOOT_SETUP_MAX];

 
static int netdev_boot_setup_add(char *name, struct ifmap *map)
{
	struct netdev_boot_setup *s;
	int i;

	s = dev_boot_setup;
	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] == '\0' || s[i].name[0] == ' ') {
			memset(s[i].name, 0, sizeof(s[i].name));
			strscpy(s[i].name, name, IFNAMSIZ);
			memcpy(&s[i].map, map, sizeof(s[i].map));
			break;
		}
	}

	return i >= NETDEV_BOOT_SETUP_MAX ? 0 : 1;
}

 
int netdev_boot_setup_check(struct net_device *dev)
{
	struct netdev_boot_setup *s = dev_boot_setup;
	int i;

	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++) {
		if (s[i].name[0] != '\0' && s[i].name[0] != ' ' &&
		    !strcmp(dev->name, s[i].name)) {
			dev->irq = s[i].map.irq;
			dev->base_addr = s[i].map.base_addr;
			dev->mem_start = s[i].map.mem_start;
			dev->mem_end = s[i].map.mem_end;
			return 1;
		}
	}
	return 0;
}
EXPORT_SYMBOL(netdev_boot_setup_check);

 
static unsigned long netdev_boot_base(const char *prefix, int unit)
{
	const struct netdev_boot_setup *s = dev_boot_setup;
	char name[IFNAMSIZ];
	int i;

	sprintf(name, "%s%d", prefix, unit);

	 
	if (__dev_get_by_name(&init_net, name))
		return 1;

	for (i = 0; i < NETDEV_BOOT_SETUP_MAX; i++)
		if (!strcmp(name, s[i].name))
			return s[i].map.base_addr;
	return 0;
}

 
static int __init netdev_boot_setup(char *str)
{
	int ints[5];
	struct ifmap map;

	str = get_options(str, ARRAY_SIZE(ints), ints);
	if (!str || !*str)
		return 0;

	 
	memset(&map, 0, sizeof(map));
	if (ints[0] > 0)
		map.irq = ints[1];
	if (ints[0] > 1)
		map.base_addr = ints[2];
	if (ints[0] > 2)
		map.mem_start = ints[3];
	if (ints[0] > 3)
		map.mem_end = ints[4];

	 
	return netdev_boot_setup_add(str, &map);
}

__setup("netdev=", netdev_boot_setup);

static int __init ether_boot_setup(char *str)
{
	return netdev_boot_setup(str);
}
__setup("ether=", ether_boot_setup);


 

struct devprobe2 {
	struct net_device *(*probe)(int unit);
	int status;	 
};

static int __init probe_list2(int unit, struct devprobe2 *p, int autoprobe)
{
	struct net_device *dev;

	for (; p->probe; p++) {
		if (autoprobe && p->status)
			continue;
		dev = p->probe(unit);
		if (!IS_ERR(dev))
			return 0;
		if (autoprobe)
			p->status = PTR_ERR(dev);
	}
	return -ENODEV;
}

 
static struct devprobe2 isa_probes[] __initdata = {
#ifdef CONFIG_3C515
	{tc515_probe, 0},
#endif
#ifdef CONFIG_ULTRA
	{ultra_probe, 0},
#endif
#ifdef CONFIG_WD80x3
	{wd_probe, 0},
#endif
#if defined(CONFIG_NE2000)  
	{ne_probe, 0},
#endif
#ifdef CONFIG_LANCE		 
	{lance_probe, 0},
#endif
#ifdef CONFIG_SMC9194
	{smc_init, 0},
#endif
#ifdef CONFIG_CS89x0_ISA
	{cs89x0_probe, 0},
#endif
	{NULL, 0},
};

 

static void __init ethif_probe2(int unit)
{
	unsigned long base_addr = netdev_boot_base("eth", unit);

	if (base_addr == 1)
		return;

	probe_list2(unit, isa_probes, base_addr == 0);
}

 
static int __init net_olddevs_init(void)
{
	int num;

	for (num = 0; num < 8; ++num)
		ethif_probe2(num);

#ifdef CONFIG_COPS
	cops_probe(0);
	cops_probe(1);
	cops_probe(2);
#endif

	return 0;
}

device_initcall(net_olddevs_init);
