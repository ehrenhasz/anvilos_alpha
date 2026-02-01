
 

#include <linux/virtio.h>
#include <linux/virtio_balloon.h>
#include <linux/swap.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/balloon_compaction.h>
#include <linux/oom.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/page_reporting.h>

 
#define VIRTIO_BALLOON_PAGES_PER_PAGE (unsigned int)(PAGE_SIZE >> VIRTIO_BALLOON_PFN_SHIFT)
#define VIRTIO_BALLOON_ARRAY_PFNS_MAX 256
 
#define VIRTIO_BALLOON_OOM_NR_PAGES 256
#define VIRTIO_BALLOON_OOM_NOTIFY_PRIORITY 80

#define VIRTIO_BALLOON_FREE_PAGE_ALLOC_FLAG (__GFP_NORETRY | __GFP_NOWARN | \
					     __GFP_NOMEMALLOC)
 
#define VIRTIO_BALLOON_HINT_BLOCK_ORDER MAX_ORDER
 
#define VIRTIO_BALLOON_HINT_BLOCK_BYTES \
	(1 << (VIRTIO_BALLOON_HINT_BLOCK_ORDER + PAGE_SHIFT))
#define VIRTIO_BALLOON_HINT_BLOCK_PAGES (1 << VIRTIO_BALLOON_HINT_BLOCK_ORDER)

enum virtio_balloon_vq {
	VIRTIO_BALLOON_VQ_INFLATE,
	VIRTIO_BALLOON_VQ_DEFLATE,
	VIRTIO_BALLOON_VQ_STATS,
	VIRTIO_BALLOON_VQ_FREE_PAGE,
	VIRTIO_BALLOON_VQ_REPORTING,
	VIRTIO_BALLOON_VQ_MAX
};

enum virtio_balloon_config_read {
	VIRTIO_BALLOON_CONFIG_READ_CMD_ID = 0,
};

struct virtio_balloon {
	struct virtio_device *vdev;
	struct virtqueue *inflate_vq, *deflate_vq, *stats_vq, *free_page_vq;

	 
	struct workqueue_struct *balloon_wq;
	 
	struct work_struct report_free_page_work;

	 
	struct work_struct update_balloon_stats_work;
	struct work_struct update_balloon_size_work;

	 
	spinlock_t stop_update_lock;
	bool stop_update;
	 
	unsigned long config_read_bitmap;

	 
	struct list_head free_page_list;
	spinlock_t free_page_list_lock;
	 
	unsigned long num_free_page_blocks;
	 
	u32 cmd_id_received_cache;
	 
	__virtio32 cmd_id_active;
	 
	__virtio32 cmd_id_stop;

	 
	wait_queue_head_t acked;

	 
	unsigned int num_pages;
	 
	struct balloon_dev_info vb_dev_info;

	 
	struct mutex balloon_lock;

	 
	unsigned int num_pfns;
	__virtio32 pfns[VIRTIO_BALLOON_ARRAY_PFNS_MAX];

	 
	struct virtio_balloon_stat stats[VIRTIO_BALLOON_S_NR];

	 
	struct shrinker shrinker;

	 
	struct notifier_block oom_nb;

	 
	struct virtqueue *reporting_vq;
	struct page_reporting_dev_info pr_dev_info;
};

static const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_BALLOON, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static u32 page_to_balloon_pfn(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);

	BUILD_BUG_ON(PAGE_SHIFT < VIRTIO_BALLOON_PFN_SHIFT);
	 
	return pfn * VIRTIO_BALLOON_PAGES_PER_PAGE;
}

static void balloon_ack(struct virtqueue *vq)
{
	struct virtio_balloon *vb = vq->vdev->priv;

	wake_up(&vb->acked);
}

static void tell_host(struct virtio_balloon *vb, struct virtqueue *vq)
{
	struct scatterlist sg;
	unsigned int len;

	sg_init_one(&sg, vb->pfns, sizeof(vb->pfns[0]) * vb->num_pfns);

	 
	virtqueue_add_outbuf(vq, &sg, 1, vb, GFP_KERNEL);
	virtqueue_kick(vq);

	 
	wait_event(vb->acked, virtqueue_get_buf(vq, &len));

}

