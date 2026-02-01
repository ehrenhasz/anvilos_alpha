
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/memblock.h>
#include <linux/numa.h>

 
unsigned int cpumask_next_wrap(int n, const struct cpumask *mask, int start, bool wrap)
{
	unsigned int next;

again:
	next = cpumask_next(n, mask);

	if (wrap && n < start && next >= start) {
		return nr_cpumask_bits;

	} else if (next >= nr_cpumask_bits) {
		wrap = true;
		n = -1;
		goto again;
	}

	return next;
}
EXPORT_SYMBOL(cpumask_next_wrap);

 
#ifdef CONFIG_CPUMASK_OFFSTACK
 
bool alloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node)
{
	*mask = kmalloc_node(cpumask_size(), flags, node);

#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	if (!*mask) {
		printk(KERN_ERR "=> alloc_cpumask_var: failed!\n");
		dump_stack();
	}
#endif

	return *mask != NULL;
}
EXPORT_SYMBOL(alloc_cpumask_var_node);

 
void __init alloc_bootmem_cpumask_var(cpumask_var_t *mask)
{
	*mask = memblock_alloc(cpumask_size(), SMP_CACHE_BYTES);
	if (!*mask)
		panic("%s: Failed to allocate %u bytes\n", __func__,
		      cpumask_size());
}

 
void free_cpumask_var(cpumask_var_t mask)
{
	kfree(mask);
}
EXPORT_SYMBOL(free_cpumask_var);

 
void __init free_bootmem_cpumask_var(cpumask_var_t mask)
{
	memblock_free(mask, cpumask_size());
}
#endif

 
unsigned int cpumask_local_spread(unsigned int i, int node)
{
	unsigned int cpu;

	 
	i %= num_online_cpus();

	cpu = (node == NUMA_NO_NODE) ?
		cpumask_nth(i, cpu_online_mask) :
		sched_numa_find_nth_cpu(cpu_online_mask, i, node);

	WARN_ON(cpu >= nr_cpu_ids);
	return cpu;
}
EXPORT_SYMBOL(cpumask_local_spread);

static DEFINE_PER_CPU(int, distribute_cpu_mask_prev);

 
unsigned int cpumask_any_and_distribute(const struct cpumask *src1p,
			       const struct cpumask *src2p)
{
	unsigned int next, prev;

	 
	prev = __this_cpu_read(distribute_cpu_mask_prev);

	next = find_next_and_bit_wrap(cpumask_bits(src1p), cpumask_bits(src2p),
					nr_cpumask_bits, prev + 1);
	if (next < nr_cpu_ids)
		__this_cpu_write(distribute_cpu_mask_prev, next);

	return next;
}
EXPORT_SYMBOL(cpumask_any_and_distribute);

unsigned int cpumask_any_distribute(const struct cpumask *srcp)
{
	unsigned int next, prev;

	 
	prev = __this_cpu_read(distribute_cpu_mask_prev);
	next = find_next_bit_wrap(cpumask_bits(srcp), nr_cpumask_bits, prev + 1);
	if (next < nr_cpu_ids)
		__this_cpu_write(distribute_cpu_mask_prev, next);

	return next;
}
EXPORT_SYMBOL(cpumask_any_distribute);
