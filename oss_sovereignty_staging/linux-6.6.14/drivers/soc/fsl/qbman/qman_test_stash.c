 

#include "qman_test.h"

#include <linux/dma-mapping.h>
#include <linux/delay.h>

 

 
struct bstrap {
	int (*fn)(void);
	atomic_t started;
};
static int bstrap_fn(void *bs)
{
	struct bstrap *bstrap = bs;
	int err;

	atomic_inc(&bstrap->started);
	err = bstrap->fn();
	if (err)
		return err;
	while (!kthread_should_stop())
		msleep(20);
	return 0;
}
static int on_all_cpus(int (*fn)(void))
{
	int cpu;

	for_each_cpu(cpu, cpu_online_mask) {
		struct bstrap bstrap = {
			.fn = fn,
			.started = ATOMIC_INIT(0)
		};
		struct task_struct *k = kthread_create(bstrap_fn, &bstrap,
			"hotpotato%d", cpu);
		int ret;

		if (IS_ERR(k))
			return -ENOMEM;
		kthread_bind(k, cpu);
		wake_up_process(k);
		 
		while (!atomic_read(&bstrap.started))
			msleep(20);
		ret = kthread_stop(k);
		if (ret)
			return ret;
	}
	return 0;
}

struct hp_handler {

	 
	 
	 
	struct qman_fq rx;
	 
	struct qman_fq tx;
	 
	u32 rx_mixer;
	 
	u32 tx_mixer;
	 
	dma_addr_t addr;
	u32 *frame_ptr;

	 
	 
	u32 fqid_rx, fqid_tx;
	 
	struct list_head node;
	 
	unsigned int processor_id;
} ____cacheline_aligned;

struct hp_cpu {
	 
	unsigned int processor_id;
	 
	struct list_head handlers;
	 
	struct list_head node;
	 
	struct hp_handler *iterator;
};

 
static DEFINE_PER_CPU(struct hp_cpu, hp_cpus);

 
static LIST_HEAD(hp_cpu_list);
static DEFINE_SPINLOCK(hp_lock);

static unsigned int hp_cpu_list_length;

 
static struct hp_handler *special_handler;
static int loop_counter;

 
static struct kmem_cache *hp_handler_slab;

 
static void *__frame_ptr;
static u32 *frame_ptr;
static dma_addr_t frame_dma;

 
static const struct qm_portal_config *pcfg;

 
static DECLARE_WAIT_QUEUE_HEAD(queue);

#define HP_PER_CPU	2
#define HP_LOOPS	8
 
#define HP_NUM_WORDS	80
 
#define HP_FIRST_WORD	0xabbaf00d

static inline u32 do_lfsr(u32 prev)
{
	return (prev >> 1) ^ (-(prev & 1u) & 0xd0000001u);
}

static int allocate_frame_data(void)
{
	u32 lfsr = HP_FIRST_WORD;
	int loop;

	if (!qman_dma_portal) {
		pr_crit("portal not available\n");
		return -EIO;
	}

	pcfg = qman_get_qm_portal_config(qman_dma_portal);

	__frame_ptr = kmalloc(4 * HP_NUM_WORDS, GFP_KERNEL);
	if (!__frame_ptr)
		return -ENOMEM;

	frame_ptr = PTR_ALIGN(__frame_ptr, 64);
	for (loop = 0; loop < HP_NUM_WORDS; loop++) {
		frame_ptr[loop] = lfsr;
		lfsr = do_lfsr(lfsr);
	}

	frame_dma = dma_map_single(pcfg->dev, frame_ptr, 4 * HP_NUM_WORDS,
				   DMA_BIDIRECTIONAL);
	if (dma_mapping_error(pcfg->dev, frame_dma)) {
		pr_crit("dma mapping failure\n");
		kfree(__frame_ptr);
		return -EIO;
	}

	return 0;
}

static void deallocate_frame_data(void)
{
	dma_unmap_single(pcfg->dev, frame_dma, 4 * HP_NUM_WORDS,
			 DMA_BIDIRECTIONAL);
	kfree(__frame_ptr);
}