static int virtballoon_free_page_report(struct page_reporting_dev_info *pr_dev_info,
				   struct scatterlist *sg, unsigned int nents)
{
	struct virtio_balloon *vb =
		container_of(pr_dev_info, struct virtio_balloon, pr_dev_info);
	struct virtqueue *vq = vb->reporting_vq;
	unsigned int unused, err;

	 
	err = virtqueue_add_inbuf(vq, sg, nents, vb, GFP_NOWAIT | __GFP_NOWARN);

	 
	if (WARN_ON_ONCE(err))
		return err;

	virtqueue_kick(vq);

	 
	wait_event(vb->acked, virtqueue_get_buf(vq, &unused));

	return 0;
}

static void set_page_pfns(struct virtio_balloon *vb,
			  __virtio32 pfns[], struct page *page)
{
	unsigned int i;

	BUILD_BUG_ON(VIRTIO_BALLOON_PAGES_PER_PAGE > VIRTIO_BALLOON_ARRAY_PFNS_MAX);

	 
	for (i = 0; i < VIRTIO_BALLOON_PAGES_PER_PAGE; i++)
		pfns[i] = cpu_to_virtio32(vb->vdev,
					  page_to_balloon_pfn(page) + i);
}

static unsigned int fill_balloon(struct virtio_balloon *vb, size_t num)
{
	unsigned int num_allocated_pages;
	unsigned int num_pfns;
	struct page *page;
	LIST_HEAD(pages);

	 
	num = min(num, ARRAY_SIZE(vb->pfns));

	for (num_pfns = 0; num_pfns < num;
	     num_pfns += VIRTIO_BALLOON_PAGES_PER_PAGE) {
		struct page *page = balloon_page_alloc();

		if (!page) {
			dev_info_ratelimited(&vb->vdev->dev,
					     "Out of puff! Can't get %u pages\n",
					     VIRTIO_BALLOON_PAGES_PER_PAGE);
			 
			msleep(200);
			break;
		}

		balloon_page_push(&pages, page);
	}

	mutex_lock(&vb->balloon_lock);

	vb->num_pfns = 0;

	while ((page = balloon_page_pop(&pages))) {
		balloon_page_enqueue(&vb->vb_dev_info, page);

		set_page_pfns(vb, vb->pfns + vb->num_pfns, page);
		vb->num_pages += VIRTIO_BALLOON_PAGES_PER_PAGE;
		if (!virtio_has_feature(vb->vdev,
					VIRTIO_BALLOON_F_DEFLATE_ON_OOM))
			adjust_managed_page_count(page, -1);
		vb->num_pfns += VIRTIO_BALLOON_PAGES_PER_PAGE;
	}

	num_allocated_pages = vb->num_pfns;
	 
	if (vb->num_pfns != 0)
		tell_host(vb, vb->inflate_vq);
	mutex_unlock(&vb->balloon_lock);

	return num_allocated_pages;
}

static void release_pages_balloon(struct virtio_balloon *vb,
				 struct list_head *pages)
{
	struct page *page, *next;

	list_for_each_entry_safe(page, next, pages, lru) {
		if (!virtio_has_feature(vb->vdev,
					VIRTIO_BALLOON_F_DEFLATE_ON_OOM))
			adjust_managed_page_count(page, 1);
		list_del(&page->lru);
		put_page(page);  
	}
}

static unsigned int leak_balloon(struct virtio_balloon *vb, size_t num)
{
	unsigned int num_freed_pages;
	struct page *page;
	struct balloon_dev_info *vb_dev_info = &vb->vb_dev_info;
	LIST_HEAD(pages);

	 
	num = min(num, ARRAY_SIZE(vb->pfns));

	mutex_lock(&vb->balloon_lock);
	 
	num = min(num, (size_t)vb->num_pages);
	for (vb->num_pfns = 0; vb->num_pfns < num;
	     vb->num_pfns += VIRTIO_BALLOON_PAGES_PER_PAGE) {
		page = balloon_page_dequeue(vb_dev_info);
		if (!page)
			break;
		set_page_pfns(vb, vb->pfns + vb->num_pfns, page);
		list_add(&page->lru, &pages);
		vb->num_pages -= VIRTIO_BALLOON_PAGES_PER_PAGE;
	}

	num_freed_pages = vb->num_pfns;
	 
	if (vb->num_pfns != 0)
		tell_host(vb, vb->deflate_vq);
	release_pages_balloon(vb, &pages);
	mutex_unlock(&vb->balloon_lock);
	return num_freed_pages;
}

