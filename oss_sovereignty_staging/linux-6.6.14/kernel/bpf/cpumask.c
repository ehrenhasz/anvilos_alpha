
 
#include <linux/bpf.h>
#include <linux/bpf_mem_alloc.h>
#include <linux/btf.h>
#include <linux/btf_ids.h>
#include <linux/cpumask.h>

 
struct bpf_cpumask {
	cpumask_t cpumask;
	refcount_t usage;
};

static struct bpf_mem_alloc bpf_cpumask_ma;

static bool cpu_valid(u32 cpu)
{
	return cpu < nr_cpu_ids;
}

__diag_push();
__diag_ignore_all("-Wmissing-prototypes",
		  "Global kfuncs as their definitions will be in BTF");

 
__bpf_kfunc struct bpf_cpumask *bpf_cpumask_create(void)
{
	struct bpf_cpumask *cpumask;

	 
	BUILD_BUG_ON(offsetof(struct bpf_cpumask, cpumask) != 0);

	cpumask = bpf_mem_cache_alloc(&bpf_cpumask_ma);
	if (!cpumask)
		return NULL;

	memset(cpumask, 0, sizeof(*cpumask));
	refcount_set(&cpumask->usage, 1);

	return cpumask;
}

 
__bpf_kfunc struct bpf_cpumask *bpf_cpumask_acquire(struct bpf_cpumask *cpumask)
{
	refcount_inc(&cpumask->usage);
	return cpumask;
}

 
__bpf_kfunc void bpf_cpumask_release(struct bpf_cpumask *cpumask)
{
	if (!refcount_dec_and_test(&cpumask->usage))
		return;

	migrate_disable();
	bpf_mem_cache_free_rcu(&bpf_cpumask_ma, cpumask);
	migrate_enable();
}

 
__bpf_kfunc u32 bpf_cpumask_first(const struct cpumask *cpumask)
{
	return cpumask_first(cpumask);
}

 
__bpf_kfunc u32 bpf_cpumask_first_zero(const struct cpumask *cpumask)
{
	return cpumask_first_zero(cpumask);
}

 
__bpf_kfunc u32 bpf_cpumask_first_and(const struct cpumask *src1,
				      const struct cpumask *src2)
{
	return cpumask_first_and(src1, src2);
}

 
__bpf_kfunc void bpf_cpumask_set_cpu(u32 cpu, struct bpf_cpumask *cpumask)
{
	if (!cpu_valid(cpu))
		return;

	cpumask_set_cpu(cpu, (struct cpumask *)cpumask);
}

 
__bpf_kfunc void bpf_cpumask_clear_cpu(u32 cpu, struct bpf_cpumask *cpumask)
{
	if (!cpu_valid(cpu))
		return;

	cpumask_clear_cpu(cpu, (struct cpumask *)cpumask);
}

 
__bpf_kfunc bool bpf_cpumask_test_cpu(u32 cpu, const struct cpumask *cpumask)
{
	if (!cpu_valid(cpu))
		return false;

	return cpumask_test_cpu(cpu, (struct cpumask *)cpumask);
}

 
__bpf_kfunc bool bpf_cpumask_test_and_set_cpu(u32 cpu, struct bpf_cpumask *cpumask)
{
	if (!cpu_valid(cpu))
		return false;

	return cpumask_test_and_set_cpu(cpu, (struct cpumask *)cpumask);
}

 
__bpf_kfunc bool bpf_cpumask_test_and_clear_cpu(u32 cpu, struct bpf_cpumask *cpumask)
{
	if (!cpu_valid(cpu))
		return false;

	return cpumask_test_and_clear_cpu(cpu, (struct cpumask *)cpumask);
}

 
__bpf_kfunc void bpf_cpumask_setall(struct bpf_cpumask *cpumask)
{
	cpumask_setall((struct cpumask *)cpumask);
}

 
__bpf_kfunc void bpf_cpumask_clear(struct bpf_cpumask *cpumask)
{
	cpumask_clear((struct cpumask *)cpumask);
}

 
__bpf_kfunc bool bpf_cpumask_and(struct bpf_cpumask *dst,
				 const struct cpumask *src1,
				 const struct cpumask *src2)
{
	return cpumask_and((struct cpumask *)dst, src1, src2);
}

 
__bpf_kfunc void bpf_cpumask_or(struct bpf_cpumask *dst,
				const struct cpumask *src1,
				const struct cpumask *src2)
{
	cpumask_or((struct cpumask *)dst, src1, src2);
}

 
__bpf_kfunc void bpf_cpumask_xor(struct bpf_cpumask *dst,
				 const struct cpumask *src1,
				 const struct cpumask *src2)
{
	cpumask_xor((struct cpumask *)dst, src1, src2);
}

 
__bpf_kfunc bool bpf_cpumask_equal(const struct cpumask *src1, const struct cpumask *src2)
{
	return cpumask_equal(src1, src2);
}

 
__bpf_kfunc bool bpf_cpumask_intersects(const struct cpumask *src1, const struct cpumask *src2)
{
	return cpumask_intersects(src1, src2);
}

 
__bpf_kfunc bool bpf_cpumask_subset(const struct cpumask *src1, const struct cpumask *src2)
{
	return cpumask_subset(src1, src2);
}

 
__bpf_kfunc bool bpf_cpumask_empty(const struct cpumask *cpumask)
{
	return cpumask_empty(cpumask);
}

 
__bpf_kfunc bool bpf_cpumask_full(const struct cpumask *cpumask)
{
	return cpumask_full(cpumask);
}

 
__bpf_kfunc void bpf_cpumask_copy(struct bpf_cpumask *dst, const struct cpumask *src)
{
	cpumask_copy((struct cpumask *)dst, src);
}

 
__bpf_kfunc u32 bpf_cpumask_any_distribute(const struct cpumask *cpumask)
{
	return cpumask_any_distribute(cpumask);
}

 
__bpf_kfunc u32 bpf_cpumask_any_and_distribute(const struct cpumask *src1,
					       const struct cpumask *src2)
{
	return cpumask_any_and_distribute(src1, src2);
}

