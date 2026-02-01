
 

 
 
#include <linux/bitops.h>
#include <linux/bpf.h>
#include <linux/filter.h>
#include <linux/ptr_ring.h>
#include <net/xdp.h>

#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <trace/events/xdp.h>
#include <linux/btf_ids.h>

#include <linux/netdevice.h>    
#include <linux/etherdevice.h>  

 

#define CPU_MAP_BULK_SIZE 8   
struct bpf_cpu_map_entry;
struct bpf_cpu_map;

struct xdp_bulk_queue {
	void *q[CPU_MAP_BULK_SIZE];
	struct list_head flush_node;
	struct bpf_cpu_map_entry *obj;
	unsigned int count;
};

 
struct bpf_cpu_map_entry {
	u32 cpu;     
	int map_id;  

	 
	struct xdp_bulk_queue __percpu *bulkq;

	 
	struct ptr_ring *queue;
	struct task_struct *kthread;

	struct bpf_cpumap_val value;
	struct bpf_prog *prog;

	struct completion kthread_running;
	struct rcu_work free_work;
};

struct bpf_cpu_map {
	struct bpf_map map;
	 
	struct bpf_cpu_map_entry __rcu **cpu_map;
};

static DEFINE_PER_CPU(struct list_head, cpu_map_flush_list);

static struct bpf_map *cpu_map_alloc(union bpf_attr *attr)
{
	u32 value_size = attr->value_size;
	struct bpf_cpu_map *cmap;

	 
	if (attr->max_entries == 0 || attr->key_size != 4 ||
	    (value_size != offsetofend(struct bpf_cpumap_val, qsize) &&
	     value_size != offsetofend(struct bpf_cpumap_val, bpf_prog.fd)) ||
	    attr->map_flags & ~BPF_F_NUMA_NODE)
		return ERR_PTR(-EINVAL);

	 
	if (attr->max_entries > NR_CPUS)
		return ERR_PTR(-E2BIG);

	cmap = bpf_map_area_alloc(sizeof(*cmap), NUMA_NO_NODE);
	if (!cmap)
		return ERR_PTR(-ENOMEM);

	bpf_map_init_from_attr(&cmap->map, attr);

	 
	cmap->cpu_map = bpf_map_area_alloc(cmap->map.max_entries *
					   sizeof(struct bpf_cpu_map_entry *),
					   cmap->map.numa_node);
	if (!cmap->cpu_map) {
		bpf_map_area_free(cmap);
		return ERR_PTR(-ENOMEM);
	}

	return &cmap->map;
}

static void __cpu_map_ring_cleanup(struct ptr_ring *ring)
{
	 
	void *ptr;

	while ((ptr = ptr_ring_consume(ring))) {
		WARN_ON_ONCE(1);
		if (unlikely(__ptr_test_bit(0, &ptr))) {
			__ptr_clear_bit(0, &ptr);
			kfree_skb(ptr);
			continue;
		}
		xdp_return_frame(ptr);
	}
}

static void cpu_map_bpf_prog_run_skb(struct bpf_cpu_map_entry *rcpu,
				     struct list_head *listp,
				     struct xdp_cpumap_stats *stats)
{
	struct sk_buff *skb, *tmp;
	struct xdp_buff xdp;
	u32 act;
	int err;

	list_for_each_entry_safe(skb, tmp, listp, list) {
		act = bpf_prog_run_generic_xdp(skb, &xdp, rcpu->prog);
		switch (act) {
		case XDP_PASS:
			break;
		case XDP_REDIRECT:
			skb_list_del_init(skb);
			err = xdp_do_generic_redirect(skb->dev, skb, &xdp,
						      rcpu->prog);
			if (unlikely(err)) {
				kfree_skb(skb);
				stats->drop++;
			} else {
				stats->redirect++;
			}
			return;
		default:
			bpf_warn_invalid_xdp_action(NULL, rcpu->prog, act);
			fallthrough;
		case XDP_ABORTED:
			trace_xdp_exception(skb->dev, rcpu->prog, act);
			fallthrough;
		case XDP_DROP:
			skb_list_del_init(skb);
			kfree_skb(skb);
			stats->drop++;
			return;
		}
	}
}