static inline int process_frame_data(struct hp_handler *handler,
				     const struct qm_fd *fd)
{
	u32 *p = handler->frame_ptr;
	u32 lfsr = HP_FIRST_WORD;
	int loop;

	if (qm_fd_addr_get64(fd) != handler->addr) {
		pr_crit("bad frame address, [%llX != %llX]\n",
			qm_fd_addr_get64(fd), handler->addr);
		return -EIO;
	}
	for (loop = 0; loop < HP_NUM_WORDS; loop++, p++) {
		*p ^= handler->rx_mixer;
		if (*p != lfsr) {
			pr_crit("corrupt frame data");
			return -EIO;
		}
		*p ^= handler->tx_mixer;
		lfsr = do_lfsr(lfsr);
	}
	return 0;
}

static enum qman_cb_dqrr_result normal_dqrr(struct qman_portal *portal,
					    struct qman_fq *fq,
					    const struct qm_dqrr_entry *dqrr,
					    bool sched_napi)
{
	struct hp_handler *handler = (struct hp_handler *)fq;

	if (process_frame_data(handler, &dqrr->fd)) {
		WARN_ON(1);
		goto skip;
	}
	if (qman_enqueue(&handler->tx, &dqrr->fd)) {
		pr_crit("qman_enqueue() failed");
		WARN_ON(1);
	}
skip:
	return qman_cb_dqrr_consume;
}

static enum qman_cb_dqrr_result special_dqrr(struct qman_portal *portal,
					     struct qman_fq *fq,
					     const struct qm_dqrr_entry *dqrr,
					     bool sched_napi)
{
	struct hp_handler *handler = (struct hp_handler *)fq;

	process_frame_data(handler, &dqrr->fd);
	if (++loop_counter < HP_LOOPS) {
		if (qman_enqueue(&handler->tx, &dqrr->fd)) {
			pr_crit("qman_enqueue() failed");
			WARN_ON(1);
			goto skip;
		}
	} else {
		pr_info("Received final (%dth) frame\n", loop_counter);
		wake_up(&queue);
	}
skip:
	return qman_cb_dqrr_consume;
}

static int create_per_cpu_handlers(void)
{
	struct hp_handler *handler;
	int loop;
	struct hp_cpu *hp_cpu = this_cpu_ptr(&hp_cpus);

	hp_cpu->processor_id = smp_processor_id();
	spin_lock(&hp_lock);
	list_add_tail(&hp_cpu->node, &hp_cpu_list);
	hp_cpu_list_length++;
	spin_unlock(&hp_lock);
	INIT_LIST_HEAD(&hp_cpu->handlers);
	for (loop = 0; loop < HP_PER_CPU; loop++) {
		handler = kmem_cache_alloc(hp_handler_slab, GFP_KERNEL);
		if (!handler) {
			pr_crit("kmem_cache_alloc() failed");
			WARN_ON(1);
			return -EIO;
		}
		handler->processor_id = hp_cpu->processor_id;
		handler->addr = frame_dma;
		handler->frame_ptr = frame_ptr;
		list_add_tail(&handler->node, &hp_cpu->handlers);
	}
	return 0;
}

static int destroy_per_cpu_handlers(void)
{
	struct list_head *loop, *tmp;
	struct hp_cpu *hp_cpu = this_cpu_ptr(&hp_cpus);

	spin_lock(&hp_lock);
	list_del(&hp_cpu->node);
	spin_unlock(&hp_lock);
	list_for_each_safe(loop, tmp, &hp_cpu->handlers) {
		u32 flags = 0;
		struct hp_handler *handler = list_entry(loop, struct hp_handler,
							node);
		if (qman_retire_fq(&handler->rx, &flags) ||
		    (flags & QMAN_FQ_STATE_BLOCKOOS)) {
			pr_crit("qman_retire_fq(rx) failed, flags: %x", flags);
			WARN_ON(1);
			return -EIO;
		}
		if (qman_oos_fq(&handler->rx)) {
			pr_crit("qman_oos_fq(rx) failed");
			WARN_ON(1);
			return -EIO;
		}
		qman_destroy_fq(&handler->rx);
		qman_destroy_fq(&handler->tx);
		qman_release_fqid(handler->fqid_rx);
		list_del(&handler->node);
		kmem_cache_free(hp_handler_slab, handler);
	}
	return 0;
}

