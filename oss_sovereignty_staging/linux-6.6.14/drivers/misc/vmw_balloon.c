
 


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/balloon_compaction.h>
#include <linux/vmw_vmci_defs.h>
#include <linux/vmw_vmci_api.h>
#include <asm/hypervisor.h>

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Memory Control (Balloon) Driver");
MODULE_ALIAS("dmi:*:svnVMware*:*");
MODULE_ALIAS("vmware_vmmemctl");
MODULE_LICENSE("GPL");

static bool __read_mostly vmwballoon_shrinker_enable;
module_param(vmwballoon_shrinker_enable, bool, 0444);
MODULE_PARM_DESC(vmwballoon_shrinker_enable,
	"Enable non-cooperative out-of-memory protection. Disabled by default as it may degrade performance.");

 
#define VMBALLOON_SHRINK_DELAY		(5)

 
#define VMW_BALLOON_MAX_REFUSED		16

 
#define BALLOON_VMW_MAGIC		0x0ba11007

 
#define VMW_BALLOON_HV_PORT		0x5670
#define VMW_BALLOON_HV_MAGIC		0x456c6d6f
#define VMW_BALLOON_GUEST_ID		1	 

enum vmwballoon_capabilities {
	 
	VMW_BALLOON_BASIC_CMDS			= (1 << 1),
	VMW_BALLOON_BATCHED_CMDS		= (1 << 2),
	VMW_BALLOON_BATCHED_2M_CMDS		= (1 << 3),
	VMW_BALLOON_SIGNALLED_WAKEUP_CMD	= (1 << 4),
	VMW_BALLOON_64_BIT_TARGET		= (1 << 5)
};

#define VMW_BALLOON_CAPABILITIES_COMMON	(VMW_BALLOON_BASIC_CMDS \
					| VMW_BALLOON_BATCHED_CMDS \
					| VMW_BALLOON_BATCHED_2M_CMDS \
					| VMW_BALLOON_SIGNALLED_WAKEUP_CMD)

#define VMW_BALLOON_2M_ORDER		(PMD_SHIFT - PAGE_SHIFT)

 
#ifdef CONFIG_64BIT
#define VMW_BALLOON_CAPABILITIES	(VMW_BALLOON_CAPABILITIES_COMMON \
					| VMW_BALLOON_64_BIT_TARGET)
#else
#define VMW_BALLOON_CAPABILITIES	VMW_BALLOON_CAPABILITIES_COMMON
#endif

enum vmballoon_page_size_type {
	VMW_BALLOON_4K_PAGE,
	VMW_BALLOON_2M_PAGE,
	VMW_BALLOON_LAST_SIZE = VMW_BALLOON_2M_PAGE
};

#define VMW_BALLOON_NUM_PAGE_SIZES	(VMW_BALLOON_LAST_SIZE + 1)

static const char * const vmballoon_page_size_names[] = {
	[VMW_BALLOON_4K_PAGE]			= "4k",
	[VMW_BALLOON_2M_PAGE]			= "2M"
};

enum vmballoon_op {
	VMW_BALLOON_INFLATE,
	VMW_BALLOON_DEFLATE
};

enum vmballoon_op_stat_type {
	VMW_BALLOON_OP_STAT,
	VMW_BALLOON_OP_FAIL_STAT
};

#define VMW_BALLOON_OP_STAT_TYPES	(VMW_BALLOON_OP_FAIL_STAT + 1)

 
enum vmballoon_cmd_type {
	VMW_BALLOON_CMD_START,
	VMW_BALLOON_CMD_GET_TARGET,
	VMW_BALLOON_CMD_LOCK,
	VMW_BALLOON_CMD_UNLOCK,
	VMW_BALLOON_CMD_GUEST_ID,
	 
	VMW_BALLOON_CMD_BATCHED_LOCK = 6,
	VMW_BALLOON_CMD_BATCHED_UNLOCK,
	VMW_BALLOON_CMD_BATCHED_2M_LOCK,
	VMW_BALLOON_CMD_BATCHED_2M_UNLOCK,
	VMW_BALLOON_CMD_VMCI_DOORBELL_SET,
	VMW_BALLOON_CMD_LAST = VMW_BALLOON_CMD_VMCI_DOORBELL_SET,
};

#define VMW_BALLOON_CMD_NUM	(VMW_BALLOON_CMD_LAST + 1)

enum vmballoon_error_codes {
	VMW_BALLOON_SUCCESS,
	VMW_BALLOON_ERROR_CMD_INVALID,
	VMW_BALLOON_ERROR_PPN_INVALID,
	VMW_BALLOON_ERROR_PPN_LOCKED,
	VMW_BALLOON_ERROR_PPN_UNLOCKED,
	VMW_BALLOON_ERROR_PPN_PINNED,
	VMW_BALLOON_ERROR_PPN_NOTNEEDED,
	VMW_BALLOON_ERROR_RESET,
	VMW_BALLOON_ERROR_BUSY
};

#define VMW_BALLOON_SUCCESS_WITH_CAPABILITIES	(0x03000000)

#define VMW_BALLOON_CMD_WITH_TARGET_MASK			\
	((1UL << VMW_BALLOON_CMD_GET_TARGET)		|	\
	 (1UL << VMW_BALLOON_CMD_LOCK)			|	\
	 (1UL << VMW_BALLOON_CMD_UNLOCK)		|	\
	 (1UL << VMW_BALLOON_CMD_BATCHED_LOCK)		|	\
	 (1UL << VMW_BALLOON_CMD_BATCHED_UNLOCK)	|	\
	 (1UL << VMW_BALLOON_CMD_BATCHED_2M_LOCK)	|	\
	 (1UL << VMW_BALLOON_CMD_BATCHED_2M_UNLOCK))

