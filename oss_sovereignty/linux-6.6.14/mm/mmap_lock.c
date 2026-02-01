
#define CREATE_TRACE_POINTS
#include <trace/events/mmap_lock.h>

#include <linux/mm.h>
#include <linux/cgroup.h>
#include <linux/memcontrol.h>
#include <linux/mmap_lock.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/trace_events.h>
#include <linux/local_lock.h>

EXPORT_TRACEPOINT_SYMBOL(mmap_lock_start_locking);
EXPORT_TRACEPOINT_SYMBOL(mmap_lock_acquire_returned);
EXPORT_TRACEPOINT_SYMBOL(mmap_lock_released);

#ifdef CONFIG_MEMCG

 
static DEFINE_MUTEX(reg_lock);
static int reg_refcount;  

 
#define MEMCG_PATH_BUF_SIZE MAX_FILTER_STR_VAL

 
#define CONTEXT_COUNT 4

struct memcg_path {
	local_lock_t lock;
	char __rcu *buf;
	local_t buf_idx;
};
static DEFINE_PER_CPU(struct memcg_path, memcg_paths) = {
	.lock = INIT_LOCAL_LOCK(lock),
	.buf_idx = LOCAL_INIT(0),
};

static char **tmp_bufs;

 
static void free_memcg_path_bufs(void)
{
	struct memcg_path *memcg_path;
	int cpu;
	char **old = tmp_bufs;

	for_each_possible_cpu(cpu) {
		memcg_path = per_cpu_ptr(&memcg_paths, cpu);
		*(old++) = rcu_dereference_protected(memcg_path->buf,
			lockdep_is_held(&reg_lock));
		rcu_assign_pointer(memcg_path->buf, NULL);
	}

	 
	synchronize_rcu();

	old = tmp_bufs;
	for_each_possible_cpu(cpu) {
		kfree(*(old++));
	}

	kfree(tmp_bufs);
	tmp_bufs = NULL;
}

int trace_mmap_lock_reg(void)
{
	int cpu;
	char *new;

	mutex_lock(&reg_lock);

	 
	if (reg_refcount++)
		goto out;

	tmp_bufs = kmalloc_array(num_possible_cpus(), sizeof(*tmp_bufs),
				 GFP_KERNEL);
	if (tmp_bufs == NULL)
		goto out_fail;

	for_each_possible_cpu(cpu) {
		new = kmalloc(MEMCG_PATH_BUF_SIZE * CONTEXT_COUNT, GFP_KERNEL);
		if (new == NULL)
			goto out_fail_free;
		rcu_assign_pointer(per_cpu_ptr(&memcg_paths, cpu)->buf, new);
		 
	}

out:
	mutex_unlock(&reg_lock);
	return 0;

out_fail_free:
	free_memcg_path_bufs();
out_fail:
	 
	--reg_refcount;

	mutex_unlock(&reg_lock);
	return -ENOMEM;
}

void trace_mmap_lock_unreg(void)
{
	mutex_lock(&reg_lock);

	 
	if (--reg_refcount)
		goto out;

	free_memcg_path_bufs();

out:
	mutex_unlock(&reg_lock);
}

static inline char *get_memcg_path_buf(void)
{
	struct memcg_path *memcg_path = this_cpu_ptr(&memcg_paths);
	char *buf;
	int idx;

	rcu_read_lock();
	buf = rcu_dereference(memcg_path->buf);
	if (buf == NULL) {
		rcu_read_unlock();
		return NULL;
	}
	idx = local_add_return(MEMCG_PATH_BUF_SIZE, &memcg_path->buf_idx) -
	      MEMCG_PATH_BUF_SIZE;
	return &buf[idx];
}

static inline void put_memcg_path_buf(void)
{
	local_sub(MEMCG_PATH_BUF_SIZE, &this_cpu_ptr(&memcg_paths)->buf_idx);
	rcu_read_unlock();
}

#define TRACE_MMAP_LOCK_EVENT(type, mm, ...)                                   \
	do {                                                                   \
		const char *memcg_path;                                        \
		local_lock(&memcg_paths.lock);                                 \
		memcg_path = get_mm_memcg_path(mm);                            \
		trace_mmap_lock_##type(mm,                                     \
				       memcg_path != NULL ? memcg_path : "",   \
				       ##__VA_ARGS__);                         \
		if (likely(memcg_path != NULL))                                \
			put_memcg_path_buf();                                  \
		local_unlock(&memcg_paths.lock);                               \
	} while (0)

#else  

int trace_mmap_lock_reg(void)
{
	return 0;
}

void trace_mmap_lock_unreg(void)
{
}

#define TRACE_MMAP_LOCK_EVENT(type, mm, ...)                                   \
	trace_mmap_lock_##type(mm, "", ##__VA_ARGS__)

#endif  

#ifdef CONFIG_TRACING
#ifdef CONFIG_MEMCG
 
static const char *get_mm_memcg_path(struct mm_struct *mm)
{
	char *buf = NULL;
	struct mem_cgroup *memcg = get_mem_cgroup_from_mm(mm);

	if (memcg == NULL)
		goto out;
	if (unlikely(memcg->css.cgroup == NULL))
		goto out_put;

	buf = get_memcg_path_buf();
	if (buf == NULL)
		goto out_put;

	cgroup_path(memcg->css.cgroup, buf, MEMCG_PATH_BUF_SIZE);

out_put:
	css_put(&memcg->css);
out:
	return buf;
}

#endif  

 

void __mmap_lock_do_trace_start_locking(struct mm_struct *mm, bool write)
{
	TRACE_MMAP_LOCK_EVENT(start_locking, mm, write);
}
EXPORT_SYMBOL(__mmap_lock_do_trace_start_locking);

void __mmap_lock_do_trace_acquire_returned(struct mm_struct *mm, bool write,
					   bool success)
{
	TRACE_MMAP_LOCK_EVENT(acquire_returned, mm, write, success);
}
EXPORT_SYMBOL(__mmap_lock_do_trace_acquire_returned);

void __mmap_lock_do_trace_released(struct mm_struct *mm, bool write)
{
	TRACE_MMAP_LOCK_EVENT(released, mm, write);
}
EXPORT_SYMBOL(__mmap_lock_do_trace_released);
#endif  
