
 
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/sort.h>
#include <linux/group_cpus.h>

#ifdef CONFIG_SMP

static void grp_spread_init_one(struct cpumask *irqmsk, struct cpumask *nmsk,
				unsigned int cpus_per_grp)
{
	const struct cpumask *siblmsk;
	int cpu, sibl;

	for ( ; cpus_per_grp > 0; ) {
		cpu = cpumask_first(nmsk);

		 
		if (cpu >= nr_cpu_ids)
			return;

		cpumask_clear_cpu(cpu, nmsk);
		cpumask_set_cpu(cpu, irqmsk);
		cpus_per_grp--;

		 
		siblmsk = topology_sibling_cpumask(cpu);
		for (sibl = -1; cpus_per_grp > 0; ) {
			sibl = cpumask_next(sibl, siblmsk);
			if (sibl >= nr_cpu_ids)
				break;
			if (!cpumask_test_and_clear_cpu(sibl, nmsk))
				continue;
			cpumask_set_cpu(sibl, irqmsk);
			cpus_per_grp--;
		}
	}
}

static cpumask_var_t *alloc_node_to_cpumask(void)
{
	cpumask_var_t *masks;
	int node;

	masks = kcalloc(nr_node_ids, sizeof(cpumask_var_t), GFP_KERNEL);
	if (!masks)
		return NULL;

	for (node = 0; node < nr_node_ids; node++) {
		if (!zalloc_cpumask_var(&masks[node], GFP_KERNEL))
			goto out_unwind;
	}

	return masks;

out_unwind:
	while (--node >= 0)
		free_cpumask_var(masks[node]);
	kfree(masks);
	return NULL;
}

static void free_node_to_cpumask(cpumask_var_t *masks)
{
	int node;

	for (node = 0; node < nr_node_ids; node++)
		free_cpumask_var(masks[node]);
	kfree(masks);
}

static void build_node_to_cpumask(cpumask_var_t *masks)
{
	int cpu;

	for_each_possible_cpu(cpu)
		cpumask_set_cpu(cpu, masks[cpu_to_node(cpu)]);
}

static int get_nodes_in_cpumask(cpumask_var_t *node_to_cpumask,
				const struct cpumask *mask, nodemask_t *nodemsk)
{
	int n, nodes = 0;

	 
	for_each_node(n) {
		if (cpumask_intersects(mask, node_to_cpumask[n])) {
			node_set(n, *nodemsk);
			nodes++;
		}
	}
	return nodes;
}

struct node_groups {
	unsigned id;

	union {
		unsigned ngroups;
		unsigned ncpus;
	};
};

static int ncpus_cmp_func(const void *l, const void *r)
{
	const struct node_groups *ln = l;
	const struct node_groups *rn = r;

	return ln->ncpus - rn->ncpus;
}

 
static void alloc_nodes_groups(unsigned int numgrps,
			       cpumask_var_t *node_to_cpumask,
			       const struct cpumask *cpu_mask,
			       const nodemask_t nodemsk,
			       struct cpumask *nmsk,
			       struct node_groups *node_groups)
{
	unsigned n, remaining_ncpus = 0;

	for (n = 0; n < nr_node_ids; n++) {
		node_groups[n].id = n;
		node_groups[n].ncpus = UINT_MAX;
	}

	for_each_node_mask(n, nodemsk) {
		unsigned ncpus;

		cpumask_and(nmsk, cpu_mask, node_to_cpumask[n]);
		ncpus = cpumask_weight(nmsk);

		if (!ncpus)
			continue;
		remaining_ncpus += ncpus;
		node_groups[n].ncpus = ncpus;
	}

	numgrps = min_t(unsigned, remaining_ncpus, numgrps);

	sort(node_groups, nr_node_ids, sizeof(node_groups[0]),
	     ncpus_cmp_func, NULL);

	 
	for (n = 0; n < nr_node_ids; n++) {
		unsigned ngroups, ncpus;

		if (node_groups[n].ncpus == UINT_MAX)
			continue;

		WARN_ON_ONCE(numgrps == 0);

		ncpus = node_groups[n].ncpus;
		ngroups = max_t(unsigned, 1,
				 numgrps * ncpus / remaining_ncpus);
		WARN_ON_ONCE(ngroups > ncpus);

		node_groups[n].ngroups = ngroups;

		remaining_ncpus -= ncpus;
		numgrps -= ngroups;
	}
}

