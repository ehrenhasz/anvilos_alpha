
 
#include <linux/coresight-pmu.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "coresight-trace-id.h"

 
static struct coresight_trace_id_map id_map_default;

 
static DEFINE_PER_CPU(atomic_t, cpu_id) = ATOMIC_INIT(0);
static cpumask_t cpu_id_release_pending;

 
static atomic_t perf_cs_etm_session_active = ATOMIC_INIT(0);

 
static DEFINE_SPINLOCK(id_map_lock);

 
#if defined(TRACE_ID_DEBUG) || defined(CONFIG_COMPILE_TEST)

static void coresight_trace_id_dump_table(struct coresight_trace_id_map *id_map,
					  const char *func_name)
{
	pr_debug("%s id_map::\n", func_name);
	pr_debug("Used = %*pb\n", CORESIGHT_TRACE_IDS_MAX, id_map->used_ids);
	pr_debug("Pend = %*pb\n", CORESIGHT_TRACE_IDS_MAX, id_map->pend_rel_ids);
}
#define DUMP_ID_MAP(map)   coresight_trace_id_dump_table(map, __func__)
#define DUMP_ID_CPU(cpu, id) pr_debug("%s called;  cpu=%d, id=%d\n", __func__, cpu, id)
#define DUMP_ID(id)   pr_debug("%s called; id=%d\n", __func__, id)
#define PERF_SESSION(n) pr_debug("%s perf count %d\n", __func__, n)
#else
#define DUMP_ID_MAP(map)
#define DUMP_ID(id)
#define DUMP_ID_CPU(cpu, id)
#define PERF_SESSION(n)
#endif

 
static int _coresight_trace_id_read_cpu_id(int cpu)
{
	return atomic_read(&per_cpu(cpu_id, cpu));
}

 
static int coresight_trace_id_find_odd_id(struct coresight_trace_id_map *id_map)
{
	int found_id = 0, bit = 1, next_id;

	while ((bit < CORESIGHT_TRACE_ID_RES_TOP) && !found_id) {
		 
		next_id = find_next_zero_bit(id_map->used_ids,
					     CORESIGHT_TRACE_ID_RES_TOP, bit);
		if ((next_id < CORESIGHT_TRACE_ID_RES_TOP) && (next_id & 0x1))
			found_id = next_id;
		else
			bit = next_id + 1;
	}
	return found_id;
}

 
static int coresight_trace_id_alloc_new_id(struct coresight_trace_id_map *id_map,
					   int preferred_id, bool prefer_odd_id)
{
	int id = 0;

	 
	if (IS_VALID_CS_TRACE_ID(preferred_id) &&
	    !test_bit(preferred_id, id_map->used_ids)) {
		id = preferred_id;
		goto trace_id_allocated;
	} else if (prefer_odd_id) {
	 
		id = coresight_trace_id_find_odd_id(id_map);
		if (id)
			goto trace_id_allocated;
	}

	 
	id = find_next_zero_bit(id_map->used_ids, CORESIGHT_TRACE_ID_RES_TOP, 1);
	if (id >= CORESIGHT_TRACE_ID_RES_TOP)
		return -EINVAL;

	 
trace_id_allocated:
	set_bit(id, id_map->used_ids);
	return id;
}

static void coresight_trace_id_free(int id, struct coresight_trace_id_map *id_map)
{
	if (WARN(!IS_VALID_CS_TRACE_ID(id), "Invalid Trace ID %d\n", id))
		return;
	if (WARN(!test_bit(id, id_map->used_ids), "Freeing unused ID %d\n", id))
		return;
	clear_bit(id, id_map->used_ids);
}

static void coresight_trace_id_set_pend_rel(int id, struct coresight_trace_id_map *id_map)
{
	if (WARN(!IS_VALID_CS_TRACE_ID(id), "Invalid Trace ID %d\n", id))
		return;
	set_bit(id, id_map->pend_rel_ids);
}

 
static void coresight_trace_id_release_all_pending(void)
{
	struct coresight_trace_id_map *id_map = &id_map_default;
	unsigned long flags;
	int cpu, bit;

	spin_lock_irqsave(&id_map_lock, flags);
	for_each_set_bit(bit, id_map->pend_rel_ids, CORESIGHT_TRACE_ID_RES_TOP) {
		clear_bit(bit, id_map->used_ids);
		clear_bit(bit, id_map->pend_rel_ids);
	}
	for_each_cpu(cpu, &cpu_id_release_pending) {
		atomic_set(&per_cpu(cpu_id, cpu), 0);
		cpumask_clear_cpu(cpu, &cpu_id_release_pending);
	}
	spin_unlock_irqrestore(&id_map_lock, flags);
	DUMP_ID_MAP(id_map);
}

