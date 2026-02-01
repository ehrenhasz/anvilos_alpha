
 

#include "mlx5_core.h"
#include "mlx5_irq.h"
#include "pci_irq.h"

static void cpu_put(struct mlx5_irq_pool *pool, int cpu)
{
	pool->irqs_per_cpu[cpu]--;
}

static void cpu_get(struct mlx5_irq_pool *pool, int cpu)
{
	pool->irqs_per_cpu[cpu]++;
}

 
static int cpu_get_least_loaded(struct mlx5_irq_pool *pool,
				const struct cpumask *req_mask)
{
	int best_cpu = -1;
	int cpu;

	for_each_cpu_and(cpu, req_mask, cpu_online_mask) {
		 
		if (!pool->irqs_per_cpu[cpu]) {
			best_cpu = cpu;
			break;
		}
		if (best_cpu < 0)
			best_cpu = cpu;
		if (pool->irqs_per_cpu[cpu] < pool->irqs_per_cpu[best_cpu])
			best_cpu = cpu;
	}
	if (best_cpu == -1) {
		 
		mlx5_core_err(pool->dev, "NO online CPUs in req_mask (%*pbl)\n",
			      cpumask_pr_args(req_mask));
		best_cpu = cpumask_first(cpu_online_mask);
	}
	pool->irqs_per_cpu[best_cpu]++;
	return best_cpu;
}

 
static struct mlx5_irq *
irq_pool_request_irq(struct mlx5_irq_pool *pool, struct irq_affinity_desc *af_desc)
{
	struct irq_affinity_desc auto_desc = {};
	u32 irq_index;
	int err;

	err = xa_alloc(&pool->irqs, &irq_index, NULL, pool->xa_num_irqs, GFP_KERNEL);
	if (err)
		return ERR_PTR(err);
	if (pool->irqs_per_cpu) {
		if (cpumask_weight(&af_desc->mask) > 1)
			 
			cpumask_set_cpu(cpu_get_least_loaded(pool, &af_desc->mask),
					&auto_desc.mask);
		else
			cpu_get(pool, cpumask_first(&af_desc->mask));
	}
	return mlx5_irq_alloc(pool, irq_index,
			      cpumask_empty(&auto_desc.mask) ? af_desc : &auto_desc,
			      NULL);
}

 
static struct mlx5_irq *
irq_pool_find_least_loaded(struct mlx5_irq_pool *pool, const struct cpumask *req_mask)
{
	int start = pool->xa_num_irqs.min;
	int end = pool->xa_num_irqs.max;
	struct mlx5_irq *irq = NULL;
	struct mlx5_irq *iter;
	int irq_refcount = 0;
	unsigned long index;

	lockdep_assert_held(&pool->lock);
	xa_for_each_range(&pool->irqs, index, iter, start, end) {
		struct cpumask *iter_mask = mlx5_irq_get_affinity_mask(iter);
		int iter_refcount = mlx5_irq_read_locked(iter);

		if (!cpumask_subset(iter_mask, req_mask))
			 
			continue;
		if (iter_refcount < pool->min_threshold)
			 
			return iter;
		if (!irq || iter_refcount < irq_refcount) {
			 
			irq_refcount = iter_refcount;
			irq = iter;
		}
	}
	return irq;
}

 
struct mlx5_irq *
mlx5_irq_affinity_request(struct mlx5_irq_pool *pool, struct irq_affinity_desc *af_desc)
{
	struct mlx5_irq *least_loaded_irq, *new_irq;

	mutex_lock(&pool->lock);
	least_loaded_irq = irq_pool_find_least_loaded(pool, &af_desc->mask);
	if (least_loaded_irq &&
	    mlx5_irq_read_locked(least_loaded_irq) < pool->min_threshold)
		goto out;
	 
	new_irq = irq_pool_request_irq(pool, af_desc);
	if (IS_ERR(new_irq)) {
		if (!least_loaded_irq) {
			 
			mlx5_core_err(pool->dev, "Didn't find a matching IRQ. err = %ld\n",
				      PTR_ERR(new_irq));
			mutex_unlock(&pool->lock);
			return new_irq;
		}
		 
		goto out;
	}
	least_loaded_irq = new_irq;
	goto unlock;
out:
	mlx5_irq_get_locked(least_loaded_irq);
	if (mlx5_irq_read_locked(least_loaded_irq) > pool->max_threshold)
		mlx5_core_dbg(pool->dev, "IRQ %u overloaded, pool_name: %s, %u EQs on this irq\n",
			      pci_irq_vector(pool->dev->pdev,
					     mlx5_irq_get_index(least_loaded_irq)), pool->name,
			      mlx5_irq_read_locked(least_loaded_irq) / MLX5_EQ_REFS_PER_IRQ);
unlock:
	mutex_unlock(&pool->lock);
	return least_loaded_irq;
}

void mlx5_irq_affinity_irq_release(struct mlx5_core_dev *dev, struct mlx5_irq *irq)
{
	struct mlx5_irq_pool *pool = mlx5_irq_pool_get(dev);
	int cpu;

	cpu = cpumask_first(mlx5_irq_get_affinity_mask(irq));
	synchronize_irq(pci_irq_vector(pool->dev->pdev,
				       mlx5_irq_get_index(irq)));
	if (mlx5_irq_put(irq))
		if (pool->irqs_per_cpu)
			cpu_put(pool, cpu);
}