static int __group_cpus_evenly(unsigned int startgrp, unsigned int numgrps,
			       cpumask_var_t *node_to_cpumask,
			       const struct cpumask *cpu_mask,
			       struct cpumask *nmsk, struct cpumask *masks)
{
	unsigned int i, n, nodes, cpus_per_grp, extra_grps, done = 0;
	unsigned int last_grp = numgrps;
	unsigned int curgrp = startgrp;
	nodemask_t nodemsk = NODE_MASK_NONE;
	struct node_groups *node_groups;

	if (cpumask_empty(cpu_mask))
		return 0;

	nodes = get_nodes_in_cpumask(node_to_cpumask, cpu_mask, &nodemsk);

	 
	if (numgrps <= nodes) {
		for_each_node_mask(n, nodemsk) {
			 
			cpumask_and(nmsk, cpu_mask, node_to_cpumask[n]);
			cpumask_or(&masks[curgrp], &masks[curgrp], nmsk);
			if (++curgrp == last_grp)
				curgrp = 0;
		}
		return numgrps;
	}

	node_groups = kcalloc(nr_node_ids,
			       sizeof(struct node_groups),
			       GFP_KERNEL);
	if (!node_groups)
		return -ENOMEM;

	 
	alloc_nodes_groups(numgrps, node_to_cpumask, cpu_mask,
			   nodemsk, nmsk, node_groups);
	for (i = 0; i < nr_node_ids; i++) {
		unsigned int ncpus, v;
		struct node_groups *nv = &node_groups[i];

		if (nv->ngroups == UINT_MAX)
			continue;

		 
		cpumask_and(nmsk, cpu_mask, node_to_cpumask[nv->id]);
		ncpus = cpumask_weight(nmsk);
		if (!ncpus)
			continue;

		WARN_ON_ONCE(nv->ngroups > ncpus);

		 
		extra_grps = ncpus - nv->ngroups * (ncpus / nv->ngroups);

		 
		for (v = 0; v < nv->ngroups; v++, curgrp++) {
			cpus_per_grp = ncpus / nv->ngroups;

			 
			if (extra_grps) {
				cpus_per_grp++;
				--extra_grps;
			}

			 
			if (curgrp >= last_grp)
				curgrp = 0;
			grp_spread_init_one(&masks[curgrp], nmsk,
						cpus_per_grp);
		}
		done += nv->ngroups;
	}
	kfree(node_groups);
	return done;
}

 
struct cpumask *group_cpus_evenly(unsigned int numgrps)
{
	unsigned int curgrp = 0, nr_present = 0, nr_others = 0;
	cpumask_var_t *node_to_cpumask;
	cpumask_var_t nmsk, npresmsk;
	int ret = -ENOMEM;
	struct cpumask *masks = NULL;

	if (!zalloc_cpumask_var(&nmsk, GFP_KERNEL))
		return NULL;

	if (!zalloc_cpumask_var(&npresmsk, GFP_KERNEL))
		goto fail_nmsk;

	node_to_cpumask = alloc_node_to_cpumask();
	if (!node_to_cpumask)
		goto fail_npresmsk;

	masks = kcalloc(numgrps, sizeof(*masks), GFP_KERNEL);
	if (!masks)
		goto fail_node_to_cpumask;

	build_node_to_cpumask(node_to_cpumask);

	 
	cpumask_copy(npresmsk, data_race(cpu_present_mask));

	 
	ret = __group_cpus_evenly(curgrp, numgrps, node_to_cpumask,
				  npresmsk, nmsk, masks);
	if (ret < 0)
		goto fail_build_affinity;
	nr_present = ret;

	 
	if (nr_present >= numgrps)
		curgrp = 0;
	else
		curgrp = nr_present;
	cpumask_andnot(npresmsk, cpu_possible_mask, npresmsk);
	ret = __group_cpus_evenly(curgrp, numgrps, node_to_cpumask,
				  npresmsk, nmsk, masks);
	if (ret >= 0)
		nr_others = ret;

 fail_build_affinity:
	if (ret >= 0)
		WARN_ON(nr_present + nr_others < numgrps);

 fail_node_to_cpumask:
	free_node_to_cpumask(node_to_cpumask);

 fail_npresmsk:
	free_cpumask_var(npresmsk);

 fail_nmsk:
	free_cpumask_var(nmsk);
	if (ret < 0) {
		kfree(masks);
		return NULL;
	}
	return masks;
}
#else  
struct cpumask *group_cpus_evenly(unsigned int numgrps)
{
	struct cpumask *masks = kcalloc(numgrps, sizeof(*masks), GFP_KERNEL);

	if (!masks)
		return NULL;

	 
	cpumask_copy(&masks[0], cpu_possible_mask);
	return masks;
}
#endif  
EXPORT_SYMBOL_GPL(group_cpus_evenly);
