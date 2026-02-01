 

#include <linux/arm-cci.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>

static void __iomem *cci_ctrl_base __ro_after_init;
static unsigned long cci_ctrl_phys __ro_after_init;

#ifdef CONFIG_ARM_CCI400_PORT_CTRL
struct cci_nb_ports {
	unsigned int nb_ace;
	unsigned int nb_ace_lite;
};

static const struct cci_nb_ports cci400_ports = {
	.nb_ace = 2,
	.nb_ace_lite = 3
};

#define CCI400_PORTS_DATA	(&cci400_ports)
#else
#define CCI400_PORTS_DATA	(NULL)
#endif

static const struct of_device_id arm_cci_matches[] = {
#ifdef CONFIG_ARM_CCI400_COMMON
	{.compatible = "arm,cci-400", .data = CCI400_PORTS_DATA },
#endif
#ifdef CONFIG_ARM_CCI5xx_PMU
	{ .compatible = "arm,cci-500", },
	{ .compatible = "arm,cci-550", },
#endif
	{},
};

static const struct of_dev_auxdata arm_cci_auxdata[] = {
	OF_DEV_AUXDATA("arm,cci-400-pmu", 0, NULL, &cci_ctrl_base),
	OF_DEV_AUXDATA("arm,cci-400-pmu,r0", 0, NULL, &cci_ctrl_base),
	OF_DEV_AUXDATA("arm,cci-400-pmu,r1", 0, NULL, &cci_ctrl_base),
	OF_DEV_AUXDATA("arm,cci-500-pmu,r0", 0, NULL, &cci_ctrl_base),
	OF_DEV_AUXDATA("arm,cci-550-pmu,r0", 0, NULL, &cci_ctrl_base),
	{}
};

#define DRIVER_NAME		"ARM-CCI"

static int cci_platform_probe(struct platform_device *pdev)
{
	if (!cci_probed())
		return -ENODEV;

	return of_platform_populate(pdev->dev.of_node, NULL,
				    arm_cci_auxdata, &pdev->dev);
}

static struct platform_driver cci_platform_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .of_match_table = arm_cci_matches,
		  },
	.probe = cci_platform_probe,
};

static int __init cci_platform_init(void)
{
	return platform_driver_register(&cci_platform_driver);
}

#ifdef CONFIG_ARM_CCI400_PORT_CTRL

#define CCI_PORT_CTRL		0x0
#define CCI_CTRL_STATUS		0xc

#define CCI_ENABLE_SNOOP_REQ	0x1
#define CCI_ENABLE_DVM_REQ	0x2
#define CCI_ENABLE_REQ		(CCI_ENABLE_SNOOP_REQ | CCI_ENABLE_DVM_REQ)

enum cci_ace_port_type {
	ACE_INVALID_PORT = 0x0,
	ACE_PORT,
	ACE_LITE_PORT,
};

struct cci_ace_port {
	void __iomem *base;
	unsigned long phys;
	enum cci_ace_port_type type;
	struct device_node *dn;
};

static struct cci_ace_port *ports;
static unsigned int nb_cci_ports;

struct cpu_port {
	u64 mpidr;
	u32 port;
};

 
#define PORT_VALID_SHIFT	31
#define PORT_VALID		(0x1 << PORT_VALID_SHIFT)

static inline void init_cpu_port(struct cpu_port *port, u32 index, u64 mpidr)
{
	port->port = PORT_VALID | index;
	port->mpidr = mpidr;
}

static inline bool cpu_port_is_valid(struct cpu_port *port)
{
	return !!(port->port & PORT_VALID);
}

static inline bool cpu_port_match(struct cpu_port *port, u64 mpidr)
{
	return port->mpidr == (mpidr & MPIDR_HWID_BITMASK);
}

static struct cpu_port cpu_port[NR_CPUS];

 
static int __cci_ace_get_port(struct device_node *dn, int type)
{
	int i;
	bool ace_match;
	struct device_node *cci_portn;

	cci_portn = of_parse_phandle(dn, "cci-control-port", 0);
	for (i = 0; i < nb_cci_ports; i++) {
		ace_match = ports[i].type == type;
		if (ace_match && cci_portn == ports[i].dn)
			return i;
	}
	return -ENODEV;
}

int cci_ace_get_port(struct device_node *dn)
{
	return __cci_ace_get_port(dn, ACE_LITE_PORT);
}
EXPORT_SYMBOL_GPL(cci_ace_get_port);