static inline void update_stat(struct virtio_balloon *vb, int idx,
			       u16 tag, u64 val)
{
	BUG_ON(idx >= VIRTIO_BALLOON_S_NR);
	vb->stats[idx].tag = cpu_to_virtio16(vb->vdev, tag);
	vb->stats[idx].val = cpu_to_virtio64(vb->vdev, val);
}

#define pages_to_bytes(x) ((u64)(x) << PAGE_SHIFT)

static unsigned int update_balloon_stats(struct virtio_balloon *vb)
{
	unsigned long events[NR_VM_EVENT_ITEMS];
	struct sysinfo i;
	unsigned int idx = 0;
	long available;
	unsigned long caches;

	all_vm_events(events);
	si_meminfo(&i);

	available = si_mem_available();
	caches = global_node_page_state(NR_FILE_PAGES);

#ifdef CONFIG_VM_EVENT_COUNTERS
	update_stat(vb, idx++, VIRTIO_BALLOON_S_SWAP_IN,
				pages_to_bytes(events[PSWPIN]));
	update_stat(vb, idx++, VIRTIO_BALLOON_S_SWAP_OUT,
				pages_to_bytes(events[PSWPOUT]));
	update_stat(vb, idx++, VIRTIO_BALLOON_S_MAJFLT, events[PGMAJFAULT]);
	update_stat(vb, idx++, VIRTIO_BALLOON_S_MINFLT, events[PGFAULT]);
#ifdef CONFIG_HUGETLB_PAGE
	update_stat(vb, idx++, VIRTIO_BALLOON_S_HTLB_PGALLOC,
		    events[HTLB_BUDDY_PGALLOC]);
	update_stat(vb, idx++, VIRTIO_BALLOON_S_HTLB_PGFAIL,
		    events[HTLB_BUDDY_PGALLOC_FAIL]);
#endif
#endif
	update_stat(vb, idx++, VIRTIO_BALLOON_S_MEMFREE,
				pages_to_bytes(i.freeram));
	update_stat(vb, idx++, VIRTIO_BALLOON_S_MEMTOT,
				pages_to_bytes(i.totalram));
	update_stat(vb, idx++, VIRTIO_BALLOON_S_AVAIL,
				pages_to_bytes(available));
	update_stat(vb, idx++, VIRTIO_BALLOON_S_CACHES,
				pages_to_bytes(caches));

	return idx;
}

 
static void stats_request(struct virtqueue *vq)
{
	struct virtio_balloon *vb = vq->vdev->priv;

	spin_lock(&vb->stop_update_lock);
	if (!vb->stop_update)
		queue_work(system_freezable_wq, &vb->update_balloon_stats_work);
	spin_unlock(&vb->stop_update_lock);
}

static void stats_handle_request(struct virtio_balloon *vb)
{
	struct virtqueue *vq;
	struct scatterlist sg;
	unsigned int len, num_stats;

	num_stats = update_balloon_stats(vb);

	vq = vb->stats_vq;
	if (!virtqueue_get_buf(vq, &len))
		return;
	sg_init_one(&sg, vb->stats, sizeof(vb->stats[0]) * num_stats);
	virtqueue_add_outbuf(vq, &sg, 1, vb, GFP_KERNEL);
	virtqueue_kick(vq);
}

static inline s64 towards_target(struct virtio_balloon *vb)
{
	s64 target;
	u32 num_pages;

	 
	virtio_cread_le(vb->vdev, struct virtio_balloon_config, num_pages,
			&num_pages);

	 
	target = ALIGN(num_pages, VIRTIO_BALLOON_PAGES_PER_PAGE);
	return target - vb->num_pages;
}

 
static unsigned long return_free_pages_to_mm(struct virtio_balloon *vb,
					     unsigned long num_to_return)
{
	struct page *page;
	unsigned long num_returned;

	spin_lock_irq(&vb->free_page_list_lock);
	for (num_returned = 0; num_returned < num_to_return; num_returned++) {
		page = balloon_page_pop(&vb->free_page_list);
		if (!page)
			break;
		free_pages((unsigned long)page_address(page),
			   VIRTIO_BALLOON_HINT_BLOCK_ORDER);
	}
	vb->num_free_page_blocks -= num_returned;
	spin_unlock_irq(&vb->free_page_list_lock);

	return num_returned;
}