static int cpu_map_bpf_prog_run_xdp(struct bpf_cpu_map_entry *rcpu,
				    void **frames, int n,
				    struct xdp_cpumap_stats *stats)
{
	struct xdp_rxq_info rxq;
	struct xdp_buff xdp;
	int i, nframes = 0;

	xdp_set_return_frame_no_direct();
	xdp.rxq = &rxq;

	for (i = 0; i < n; i++) {
		struct xdp_frame *xdpf = frames[i];
		u32 act;
		int err;

		rxq.dev = xdpf->dev_rx;
		rxq.mem = xdpf->mem;
		 

		xdp_convert_frame_to_buff(xdpf, &xdp);

		act = bpf_prog_run_xdp(rcpu->prog, &xdp);
		switch (act) {
		case XDP_PASS:
			err = xdp_update_frame_from_buff(&xdp, xdpf);
			if (err < 0) {
				xdp_return_frame(xdpf);
				stats->drop++;
			} else {
				frames[nframes++] = xdpf;
				stats->pass++;
			}
			break;
		case XDP_REDIRECT:
			err = xdp_do_redirect(xdpf->dev_rx, &xdp,
					      rcpu->prog);
			if (unlikely(err)) {
				xdp_return_frame(xdpf);
				stats->drop++;
			} else {
				stats->redirect++;
			}
			break;
		default:
			bpf_warn_invalid_xdp_action(NULL, rcpu->prog, act);
			fallthrough;
		case XDP_DROP:
			xdp_return_frame(xdpf);
			stats->drop++;
			break;
		}
	}

	xdp_clear_return_frame_no_direct();

	return nframes;
}

#define CPUMAP_BATCH 8

static int cpu_map_bpf_prog_run(struct bpf_cpu_map_entry *rcpu, void **frames,
				int xdp_n, struct xdp_cpumap_stats *stats,
				struct list_head *list)
{
	int nframes;

	if (!rcpu->prog)
		return xdp_n;

	rcu_read_lock_bh();

	nframes = cpu_map_bpf_prog_run_xdp(rcpu, frames, xdp_n, stats);

	if (stats->redirect)
		xdp_do_flush();

	if (unlikely(!list_empty(list)))
		cpu_map_bpf_prog_run_skb(rcpu, list, stats);

	rcu_read_unlock_bh();  

	return nframes;
}