__diag_pop();

BTF_SET8_START(cpumask_kfunc_btf_ids)
BTF_ID_FLAGS(func, bpf_cpumask_create, KF_ACQUIRE | KF_RET_NULL)
BTF_ID_FLAGS(func, bpf_cpumask_release, KF_RELEASE)
BTF_ID_FLAGS(func, bpf_cpumask_acquire, KF_ACQUIRE | KF_TRUSTED_ARGS)
BTF_ID_FLAGS(func, bpf_cpumask_first, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_first_zero, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_first_and, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_set_cpu, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_clear_cpu, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_test_cpu, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_test_and_set_cpu, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_test_and_clear_cpu, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_setall, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_clear, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_and, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_or, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_xor, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_equal, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_intersects, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_subset, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_empty, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_full, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_copy, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_any_distribute, KF_RCU)
BTF_ID_FLAGS(func, bpf_cpumask_any_and_distribute, KF_RCU)
BTF_SET8_END(cpumask_kfunc_btf_ids)

static const struct btf_kfunc_id_set cpumask_kfunc_set = {
	.owner = THIS_MODULE,
	.set   = &cpumask_kfunc_btf_ids,
};

BTF_ID_LIST(cpumask_dtor_ids)
BTF_ID(struct, bpf_cpumask)
BTF_ID(func, bpf_cpumask_release)

static int __init cpumask_kfunc_init(void)
{
	int ret;
	const struct btf_id_dtor_kfunc cpumask_dtors[] = {
		{
			.btf_id	      = cpumask_dtor_ids[0],
			.kfunc_btf_id = cpumask_dtor_ids[1]
		},
	};

	ret = bpf_mem_alloc_init(&bpf_cpumask_ma, sizeof(struct bpf_cpumask), false);
	ret = ret ?: register_btf_kfunc_id_set(BPF_PROG_TYPE_TRACING, &cpumask_kfunc_set);
	ret = ret ?: register_btf_kfunc_id_set(BPF_PROG_TYPE_STRUCT_OPS, &cpumask_kfunc_set);
	return  ret ?: register_btf_id_dtor_kfuncs(cpumask_dtors,
						   ARRAY_SIZE(cpumask_dtors),
						   THIS_MODULE);
}

late_initcall(cpumask_kfunc_init);
