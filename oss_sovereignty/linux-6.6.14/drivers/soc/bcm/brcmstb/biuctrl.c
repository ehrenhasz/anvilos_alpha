
 

#define pr_fmt(fmt)	"brcmstb: " KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/soc/brcmstb/brcmstb.h>

#define RACENPREF_MASK			0x3
#define RACPREFINST_SHIFT		0
#define RACENINST_SHIFT			2
#define RACPREFDATA_SHIFT		4
#define RACENDATA_SHIFT			6
#define RAC_CPU_SHIFT			8
#define RACCFG_MASK			0xff
#define DPREF_LINE_2_SHIFT		24
#define DPREF_LINE_2_MASK		0xff

 
#define RAC_DATA_INST_EN_MASK		(1 << RACPREFINST_SHIFT | \
					 RACENPREF_MASK << RACENINST_SHIFT | \
					 1 << RACPREFDATA_SHIFT | \
					 RACENPREF_MASK << RACENDATA_SHIFT)

#define  CPU_CREDIT_REG_MCPx_WR_PAIRING_EN_MASK	0x70000000
#define CPU_CREDIT_REG_MCPx_READ_CRED_MASK	0xf
#define CPU_CREDIT_REG_MCPx_WRITE_CRED_MASK	0xf
#define CPU_CREDIT_REG_MCPx_READ_CRED_SHIFT(x)	((x) * 8)
#define CPU_CREDIT_REG_MCPx_WRITE_CRED_SHIFT(x)	(((x) * 8) + 4)

#define CPU_MCP_FLOW_REG_MCPx_RDBUFF_CRED_SHIFT(x)	((x) * 8)
#define CPU_MCP_FLOW_REG_MCPx_RDBUFF_CRED_MASK		0xff

#define CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_THRESHOLD_MASK	0xf
#define CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_TIMEOUT_MASK		0xf
#define CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_TIMEOUT_SHIFT	4
#define CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_ENABLE		BIT(8)

static void __iomem *cpubiuctrl_base;
static bool mcp_wr_pairing_en;
static const int *cpubiuctrl_regs;

enum cpubiuctrl_regs {
	CPU_CREDIT_REG = 0,
	CPU_MCP_FLOW_REG,
	CPU_WRITEBACK_CTRL_REG,
	RAC_CONFIG0_REG,
	RAC_CONFIG1_REG,
	NUM_CPU_BIUCTRL_REGS,
};

static inline u32 cbc_readl(int reg)
{
	int offset = cpubiuctrl_regs[reg];

	if (offset == -1 ||
	    (IS_ENABLED(CONFIG_CACHE_B15_RAC) && reg >= RAC_CONFIG0_REG))
		return (u32)-1;

	return readl_relaxed(cpubiuctrl_base + offset);
}

static inline void cbc_writel(u32 val, int reg)
{
	int offset = cpubiuctrl_regs[reg];

	if (offset == -1 ||
	    (IS_ENABLED(CONFIG_CACHE_B15_RAC) && reg >= RAC_CONFIG0_REG))
		return;

	writel(val, cpubiuctrl_base + offset);
}

static const int b15_cpubiuctrl_regs[] = {
	[CPU_CREDIT_REG] = 0x184,
	[CPU_MCP_FLOW_REG] = -1,
	[CPU_WRITEBACK_CTRL_REG] = -1,
	[RAC_CONFIG0_REG] = -1,
	[RAC_CONFIG1_REG] = -1,
};

 
static const int b53_cpubiuctrl_no_wb_regs[] = {
	[CPU_CREDIT_REG] = 0x0b0,
	[CPU_MCP_FLOW_REG] = 0x0b4,
	[CPU_WRITEBACK_CTRL_REG] = -1,
	[RAC_CONFIG0_REG] = 0x78,
	[RAC_CONFIG1_REG] = 0x7c,
};

static const int b53_cpubiuctrl_regs[] = {
	[CPU_CREDIT_REG] = 0x0b0,
	[CPU_MCP_FLOW_REG] = 0x0b4,
	[CPU_WRITEBACK_CTRL_REG] = 0x22c,
	[RAC_CONFIG0_REG] = 0x78,
	[RAC_CONFIG1_REG] = 0x7c,
};