static void virtio_balloon_queue_free_page_work(struct virtio_balloon *vb)
{
	if (!virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		return;

	 
	if (test_and_set_bit(VIRTIO_BALLOON_CONFIG_READ_CMD_ID,
			     &vb->config_read_bitmap))
		return;

	queue_work(vb->balloon_wq, &vb->report_free_page_work);
}

static void virtballoon_changed(struct virtio_device *vdev)
{
	struct virtio_balloon *vb = vdev->priv;
	unsigned long flags;

	spin_lock_irqsave(&vb->stop_update_lock, flags);
	if (!vb->stop_update) {
		queue_work(system_freezable_wq,
			   &vb->update_balloon_size_work);
		virtio_balloon_queue_free_page_work(vb);
	}
	spin_unlock_irqrestore(&vb->stop_update_lock, flags);
}

static void update_balloon_size(struct virtio_balloon *vb)
{
	u32 actual = vb->num_pages;

	 
	virtio_cwrite_le(vb->vdev, struct virtio_balloon_config, actual,
			 &actual);
}

static void update_balloon_stats_func(struct work_struct *work)
{
	struct virtio_balloon *vb;

	vb = container_of(work, struct virtio_balloon,
			  update_balloon_stats_work);
	stats_handle_request(vb);
}

static void update_balloon_size_func(struct work_struct *work)
{
	struct virtio_balloon *vb;
	s64 diff;

	vb = container_of(work, struct virtio_balloon,
			  update_balloon_size_work);
	diff = towards_target(vb);

	if (!diff)
		return;

	if (diff > 0)
		diff -= fill_balloon(vb, diff);
	else
		diff += leak_balloon(vb, -diff);
	update_balloon_size(vb);

	if (diff)
		queue_work(system_freezable_wq, work);
}

static int init_vqs(struct virtio_balloon *vb)
{
	struct virtqueue *vqs[VIRTIO_BALLOON_VQ_MAX];
	vq_callback_t *callbacks[VIRTIO_BALLOON_VQ_MAX];
	const char *names[VIRTIO_BALLOON_VQ_MAX];
	int err;

	 
	callbacks[VIRTIO_BALLOON_VQ_INFLATE] = balloon_ack;
	names[VIRTIO_BALLOON_VQ_INFLATE] = "inflate";
	callbacks[VIRTIO_BALLOON_VQ_DEFLATE] = balloon_ack;
	names[VIRTIO_BALLOON_VQ_DEFLATE] = "deflate";
	callbacks[VIRTIO_BALLOON_VQ_STATS] = NULL;
	names[VIRTIO_BALLOON_VQ_STATS] = NULL;
	callbacks[VIRTIO_BALLOON_VQ_FREE_PAGE] = NULL;
	names[VIRTIO_BALLOON_VQ_FREE_PAGE] = NULL;
	names[VIRTIO_BALLOON_VQ_REPORTING] = NULL;

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_STATS_VQ)) {
		names[VIRTIO_BALLOON_VQ_STATS] = "stats";
		callbacks[VIRTIO_BALLOON_VQ_STATS] = stats_request;
	}

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT)) {
		names[VIRTIO_BALLOON_VQ_FREE_PAGE] = "free_page_vq";
		callbacks[VIRTIO_BALLOON_VQ_FREE_PAGE] = NULL;
	}

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_REPORTING)) {
		names[VIRTIO_BALLOON_VQ_REPORTING] = "reporting_vq";
		callbacks[VIRTIO_BALLOON_VQ_REPORTING] = balloon_ack;
	}

	err = virtio_find_vqs(vb->vdev, VIRTIO_BALLOON_VQ_MAX, vqs,
			      callbacks, names, NULL);
	if (err)
		return err;

	vb->inflate_vq = vqs[VIRTIO_BALLOON_VQ_INFLATE];
	vb->deflate_vq = vqs[VIRTIO_BALLOON_VQ_DEFLATE];
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_STATS_VQ)) {
		struct scatterlist sg;
		unsigned int num_stats;
		vb->stats_vq = vqs[VIRTIO_BALLOON_VQ_STATS];

		 
		num_stats = update_balloon_stats(vb);

		sg_init_one(&sg, vb->stats, sizeof(vb->stats[0]) * num_stats);
		err = virtqueue_add_outbuf(vb->stats_vq, &sg, 1, vb,
					   GFP_KERNEL);
		if (err) {
			dev_warn(&vb->vdev->dev, "%s: add stat_vq failed\n",
				 __func__);
			return err;
		}
		virtqueue_kick(vb->stats_vq);
	}

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		vb->free_page_vq = vqs[VIRTIO_BALLOON_VQ_FREE_PAGE];

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_REPORTING))
		vb->reporting_vq = vqs[VIRTIO_BALLOON_VQ_REPORTING];

	return 0;
}

