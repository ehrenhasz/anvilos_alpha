
 

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mempool.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/device-mapper.h>
#include <linux/dm-kcopyd.h>

#include "dm-core.h"

#define SPLIT_COUNT	8
#define MIN_JOBS	8

#define DEFAULT_SUB_JOB_SIZE_KB 512
#define MAX_SUB_JOB_SIZE_KB     1024

static unsigned int kcopyd_subjob_size_kb = DEFAULT_SUB_JOB_SIZE_KB;

module_param(kcopyd_subjob_size_kb, uint, 0644);
MODULE_PARM_DESC(kcopyd_subjob_size_kb, "Sub-job size for dm-kcopyd clients");

static unsigned int dm_get_kcopyd_subjob_size(void)
{
	unsigned int sub_job_size_kb;

	sub_job_size_kb = __dm_get_module_param(&kcopyd_subjob_size_kb,
						DEFAULT_SUB_JOB_SIZE_KB,
						MAX_SUB_JOB_SIZE_KB);

	return sub_job_size_kb << 1;
}

 
struct dm_kcopyd_client {
	struct page_list *pages;
	unsigned int nr_reserved_pages;
	unsigned int nr_free_pages;
	unsigned int sub_job_size;

	struct dm_io_client *io_client;

	wait_queue_head_t destroyq;

	mempool_t job_pool;

	struct workqueue_struct *kcopyd_wq;
	struct work_struct kcopyd_work;

	struct dm_kcopyd_throttle *throttle;

	atomic_t nr_jobs;

 
	spinlock_t job_lock;
	struct list_head callback_jobs;
	struct list_head complete_jobs;
	struct list_head io_jobs;
	struct list_head pages_jobs;
};

static struct page_list zero_page_list;

static DEFINE_SPINLOCK(throttle_spinlock);

 
#define ACCOUNT_INTERVAL_SHIFT		SHIFT_HZ

 
#define SLEEP_USEC			100000

 
#define MAX_SLEEPS			10

static void io_job_start(struct dm_kcopyd_throttle *t)
{
	unsigned int throttle, now, difference;
	int slept = 0, skew;

	if (unlikely(!t))
		return;

try_again:
	spin_lock_irq(&throttle_spinlock);

	throttle = READ_ONCE(t->throttle);

	if (likely(throttle >= 100))
		goto skip_limit;

	now = jiffies;
	difference = now - t->last_jiffies;
	t->last_jiffies = now;
	if (t->num_io_jobs)
		t->io_period += difference;
	t->total_period += difference;

	 
	if (unlikely(t->io_period > t->total_period))
		t->io_period = t->total_period;

	if (unlikely(t->total_period >= (1 << ACCOUNT_INTERVAL_SHIFT))) {
		int shift = fls(t->total_period >> ACCOUNT_INTERVAL_SHIFT);

		t->total_period >>= shift;
		t->io_period >>= shift;
	}

	skew = t->io_period - throttle * t->total_period / 100;

	if (unlikely(skew > 0) && slept < MAX_SLEEPS) {
		slept++;
		spin_unlock_irq(&throttle_spinlock);
		fsleep(SLEEP_USEC);
		goto try_again;
	}

skip_limit:
	t->num_io_jobs++;

	spin_unlock_irq(&throttle_spinlock);
}

static void io_job_finish(struct dm_kcopyd_throttle *t)
{
	unsigned long flags;

	if (unlikely(!t))
		return;

	spin_lock_irqsave(&throttle_spinlock, flags);

	t->num_io_jobs--;

	if (likely(READ_ONCE(t->throttle) >= 100))
		goto skip_limit;

	if (!t->num_io_jobs) {
		unsigned int now, difference;

		now = jiffies;
		difference = now - t->last_jiffies;
		t->last_jiffies = now;

		t->io_period += difference;
		t->total_period += difference;

		 
		if (unlikely(t->io_period > t->total_period))
			t->io_period = t->total_period;
	}

skip_limit:
	spin_unlock_irqrestore(&throttle_spinlock, flags);
}


static void wake(struct dm_kcopyd_client *kc)
{
	queue_work(kc->kcopyd_wq, &kc->kcopyd_work);
}

 
static struct page_list *alloc_pl(gfp_t gfp)
{
	struct page_list *pl;

	pl = kmalloc(sizeof(*pl), gfp);
	if (!pl)
		return NULL;

	pl->page = alloc_page(gfp | __GFP_HIGHMEM);
	if (!pl->page) {
		kfree(pl);
		return NULL;
	}

	return pl;
}