static const int a72_cpubiuctrl_regs[] = {
	[CPU_CREDIT_REG] = 0x18,
	[CPU_MCP_FLOW_REG] = 0x1c,
	[CPU_WRITEBACK_CTRL_REG] = 0x20,
	[RAC_CONFIG0_REG] = 0x08,
	[RAC_CONFIG1_REG] = 0x0c,
};

static int __init mcp_write_pairing_set(void)
{
	u32 creds = 0;

	if (!cpubiuctrl_base)
		return -1;

	creds = cbc_readl(CPU_CREDIT_REG);
	if (mcp_wr_pairing_en) {
		pr_info("MCP: Enabling write pairing\n");
		cbc_writel(creds | CPU_CREDIT_REG_MCPx_WR_PAIRING_EN_MASK,
			   CPU_CREDIT_REG);
	} else if (creds & CPU_CREDIT_REG_MCPx_WR_PAIRING_EN_MASK) {
		pr_info("MCP: Disabling write pairing\n");
		cbc_writel(creds & ~CPU_CREDIT_REG_MCPx_WR_PAIRING_EN_MASK,
			   CPU_CREDIT_REG);
	} else {
		pr_info("MCP: Write pairing already disabled\n");
	}

	return 0;
}

static const u32 a72_b53_mach_compat[] = {
	0x7211,
	0x72113,
	0x72116,
	0x7216,
	0x72164,
	0x72165,
	0x7255,
	0x7260,
	0x7268,
	0x7271,
	0x7278,
};

 
static void __init a72_b53_rac_enable_all(struct device_node *np)
{
	unsigned int cpu;
	u32 enable = 0, pref_dist, shift;

	if (IS_ENABLED(CONFIG_CACHE_B15_RAC))
		return;

	if (WARN(num_possible_cpus() > 4, "RAC only supports 4 CPUs\n"))
		return;

	pref_dist = cbc_readl(RAC_CONFIG1_REG);
	for_each_possible_cpu(cpu) {
		shift = cpu * RAC_CPU_SHIFT + RACPREFDATA_SHIFT;
		enable |= RAC_DATA_INST_EN_MASK << (cpu * RAC_CPU_SHIFT);
		if (cpubiuctrl_regs == a72_cpubiuctrl_regs) {
			enable &= ~(RACENPREF_MASK << shift);
			enable |= 3 << shift;
			pref_dist |= 1 << (cpu + DPREF_LINE_2_SHIFT);
		}
	}

	cbc_writel(enable, RAC_CONFIG0_REG);
	cbc_writel(pref_dist, RAC_CONFIG1_REG);

	pr_info("%pOF: Broadcom %s read-ahead cache\n",
		np, cpubiuctrl_regs == a72_cpubiuctrl_regs ?
		"Cortex-A72" : "Brahma-B53");
}

static void __init mcp_a72_b53_set(void)
{
	unsigned int i;
	u32 reg;

	reg = brcmstb_get_family_id();

	for (i = 0; i < ARRAY_SIZE(a72_b53_mach_compat); i++) {
		if (BRCM_ID(reg) == a72_b53_mach_compat[i])
			break;
	}

	if (i == ARRAY_SIZE(a72_b53_mach_compat))
		return;

	 
	reg = cbc_readl(CPU_CREDIT_REG);
	for (i = 0; i < 3; i++) {
		reg &= ~(CPU_CREDIT_REG_MCPx_WRITE_CRED_MASK <<
			 CPU_CREDIT_REG_MCPx_WRITE_CRED_SHIFT(i));
		reg &= ~(CPU_CREDIT_REG_MCPx_READ_CRED_MASK <<
			 CPU_CREDIT_REG_MCPx_READ_CRED_SHIFT(i));
		reg |= 8 << CPU_CREDIT_REG_MCPx_WRITE_CRED_SHIFT(i);
		reg |= 8 << CPU_CREDIT_REG_MCPx_READ_CRED_SHIFT(i);
	}
	cbc_writel(reg, CPU_CREDIT_REG);

	 
	reg = cbc_readl(CPU_MCP_FLOW_REG);
	for (i = 0; i < 3; i++)
		reg |= CPU_MCP_FLOW_REG_MCPx_RDBUFF_CRED_MASK <<
			CPU_MCP_FLOW_REG_MCPx_RDBUFF_CRED_SHIFT(i);
	cbc_writel(reg, CPU_MCP_FLOW_REG);

	 
	reg = cbc_readl(CPU_WRITEBACK_CTRL_REG);
	reg |= CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_ENABLE;
	reg &= ~CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_THRESHOLD_MASK;
	reg &= ~(CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_TIMEOUT_MASK <<
		 CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_TIMEOUT_SHIFT);
	reg |= 8;
	reg |= 7 << CPU_WRITEBACK_CTRL_REG_WB_THROTTLE_TIMEOUT_SHIFT;
	cbc_writel(reg, CPU_WRITEBACK_CTRL_REG);
}