static u32 virtio_balloon_cmd_id_received(struct virtio_balloon *vb)
{
	if (test_and_clear_bit(VIRTIO_BALLOON_CONFIG_READ_CMD_ID,
			       &vb->config_read_bitmap)) {
		 
		virtio_cread_le(vb->vdev, struct virtio_balloon_config,
				free_page_hint_cmd_id,
				&vb->cmd_id_received_cache);
	}

	return vb->cmd_id_received_cache;
}

static int send_cmd_id_start(struct virtio_balloon *vb)
{
	struct scatterlist sg;
	struct virtqueue *vq = vb->free_page_vq;
	int err, unused;

	 
	while (virtqueue_get_buf(vq, &unused))
		;

	vb->cmd_id_active = cpu_to_virtio32(vb->vdev,
					virtio_balloon_cmd_id_received(vb));
	sg_init_one(&sg, &vb->cmd_id_active, sizeof(vb->cmd_id_active));
	err = virtqueue_add_outbuf(vq, &sg, 1, &vb->cmd_id_active, GFP_KERNEL);
	if (!err)
		virtqueue_kick(vq);
	return err;
}

static int send_cmd_id_stop(struct virtio_balloon *vb)
{
	struct scatterlist sg;
	struct virtqueue *vq = vb->free_page_vq;
	int err, unused;

	 
	while (virtqueue_get_buf(vq, &unused))
		;

	sg_init_one(&sg, &vb->cmd_id_stop, sizeof(vb->cmd_id_stop));
	err = virtqueue_add_outbuf(vq, &sg, 1, &vb->cmd_id_stop, GFP_KERNEL);
	if (!err)
		virtqueue_kick(vq);
	return err;
}

static int get_free_page_and_send(struct virtio_balloon *vb)
{
	struct virtqueue *vq = vb->free_page_vq;
	struct page *page;
	struct scatterlist sg;
	int err, unused;
	void *p;

	 
	while (virtqueue_get_buf(vq, &unused))
		;

	page = alloc_pages(VIRTIO_BALLOON_FREE_PAGE_ALLOC_FLAG,
			   VIRTIO_BALLOON_HINT_BLOCK_ORDER);
	 
	if (!page)
		return -EINTR;

	p = page_address(page);
	sg_init_one(&sg, p, VIRTIO_BALLOON_HINT_BLOCK_BYTES);
	 
	if (vq->num_free > 1) {
		err = virtqueue_add_inbuf(vq, &sg, 1, p, GFP_KERNEL);
		if (unlikely(err)) {
			free_pages((unsigned long)p,
				   VIRTIO_BALLOON_HINT_BLOCK_ORDER);
			return err;
		}
		virtqueue_kick(vq);
		spin_lock_irq(&vb->free_page_list_lock);
		balloon_page_push(&vb->free_page_list, page);
		vb->num_free_page_blocks++;
		spin_unlock_irq(&vb->free_page_list_lock);
	} else {
		 
		free_pages((unsigned long)p, VIRTIO_BALLOON_HINT_BLOCK_ORDER);
	}

	return 0;
}

static int send_free_pages(struct virtio_balloon *vb)
{
	int err;
	u32 cmd_id_active;

	while (1) {
		 
		cmd_id_active = virtio32_to_cpu(vb->vdev, vb->cmd_id_active);
		if (unlikely(cmd_id_active !=
			     virtio_balloon_cmd_id_received(vb)))
			break;

		 
		err = get_free_page_and_send(vb);
		if (err == -EINTR)
			break;
		else if (unlikely(err))
			return err;
	}

	return 0;
}