static void free_pl(struct page_list *pl)
{
	__free_page(pl->page);
	kfree(pl);
}

 
static void kcopyd_put_pages(struct dm_kcopyd_client *kc, struct page_list *pl)
{
	struct page_list *next;

	do {
		next = pl->next;

		if (kc->nr_free_pages >= kc->nr_reserved_pages)
			free_pl(pl);
		else {
			pl->next = kc->pages;
			kc->pages = pl;
			kc->nr_free_pages++;
		}

		pl = next;
	} while (pl);
}

static int kcopyd_get_pages(struct dm_kcopyd_client *kc,
			    unsigned int nr, struct page_list **pages)
{
	struct page_list *pl;

	*pages = NULL;

	do {
		pl = alloc_pl(__GFP_NOWARN | __GFP_NORETRY | __GFP_KSWAPD_RECLAIM);
		if (unlikely(!pl)) {
			 
			pl = kc->pages;
			if (unlikely(!pl))
				goto out_of_memory;
			kc->pages = pl->next;
			kc->nr_free_pages--;
		}
		pl->next = *pages;
		*pages = pl;
	} while (--nr);

	return 0;

out_of_memory:
	if (*pages)
		kcopyd_put_pages(kc, *pages);
	return -ENOMEM;
}

 
static void drop_pages(struct page_list *pl)
{
	struct page_list *next;

	while (pl) {
		next = pl->next;
		free_pl(pl);
		pl = next;
	}
}

 
static int client_reserve_pages(struct dm_kcopyd_client *kc, unsigned int nr_pages)
{
	unsigned int i;
	struct page_list *pl = NULL, *next;

	for (i = 0; i < nr_pages; i++) {
		next = alloc_pl(GFP_KERNEL);
		if (!next) {
			if (pl)
				drop_pages(pl);
			return -ENOMEM;
		}
		next->next = pl;
		pl = next;
	}

	kc->nr_reserved_pages += nr_pages;
	kcopyd_put_pages(kc, pl);

	return 0;
}

static void client_free_pages(struct dm_kcopyd_client *kc)
{
	BUG_ON(kc->nr_free_pages != kc->nr_reserved_pages);
	drop_pages(kc->pages);
	kc->pages = NULL;
	kc->nr_free_pages = kc->nr_reserved_pages = 0;
}

 
struct kcopyd_job {
	struct dm_kcopyd_client *kc;
	struct list_head list;
	unsigned int flags;

	 
	int read_err;
	unsigned long write_err;

	 
	enum req_op op;
	struct dm_io_region source;

	 
	unsigned int num_dests;
	struct dm_io_region dests[DM_KCOPYD_MAX_REGIONS];

	struct page_list *pages;

	 
	dm_kcopyd_notify_fn fn;
	void *context;

	 
	struct mutex lock;
	atomic_t sub_jobs;
	sector_t progress;
	sector_t write_offset;

	struct kcopyd_job *master_job;
};

static struct kmem_cache *_job_cache;

int __init dm_kcopyd_init(void)
{
	_job_cache = kmem_cache_create("kcopyd_job",
				sizeof(struct kcopyd_job) * (SPLIT_COUNT + 1),
				__alignof__(struct kcopyd_job), 0, NULL);
	if (!_job_cache)
		return -ENOMEM;

	zero_page_list.next = &zero_page_list;
	zero_page_list.page = ZERO_PAGE(0);

	return 0;
}

void dm_kcopyd_exit(void)
{
	kmem_cache_destroy(_job_cache);
	_job_cache = NULL;
}

 
static struct kcopyd_job *pop_io_job(struct list_head *jobs,
				     struct dm_kcopyd_client *kc)
{
	struct kcopyd_job *job;

	 
	list_for_each_entry(job, jobs, list) {
		if (job->op == REQ_OP_READ ||
		    !(job->flags & BIT(DM_KCOPYD_WRITE_SEQ))) {
			list_del(&job->list);
			return job;
		}

		if (job->write_offset == job->master_job->write_offset) {
			job->master_job->write_offset += job->source.count;
			list_del(&job->list);
			return job;
		}
	}

	return NULL;
}

static struct kcopyd_job *pop(struct list_head *jobs,
			      struct dm_kcopyd_client *kc)
{
	struct kcopyd_job *job = NULL;

	spin_lock_irq(&kc->job_lock);

	if (!list_empty(jobs)) {
		if (jobs == &kc->io_jobs)
			job = pop_io_job(jobs, kc);
		else {
			job = list_entry(jobs->next, struct kcopyd_job, list);
			list_del(&job->list);
		}
	}
	spin_unlock_irq(&kc->job_lock);