static const char * const vmballoon_cmd_names[] = {
	[VMW_BALLOON_CMD_START]			= "start",
	[VMW_BALLOON_CMD_GET_TARGET]		= "target",
	[VMW_BALLOON_CMD_LOCK]			= "lock",
	[VMW_BALLOON_CMD_UNLOCK]		= "unlock",
	[VMW_BALLOON_CMD_GUEST_ID]		= "guestType",
	[VMW_BALLOON_CMD_BATCHED_LOCK]		= "batchLock",
	[VMW_BALLOON_CMD_BATCHED_UNLOCK]	= "batchUnlock",
	[VMW_BALLOON_CMD_BATCHED_2M_LOCK]	= "2m-lock",
	[VMW_BALLOON_CMD_BATCHED_2M_UNLOCK]	= "2m-unlock",
	[VMW_BALLOON_CMD_VMCI_DOORBELL_SET]	= "doorbellSet"
};

enum vmballoon_stat_page {
	VMW_BALLOON_PAGE_STAT_ALLOC,
	VMW_BALLOON_PAGE_STAT_ALLOC_FAIL,
	VMW_BALLOON_PAGE_STAT_REFUSED_ALLOC,
	VMW_BALLOON_PAGE_STAT_REFUSED_FREE,
	VMW_BALLOON_PAGE_STAT_FREE,
	VMW_BALLOON_PAGE_STAT_LAST = VMW_BALLOON_PAGE_STAT_FREE
};

#define VMW_BALLOON_PAGE_STAT_NUM	(VMW_BALLOON_PAGE_STAT_LAST + 1)

enum vmballoon_stat_general {
	VMW_BALLOON_STAT_TIMER,
	VMW_BALLOON_STAT_DOORBELL,
	VMW_BALLOON_STAT_RESET,
	VMW_BALLOON_STAT_SHRINK,
	VMW_BALLOON_STAT_SHRINK_FREE,
	VMW_BALLOON_STAT_LAST = VMW_BALLOON_STAT_SHRINK_FREE
};

#define VMW_BALLOON_STAT_NUM		(VMW_BALLOON_STAT_LAST + 1)

static DEFINE_STATIC_KEY_TRUE(vmw_balloon_batching);
static DEFINE_STATIC_KEY_FALSE(balloon_stat_enabled);

struct vmballoon_ctl {
	struct list_head pages;
	struct list_head refused_pages;
	struct list_head prealloc_pages;
	unsigned int n_refused_pages;
	unsigned int n_pages;
	enum vmballoon_page_size_type page_size;
	enum vmballoon_op op;
};

 
struct vmballoon_batch_entry {
	u64 status : 5;
	u64 reserved : PAGE_SHIFT - 5;
	u64 pfn : 52;
} __packed;

struct vmballoon {
	 
	enum vmballoon_page_size_type max_page_size;

	 
	atomic64_t size;

	 
	unsigned long target;

	 
	bool reset_required;

	 
	unsigned long capabilities;

	 
	struct vmballoon_batch_entry *batch_page;

	 
	unsigned int batch_max_pages;

	 
	struct page *page;

	 
	unsigned long shrink_timeout;

	 
	struct vmballoon_stats *stats;

	 
	struct balloon_dev_info b_dev_info;

	struct delayed_work dwork;

	 
	struct list_head huge_pages;

	 
	struct vmci_handle vmci_doorbell;

	 
	struct rw_semaphore conf_sem;

	 
	spinlock_t comm_lock;

	 
	struct shrinker shrinker;

	 
	bool shrinker_registered;
};

static struct vmballoon balloon;

struct vmballoon_stats {
	 
	atomic64_t general_stat[VMW_BALLOON_STAT_NUM];

	 
	atomic64_t
	       page_stat[VMW_BALLOON_PAGE_STAT_NUM][VMW_BALLOON_NUM_PAGE_SIZES];

	 
	atomic64_t ops[VMW_BALLOON_CMD_NUM][VMW_BALLOON_OP_STAT_TYPES];
};

static inline bool is_vmballoon_stats_on(void)
{
	return IS_ENABLED(CONFIG_DEBUG_FS) &&
		static_branch_unlikely(&balloon_stat_enabled);
}

static inline void vmballoon_stats_op_inc(struct vmballoon *b, unsigned int op,
					  enum vmballoon_op_stat_type type)
{
	if (is_vmballoon_stats_on())
		atomic64_inc(&b->stats->ops[op][type]);
}

static inline void vmballoon_stats_gen_inc(struct vmballoon *b,
					   enum vmballoon_stat_general stat)
{
	if (is_vmballoon_stats_on())
		atomic64_inc(&b->stats->general_stat[stat]);
}

static inline void vmballoon_stats_gen_add(struct vmballoon *b,
					   enum vmballoon_stat_general stat,
					   unsigned int val)
{
	if (is_vmballoon_stats_on())
		atomic64_add(val, &b->stats->general_stat[stat]);
}

static inline void vmballoon_stats_page_inc(struct vmballoon *b,
					    enum vmballoon_stat_page stat,
					    enum vmballoon_page_size_type size)
{
	if (is_vmballoon_stats_on())
		atomic64_inc(&b->stats->page_stat[stat][size]);
}