static int __init setup_hifcpubiuctrl_regs(struct device_node *np)
{
	struct device_node *cpu_dn;
	u32 family_id;
	int ret = 0;

	cpubiuctrl_base = of_iomap(np, 0);
	if (!cpubiuctrl_base) {
		pr_err("failed to remap BIU control base\n");
		ret = -ENOMEM;
		goto out;
	}

	mcp_wr_pairing_en = of_property_read_bool(np, "brcm,write-pairing");

	cpu_dn = of_get_cpu_node(0, NULL);
	if (!cpu_dn) {
		pr_err("failed to obtain CPU device node\n");
		ret = -ENODEV;
		goto out;
	}

	if (of_device_is_compatible(cpu_dn, "brcm,brahma-b15"))
		cpubiuctrl_regs = b15_cpubiuctrl_regs;
	else if (of_device_is_compatible(cpu_dn, "brcm,brahma-b53"))
		cpubiuctrl_regs = b53_cpubiuctrl_regs;
	else if (of_device_is_compatible(cpu_dn, "arm,cortex-a72"))
		cpubiuctrl_regs = a72_cpubiuctrl_regs;
	else {
		pr_err("unsupported CPU\n");
		ret = -EINVAL;
	}
	of_node_put(cpu_dn);

	family_id = brcmstb_get_family_id();
	if (BRCM_ID(family_id) == 0x7260 && BRCM_REV(family_id) == 0)
		cpubiuctrl_regs = b53_cpubiuctrl_no_wb_regs;
out:
	if (ret && cpubiuctrl_base) {
		iounmap(cpubiuctrl_base);
		cpubiuctrl_base = NULL;
	}
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static u32 cpubiuctrl_reg_save[NUM_CPU_BIUCTRL_REGS];

static int brcmstb_cpu_credit_reg_suspend(void)
{
	unsigned int i;

	if (!cpubiuctrl_base)
		return 0;

	for (i = 0; i < NUM_CPU_BIUCTRL_REGS; i++)
		cpubiuctrl_reg_save[i] = cbc_readl(i);

	return 0;
}

static void brcmstb_cpu_credit_reg_resume(void)
{
	unsigned int i;

	if (!cpubiuctrl_base)
		return;

	for (i = 0; i < NUM_CPU_BIUCTRL_REGS; i++)
		cbc_writel(cpubiuctrl_reg_save[i], i);
}

static struct syscore_ops brcmstb_cpu_credit_syscore_ops = {
	.suspend = brcmstb_cpu_credit_reg_suspend,
	.resume = brcmstb_cpu_credit_reg_resume,
};
#endif


static int __init brcmstb_biuctrl_init(void)
{
	struct device_node *np;
	int ret;

	 
	np = of_find_compatible_node(NULL, NULL, "brcm,brcmstb-cpu-biu-ctrl");
	if (!np)
		return 0;

	ret = setup_hifcpubiuctrl_regs(np);
	if (ret)
		goto out_put;

	ret = mcp_write_pairing_set();
	if (ret) {
		pr_err("MCP: Unable to disable write pairing!\n");
		goto out_put;
	}

	a72_b53_rac_enable_all(np);
	mcp_a72_b53_set();
#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&brcmstb_cpu_credit_syscore_ops);
#endif
	ret = 0;
out_put:
	of_node_put(np);
	return ret;
}
early_initcall(brcmstb_biuctrl_init);
