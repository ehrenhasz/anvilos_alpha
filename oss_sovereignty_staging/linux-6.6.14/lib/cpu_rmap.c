
 

#include <linux/cpu_rmap.h>
#include <linux/interrupt.h>
#include <linux/export.h>

 

 
struct cpu_rmap *alloc_cpu_rmap(unsigned int size, gfp_t flags)
{
	struct cpu_rmap *rmap;
	unsigned int cpu;
	size_t obj_offset;

	 
	if (size > 0xffff)
		return NULL;

	 
	obj_offset = ALIGN(offsetof(struct cpu_rmap, near[nr_cpu_ids]),
			   sizeof(void *));

	rmap = kzalloc(obj_offset + size * sizeof(rmap->obj[0]), flags);
	if (!rmap)
		return NULL;

	kref_init(&rmap->refcount);
	rmap->obj = (void **)((char *)rmap + obj_offset);

	 
	for_each_possible_cpu(cpu) {
		rmap->near[cpu].index = cpu % size;
		rmap->near[cpu].dist = CPU_RMAP_DIST_INF;
	}

	rmap->size = size;
	return rmap;
}
EXPORT_SYMBOL(alloc_cpu_rmap);

 
static void cpu_rmap_release(struct kref *ref)
{
	struct cpu_rmap *rmap = container_of(ref, struct cpu_rmap, refcount);
	kfree(rmap);
}

 
static inline void cpu_rmap_get(struct cpu_rmap *rmap)
{
	kref_get(&rmap->refcount);
}

 
int cpu_rmap_put(struct cpu_rmap *rmap)
{
	return kref_put(&rmap->refcount, cpu_rmap_release);
}
EXPORT_SYMBOL(cpu_rmap_put);

 
static bool cpu_rmap_copy_neigh(struct cpu_rmap *rmap, unsigned int cpu,
				const struct cpumask *mask, u16 dist)
{
	int neigh;

	for_each_cpu(neigh, mask) {
		if (rmap->near[cpu].dist > dist &&
		    rmap->near[neigh].dist <= dist) {
			rmap->near[cpu].index = rmap->near[neigh].index;
			rmap->near[cpu].dist = dist;
			return true;
		}
	}
	return false;
}

#ifdef DEBUG
static void debug_print_rmap(const struct cpu_rmap *rmap, const char *prefix)
{
	unsigned index;
	unsigned int cpu;

	pr_info("cpu_rmap %p, %s:\n", rmap, prefix);

	for_each_possible_cpu(cpu) {
		index = rmap->near[cpu].index;
		pr_info("cpu %d -> obj %u (distance %u)\n",
			cpu, index, rmap->near[cpu].dist);
	}
}
#else
static inline void
debug_print_rmap(const struct cpu_rmap *rmap, const char *prefix)
{
}
#endif

static int get_free_index(struct cpu_rmap *rmap)
{
	int i;

	for (i = 0; i < rmap->size; i++)
		if (!rmap->obj[i])
			return i;

	return -ENOSPC;
}

 
int cpu_rmap_add(struct cpu_rmap *rmap, void *obj)
{
	int index = get_free_index(rmap);

	if (index < 0)
		return index;

	rmap->obj[index] = obj;
	return index;
}
EXPORT_SYMBOL(cpu_rmap_add);

 
int cpu_rmap_update(struct cpu_rmap *rmap, u16 index,
		    const struct cpumask *affinity)
{
	cpumask_var_t update_mask;
	unsigned int cpu;

	if (unlikely(!zalloc_cpumask_var(&update_mask, GFP_KERNEL)))
		return -ENOMEM;

	 
	for_each_online_cpu(cpu) {
		if (rmap->near[cpu].index == index) {
			rmap->near[cpu].dist = CPU_RMAP_DIST_INF;
			cpumask_set_cpu(cpu, update_mask);
		}
	}

	debug_print_rmap(rmap, "after invalidating old distances");

	 
	for_each_cpu(cpu, affinity) {
		rmap->near[cpu].index = index;
		rmap->near[cpu].dist = 0;
		cpumask_or(update_mask, update_mask,
			   cpumask_of_node(cpu_to_node(cpu)));
	}

	debug_print_rmap(rmap, "after updating neighbours");

	 
	for_each_cpu(cpu, update_mask) {
		if (cpu_rmap_copy_neigh(rmap, cpu,
					topology_sibling_cpumask(cpu), 1))
			continue;
		if (cpu_rmap_copy_neigh(rmap, cpu,
					topology_core_cpumask(cpu), 2))
			continue;
		if (cpu_rmap_copy_neigh(rmap, cpu,
					cpumask_of_node(cpu_to_node(cpu)), 3))
			continue;
		 
	}

	debug_print_rmap(rmap, "after copying neighbours");

	free_cpumask_var(update_mask);
	return 0;
}
EXPORT_SYMBOL(cpu_rmap_update);

 

struct irq_glue {
	struct irq_affinity_notify notify;
	struct cpu_rmap *rmap;
	u16 index;
};

 
void free_irq_cpu_rmap(struct cpu_rmap *rmap)
{
	struct irq_glue *glue;
	u16 index;

	if (!rmap)
		return;

	for (index = 0; index < rmap->size; index++) {
		glue = rmap->obj[index];
		if (glue)
			irq_set_affinity_notifier(glue->notify.irq, NULL);
	}

	cpu_rmap_put(rmap);
}
EXPORT_SYMBOL(free_irq_cpu_rmap);

 
static void
irq_cpu_rmap_notify(struct irq_affinity_notify *notify, const cpumask_t *mask)
{
	struct irq_glue *glue =
		container_of(notify, struct irq_glue, notify);
	int rc;

	rc = cpu_rmap_update(glue->rmap, glue->index, mask);
	if (rc)
		pr_warn("irq_cpu_rmap_notify: update failed: %d\n", rc);
}

 
static void irq_cpu_rmap_release(struct kref *ref)
{
	struct irq_glue *glue =
		container_of(ref, struct irq_glue, notify.kref);

	glue->rmap->obj[glue->index] = NULL;
	cpu_rmap_put(glue->rmap);
	kfree(glue);
}

 
int irq_cpu_rmap_remove(struct cpu_rmap *rmap, int irq)
{
	return irq_set_affinity_notifier(irq, NULL);
}
EXPORT_SYMBOL(irq_cpu_rmap_remove);

 
int irq_cpu_rmap_add(struct cpu_rmap *rmap, int irq)
{
	struct irq_glue *glue = kzalloc(sizeof(*glue), GFP_KERNEL);
	int rc;

	if (!glue)
		return -ENOMEM;
	glue->notify.notify = irq_cpu_rmap_notify;
	glue->notify.release = irq_cpu_rmap_release;
	glue->rmap = rmap;
	cpu_rmap_get(rmap);
	rc = cpu_rmap_add(rmap, glue);
	if (rc < 0)
		goto err_add;

	glue->index = rc;
	rc = irq_set_affinity_notifier(irq, &glue->notify);
	if (rc)
		goto err_set;

	return rc;

err_set:
	rmap->obj[glue->index] = NULL;
err_add:
	cpu_rmap_put(glue->rmap);
	kfree(glue);
	return rc;
}
EXPORT_SYMBOL(irq_cpu_rmap_add);