static inline void vmballoon_stats_page_add(struct vmballoon *b,
					    enum vmballoon_stat_page stat,
					    enum vmballoon_page_size_type size,
					    unsigned int val)
{
	if (is_vmballoon_stats_on())
		atomic64_add(val, &b->stats->page_stat[stat][size]);
}

static inline unsigned long
__vmballoon_cmd(struct vmballoon *b, unsigned long cmd, unsigned long arg1,
		unsigned long arg2, unsigned long *result)
{
	unsigned long status, dummy1, dummy2, dummy3, local_result;

	vmballoon_stats_op_inc(b, cmd, VMW_BALLOON_OP_STAT);

	asm volatile ("inl %%dx" :
		"=a"(status),
		"=c"(dummy1),
		"=d"(dummy2),
		"=b"(local_result),
		"=S"(dummy3) :
		"0"(VMW_BALLOON_HV_MAGIC),
		"1"(cmd),
		"2"(VMW_BALLOON_HV_PORT),
		"3"(arg1),
		"4"(arg2) :
		"memory");

	 
	if (result)
		*result = (cmd == VMW_BALLOON_CMD_START) ? dummy1 :
							   local_result;

	 
	if (status == VMW_BALLOON_SUCCESS &&
	    ((1ul << cmd) & VMW_BALLOON_CMD_WITH_TARGET_MASK))
		WRITE_ONCE(b->target, local_result);

	if (status != VMW_BALLOON_SUCCESS &&
	    status != VMW_BALLOON_SUCCESS_WITH_CAPABILITIES) {
		vmballoon_stats_op_inc(b, cmd, VMW_BALLOON_OP_FAIL_STAT);
		pr_debug("%s: %s [0x%lx,0x%lx) failed, returned %ld\n",
			 __func__, vmballoon_cmd_names[cmd], arg1, arg2,
			 status);
	}

	 
	if (status == VMW_BALLOON_ERROR_RESET)
		b->reset_required = true;

	return status;
}

static __always_inline unsigned long
vmballoon_cmd(struct vmballoon *b, unsigned long cmd, unsigned long arg1,
	      unsigned long arg2)
{
	unsigned long dummy;

	return __vmballoon_cmd(b, cmd, arg1, arg2, &dummy);
}

 
static int vmballoon_send_start(struct vmballoon *b, unsigned long req_caps)
{
	unsigned long status, capabilities;

	status = __vmballoon_cmd(b, VMW_BALLOON_CMD_START, req_caps, 0,
				 &capabilities);

	switch (status) {
	case VMW_BALLOON_SUCCESS_WITH_CAPABILITIES:
		b->capabilities = capabilities;
		break;
	case VMW_BALLOON_SUCCESS:
		b->capabilities = VMW_BALLOON_BASIC_CMDS;
		break;
	default:
		return -EIO;
	}

	 
	b->max_page_size = VMW_BALLOON_4K_PAGE;
	if ((b->capabilities & VMW_BALLOON_BATCHED_2M_CMDS) &&
	    (b->capabilities & VMW_BALLOON_BATCHED_CMDS))
		b->max_page_size = VMW_BALLOON_2M_PAGE;


	return 0;
}

 
static int vmballoon_send_guest_id(struct vmballoon *b)
{
	unsigned long status;

	status = vmballoon_cmd(b, VMW_BALLOON_CMD_GUEST_ID,
			       VMW_BALLOON_GUEST_ID, 0);

	return status == VMW_BALLOON_SUCCESS ? 0 : -EIO;
}

 
static inline
unsigned int vmballoon_page_order(enum vmballoon_page_size_type page_size)
{
	return page_size == VMW_BALLOON_2M_PAGE ? VMW_BALLOON_2M_ORDER : 0;
}

 
static inline unsigned int
vmballoon_page_in_frames(enum vmballoon_page_size_type page_size)
{
	return 1 << vmballoon_page_order(page_size);
}

 
static void
vmballoon_mark_page_offline(struct page *page,
			    enum vmballoon_page_size_type page_size)
{
	int i;

	for (i = 0; i < vmballoon_page_in_frames(page_size); i++)
		__SetPageOffline(page + i);
}

 
static void
vmballoon_mark_page_online(struct page *page,
			   enum vmballoon_page_size_type page_size)
{
	int i;