static void cci_ace_init_ports(void)
{
	int port, cpu;
	struct device_node *cpun;

	 
	for_each_possible_cpu(cpu) {
		 
		cpun = of_get_cpu_node(cpu, NULL);

		if (WARN(!cpun, "Missing cpu device node\n"))
			continue;

		port = __cci_ace_get_port(cpun, ACE_PORT);
		if (port < 0)
			continue;

		init_cpu_port(&cpu_port[cpu], port, cpu_logical_map(cpu));
	}

	for_each_possible_cpu(cpu) {
		WARN(!cpu_port_is_valid(&cpu_port[cpu]),
			"CPU %u does not have an associated CCI port\n",
			cpu);
	}
}
 

 
static void notrace cci_port_control(unsigned int port, bool enable)
{
	void __iomem *base = ports[port].base;

	writel_relaxed(enable ? CCI_ENABLE_REQ : 0, base + CCI_PORT_CTRL);
	 
	while (readl_relaxed(cci_ctrl_base + CCI_CTRL_STATUS) & 0x1)
			;
}

 
int notrace cci_disable_port_by_cpu(u64 mpidr)
{
	int cpu;
	bool is_valid;
	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		is_valid = cpu_port_is_valid(&cpu_port[cpu]);
		if (is_valid && cpu_port_match(&cpu_port[cpu], mpidr)) {
			cci_port_control(cpu_port[cpu].port, false);
			return 0;
		}
	}
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(cci_disable_port_by_cpu);

 
asmlinkage void __naked cci_enable_port_for_self(void)
{
	asm volatile ("\n"
"	.arch armv7-a\n"
"	mrc	p15, 0, r0, c0, c0, 5	@ get MPIDR value \n"
"	and	r0, r0, #"__stringify(MPIDR_HWID_BITMASK)" \n"
"	adr	r1, 5f \n"
"	ldr	r2, [r1] \n"
"	add	r1, r1, r2		@ &cpu_port \n"
"	add	ip, r1, %[sizeof_cpu_port] \n"

	 
"1:	ldr	r2, [r1, %[offsetof_cpu_port_mpidr_lsb]] \n"
"	cmp	r2, r0 			@ compare MPIDR \n"
"	bne	2f \n"

	 
"	ldr	r3, [r1, %[offsetof_cpu_port_port]] \n"
"	tst	r3, #"__stringify(PORT_VALID)" \n"
"	bne	3f \n"

	 
"2:	add	r1, r1, %[sizeof_struct_cpu_port] \n"
"	cmp	r1, ip			@ done? \n"
"	blo	1b \n"

	 
"cci_port_not_found: \n"
"	wfi \n"
"	wfe \n"
"	b	cci_port_not_found \n"

	 
"3:	bic	r3, r3, #"__stringify(PORT_VALID)" \n"
"	adr	r0, 6f \n"
"	ldmia	r0, {r1, r2} \n"
"	sub	r1, r1, r0 		@ virt - phys \n"
"	ldr	r0, [r0, r2] 		@ *(&ports) \n"
"	mov	r2, %[sizeof_struct_ace_port] \n"
"	mla	r0, r2, r3, r0		@ &ports[index] \n"
"	sub	r0, r0, r1		@ virt_to_phys() \n"

	 
"	ldr	r0, [r0, %[offsetof_port_phys]] \n"
"	mov	r3, %[cci_enable_req]\n"		   
"	str	r3, [r0, #"__stringify(CCI_PORT_CTRL)"] \n"

	 
"	adr	r1, 7f \n"
"	ldr	r0, [r1] \n"
"	ldr	r0, [r0, r1]		@ cci_ctrl_base \n"
"4:	ldr	r1, [r0, #"__stringify(CCI_CTRL_STATUS)"] \n"
"	tst	r1, %[cci_control_status_bits] \n"			
"	bne	4b \n"

"	mov	r0, #0 \n"
"	bx	lr \n"

"	.align	2 \n"
"5:	.word	cpu_port - . \n"
"6:	.word	. \n"
"	.word	ports - 6b \n"
"7:	.word	cci_ctrl_phys - . \n"
	: :
	[sizeof_cpu_port] "i" (sizeof(cpu_port)),
	[cci_enable_req] "i" cpu_to_le32(CCI_ENABLE_REQ),
	[cci_control_status_bits] "i" cpu_to_le32(1),
#ifndef __ARMEB__
	[offsetof_cpu_port_mpidr_lsb] "i" (offsetof(struct cpu_port, mpidr)),
#else
	[offsetof_cpu_port_mpidr_lsb] "i" (offsetof(struct cpu_port, mpidr)+4),
#endif
	[offsetof_cpu_port_port] "i" (offsetof(struct cpu_port, port)),
	[sizeof_struct_cpu_port] "i" (sizeof(struct cpu_port)),
	[sizeof_struct_ace_port] "i" (sizeof(struct cci_ace_port)),
	[offsetof_port_phys] "i" (offsetof(struct cci_ace_port, phys)) );
}

 
int notrace __cci_control_port_by_device(struct device_node *dn, bool enable)
{
	int port;

	if (!dn)
		return -ENODEV;

	port = __cci_ace_get_port(dn, ACE_LITE_PORT);
	if (WARN_ONCE(port < 0, "node %pOF ACE lite port look-up failure\n",
				dn))
		return -ENODEV;
	cci_port_control(port, enable);
	return 0;
}
EXPORT_SYMBOL_GPL(__cci_control_port_by_device);

 
int notrace __cci_control_port_by_index(u32 port, bool enable)
{
	if (port >= nb_cci_ports || ports[port].type == ACE_INVALID_PORT)
		return -ENODEV;
	 
	if (ports[port].type == ACE_PORT)
		return -EPERM;

	cci_port_control(port, enable);
	return 0;
}
EXPORT_SYMBOL_GPL(__cci_control_port_by_index);