static inline u8 num_cachelines(u32 offset)
{
	u8 res = (offset + (L1_CACHE_BYTES - 1))
			 / (L1_CACHE_BYTES);
	if (res > 3)
		return 3;
	return res;
}
#define STASH_DATA_CL \
	num_cachelines(HP_NUM_WORDS * 4)
#define STASH_CTX_CL \
	num_cachelines(offsetof(struct hp_handler, fqid_rx))

static int init_handler(void *h)
{
	struct qm_mcc_initfq opts;
	struct hp_handler *handler = h;
	int err;

	if (handler->processor_id != smp_processor_id()) {
		err = -EIO;
		goto failed;
	}
	 
	memset(&handler->rx, 0, sizeof(handler->rx));
	if (handler == special_handler)
		handler->rx.cb.dqrr = special_dqrr;
	else
		handler->rx.cb.dqrr = normal_dqrr;
	err = qman_create_fq(handler->fqid_rx, 0, &handler->rx);
	if (err) {
		pr_crit("qman_create_fq(rx) failed");
		goto failed;
	}
	memset(&opts, 0, sizeof(opts));
	opts.we_mask = cpu_to_be16(QM_INITFQ_WE_FQCTRL |
				   QM_INITFQ_WE_CONTEXTA);
	opts.fqd.fq_ctrl = cpu_to_be16(QM_FQCTRL_CTXASTASHING);
	qm_fqd_set_stashing(&opts.fqd, 0, STASH_DATA_CL, STASH_CTX_CL);
	err = qman_init_fq(&handler->rx, QMAN_INITFQ_FLAG_SCHED |
			   QMAN_INITFQ_FLAG_LOCAL, &opts);
	if (err) {
		pr_crit("qman_init_fq(rx) failed");
		goto failed;
	}
	 
	memset(&handler->tx, 0, sizeof(handler->tx));
	err = qman_create_fq(handler->fqid_tx, QMAN_FQ_FLAG_NO_MODIFY,
			     &handler->tx);
	if (err) {
		pr_crit("qman_create_fq(tx) failed");
		goto failed;
	}

	return 0;
failed:
	return err;
}

static void init_handler_cb(void *h)
{
	if (init_handler(h))
		WARN_ON(1);
}

static int init_phase2(void)
{
	int loop;
	u32 fqid = 0;
	u32 lfsr = 0xdeadbeef;
	struct hp_cpu *hp_cpu;
	struct hp_handler *handler;

	for (loop = 0; loop < HP_PER_CPU; loop++) {
		list_for_each_entry(hp_cpu, &hp_cpu_list, node) {
			int err;

			if (!loop)
				hp_cpu->iterator = list_first_entry(
						&hp_cpu->handlers,
						struct hp_handler, node);
			else
				hp_cpu->iterator = list_entry(
						hp_cpu->iterator->node.next,
						struct hp_handler, node);
			 
			hp_cpu->iterator->fqid_rx = fqid;
			 
			err = qman_alloc_fqid(&fqid);
			if (err) {
				pr_crit("qman_alloc_fqid() failed");
				return err;
			}
			hp_cpu->iterator->fqid_tx = fqid;
			 
			hp_cpu->iterator->rx_mixer = lfsr;
			 
			lfsr = do_lfsr(lfsr);
			hp_cpu->iterator->tx_mixer = lfsr;
		}
	}
	 
	hp_cpu = list_first_entry(&hp_cpu_list, struct hp_cpu, node);
	handler = list_first_entry(&hp_cpu->handlers, struct hp_handler, node);
	if (handler->fqid_rx != 0 || handler->rx_mixer != 0xdeadbeef)
		return 1;
	handler->fqid_rx = fqid;
	handler->rx_mixer = lfsr;
	 
	special_handler = handler;
	return 0;
}