	for (i = 0; i < vmballoon_page_in_frames(page_size); i++)
		__ClearPageOffline(page + i);
}

 
static int vmballoon_send_get_target(struct vmballoon *b)
{
	unsigned long status;
	unsigned long limit;

	limit = totalram_pages();

	 
	if (!(b->capabilities & VMW_BALLOON_64_BIT_TARGET) &&
	    limit != (u32)limit)
		return -EINVAL;

	status = vmballoon_cmd(b, VMW_BALLOON_CMD_GET_TARGET, limit, 0);

	return status == VMW_BALLOON_SUCCESS ? 0 : -EIO;
}

 
static int vmballoon_alloc_page_list(struct vmballoon *b,
				     struct vmballoon_ctl *ctl,
				     unsigned int req_n_pages)
{
	struct page *page;
	unsigned int i;

	for (i = 0; i < req_n_pages; i++) {
		 
		if (!list_empty(&ctl->prealloc_pages)) {
			page = list_first_entry(&ctl->prealloc_pages,
						struct page, lru);
			list_del(&page->lru);
		} else {
			if (ctl->page_size == VMW_BALLOON_2M_PAGE)
				page = alloc_pages(__GFP_HIGHMEM|__GFP_NOWARN|
					__GFP_NOMEMALLOC, VMW_BALLOON_2M_ORDER);
			else
				page = balloon_page_alloc();

			vmballoon_stats_page_inc(b, VMW_BALLOON_PAGE_STAT_ALLOC,
						 ctl->page_size);
		}

		if (page) {
			 
			list_add(&page->lru, &ctl->pages);
			continue;
		}

		 
		vmballoon_stats_page_inc(b, VMW_BALLOON_PAGE_STAT_ALLOC_FAIL,
					 ctl->page_size);
		break;
	}

	ctl->n_pages = i;

	return req_n_pages == ctl->n_pages ? 0 : -ENOMEM;
}

 
static int vmballoon_handle_one_result(struct vmballoon *b, struct page *page,
				       enum vmballoon_page_size_type page_size,
				       unsigned long status)
{
	 
	if (likely(status == VMW_BALLOON_SUCCESS))
		return 0;

	pr_debug("%s: failed comm pfn %lx status %lu page_size %s\n", __func__,
		 page_to_pfn(page), status,
		 vmballoon_page_size_names[page_size]);

	 
	vmballoon_stats_page_inc(b, VMW_BALLOON_PAGE_STAT_REFUSED_ALLOC,
				 page_size);

	return -EIO;
}

 
static unsigned long vmballoon_status_page(struct vmballoon *b, int idx,
					   struct page **p)
{
	if (static_branch_likely(&vmw_balloon_batching)) {
		 
		*p = pfn_to_page(b->batch_page[idx].pfn);
		return b->batch_page[idx].status;
	}

	 
	*p = b->page;

	 
	return VMW_BALLOON_SUCCESS;
}

 
static unsigned long vmballoon_lock_op(struct vmballoon *b,
				       unsigned int num_pages,
				       enum vmballoon_page_size_type page_size,
				       enum vmballoon_op op)
{
	unsigned long cmd, pfn;

	lockdep_assert_held(&b->comm_lock);

	if (static_branch_likely(&vmw_balloon_batching)) {
		if (op == VMW_BALLOON_INFLATE)
			cmd = page_size == VMW_BALLOON_2M_PAGE ?
				VMW_BALLOON_CMD_BATCHED_2M_LOCK :
				VMW_BALLOON_CMD_BATCHED_LOCK;
		else
			cmd = page_size == VMW_BALLOON_2M_PAGE ?
				VMW_BALLOON_CMD_BATCHED_2M_UNLOCK :
				VMW_BALLOON_CMD_BATCHED_UNLOCK;

		pfn = PHYS_PFN(virt_to_phys(b->batch_page));
	} else {
		cmd = op == VMW_BALLOON_INFLATE ? VMW_BALLOON_CMD_LOCK :
						  VMW_BALLOON_CMD_UNLOCK;
		pfn = page_to_pfn(b->page);

		 
		if (unlikely(pfn != (u32)pfn))
			return VMW_BALLOON_ERROR_PPN_INVALID;
	}

	return vmballoon_cmd(b, cmd, pfn, num_pages);
}

 
static void vmballoon_add_page(struct vmballoon *b, unsigned int idx,
			       struct page *p)
{
	lockdep_assert_held(&b->comm_lock);

	if (static_branch_likely(&vmw_balloon_batching))
		b->batch_page[idx] = (struct vmballoon_batch_entry)
					{ .pfn = page_to_pfn(p) };
	else
		b->page = p;
}

 
static int vmballoon_lock(struct vmballoon *b, struct vmballoon_ctl *ctl)
{
	unsigned long batch_status;
	struct page *page;
	unsigned int i, num_pages;

	num_pages = ctl->n_pages;
	if (num_pages == 0)
		return 0;

	 
	spin_lock(&b->comm_lock);

	i = 0;
	list_for_each_entry(page, &ctl->pages, lru)
		vmballoon_add_page(b, i++, page);

	batch_status = vmballoon_lock_op(b, ctl->n_pages, ctl->page_size,
					 ctl->op);

	 
	for (i = 0; i < num_pages; i++) {
		unsigned long status;

		status = vmballoon_status_page(b, i, &page);

		 
		if (batch_status != VMW_BALLOON_SUCCESS)
			status = batch_status;

		 
		if (!vmballoon_handle_one_result(b, page, ctl->page_size,
						 status))
			continue;

		 
		list_move(&page->lru, &ctl->refused_pages);
		ctl->n_pages--;
		ctl->n_refused_pages++;
	}

	spin_unlock(&b->comm_lock);

	return batch_status == VMW_BALLOON_SUCCESS ? 0 : -EIO;
}

 
static void vmballoon_release_page_list(struct list_head *page_list,
				       int *n_pages,
				       enum vmballoon_page_size_type page_size)
{
	struct page *page, *tmp;

	list_for_each_entry_safe(page, tmp, page_list, lru) {
		list_del(&page->lru);
		__free_pages(page, vmballoon_page_order(page_size));
	}

	if (n_pages)
		*n_pages = 0;
}


 
static void vmballoon_release_refused_pages(struct vmballoon *b,
					    struct vmballoon_ctl *ctl)
{
	vmballoon_stats_page_inc(b, VMW_BALLOON_PAGE_STAT_REFUSED_FREE,
				 ctl->page_size);