	return job;
}

static void push(struct list_head *jobs, struct kcopyd_job *job)
{
	unsigned long flags;
	struct dm_kcopyd_client *kc = job->kc;

	spin_lock_irqsave(&kc->job_lock, flags);
	list_add_tail(&job->list, jobs);
	spin_unlock_irqrestore(&kc->job_lock, flags);
}


static void push_head(struct list_head *jobs, struct kcopyd_job *job)
{
	struct dm_kcopyd_client *kc = job->kc;

	spin_lock_irq(&kc->job_lock);
	list_add(&job->list, jobs);
	spin_unlock_irq(&kc->job_lock);
}

 
static int run_complete_job(struct kcopyd_job *job)
{
	void *context = job->context;
	int read_err = job->read_err;
	unsigned long write_err = job->write_err;
	dm_kcopyd_notify_fn fn = job->fn;
	struct dm_kcopyd_client *kc = job->kc;

	if (job->pages && job->pages != &zero_page_list)
		kcopyd_put_pages(kc, job->pages);
	 
	if (job->master_job == job) {
		mutex_destroy(&job->lock);
		mempool_free(job, &kc->job_pool);
	}
	fn(read_err, write_err, context);

	if (atomic_dec_and_test(&kc->nr_jobs))
		wake_up(&kc->destroyq);

	cond_resched();

	return 0;
}

static void complete_io(unsigned long error, void *context)
{
	struct kcopyd_job *job = context;
	struct dm_kcopyd_client *kc = job->kc;

	io_job_finish(kc->throttle);

	if (error) {
		if (op_is_write(job->op))
			job->write_err |= error;
		else
			job->read_err = 1;

		if (!(job->flags & BIT(DM_KCOPYD_IGNORE_ERROR))) {
			push(&kc->complete_jobs, job);
			wake(kc);
			return;
		}
	}

	if (op_is_write(job->op))
		push(&kc->complete_jobs, job);

	else {
		job->op = REQ_OP_WRITE;
		push(&kc->io_jobs, job);
	}

	wake(kc);
}

 
static int run_io_job(struct kcopyd_job *job)
{
	int r;
	struct dm_io_request io_req = {
		.bi_opf = job->op,
		.mem.type = DM_IO_PAGE_LIST,
		.mem.ptr.pl = job->pages,
		.mem.offset = 0,
		.notify.fn = complete_io,
		.notify.context = job,
		.client = job->kc->io_client,
	};

	 
	if (job->flags & BIT(DM_KCOPYD_WRITE_SEQ) &&
	    job->master_job->write_err) {
		job->write_err = job->master_job->write_err;
		return -EIO;
	}

	io_job_start(job->kc->throttle);

	if (job->op == REQ_OP_READ)
		r = dm_io(&io_req, 1, &job->source, NULL);
	else
		r = dm_io(&io_req, job->num_dests, job->dests, NULL);

	return r;
}

static int run_pages_job(struct kcopyd_job *job)
{
	int r;
	unsigned int nr_pages = dm_div_up(job->dests[0].count, PAGE_SIZE >> 9);

	r = kcopyd_get_pages(job->kc, nr_pages, &job->pages);
	if (!r) {
		 
		push(&job->kc->io_jobs, job);
		return 0;
	}

	if (r == -ENOMEM)
		 
		return 1;

	return r;
}

 
static int process_jobs(struct list_head *jobs, struct dm_kcopyd_client *kc,
			int (*fn)(struct kcopyd_job *))
{
	struct kcopyd_job *job;
	int r, count = 0;

	while ((job = pop(jobs, kc))) {

		r = fn(job);

		if (r < 0) {
			 
			if (op_is_write(job->op))
				job->write_err = (unsigned long) -1L;
			else
				job->read_err = 1;
			push(&kc->complete_jobs, job);
			wake(kc);
			break;
		}

		if (r > 0) {
			 
			push_head(jobs, job);
			break;
		}

		count++;
	}

	return count;
}

 
static void do_work(struct work_struct *work)
{
	struct dm_kcopyd_client *kc = container_of(work,
					struct dm_kcopyd_client, kcopyd_work);
	struct blk_plug plug;

	 
	spin_lock_irq(&kc->job_lock);
	list_splice_tail_init(&kc->callback_jobs, &kc->complete_jobs);
	spin_unlock_irq(&kc->job_lock);

	blk_start_plug(&plug);
	process_jobs(&kc->complete_jobs, kc, run_complete_job);
	process_jobs(&kc->pages_jobs, kc, run_pages_job);
	process_jobs(&kc->io_jobs, kc, run_io_job);
	blk_finish_plug(&plug);
}

 
static void dispatch_job(struct kcopyd_job *job)
{
	struct dm_kcopyd_client *kc = job->kc;

	atomic_inc(&kc->nr_jobs);
	if (unlikely(!job->source.count))
		push(&kc->callback_jobs, job);
	else if (job->pages == &zero_page_list)
		push(&kc->io_jobs, job);
	else
		push(&kc->pages_jobs, job);
	wake(kc);
}