static void virtio_balloon_report_free_page(struct virtio_balloon *vb)
{
	int err;
	struct device *dev = &vb->vdev->dev;

	 
	err = send_cmd_id_start(vb);
	if (unlikely(err))
		dev_err(dev, "Failed to send a start id, err = %d\n", err);

	err = send_free_pages(vb);
	if (unlikely(err))
		dev_err(dev, "Failed to send a free page, err = %d\n", err);

	 
	err = send_cmd_id_stop(vb);
	if (unlikely(err))
		dev_err(dev, "Failed to send a stop id, err = %d\n", err);
}

static void report_free_page_func(struct work_struct *work)
{
	struct virtio_balloon *vb = container_of(work, struct virtio_balloon,
						 report_free_page_work);
	u32 cmd_id_received;

	cmd_id_received = virtio_balloon_cmd_id_received(vb);
	if (cmd_id_received == VIRTIO_BALLOON_CMD_ID_DONE) {
		 
		return_free_pages_to_mm(vb, ULONG_MAX);
	} else if (cmd_id_received != VIRTIO_BALLOON_CMD_ID_STOP &&
		   cmd_id_received !=
		   virtio32_to_cpu(vb->vdev, vb->cmd_id_active)) {
		virtio_balloon_report_free_page(vb);
	}
}

#ifdef CONFIG_BALLOON_COMPACTION
 
static int virtballoon_migratepage(struct balloon_dev_info *vb_dev_info,
		struct page *newpage, struct page *page, enum migrate_mode mode)
{
	struct virtio_balloon *vb = container_of(vb_dev_info,
			struct virtio_balloon, vb_dev_info);
	unsigned long flags;

	 
	if (!mutex_trylock(&vb->balloon_lock))
		return -EAGAIN;

	get_page(newpage);  

	 
	if (!virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_DEFLATE_ON_OOM) &&
	    page_zone(page) != page_zone(newpage)) {
		adjust_managed_page_count(page, 1);
		adjust_managed_page_count(newpage, -1);
	}

	 
	spin_lock_irqsave(&vb_dev_info->pages_lock, flags);
	balloon_page_insert(vb_dev_info, newpage);
	vb_dev_info->isolated_pages--;
	__count_vm_event(BALLOON_MIGRATE);
	spin_unlock_irqrestore(&vb_dev_info->pages_lock, flags);
	vb->num_pfns = VIRTIO_BALLOON_PAGES_PER_PAGE;
	set_page_pfns(vb, vb->pfns, newpage);
	tell_host(vb, vb->inflate_vq);

	 
	spin_lock_irqsave(&vb_dev_info->pages_lock, flags);
	balloon_page_delete(page);
	spin_unlock_irqrestore(&vb_dev_info->pages_lock, flags);
	vb->num_pfns = VIRTIO_BALLOON_PAGES_PER_PAGE;
	set_page_pfns(vb, vb->pfns, page);
	tell_host(vb, vb->deflate_vq);

	mutex_unlock(&vb->balloon_lock);

	put_page(page);  

	return MIGRATEPAGE_SUCCESS;
}
#endif  

static unsigned long shrink_free_pages(struct virtio_balloon *vb,
				       unsigned long pages_to_free)
{
	unsigned long blocks_to_free, blocks_freed;

	pages_to_free = round_up(pages_to_free,
				 VIRTIO_BALLOON_HINT_BLOCK_PAGES);
	blocks_to_free = pages_to_free / VIRTIO_BALLOON_HINT_BLOCK_PAGES;
	blocks_freed = return_free_pages_to_mm(vb, blocks_to_free);

	return blocks_freed * VIRTIO_BALLOON_HINT_BLOCK_PAGES;
}

static unsigned long virtio_balloon_shrinker_scan(struct shrinker *shrinker,
						  struct shrink_control *sc)
{
	struct virtio_balloon *vb = container_of(shrinker,
					struct virtio_balloon, shrinker);

	return shrink_free_pages(vb, sc->nr_to_scan);
}

static unsigned long virtio_balloon_shrinker_count(struct shrinker *shrinker,
						   struct shrink_control *sc)
{
	struct virtio_balloon *vb = container_of(shrinker,
					struct virtio_balloon, shrinker);

	return vb->num_free_page_blocks * VIRTIO_BALLOON_HINT_BLOCK_PAGES;
}