	vmballoon_release_page_list(&ctl->refused_pages, &ctl->n_refused_pages,
				    ctl->page_size);
}

 
static int64_t vmballoon_change(struct vmballoon *b)
{
	int64_t size, target;

	size = atomic64_read(&b->size);
	target = READ_ONCE(b->target);

	 

	if (b->reset_required)
		return 0;

	 
	if (target < size && target != 0 &&
	    size - target < vmballoon_page_in_frames(VMW_BALLOON_2M_PAGE))
		return 0;

	 
	if (target > size && time_before(jiffies, READ_ONCE(b->shrink_timeout)))
		return 0;

	return target - size;
}

 
static void vmballoon_enqueue_page_list(struct vmballoon *b,
					struct list_head *pages,
					unsigned int *n_pages,
					enum vmballoon_page_size_type page_size)
{
	unsigned long flags;
	struct page *page;

	if (page_size == VMW_BALLOON_4K_PAGE) {
		balloon_page_list_enqueue(&b->b_dev_info, pages);
	} else {
		 
		spin_lock_irqsave(&b->b_dev_info.pages_lock, flags);

		list_for_each_entry(page, pages, lru) {
			vmballoon_mark_page_offline(page, VMW_BALLOON_2M_PAGE);
		}

		list_splice_init(pages, &b->huge_pages);
		__count_vm_events(BALLOON_INFLATE, *n_pages *
				  vmballoon_page_in_frames(VMW_BALLOON_2M_PAGE));
		spin_unlock_irqrestore(&b->b_dev_info.pages_lock, flags);
	}