static int coresight_trace_id_map_get_cpu_id(int cpu, struct coresight_trace_id_map *id_map)
{
	unsigned long flags;
	int id;

	spin_lock_irqsave(&id_map_lock, flags);

	 
	id = _coresight_trace_id_read_cpu_id(cpu);
	if (id)
		goto get_cpu_id_clr_pend;

	 
	id = coresight_trace_id_alloc_new_id(id_map,
					     CORESIGHT_LEGACY_CPU_TRACE_ID(cpu),
					     false);
	if (!IS_VALID_CS_TRACE_ID(id))
		goto get_cpu_id_out_unlock;

	 
	atomic_set(&per_cpu(cpu_id, cpu), id);

get_cpu_id_clr_pend:
	 
	cpumask_clear_cpu(cpu, &cpu_id_release_pending);
	clear_bit(id, id_map->pend_rel_ids);

get_cpu_id_out_unlock:
	spin_unlock_irqrestore(&id_map_lock, flags);

	DUMP_ID_CPU(cpu, id);
	DUMP_ID_MAP(id_map);
	return id;
}

static void coresight_trace_id_map_put_cpu_id(int cpu, struct coresight_trace_id_map *id_map)
{
	unsigned long flags;
	int id;

	 
	id = _coresight_trace_id_read_cpu_id(cpu);
	if (!id)
		return;

	spin_lock_irqsave(&id_map_lock, flags);

	if (atomic_read(&perf_cs_etm_session_active)) {
		 
		coresight_trace_id_set_pend_rel(id, id_map);
		cpumask_set_cpu(cpu, &cpu_id_release_pending);
	} else {
		 
		coresight_trace_id_free(id, id_map);
		atomic_set(&per_cpu(cpu_id, cpu), 0);
	}

	spin_unlock_irqrestore(&id_map_lock, flags);
	DUMP_ID_CPU(cpu, id);
	DUMP_ID_MAP(id_map);
}

static int coresight_trace_id_map_get_system_id(struct coresight_trace_id_map *id_map)
{
	unsigned long flags;
	int id;

	spin_lock_irqsave(&id_map_lock, flags);
	 
	id = coresight_trace_id_alloc_new_id(id_map, 0, true);
	spin_unlock_irqrestore(&id_map_lock, flags);

	DUMP_ID(id);
	DUMP_ID_MAP(id_map);
	return id;
}

static void coresight_trace_id_map_put_system_id(struct coresight_trace_id_map *id_map, int id)
{
	unsigned long flags;

	spin_lock_irqsave(&id_map_lock, flags);
	coresight_trace_id_free(id, id_map);
	spin_unlock_irqrestore(&id_map_lock, flags);

	DUMP_ID(id);
	DUMP_ID_MAP(id_map);
}

 

int coresight_trace_id_get_cpu_id(int cpu)
{
	return coresight_trace_id_map_get_cpu_id(cpu, &id_map_default);
}
EXPORT_SYMBOL_GPL(coresight_trace_id_get_cpu_id);

void coresight_trace_id_put_cpu_id(int cpu)
{
	coresight_trace_id_map_put_cpu_id(cpu, &id_map_default);
}
EXPORT_SYMBOL_GPL(coresight_trace_id_put_cpu_id);

int coresight_trace_id_read_cpu_id(int cpu)
{
	return _coresight_trace_id_read_cpu_id(cpu);
}
EXPORT_SYMBOL_GPL(coresight_trace_id_read_cpu_id);

int coresight_trace_id_get_system_id(void)
{
	return coresight_trace_id_map_get_system_id(&id_map_default);
}
EXPORT_SYMBOL_GPL(coresight_trace_id_get_system_id);

void coresight_trace_id_put_system_id(int id)
{
	coresight_trace_id_map_put_system_id(&id_map_default, id);
}
EXPORT_SYMBOL_GPL(coresight_trace_id_put_system_id);

void coresight_trace_id_perf_start(void)
{
	atomic_inc(&perf_cs_etm_session_active);
	PERF_SESSION(atomic_read(&perf_cs_etm_session_active));
}
EXPORT_SYMBOL_GPL(coresight_trace_id_perf_start);

void coresight_trace_id_perf_stop(void)
{
	if (!atomic_dec_return(&perf_cs_etm_session_active))
		coresight_trace_id_release_all_pending();
	PERF_SESSION(atomic_read(&perf_cs_etm_session_active));
}
EXPORT_SYMBOL_GPL(coresight_trace_id_perf_stop);