static void segment_complete(int read_err, unsigned long write_err,
			     void *context)
{
	 
	sector_t progress = 0;
	sector_t count = 0;
	struct kcopyd_job *sub_job = context;
	struct kcopyd_job *job = sub_job->master_job;
	struct dm_kcopyd_client *kc = job->kc;

	mutex_lock(&job->lock);

	 
	if (read_err)
		job->read_err = 1;

	if (write_err)
		job->write_err |= write_err;

	 
	if ((!job->read_err && !job->write_err) ||
	    job->flags & BIT(DM_KCOPYD_IGNORE_ERROR)) {
		 
		progress = job->progress;
		count = job->source.count - progress;
		if (count) {
			if (count > kc->sub_job_size)
				count = kc->sub_job_size;

			job->progress += count;
		}
	}
	mutex_unlock(&job->lock);

	if (count) {
		int i;

		*sub_job = *job;
		sub_job->write_offset = progress;
		sub_job->source.sector += progress;
		sub_job->source.count = count;

		for (i = 0; i < job->num_dests; i++) {
			sub_job->dests[i].sector += progress;
			sub_job->dests[i].count = count;
		}

		sub_job->fn = segment_complete;
		sub_job->context = sub_job;
		dispatch_job(sub_job);

	} else if (atomic_dec_and_test(&job->sub_jobs)) {

		 
		push(&kc->complete_jobs, job);
		wake(kc);
	}
}

 
static void split_job(struct kcopyd_job *master_job)
{
	int i;

	atomic_inc(&master_job->kc->nr_jobs);

	atomic_set(&master_job->sub_jobs, SPLIT_COUNT);
	for (i = 0; i < SPLIT_COUNT; i++) {
		master_job[i + 1].master_job = master_job;
		segment_complete(0, 0u, &master_job[i + 1]);
	}
}

void dm_kcopyd_copy(struct dm_kcopyd_client *kc, struct dm_io_region *from,
		    unsigned int num_dests, struct dm_io_region *dests,
		    unsigned int flags, dm_kcopyd_notify_fn fn, void *context)
{
	struct kcopyd_job *job;
	int i;

	 
	job = mempool_alloc(&kc->job_pool, GFP_NOIO);
	mutex_init(&job->lock);

	 
	job->kc = kc;
	job->flags = flags;
	job->read_err = 0;
	job->write_err = 0;

	job->num_dests = num_dests;
	memcpy(&job->dests, dests, sizeof(*dests) * num_dests);

	 
	if (!(job->flags & BIT(DM_KCOPYD_WRITE_SEQ))) {
		for (i = 0; i < job->num_dests; i++) {
			if (bdev_zoned_model(dests[i].bdev) == BLK_ZONED_HM) {
				job->flags |= BIT(DM_KCOPYD_WRITE_SEQ);
				break;
			}
		}
	}

	 
	if (job->flags & BIT(DM_KCOPYD_WRITE_SEQ) &&
	    job->flags & BIT(DM_KCOPYD_IGNORE_ERROR))
		job->flags &= ~BIT(DM_KCOPYD_IGNORE_ERROR);

	if (from) {
		job->source = *from;
		job->pages = NULL;
		job->op = REQ_OP_READ;
	} else {
		memset(&job->source, 0, sizeof(job->source));
		job->source.count = job->dests[0].count;
		job->pages = &zero_page_list;

		 
		job->op = REQ_OP_WRITE_ZEROES;
		for (i = 0; i < job->num_dests; i++)
			if (!bdev_write_zeroes_sectors(job->dests[i].bdev)) {
				job->op = REQ_OP_WRITE;
				break;
			}
	}

	job->fn = fn;
	job->context = context;
	job->master_job = job;
	job->write_offset = 0;

	if (job->source.count <= kc->sub_job_size)
		dispatch_job(job);
	else {
		job->progress = 0;
		split_job(job);
	}
}
EXPORT_SYMBOL(dm_kcopyd_copy);