	*n_pages = 0;
}

 
static void vmballoon_dequeue_page_list(struct vmballoon *b,
					struct list_head *pages,
					unsigned int *n_pages,
					enum vmballoon_page_size_type page_size,
					unsigned int n_req_pages)
{
	struct page *page, *tmp;
	unsigned int i = 0;
	unsigned long flags;

	 
	if (page_size == VMW_BALLOON_4K_PAGE) {
		*n_pages = balloon_page_list_dequeue(&b->b_dev_info, pages,
						     n_req_pages);
		return;
	}

	 
	spin_lock_irqsave(&b->b_dev_info.pages_lock, flags);
	list_for_each_entry_safe(page, tmp, &b->huge_pages, lru) {
		vmballoon_mark_page_online(page, VMW_BALLOON_2M_PAGE);

		list_move(&page->lru, pages);
		if (++i == n_req_pages)
			break;
	}

	__count_vm_events(BALLOON_DEFLATE,
			  i * vmballoon_page_in_frames(VMW_BALLOON_2M_PAGE));
	spin_unlock_irqrestore(&b->b_dev_info.pages_lock, flags);
	*n_pages = i;
}

 
static void vmballoon_split_refused_pages(struct vmballoon_ctl *ctl)
{
	struct page *page, *tmp;
	unsigned int i, order;

	order = vmballoon_page_order(ctl->page_size);

	list_for_each_entry_safe(page, tmp, &ctl->refused_pages, lru) {
		list_del(&page->lru);
		split_page(page, order);
		for (i = 0; i < (1 << order); i++)
			list_add(&page[i].lru, &ctl->prealloc_pages);
	}
	ctl->n_refused_pages = 0;
}

 
static void vmballoon_inflate(struct vmballoon *b)
{
	int64_t to_inflate_frames;
	struct vmballoon_ctl ctl = {
		.pages = LIST_HEAD_INIT(ctl.pages),
		.refused_pages = LIST_HEAD_INIT(ctl.refused_pages),
		.prealloc_pages = LIST_HEAD_INIT(ctl.prealloc_pages),
		.page_size = b->max_page_size,
		.op = VMW_BALLOON_INFLATE
	};

	while ((to_inflate_frames = vmballoon_change(b)) > 0) {
		unsigned int to_inflate_pages, page_in_frames;
		int alloc_error, lock_error = 0;

		VM_BUG_ON(!list_empty(&ctl.pages));
		VM_BUG_ON(ctl.n_pages != 0);

		page_in_frames = vmballoon_page_in_frames(ctl.page_size);

		to_inflate_pages = min_t(unsigned long, b->batch_max_pages,
					 DIV_ROUND_UP_ULL(to_inflate_frames,
							  page_in_frames));

		 
		alloc_error = vmballoon_alloc_page_list(b, &ctl,
							to_inflate_pages);

		 
		lock_error = vmballoon_lock(b, &ctl);

		 
		if (lock_error)
			break;

		 
		atomic64_add(ctl.n_pages * page_in_frames, &b->size);

		vmballoon_enqueue_page_list(b, &ctl.pages, &ctl.n_pages,
					    ctl.page_size);

		 
		if (alloc_error ||
		    ctl.n_refused_pages >= VMW_BALLOON_MAX_REFUSED) {
			if (ctl.page_size == VMW_BALLOON_4K_PAGE)
				break;

			 
			vmballoon_split_refused_pages(&ctl);
			ctl.page_size--;
		}

		cond_resched();
	}

	 
	if (ctl.n_refused_pages != 0)
		vmballoon_release_refused_pages(b, &ctl);

	vmballoon_release_page_list(&ctl.prealloc_pages, NULL, ctl.page_size);
}

 
static unsigned long vmballoon_deflate(struct vmballoon *b, uint64_t n_frames,
				       bool coordinated)
{
	unsigned long deflated_frames = 0;
	unsigned long tried_frames = 0;
	struct vmballoon_ctl ctl = {
		.pages = LIST_HEAD_INIT(ctl.pages),
		.refused_pages = LIST_HEAD_INIT(ctl.refused_pages),
		.page_size = VMW_BALLOON_4K_PAGE,
		.op = VMW_BALLOON_DEFLATE
	};

	 
	while (true) {
		unsigned int to_deflate_pages, n_unlocked_frames;
		unsigned int page_in_frames;
		int64_t to_deflate_frames;
		bool deflated_all;

		page_in_frames = vmballoon_page_in_frames(ctl.page_size);

		VM_BUG_ON(!list_empty(&ctl.pages));
		VM_BUG_ON(ctl.n_pages);
		VM_BUG_ON(!list_empty(&ctl.refused_pages));
		VM_BUG_ON(ctl.n_refused_pages);

		 
		to_deflate_frames = n_frames ? n_frames - tried_frames :
					       -vmballoon_change(b);

		 
		if (to_deflate_frames <= 0)
			break;

		 
		to_deflate_pages = min_t(unsigned long, b->batch_max_pages,
					 DIV_ROUND_UP_ULL(to_deflate_frames,
							  page_in_frames));

		 
		vmballoon_dequeue_page_list(b, &ctl.pages, &ctl.n_pages,
					    ctl.page_size, to_deflate_pages);

		 
		tried_frames += ctl.n_pages * page_in_frames;

		 
		if (coordinated)
			vmballoon_lock(b, &ctl);

		 
		deflated_all = (ctl.n_pages == to_deflate_pages);

		 
		n_unlocked_frames = ctl.n_pages * page_in_frames;
		atomic64_sub(n_unlocked_frames, &b->size);
		deflated_frames += n_unlocked_frames;

		vmballoon_stats_page_add(b, VMW_BALLOON_PAGE_STAT_FREE,
					 ctl.page_size, ctl.n_pages);

		 
		vmballoon_release_page_list(&ctl.pages, &ctl.n_pages,
					    ctl.page_size);

		 
		vmballoon_enqueue_page_list(b, &ctl.refused_pages,
					    &ctl.n_refused_pages,
					    ctl.page_size);

		 
		if (!deflated_all) {
			if (ctl.page_size == b->max_page_size)
				break;
			ctl.page_size++;
		}

		cond_resched();
	}

	return deflated_frames;
}

 
static void vmballoon_deinit_batching(struct vmballoon *b)
{
	free_page((unsigned long)b->batch_page);
	b->batch_page = NULL;
	static_branch_disable(&vmw_balloon_batching);
	b->batch_max_pages = 1;
}

 
static int vmballoon_init_batching(struct vmballoon *b)
{
	struct page *page;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page)
		return -ENOMEM;

	b->batch_page = page_address(page);
	b->batch_max_pages = PAGE_SIZE / sizeof(struct vmballoon_batch_entry);

	static_branch_enable(&vmw_balloon_batching);

	return 0;
}

 
static void vmballoon_doorbell(void *client_data)
{
	struct vmballoon *b = client_data;

	vmballoon_stats_gen_inc(b, VMW_BALLOON_STAT_DOORBELL);

	mod_delayed_work(system_freezable_wq, &b->dwork, 0);
}

 
static void vmballoon_vmci_cleanup(struct vmballoon *b)
{
	vmballoon_cmd(b, VMW_BALLOON_CMD_VMCI_DOORBELL_SET,
		      VMCI_INVALID_ID, VMCI_INVALID_ID);

	if (!vmci_handle_is_invalid(b->vmci_doorbell)) {
		vmci_doorbell_destroy(b->vmci_doorbell);
		b->vmci_doorbell = VMCI_INVALID_HANDLE;
	}
}

 
static int vmballoon_vmci_init(struct vmballoon *b)
{
	unsigned long error;

	if ((b->capabilities & VMW_BALLOON_SIGNALLED_WAKEUP_CMD) == 0)
		return 0;

	error = vmci_doorbell_create(&b->vmci_doorbell, VMCI_FLAG_DELAYED_CB,
				     VMCI_PRIVILEGE_FLAG_RESTRICTED,
				     vmballoon_doorbell, b);

	if (error != VMCI_SUCCESS)
		goto fail;

	error =	__vmballoon_cmd(b, VMW_BALLOON_CMD_VMCI_DOORBELL_SET,
				b->vmci_doorbell.context,
				b->vmci_doorbell.resource, NULL);

	if (error != VMW_BALLOON_SUCCESS)
		goto fail;

	return 0;
fail:
	vmballoon_vmci_cleanup(b);
	return -EIO;
}

 
static void vmballoon_pop(struct vmballoon *b)
{
	unsigned long size;

	while ((size = atomic64_read(&b->size)))
		vmballoon_deflate(b, size, false);
}

 
static void vmballoon_reset(struct vmballoon *b)
{
	int error;

	down_write(&b->conf_sem);

	vmballoon_vmci_cleanup(b);

	 
	vmballoon_pop(b);

	if (vmballoon_send_start(b, VMW_BALLOON_CAPABILITIES))
		goto unlock;

	if ((b->capabilities & VMW_BALLOON_BATCHED_CMDS) != 0) {
		if (vmballoon_init_batching(b)) {
			 
			vmballoon_send_start(b, 0);
			goto unlock;
		}
	} else if ((b->capabilities & VMW_BALLOON_BASIC_CMDS) != 0) {
		vmballoon_deinit_batching(b);
	}

	vmballoon_stats_gen_inc(b, VMW_BALLOON_STAT_RESET);
	b->reset_required = false;

	error = vmballoon_vmci_init(b);
	if (error)
		pr_err_once("failed to initialize vmci doorbell\n");

	if (vmballoon_send_guest_id(b))
		pr_err_once("failed to send guest ID to the host\n");

unlock:
	up_write(&b->conf_sem);
}

 
static void vmballoon_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vmballoon *b = container_of(dwork, struct vmballoon, dwork);
	int64_t change = 0;

	if (b->reset_required)
		vmballoon_reset(b);

	down_read(&b->conf_sem);

	 
	vmballoon_stats_gen_inc(b, VMW_BALLOON_STAT_TIMER);

	if (!vmballoon_send_get_target(b))
		change = vmballoon_change(b);

	if (change != 0) {
		pr_debug("%s - size: %llu, target %lu\n", __func__,
			 atomic64_read(&b->size), READ_ONCE(b->target));

		if (change > 0)
			vmballoon_inflate(b);
		else   
			vmballoon_deflate(b, 0, true);
	}

	up_read(&b->conf_sem);

	 
	queue_delayed_work(system_freezable_wq,
			   dwork, round_jiffies_relative(HZ));

}

 
static unsigned long vmballoon_shrinker_scan(struct shrinker *shrinker,
					     struct shrink_control *sc)
{
	struct vmballoon *b = &balloon;
	unsigned long deflated_frames;