static const struct of_device_id arm_cci_ctrl_if_matches[] = {
	{.compatible = "arm,cci-400-ctrl-if", },
	{},
};

static int cci_probe_ports(struct device_node *np)
{
	struct cci_nb_ports const *cci_config;
	int ret, i, nb_ace = 0, nb_ace_lite = 0;
	struct device_node *cp;
	struct resource res;
	const char *match_str;
	bool is_ace;


	cci_config = of_match_node(arm_cci_matches, np)->data;
	if (!cci_config)
		return -ENODEV;

	nb_cci_ports = cci_config->nb_ace + cci_config->nb_ace_lite;

	ports = kcalloc(nb_cci_ports, sizeof(*ports), GFP_KERNEL);
	if (!ports)
		return -ENOMEM;

	for_each_available_child_of_node(np, cp) {
		if (!of_match_node(arm_cci_ctrl_if_matches, cp))
			continue;

		i = nb_ace + nb_ace_lite;

		if (i >= nb_cci_ports)
			break;

		if (of_property_read_string(cp, "interface-type",
					&match_str)) {
			WARN(1, "node %pOF missing interface-type property\n",
				  cp);
			continue;
		}
		is_ace = strcmp(match_str, "ace") == 0;
		if (!is_ace && strcmp(match_str, "ace-lite")) {
			WARN(1, "node %pOF containing invalid interface-type property, skipping it\n",
					cp);
			continue;
		}

		ret = of_address_to_resource(cp, 0, &res);
		if (!ret) {
			ports[i].base = ioremap(res.start, resource_size(&res));
			ports[i].phys = res.start;
		}
		if (ret || !ports[i].base) {
			WARN(1, "unable to ioremap CCI port %d\n", i);
			continue;
		}

		if (is_ace) {
			if (WARN_ON(nb_ace >= cci_config->nb_ace))
				continue;
			ports[i].type = ACE_PORT;
			++nb_ace;
		} else {
			if (WARN_ON(nb_ace_lite >= cci_config->nb_ace_lite))
				continue;
			ports[i].type = ACE_LITE_PORT;
			++nb_ace_lite;
		}
		ports[i].dn = cp;
	}

	 
	if (!nb_ace && !nb_ace_lite)
		return -ENODEV;

	  
	cci_ace_init_ports();

	 
	sync_cache_w(&cci_ctrl_base);
	sync_cache_w(&cci_ctrl_phys);
	sync_cache_w(&ports);
	sync_cache_w(&cpu_port);
	__sync_cache_range_w(ports, sizeof(*ports) * nb_cci_ports);
	pr_info("ARM CCI driver probed\n");

	return 0;
}
#else  
static inline int cci_probe_ports(struct device_node *np)
{
	return 0;
}
#endif  

static int cci_probe(void)
{
	int ret;
	struct device_node *np;
	struct resource res;

	np = of_find_matching_node(NULL, arm_cci_matches);
	if (!of_device_is_available(np))
		return -ENODEV;

	ret = of_address_to_resource(np, 0, &res);
	if (!ret) {
		cci_ctrl_base = ioremap(res.start, resource_size(&res));
		cci_ctrl_phys =	res.start;
	}
	if (ret || !cci_ctrl_base) {
		WARN(1, "unable to ioremap CCI ctrl\n");
		return -ENXIO;
	}

	return cci_probe_ports(np);
}

static int cci_init_status = -EAGAIN;
static DEFINE_MUTEX(cci_probing);

static int cci_init(void)
{
	if (cci_init_status != -EAGAIN)
		return cci_init_status;

	mutex_lock(&cci_probing);
	if (cci_init_status == -EAGAIN)
		cci_init_status = cci_probe();
	mutex_unlock(&cci_probing);
	return cci_init_status;
}

 
bool cci_probed(void)
{
	return cci_init() == 0;
}
EXPORT_SYMBOL_GPL(cci_probed);

early_initcall(cci_init);
core_initcall(cci_platform_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ARM CCI support");