void dm_kcopyd_zero(struct dm_kcopyd_client *kc,
		    unsigned int num_dests, struct dm_io_region *dests,
		    unsigned int flags, dm_kcopyd_notify_fn fn, void *context)
{
	dm_kcopyd_copy(kc, NULL, num_dests, dests, flags, fn, context);
}
EXPORT_SYMBOL(dm_kcopyd_zero);

void *dm_kcopyd_prepare_callback(struct dm_kcopyd_client *kc,
				 dm_kcopyd_notify_fn fn, void *context)
{
	struct kcopyd_job *job;

	job = mempool_alloc(&kc->job_pool, GFP_NOIO);

	memset(job, 0, sizeof(struct kcopyd_job));
	job->kc = kc;
	job->fn = fn;
	job->context = context;
	job->master_job = job;

	atomic_inc(&kc->nr_jobs);

	return job;
}
EXPORT_SYMBOL(dm_kcopyd_prepare_callback);

void dm_kcopyd_do_callback(void *j, int read_err, unsigned long write_err)
{
	struct kcopyd_job *job = j;
	struct dm_kcopyd_client *kc = job->kc;

	job->read_err = read_err;
	job->write_err = write_err;

	push(&kc->callback_jobs, job);
	wake(kc);
}
EXPORT_SYMBOL(dm_kcopyd_do_callback);

 
#if 0
int kcopyd_cancel(struct kcopyd_job *job, int block)
{
	 
	return -1;
}
#endif   

 
struct dm_kcopyd_client *dm_kcopyd_client_create(struct dm_kcopyd_throttle *throttle)
{
	int r;
	unsigned int reserve_pages;
	struct dm_kcopyd_client *kc;

	kc = kzalloc(sizeof(*kc), GFP_KERNEL);
	if (!kc)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&kc->job_lock);
	INIT_LIST_HEAD(&kc->callback_jobs);
	INIT_LIST_HEAD(&kc->complete_jobs);
	INIT_LIST_HEAD(&kc->io_jobs);
	INIT_LIST_HEAD(&kc->pages_jobs);
	kc->throttle = throttle;

	r = mempool_init_slab_pool(&kc->job_pool, MIN_JOBS, _job_cache);
	if (r)
		goto bad_slab;

	INIT_WORK(&kc->kcopyd_work, do_work);
	kc->kcopyd_wq = alloc_workqueue("kcopyd", WQ_MEM_RECLAIM, 0);
	if (!kc->kcopyd_wq) {
		r = -ENOMEM;
		goto bad_workqueue;
	}

	kc->sub_job_size = dm_get_kcopyd_subjob_size();
	reserve_pages = DIV_ROUND_UP(kc->sub_job_size << SECTOR_SHIFT, PAGE_SIZE);

	kc->pages = NULL;
	kc->nr_reserved_pages = kc->nr_free_pages = 0;
	r = client_reserve_pages(kc, reserve_pages);
	if (r)
		goto bad_client_pages;

	kc->io_client = dm_io_client_create();
	if (IS_ERR(kc->io_client)) {
		r = PTR_ERR(kc->io_client);
		goto bad_io_client;
	}

	init_waitqueue_head(&kc->destroyq);
	atomic_set(&kc->nr_jobs, 0);

	return kc;

bad_io_client:
	client_free_pages(kc);
bad_client_pages:
	destroy_workqueue(kc->kcopyd_wq);
bad_workqueue:
	mempool_exit(&kc->job_pool);
bad_slab:
	kfree(kc);

	return ERR_PTR(r);
}
EXPORT_SYMBOL(dm_kcopyd_client_create);

void dm_kcopyd_client_destroy(struct dm_kcopyd_client *kc)
{
	 
	wait_event(kc->destroyq, !atomic_read(&kc->nr_jobs));

	BUG_ON(!list_empty(&kc->callback_jobs));
	BUG_ON(!list_empty(&kc->complete_jobs));
	BUG_ON(!list_empty(&kc->io_jobs));
	BUG_ON(!list_empty(&kc->pages_jobs));
	destroy_workqueue(kc->kcopyd_wq);
	dm_io_client_destroy(kc->io_client);
	client_free_pages(kc);
	mempool_exit(&kc->job_pool);
	kfree(kc);
}
EXPORT_SYMBOL(dm_kcopyd_client_destroy);

void dm_kcopyd_client_flush(struct dm_kcopyd_client *kc)
{
	flush_workqueue(kc->kcopyd_wq);
}
EXPORT_SYMBOL(dm_kcopyd_client_flush);