	pr_debug("%s - size: %llu", __func__, atomic64_read(&b->size));

	vmballoon_stats_gen_inc(b, VMW_BALLOON_STAT_SHRINK);

	 
	if (!down_read_trylock(&b->conf_sem))
		return 0;

	deflated_frames = vmballoon_deflate(b, sc->nr_to_scan, true);

	vmballoon_stats_gen_add(b, VMW_BALLOON_STAT_SHRINK_FREE,
				deflated_frames);

	 
	WRITE_ONCE(b->shrink_timeout, jiffies + HZ * VMBALLOON_SHRINK_DELAY);

	up_read(&b->conf_sem);

	return deflated_frames;
}

 
static unsigned long vmballoon_shrinker_count(struct shrinker *shrinker,
					      struct shrink_control *sc)
{
	struct vmballoon *b = &balloon;

	return atomic64_read(&b->size);
}

static void vmballoon_unregister_shrinker(struct vmballoon *b)
{
	if (b->shrinker_registered)
		unregister_shrinker(&b->shrinker);
	b->shrinker_registered = false;
}

static int vmballoon_register_shrinker(struct vmballoon *b)
{
	int r;

	 
	if (!vmwballoon_shrinker_enable)
		return 0;

	b->shrinker.scan_objects = vmballoon_shrinker_scan;
	b->shrinker.count_objects = vmballoon_shrinker_count;
	b->shrinker.seeks = DEFAULT_SEEKS;

	r = register_shrinker(&b->shrinker, "vmw-balloon");

	if (r == 0)
		b->shrinker_registered = true;

	return r;
}

 
#ifdef CONFIG_DEBUG_FS

static const char * const vmballoon_stat_page_names[] = {
	[VMW_BALLOON_PAGE_STAT_ALLOC]		= "alloc",
	[VMW_BALLOON_PAGE_STAT_ALLOC_FAIL]	= "allocFail",
	[VMW_BALLOON_PAGE_STAT_REFUSED_ALLOC]	= "errAlloc",
	[VMW_BALLOON_PAGE_STAT_REFUSED_FREE]	= "errFree",
	[VMW_BALLOON_PAGE_STAT_FREE]		= "free"
};

static const char * const vmballoon_stat_names[] = {
	[VMW_BALLOON_STAT_TIMER]		= "timer",
	[VMW_BALLOON_STAT_DOORBELL]		= "doorbell",
	[VMW_BALLOON_STAT_RESET]		= "reset",
	[VMW_BALLOON_STAT_SHRINK]		= "shrink",
	[VMW_BALLOON_STAT_SHRINK_FREE]		= "shrinkFree"
};