static int init_phase3(void)
{
	int loop, err;
	struct hp_cpu *hp_cpu;

	for (loop = 0; loop < HP_PER_CPU; loop++) {
		list_for_each_entry(hp_cpu, &hp_cpu_list, node) {
			if (!loop)
				hp_cpu->iterator = list_first_entry(
						&hp_cpu->handlers,
						struct hp_handler, node);
			else
				hp_cpu->iterator = list_entry(
						hp_cpu->iterator->node.next,
						struct hp_handler, node);
			preempt_disable();
			if (hp_cpu->processor_id == smp_processor_id()) {
				err = init_handler(hp_cpu->iterator);
				if (err)
					return err;
			} else {
				smp_call_function_single(hp_cpu->processor_id,
					init_handler_cb, hp_cpu->iterator, 1);
			}
			preempt_enable();
		}
	}
	return 0;
}

static int send_first_frame(void *ignore)
{
	u32 *p = special_handler->frame_ptr;
	u32 lfsr = HP_FIRST_WORD;
	int loop, err;
	struct qm_fd fd;

	if (special_handler->processor_id != smp_processor_id()) {
		err = -EIO;
		goto failed;
	}
	memset(&fd, 0, sizeof(fd));
	qm_fd_addr_set64(&fd, special_handler->addr);
	qm_fd_set_contig_big(&fd, HP_NUM_WORDS * 4);
	for (loop = 0; loop < HP_NUM_WORDS; loop++, p++) {
		if (*p != lfsr) {
			err = -EIO;
			pr_crit("corrupt frame data");
			goto failed;
		}
		*p ^= special_handler->tx_mixer;
		lfsr = do_lfsr(lfsr);
	}
	pr_info("Sending first frame\n");
	err = qman_enqueue(&special_handler->tx, &fd);
	if (err) {
		pr_crit("qman_enqueue() failed");
		goto failed;
	}

	return 0;
failed:
	return err;
}

static void send_first_frame_cb(void *ignore)
{
	if (send_first_frame(NULL))
		WARN_ON(1);
}

int qman_test_stash(void)
{
	int err;

	if (cpumask_weight(cpu_online_mask) < 2) {
		pr_info("%s(): skip - only 1 CPU\n", __func__);
		return 0;
	}

	pr_info("%s(): Starting\n", __func__);

	hp_cpu_list_length = 0;
	loop_counter = 0;
	hp_handler_slab = kmem_cache_create("hp_handler_slab",
			sizeof(struct hp_handler), L1_CACHE_BYTES,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!hp_handler_slab) {
		err = -EIO;
		pr_crit("kmem_cache_create() failed");
		goto failed;
	}

	err = allocate_frame_data();
	if (err)
		goto failed;

	 
	pr_info("Creating %d handlers per cpu...\n", HP_PER_CPU);
	if (on_all_cpus(create_per_cpu_handlers)) {
		err = -EIO;
		pr_crit("on_each_cpu() failed");
		goto failed;
	}
	pr_info("Number of cpus: %d, total of %d handlers\n",
		hp_cpu_list_length, hp_cpu_list_length * HP_PER_CPU);

	err = init_phase2();
	if (err)
		goto failed;

	err = init_phase3();
	if (err)
		goto failed;

	preempt_disable();
	if (special_handler->processor_id == smp_processor_id()) {
		err = send_first_frame(NULL);
		if (err)
			goto failed;
	} else {
		smp_call_function_single(special_handler->processor_id,
					 send_first_frame_cb, NULL, 1);
	}
	preempt_enable();

	wait_event(queue, loop_counter == HP_LOOPS);
	deallocate_frame_data();
	if (on_all_cpus(destroy_per_cpu_handlers)) {
		err = -EIO;
		pr_crit("on_each_cpu() failed");
		goto failed;
	}
	kmem_cache_destroy(hp_handler_slab);
	pr_info("%s(): Finished\n", __func__);

	return 0;
failed:
	WARN_ON(1);
	return err;
}