static int virtio_balloon_oom_notify(struct notifier_block *nb,
				     unsigned long dummy, void *parm)
{
	struct virtio_balloon *vb = container_of(nb,
						 struct virtio_balloon, oom_nb);
	unsigned long *freed = parm;

	*freed += leak_balloon(vb, VIRTIO_BALLOON_OOM_NR_PAGES) /
		  VIRTIO_BALLOON_PAGES_PER_PAGE;
	update_balloon_size(vb);

	return NOTIFY_OK;
}

static void virtio_balloon_unregister_shrinker(struct virtio_balloon *vb)
{
	unregister_shrinker(&vb->shrinker);
}

static int virtio_balloon_register_shrinker(struct virtio_balloon *vb)
{
	vb->shrinker.scan_objects = virtio_balloon_shrinker_scan;
	vb->shrinker.count_objects = virtio_balloon_shrinker_count;
	vb->shrinker.seeks = DEFAULT_SEEKS;

	return register_shrinker(&vb->shrinker, "virtio-balloon");
}

static int virtballoon_probe(struct virtio_device *vdev)
{
	struct virtio_balloon *vb;
	int err;

	if (!vdev->config->get) {
		dev_err(&vdev->dev, "%s failure: config access disabled\n",
			__func__);
		return -EINVAL;
	}

	vdev->priv = vb = kzalloc(sizeof(*vb), GFP_KERNEL);
	if (!vb) {
		err = -ENOMEM;
		goto out;
	}

	INIT_WORK(&vb->update_balloon_stats_work, update_balloon_stats_func);
	INIT_WORK(&vb->update_balloon_size_work, update_balloon_size_func);
	spin_lock_init(&vb->stop_update_lock);
	mutex_init(&vb->balloon_lock);
	init_waitqueue_head(&vb->acked);
	vb->vdev = vdev;

	balloon_devinfo_init(&vb->vb_dev_info);

	err = init_vqs(vb);
	if (err)
		goto out_free_vb;

#ifdef CONFIG_BALLOON_COMPACTION
	vb->vb_dev_info.migratepage = virtballoon_migratepage;
#endif
	if (virtio_has_feature(vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT)) {
		 
		if (virtqueue_get_vring_size(vb->free_page_vq) < 2) {
			err = -ENOSPC;
			goto out_del_vqs;
		}
		vb->balloon_wq = alloc_workqueue("balloon-wq",
					WQ_FREEZABLE | WQ_CPU_INTENSIVE, 0);
		if (!vb->balloon_wq) {
			err = -ENOMEM;
			goto out_del_vqs;
		}
		INIT_WORK(&vb->report_free_page_work, report_free_page_func);
		vb->cmd_id_received_cache = VIRTIO_BALLOON_CMD_ID_STOP;
		vb->cmd_id_active = cpu_to_virtio32(vb->vdev,
						  VIRTIO_BALLOON_CMD_ID_STOP);
		vb->cmd_id_stop = cpu_to_virtio32(vb->vdev,
						  VIRTIO_BALLOON_CMD_ID_STOP);
		spin_lock_init(&vb->free_page_list_lock);
		INIT_LIST_HEAD(&vb->free_page_list);
		 
		err = virtio_balloon_register_shrinker(vb);
		if (err)
			goto out_del_balloon_wq;
	}

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_DEFLATE_ON_OOM)) {
		vb->oom_nb.notifier_call = virtio_balloon_oom_notify;
		vb->oom_nb.priority = VIRTIO_BALLOON_OOM_NOTIFY_PRIORITY;
		err = register_oom_notifier(&vb->oom_nb);
		if (err < 0)
			goto out_unregister_shrinker;
	}

	if (virtio_has_feature(vdev, VIRTIO_BALLOON_F_PAGE_POISON)) {
		 
		__u32 poison_val = 0;

		 
		if (!want_init_on_free())
			memset(&poison_val, PAGE_POISON, sizeof(poison_val));

		virtio_cwrite_le(vb->vdev, struct virtio_balloon_config,
				 poison_val, &poison_val);
	}

	vb->pr_dev_info.report = virtballoon_free_page_report;
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_REPORTING)) {
		unsigned int capacity;

		capacity = virtqueue_get_vring_size(vb->reporting_vq);
		if (capacity < PAGE_REPORTING_CAPACITY) {
			err = -ENOSPC;
			goto out_unregister_oom;
		}

		 
#if defined(CONFIG_ARM64) && defined(CONFIG_ARM64_64K_PAGES)
		vb->pr_dev_info.order = 5;
#endif

		err = page_reporting_register(&vb->pr_dev_info);
		if (err)
			goto out_unregister_oom;
	}

	virtio_device_ready(vdev);

	if (towards_target(vb))
		virtballoon_changed(vdev);
	return 0;