static int vmballoon_enable_stats(struct vmballoon *b)
{
	int r = 0;

	down_write(&b->conf_sem);

	 
	if (b->stats)
		goto out;

	b->stats = kzalloc(sizeof(*b->stats), GFP_KERNEL);

	if (!b->stats) {
		 
		r = -ENOMEM;
		goto out;
	}
	static_key_enable(&balloon_stat_enabled.key);
out:
	up_write(&b->conf_sem);
	return r;
}

 
static int vmballoon_debug_show(struct seq_file *f, void *offset)
{
	struct vmballoon *b = f->private;
	int i, j;

	 
	if (!b->stats) {
		int r = vmballoon_enable_stats(b);

		if (r)
			return r;
	}

	 
	seq_printf(f, "%-22s: %#16x\n", "balloon capabilities",
		   VMW_BALLOON_CAPABILITIES);
	seq_printf(f, "%-22s: %#16lx\n", "used capabilities", b->capabilities);
	seq_printf(f, "%-22s: %16s\n", "is resetting",
		   b->reset_required ? "y" : "n");

	 
	seq_printf(f, "%-22s: %16lu\n", "target", READ_ONCE(b->target));
	seq_printf(f, "%-22s: %16llu\n", "current", atomic64_read(&b->size));

	for (i = 0; i < VMW_BALLOON_CMD_NUM; i++) {
		if (vmballoon_cmd_names[i] == NULL)
			continue;

		seq_printf(f, "%-22s: %16llu (%llu failed)\n",
			   vmballoon_cmd_names[i],
			   atomic64_read(&b->stats->ops[i][VMW_BALLOON_OP_STAT]),
			   atomic64_read(&b->stats->ops[i][VMW_BALLOON_OP_FAIL_STAT]));
	}

	for (i = 0; i < VMW_BALLOON_STAT_NUM; i++)
		seq_printf(f, "%-22s: %16llu\n",
			   vmballoon_stat_names[i],
			   atomic64_read(&b->stats->general_stat[i]));

	for (i = 0; i < VMW_BALLOON_PAGE_STAT_NUM; i++) {
		for (j = 0; j < VMW_BALLOON_NUM_PAGE_SIZES; j++)
			seq_printf(f, "%-18s(%s): %16llu\n",
				   vmballoon_stat_page_names[i],
				   vmballoon_page_size_names[j],
				   atomic64_read(&b->stats->page_stat[i][j]));
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(vmballoon_debug);

static void __init vmballoon_debugfs_init(struct vmballoon *b)
{
	debugfs_create_file("vmmemctl", S_IRUGO, NULL, b,
			    &vmballoon_debug_fops);
}

static void __exit vmballoon_debugfs_exit(struct vmballoon *b)
{
	static_key_disable(&balloon_stat_enabled.key);
	debugfs_lookup_and_remove("vmmemctl", NULL);
	kfree(b->stats);
	b->stats = NULL;
}

#else

static inline void vmballoon_debugfs_init(struct vmballoon *b)
{
}

static inline void vmballoon_debugfs_exit(struct vmballoon *b)
{
}

#endif	 


#ifdef CONFIG_BALLOON_COMPACTION
 
static int vmballoon_migratepage(struct balloon_dev_info *b_dev_info,
				 struct page *newpage, struct page *page,
				 enum migrate_mode mode)
{
	unsigned long status, flags;
	struct vmballoon *b;
	int ret;

	b = container_of(b_dev_info, struct vmballoon, b_dev_info);

	 
	if (!down_read_trylock(&b->conf_sem))
		return -EAGAIN;

	spin_lock(&b->comm_lock);
	 
	vmballoon_add_page(b, 0, page);
	status = vmballoon_lock_op(b, 1, VMW_BALLOON_4K_PAGE,
				   VMW_BALLOON_DEFLATE);

	if (status == VMW_BALLOON_SUCCESS)
		status = vmballoon_status_page(b, 0, &page);

	 
	if (status != VMW_BALLOON_SUCCESS) {
		spin_unlock(&b->comm_lock);
		ret = -EBUSY;
		goto out_unlock;
	}

	 
	balloon_page_delete(page);

	put_page(page);

	 
	vmballoon_add_page(b, 0, newpage);
	status = vmballoon_lock_op(b, 1, VMW_BALLOON_4K_PAGE,
				   VMW_BALLOON_INFLATE);

	if (status == VMW_BALLOON_SUCCESS)
		status = vmballoon_status_page(b, 0, &newpage);

	spin_unlock(&b->comm_lock);

	if (status != VMW_BALLOON_SUCCESS) {
		 
		atomic64_dec(&b->size);
		ret = -EBUSY;
	} else {
		 
		get_page(newpage);
		ret = MIGRATEPAGE_SUCCESS;
	}

	 
	spin_lock_irqsave(&b->b_dev_info.pages_lock, flags);

	 
	if (ret == MIGRATEPAGE_SUCCESS) {
		balloon_page_insert(&b->b_dev_info, newpage);
		__count_vm_event(BALLOON_MIGRATE);
	}

	 
	b->b_dev_info.isolated_pages--;
	spin_unlock_irqrestore(&b->b_dev_info.pages_lock, flags);

out_unlock:
	up_read(&b->conf_sem);
	return ret;
}

 
static __init void vmballoon_compaction_init(struct vmballoon *b)
{
	b->b_dev_info.migratepage = vmballoon_migratepage;
}

#else  
static inline void vmballoon_compaction_init(struct vmballoon *b)
{
}
#endif  

static int __init vmballoon_init(void)
{
	int error;

	 
	if (x86_hyper_type != X86_HYPER_VMWARE)
		return -ENODEV;

	INIT_DELAYED_WORK(&balloon.dwork, vmballoon_work);

	error = vmballoon_register_shrinker(&balloon);
	if (error)
		goto fail;

	 
	balloon_devinfo_init(&balloon.b_dev_info);
	vmballoon_compaction_init(&balloon);

	INIT_LIST_HEAD(&balloon.huge_pages);
	spin_lock_init(&balloon.comm_lock);
	init_rwsem(&balloon.conf_sem);
	balloon.vmci_doorbell = VMCI_INVALID_HANDLE;
	balloon.batch_page = NULL;
	balloon.page = NULL;
	balloon.reset_required = true;

	queue_delayed_work(system_freezable_wq, &balloon.dwork, 0);

	vmballoon_debugfs_init(&balloon);

	return 0;
fail:
	vmballoon_unregister_shrinker(&balloon);
	return error;
}

 
late_initcall(vmballoon_init);

static void __exit vmballoon_exit(void)
{
	vmballoon_unregister_shrinker(&balloon);
	vmballoon_vmci_cleanup(&balloon);
	cancel_delayed_work_sync(&balloon.dwork);

	vmballoon_debugfs_exit(&balloon);

	 
	vmballoon_send_start(&balloon, 0);
	vmballoon_pop(&balloon);
}
module_exit(vmballoon_exit);