static int cpu_map_kthread_run(void *data)
{
	struct bpf_cpu_map_entry *rcpu = data;

	complete(&rcpu->kthread_running);
	set_current_state(TASK_INTERRUPTIBLE);

	 
	while (!kthread_should_stop() || !__ptr_ring_empty(rcpu->queue)) {
		struct xdp_cpumap_stats stats = {};  
		unsigned int kmem_alloc_drops = 0, sched = 0;
		gfp_t gfp = __GFP_ZERO | GFP_ATOMIC;
		int i, n, m, nframes, xdp_n;
		void *frames[CPUMAP_BATCH];
		void *skbs[CPUMAP_BATCH];
		LIST_HEAD(list);

		 
		if (__ptr_ring_empty(rcpu->queue)) {
			set_current_state(TASK_INTERRUPTIBLE);
			 
			if (__ptr_ring_empty(rcpu->queue)) {
				schedule();
				sched = 1;
			} else {
				__set_current_state(TASK_RUNNING);
			}
		} else {
			sched = cond_resched();
		}

		 
		n = __ptr_ring_consume_batched(rcpu->queue, frames,
					       CPUMAP_BATCH);
		for (i = 0, xdp_n = 0; i < n; i++) {
			void *f = frames[i];
			struct page *page;

			if (unlikely(__ptr_test_bit(0, &f))) {
				struct sk_buff *skb = f;

				__ptr_clear_bit(0, &skb);
				list_add_tail(&skb->list, &list);
				continue;
			}

			frames[xdp_n++] = f;
			page = virt_to_page(f);

			 
			prefetchw(page);
		}

		 
		nframes = cpu_map_bpf_prog_run(rcpu, frames, xdp_n, &stats, &list);
		if (nframes) {
			m = kmem_cache_alloc_bulk(skbuff_cache, gfp, nframes, skbs);
			if (unlikely(m == 0)) {
				for (i = 0; i < nframes; i++)
					skbs[i] = NULL;  
				kmem_alloc_drops += nframes;
			}
		}

		local_bh_disable();
		for (i = 0; i < nframes; i++) {
			struct xdp_frame *xdpf = frames[i];
			struct sk_buff *skb = skbs[i];

			skb = __xdp_build_skb_from_frame(xdpf, skb,
							 xdpf->dev_rx);
			if (!skb) {
				xdp_return_frame(xdpf);
				continue;
			}

			list_add_tail(&skb->list, &list);
		}
		netif_receive_skb_list(&list);

		 
		trace_xdp_cpumap_kthread(rcpu->map_id, n, kmem_alloc_drops,
					 sched, &stats);

		local_bh_enable();  
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

static int __cpu_map_load_bpf_program(struct bpf_cpu_map_entry *rcpu,
				      struct bpf_map *map, int fd)
{
	struct bpf_prog *prog;

	prog = bpf_prog_get_type(fd, BPF_PROG_TYPE_XDP);
	if (IS_ERR(prog))
		return PTR_ERR(prog);

	if (prog->expected_attach_type != BPF_XDP_CPUMAP ||
	    !bpf_prog_map_compatible(map, prog)) {
		bpf_prog_put(prog);
		return -EINVAL;
	}

	rcpu->value.bpf_prog.id = prog->aux->id;
	rcpu->prog = prog;

	return 0;
}

static struct bpf_cpu_map_entry *
__cpu_map_entry_alloc(struct bpf_map *map, struct bpf_cpumap_val *value,
		      u32 cpu)
{
	int numa, err, i, fd = value->bpf_prog.fd;
	gfp_t gfp = GFP_KERNEL | __GFP_NOWARN;
	struct bpf_cpu_map_entry *rcpu;
	struct xdp_bulk_queue *bq;

	 
	numa = cpu_to_node(cpu);

	rcpu = bpf_map_kmalloc_node(map, sizeof(*rcpu), gfp | __GFP_ZERO, numa);
	if (!rcpu)
		return NULL;

	 
	rcpu->bulkq = bpf_map_alloc_percpu(map, sizeof(*rcpu->bulkq),
					   sizeof(void *), gfp);
	if (!rcpu->bulkq)
		goto free_rcu;

	for_each_possible_cpu(i) {
		bq = per_cpu_ptr(rcpu->bulkq, i);
		bq->obj = rcpu;
	}

	 
	rcpu->queue = bpf_map_kmalloc_node(map, sizeof(*rcpu->queue), gfp,
					   numa);
	if (!rcpu->queue)
		goto free_bulkq;

	err = ptr_ring_init(rcpu->queue, value->qsize, gfp);
	if (err)
		goto free_queue;

	rcpu->cpu    = cpu;
	rcpu->map_id = map->id;
	rcpu->value.qsize  = value->qsize;

	if (fd > 0 && __cpu_map_load_bpf_program(rcpu, map, fd))
		goto free_ptr_ring;

	 
	init_completion(&rcpu->kthread_running);
	rcpu->kthread = kthread_create_on_node(cpu_map_kthread_run, rcpu, numa,
					       "cpumap/%d/map:%d", cpu,
					       map->id);
	if (IS_ERR(rcpu->kthread))
		goto free_prog;

	 
	kthread_bind(rcpu->kthread, cpu);
	wake_up_process(rcpu->kthread);

	 
	wait_for_completion(&rcpu->kthread_running);

	return rcpu;

free_prog:
	if (rcpu->prog)
		bpf_prog_put(rcpu->prog);
free_ptr_ring:
	ptr_ring_cleanup(rcpu->queue, NULL);
free_queue:
	kfree(rcpu->queue);
free_bulkq:
	free_percpu(rcpu->bulkq);
free_rcu:
	kfree(rcpu);
	return NULL;
}

static void __cpu_map_entry_free(struct work_struct *work)
{
	struct bpf_cpu_map_entry *rcpu;

	 
	rcpu = container_of(to_rcu_work(work), struct bpf_cpu_map_entry, free_work);

	 
	kthread_stop(rcpu->kthread);

	if (rcpu->prog)
		bpf_prog_put(rcpu->prog);
	 
	__cpu_map_ring_cleanup(rcpu->queue);
	ptr_ring_cleanup(rcpu->queue, NULL);
	kfree(rcpu->queue);
	free_percpu(rcpu->bulkq);
	kfree(rcpu);
}

 
static void __cpu_map_entry_replace(struct bpf_cpu_map *cmap,
				    u32 key_cpu, struct bpf_cpu_map_entry *rcpu)
{
	struct bpf_cpu_map_entry *old_rcpu;

	old_rcpu = unrcu_pointer(xchg(&cmap->cpu_map[key_cpu], RCU_INITIALIZER(rcpu)));
	if (old_rcpu) {
		INIT_RCU_WORK(&old_rcpu->free_work, __cpu_map_entry_free);
		queue_rcu_work(system_wq, &old_rcpu->free_work);
	}
}

static long cpu_map_delete_elem(struct bpf_map *map, void *key)
{
	struct bpf_cpu_map *cmap = container_of(map, struct bpf_cpu_map, map);
	u32 key_cpu = *(u32 *)key;

	if (key_cpu >= map->max_entries)
		return -EINVAL;

	 
	__cpu_map_entry_replace(cmap, key_cpu, NULL);
	return 0;
}

static long cpu_map_update_elem(struct bpf_map *map, void *key, void *value,
				u64 map_flags)
{
	struct bpf_cpu_map *cmap = container_of(map, struct bpf_cpu_map, map);
	struct bpf_cpumap_val cpumap_value = {};
	struct bpf_cpu_map_entry *rcpu;
	 
	u32 key_cpu = *(u32 *)key;

	memcpy(&cpumap_value, value, map->value_size);

	if (unlikely(map_flags > BPF_EXIST))
		return -EINVAL;
	if (unlikely(key_cpu >= cmap->map.max_entries))
		return -E2BIG;
	if (unlikely(map_flags == BPF_NOEXIST))
		return -EEXIST;
	if (unlikely(cpumap_value.qsize > 16384))  
		return -EOVERFLOW;

	 
	if (key_cpu >= nr_cpumask_bits || !cpu_possible(key_cpu))
		return -ENODEV;

	if (cpumap_value.qsize == 0) {
		rcpu = NULL;  
	} else {
		 
		rcpu = __cpu_map_entry_alloc(map, &cpumap_value, key_cpu);
		if (!rcpu)
			return -ENOMEM;
	}
	rcu_read_lock();
	__cpu_map_entry_replace(cmap, key_cpu, rcpu);
	rcu_read_unlock();
	return 0;
}

static void cpu_map_free(struct bpf_map *map)
{
	struct bpf_cpu_map *cmap = container_of(map, struct bpf_cpu_map, map);
	u32 i;

	 
	synchronize_rcu();

	 
	for (i = 0; i < cmap->map.max_entries; i++) {
		struct bpf_cpu_map_entry *rcpu;

		rcpu = rcu_dereference_raw(cmap->cpu_map[i]);
		if (!rcpu)
			continue;

		 
		__cpu_map_entry_free(&rcpu->free_work.work);
	}
	bpf_map_area_free(cmap->cpu_map);
	bpf_map_area_free(cmap);
}

 
static void *__cpu_map_lookup_elem(struct bpf_map *map, u32 key)
{
	struct bpf_cpu_map *cmap = container_of(map, struct bpf_cpu_map, map);
	struct bpf_cpu_map_entry *rcpu;

	if (key >= map->max_entries)
		return NULL;

	rcpu = rcu_dereference_check(cmap->cpu_map[key],
				     rcu_read_lock_bh_held());
	return rcpu;
}

static void *cpu_map_lookup_elem(struct bpf_map *map, void *key)
{
	struct bpf_cpu_map_entry *rcpu =
		__cpu_map_lookup_elem(map, *(u32 *)key);

	return rcpu ? &rcpu->value : NULL;
}

static int cpu_map_get_next_key(struct bpf_map *map, void *key, void *next_key)
{
	struct bpf_cpu_map *cmap = container_of(map, struct bpf_cpu_map, map);
	u32 index = key ? *(u32 *)key : U32_MAX;
	u32 *next = next_key;

	if (index >= cmap->map.max_entries) {
		*next = 0;
		return 0;
	}

	if (index == cmap->map.max_entries - 1)
		return -ENOENT;
	*next = index + 1;
	return 0;
}

static long cpu_map_redirect(struct bpf_map *map, u64 index, u64 flags)
{
	return __bpf_xdp_redirect_map(map, index, flags, 0,
				      __cpu_map_lookup_elem);
}

static u64 cpu_map_mem_usage(const struct bpf_map *map)
{
	u64 usage = sizeof(struct bpf_cpu_map);

	 
	usage += (u64)map->max_entries * sizeof(struct bpf_cpu_map_entry *);
	return usage;
}

BTF_ID_LIST_SINGLE(cpu_map_btf_ids, struct, bpf_cpu_map)
const struct bpf_map_ops cpu_map_ops = {
	.map_meta_equal		= bpf_map_meta_equal,
	.map_alloc		= cpu_map_alloc,
	.map_free		= cpu_map_free,
	.map_delete_elem	= cpu_map_delete_elem,
	.map_update_elem	= cpu_map_update_elem,
	.map_lookup_elem	= cpu_map_lookup_elem,
	.map_get_next_key	= cpu_map_get_next_key,
	.map_check_btf		= map_check_no_btf,
	.map_mem_usage		= cpu_map_mem_usage,
	.map_btf_id		= &cpu_map_btf_ids[0],
	.map_redirect		= cpu_map_redirect,
};

static void bq_flush_to_queue(struct xdp_bulk_queue *bq)
{
	struct bpf_cpu_map_entry *rcpu = bq->obj;
	unsigned int processed = 0, drops = 0;
	const int to_cpu = rcpu->cpu;
	struct ptr_ring *q;
	int i;

	if (unlikely(!bq->count))
		return;

	q = rcpu->queue;
	spin_lock(&q->producer_lock);

	for (i = 0; i < bq->count; i++) {
		struct xdp_frame *xdpf = bq->q[i];
		int err;

		err = __ptr_ring_produce(q, xdpf);
		if (err) {
			drops++;
			xdp_return_frame_rx_napi(xdpf);
		}
		processed++;
	}
	bq->count = 0;
	spin_unlock(&q->producer_lock);

	__list_del_clearprev(&bq->flush_node);

	 
	trace_xdp_cpumap_enqueue(rcpu->map_id, processed, drops, to_cpu);
}

 
static void bq_enqueue(struct bpf_cpu_map_entry *rcpu, struct xdp_frame *xdpf)
{
	struct list_head *flush_list = this_cpu_ptr(&cpu_map_flush_list);
	struct xdp_bulk_queue *bq = this_cpu_ptr(rcpu->bulkq);

	if (unlikely(bq->count == CPU_MAP_BULK_SIZE))
		bq_flush_to_queue(bq);

	 
	bq->q[bq->count++] = xdpf;

	if (!bq->flush_node.prev)
		list_add(&bq->flush_node, flush_list);
}

int cpu_map_enqueue(struct bpf_cpu_map_entry *rcpu, struct xdp_frame *xdpf,
		    struct net_device *dev_rx)
{
	 
	xdpf->dev_rx = dev_rx;

	bq_enqueue(rcpu, xdpf);
	return 0;
}

int cpu_map_generic_redirect(struct bpf_cpu_map_entry *rcpu,
			     struct sk_buff *skb)
{
	int ret;

	__skb_pull(skb, skb->mac_len);
	skb_set_redirected(skb, false);
	__ptr_set_bit(0, &skb);

	ret = ptr_ring_produce(rcpu->queue, skb);
	if (ret < 0)
		goto trace;

	wake_up_process(rcpu->kthread);
trace:
	trace_xdp_cpumap_enqueue(rcpu->map_id, !ret, !!ret, rcpu->cpu);
	return ret;
}

void __cpu_map_flush(void)
{
	struct list_head *flush_list = this_cpu_ptr(&cpu_map_flush_list);
	struct xdp_bulk_queue *bq, *tmp;

	list_for_each_entry_safe(bq, tmp, flush_list, flush_node) {
		bq_flush_to_queue(bq);

		 
		wake_up_process(bq->obj->kthread);
	}
}

static int __init cpu_map_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		INIT_LIST_HEAD(&per_cpu(cpu_map_flush_list, cpu));
	return 0;
}

subsys_initcall(cpu_map_init);