out_unregister_oom:
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_DEFLATE_ON_OOM))
		unregister_oom_notifier(&vb->oom_nb);
out_unregister_shrinker:
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		virtio_balloon_unregister_shrinker(vb);
out_del_balloon_wq:
	if (virtio_has_feature(vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		destroy_workqueue(vb->balloon_wq);
out_del_vqs:
	vdev->config->del_vqs(vdev);
out_free_vb:
	kfree(vb);
out:
	return err;
}

static void remove_common(struct virtio_balloon *vb)
{
	 
	while (vb->num_pages)
		leak_balloon(vb, vb->num_pages);
	update_balloon_size(vb);

	 
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		return_free_pages_to_mm(vb, ULONG_MAX);

	 
	virtio_reset_device(vb->vdev);

	vb->vdev->config->del_vqs(vb->vdev);
}

static void virtballoon_remove(struct virtio_device *vdev)
{
	struct virtio_balloon *vb = vdev->priv;

	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_REPORTING))
		page_reporting_unregister(&vb->pr_dev_info);
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_DEFLATE_ON_OOM))
		unregister_oom_notifier(&vb->oom_nb);
	if (virtio_has_feature(vb->vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT))
		virtio_balloon_unregister_shrinker(vb);
	spin_lock_irq(&vb->stop_update_lock);
	vb->stop_update = true;
	spin_unlock_irq(&vb->stop_update_lock);
	cancel_work_sync(&vb->update_balloon_size_work);
	cancel_work_sync(&vb->update_balloon_stats_work);

	if (virtio_has_feature(vdev, VIRTIO_BALLOON_F_FREE_PAGE_HINT)) {
		cancel_work_sync(&vb->report_free_page_work);
		destroy_workqueue(vb->balloon_wq);
	}

	remove_common(vb);
	kfree(vb);
}

#ifdef CONFIG_PM_SLEEP
static int virtballoon_freeze(struct virtio_device *vdev)
{
	struct virtio_balloon *vb = vdev->priv;

	 
	remove_common(vb);
	return 0;
}

static int virtballoon_restore(struct virtio_device *vdev)
{
	struct virtio_balloon *vb = vdev->priv;
	int ret;

	ret = init_vqs(vdev->priv);
	if (ret)
		return ret;

	virtio_device_ready(vdev);

	if (towards_target(vb))
		virtballoon_changed(vdev);
	update_balloon_size(vb);
	return 0;
}
#endif

static int virtballoon_validate(struct virtio_device *vdev)
{
	 
	if (!want_init_on_free() && !page_poisoning_enabled_static())
		__virtio_clear_bit(vdev, VIRTIO_BALLOON_F_PAGE_POISON);
	else if (!virtio_has_feature(vdev, VIRTIO_BALLOON_F_PAGE_POISON))
		__virtio_clear_bit(vdev, VIRTIO_BALLOON_F_REPORTING);

	__virtio_clear_bit(vdev, VIRTIO_F_ACCESS_PLATFORM);
	return 0;
}

static unsigned int features[] = {
	VIRTIO_BALLOON_F_MUST_TELL_HOST,
	VIRTIO_BALLOON_F_STATS_VQ,
	VIRTIO_BALLOON_F_DEFLATE_ON_OOM,
	VIRTIO_BALLOON_F_FREE_PAGE_HINT,
	VIRTIO_BALLOON_F_PAGE_POISON,
	VIRTIO_BALLOON_F_REPORTING,
};

static struct virtio_driver virtio_balloon_driver = {
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.validate =	virtballoon_validate,
	.probe =	virtballoon_probe,
	.remove =	virtballoon_remove,
	.config_changed = virtballoon_changed,
#ifdef CONFIG_PM_SLEEP
	.freeze	=	virtballoon_freeze,
	.restore =	virtballoon_restore,
#endif
};

module_virtio_driver(virtio_balloon_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio balloon driver");
MODULE_LICENSE("GPL");
