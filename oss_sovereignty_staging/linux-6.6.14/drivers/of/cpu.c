
#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/of.h>

 
u64 of_get_cpu_hwid(struct device_node *cpun, unsigned int thread)
{
	const __be32 *cell;
	int ac, len;

	ac = of_n_addr_cells(cpun);
	cell = of_get_property(cpun, "reg", &len);
	if (!cell || !ac || ((sizeof(*cell) * ac * (thread + 1)) > len))
		return ~0ULL;

	cell += ac * thread;
	return of_read_number(cell, ac);
}

 
bool __weak arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return (u32)phys_id == cpu;
}

 
static bool __of_find_n_match_cpu_property(struct device_node *cpun,
			const char *prop_name, int cpu, unsigned int *thread)
{
	const __be32 *cell;
	int ac, prop_len, tid;
	u64 hwid;

	ac = of_n_addr_cells(cpun);
	cell = of_get_property(cpun, prop_name, &prop_len);
	if (!cell && !ac && arch_match_cpu_phys_id(cpu, 0))
		return true;
	if (!cell || !ac)
		return false;
	prop_len /= sizeof(*cell) * ac;
	for (tid = 0; tid < prop_len; tid++) {
		hwid = of_read_number(cell, ac);
		if (arch_match_cpu_phys_id(cpu, hwid)) {
			if (thread)
				*thread = tid;
			return true;
		}
		cell += ac;
	}
	return false;
}

 
bool __weak arch_find_n_match_cpu_physical_id(struct device_node *cpun,
					      int cpu, unsigned int *thread)
{
	 
	if (IS_ENABLED(CONFIG_PPC) &&
	    __of_find_n_match_cpu_property(cpun,
					   "ibm,ppc-interrupt-server#s",
					   cpu, thread))
		return true;

	return __of_find_n_match_cpu_property(cpun, "reg", cpu, thread);
}

 
struct device_node *of_get_cpu_node(int cpu, unsigned int *thread)
{
	struct device_node *cpun;

	for_each_of_cpu_node(cpun) {
		if (arch_find_n_match_cpu_physical_id(cpun, cpu, thread))
			return cpun;
	}
	return NULL;
}
EXPORT_SYMBOL(of_get_cpu_node);

 
struct device_node *of_cpu_device_node_get(int cpu)
{
	struct device *cpu_dev;
	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev)
		return of_get_cpu_node(cpu, NULL);
	return of_node_get(cpu_dev->of_node);
}
EXPORT_SYMBOL(of_cpu_device_node_get);

 
int of_cpu_node_to_id(struct device_node *cpu_node)
{
	int cpu;
	bool found = false;
	struct device_node *np;

	for_each_possible_cpu(cpu) {
		np = of_cpu_device_node_get(cpu);
		found = (cpu_node == np);
		of_node_put(np);
		if (found)
			return cpu;
	}

	return -ENODEV;
}
EXPORT_SYMBOL(of_cpu_node_to_id);

 
struct device_node *of_get_cpu_state_node(struct device_node *cpu_node,
					  int index)
{
	struct of_phandle_args args;
	int err;

	err = of_parse_phandle_with_args(cpu_node, "power-domains",
					"#power-domain-cells", 0, &args);
	if (!err) {
		struct device_node *state_node =
			of_parse_phandle(args.np, "domain-idle-states", index);

		of_node_put(args.np);
		if (state_node)
			return state_node;
	}

	return of_parse_phandle(cpu_node, "cpu-idle-states", index);
}
EXPORT_SYMBOL(of_get_cpu_state_node);
