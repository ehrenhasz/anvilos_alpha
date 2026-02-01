
 

#include <linux/virtio.h>
#include <linux/virtio_mem.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/memory_hotplug.h>
#include <linux/memory.h>
#include <linux/hrtimer.h>
#include <linux/crash_dump.h>
#include <linux/mutex.h>
#include <linux/bitmap.h>
#include <linux/lockdep.h>
#include <linux/log2.h>

#include <acpi/acpi_numa.h>

static bool unplug_online = true;
module_param(unplug_online, bool, 0644);
MODULE_PARM_DESC(unplug_online, "Try to unplug online memory");

static bool force_bbm;
module_param(force_bbm, bool, 0444);
MODULE_PARM_DESC(force_bbm,
		"Force Big Block Mode. Default is 0 (auto-selection)");

static unsigned long bbm_block_size;
module_param(bbm_block_size, ulong, 0444);
MODULE_PARM_DESC(bbm_block_size,
		 "Big Block size in bytes. Default is 0 (auto-detection).");

 

 
enum virtio_mem_sbm_mb_state {
	 
	VIRTIO_MEM_SBM_MB_UNUSED = 0,
	 
	VIRTIO_MEM_SBM_MB_PLUGGED,
	 
	VIRTIO_MEM_SBM_MB_OFFLINE,
	 
	VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL,
	 
	VIRTIO_MEM_SBM_MB_KERNEL,
	 
	VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL,
	 
	VIRTIO_MEM_SBM_MB_MOVABLE,
	 
	VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL,
	VIRTIO_MEM_SBM_MB_COUNT
};

 
enum virtio_mem_bbm_bb_state {
	 
	VIRTIO_MEM_BBM_BB_UNUSED = 0,
	 
	VIRTIO_MEM_BBM_BB_PLUGGED,
	 
	VIRTIO_MEM_BBM_BB_ADDED,
	 
	VIRTIO_MEM_BBM_BB_FAKE_OFFLINE,
	VIRTIO_MEM_BBM_BB_COUNT
};

struct virtio_mem {
	struct virtio_device *vdev;

	 
	bool unplug_all_required;

	 
	struct work_struct wq;
	atomic_t wq_active;
	atomic_t config_changed;

	 
	struct virtqueue *vq;

	 
	wait_queue_head_t host_resp;

	 
	struct virtio_mem_req req;
	struct virtio_mem_resp resp;

	 
	uint64_t plugged_size;
	 
	uint64_t requested_size;

	 
	uint64_t device_block_size;
	 
	int nid;
	 
	uint64_t addr;
	 
	uint64_t region_size;

	 
	struct resource *parent_resource;
	 
	const char *resource_name;
	 
	int mgid;

	 
#define VIRTIO_MEM_DEFAULT_OFFLINE_THRESHOLD		(1024 * 1024 * 1024)
	atomic64_t offline_size;
	uint64_t offline_threshold;

	 
	bool in_sbm;

	union {
		struct {
			 
			unsigned long first_mb_id;
			 
			unsigned long last_usable_mb_id;
			 
			unsigned long next_mb_id;

			 
			uint64_t sb_size;
			 
			uint32_t sbs_per_mb;

			 
			bool have_unplugged_mb;

			 
			unsigned long mb_count[VIRTIO_MEM_SBM_MB_COUNT];

			 
			uint8_t *mb_states;

			 
			unsigned long *sb_states;
		} sbm;

		struct {
			 
			unsigned long first_bb_id;
			 
			unsigned long last_usable_bb_id;
			 
			unsigned long next_bb_id;

			 
			unsigned long bb_count[VIRTIO_MEM_BBM_BB_COUNT];

			 
			uint8_t *bb_states;

			 
			uint64_t bb_size;
		} bbm;
	};

	 
	struct mutex hotplug_mutex;
	bool hotplug_active;

	 
	bool broken;

	 
	bool in_kdump;

	 
	spinlock_t removal_lock;
	bool removing;

	 
	struct hrtimer retry_timer;
	unsigned int retry_timer_ms;
#define VIRTIO_MEM_RETRY_TIMER_MIN_MS		50000
#define VIRTIO_MEM_RETRY_TIMER_MAX_MS		300000

	 
	struct notifier_block memory_notifier;

#ifdef CONFIG_PROC_VMCORE
	 
	struct vmcore_cb vmcore_cb;
	uint64_t last_block_addr;
	bool last_block_plugged;
#endif  

	 
	struct list_head next;
};

 
static DEFINE_MUTEX(virtio_mem_mutex);
static LIST_HEAD(virtio_mem_devices);

static void virtio_mem_online_page_cb(struct page *page, unsigned int order);
static void virtio_mem_fake_offline_going_offline(unsigned long pfn,
						  unsigned long nr_pages);
static void virtio_mem_fake_offline_cancel_offline(unsigned long pfn,
						   unsigned long nr_pages);
static void virtio_mem_retry(struct virtio_mem *vm);
static int virtio_mem_create_resource(struct virtio_mem *vm);
static void virtio_mem_delete_resource(struct virtio_mem *vm);

 
static int register_virtio_mem_device(struct virtio_mem *vm)
{
	int rc = 0;

	 
	mutex_lock(&virtio_mem_mutex);
	if (list_empty(&virtio_mem_devices))
		rc = set_online_page_callback(&virtio_mem_online_page_cb);
	if (!rc)
		list_add_rcu(&vm->next, &virtio_mem_devices);
	mutex_unlock(&virtio_mem_mutex);

	return rc;
}

 
static void unregister_virtio_mem_device(struct virtio_mem *vm)
{
	 
	mutex_lock(&virtio_mem_mutex);
	list_del_rcu(&vm->next);
	if (list_empty(&virtio_mem_devices))
		restore_online_page_callback(&virtio_mem_online_page_cb);
	mutex_unlock(&virtio_mem_mutex);

	synchronize_rcu();
}

 
static unsigned long virtio_mem_phys_to_mb_id(unsigned long addr)
{
	return addr / memory_block_size_bytes();
}

 
static unsigned long virtio_mem_mb_id_to_phys(unsigned long mb_id)
{
	return mb_id * memory_block_size_bytes();
}

 
static unsigned long virtio_mem_phys_to_bb_id(struct virtio_mem *vm,
					      uint64_t addr)
{
	return addr / vm->bbm.bb_size;
}

 
static uint64_t virtio_mem_bb_id_to_phys(struct virtio_mem *vm,
					 unsigned long bb_id)
{
	return bb_id * vm->bbm.bb_size;
}

 
static unsigned long virtio_mem_phys_to_sb_id(struct virtio_mem *vm,
					      unsigned long addr)
{
	const unsigned long mb_id = virtio_mem_phys_to_mb_id(addr);
	const unsigned long mb_addr = virtio_mem_mb_id_to_phys(mb_id);

	return (addr - mb_addr) / vm->sbm.sb_size;
}

 
static void virtio_mem_bbm_set_bb_state(struct virtio_mem *vm,
					unsigned long bb_id,
					enum virtio_mem_bbm_bb_state state)
{
	const unsigned long idx = bb_id - vm->bbm.first_bb_id;
	enum virtio_mem_bbm_bb_state old_state;

	old_state = vm->bbm.bb_states[idx];
	vm->bbm.bb_states[idx] = state;

	BUG_ON(vm->bbm.bb_count[old_state] == 0);
	vm->bbm.bb_count[old_state]--;
	vm->bbm.bb_count[state]++;
}

 
static enum virtio_mem_bbm_bb_state virtio_mem_bbm_get_bb_state(struct virtio_mem *vm,
								unsigned long bb_id)
{
	return vm->bbm.bb_states[bb_id - vm->bbm.first_bb_id];
}

 
static int virtio_mem_bbm_bb_states_prepare_next_bb(struct virtio_mem *vm)
{
	unsigned long old_bytes = vm->bbm.next_bb_id - vm->bbm.first_bb_id;
	unsigned long new_bytes = old_bytes + 1;
	int old_pages = PFN_UP(old_bytes);
	int new_pages = PFN_UP(new_bytes);
	uint8_t *new_array;

	if (vm->bbm.bb_states && old_pages == new_pages)
		return 0;

	new_array = vzalloc(new_pages * PAGE_SIZE);
	if (!new_array)
		return -ENOMEM;

	mutex_lock(&vm->hotplug_mutex);
	if (vm->bbm.bb_states)
		memcpy(new_array, vm->bbm.bb_states, old_pages * PAGE_SIZE);
	vfree(vm->bbm.bb_states);
	vm->bbm.bb_states = new_array;
	mutex_unlock(&vm->hotplug_mutex);

	return 0;
}

#define virtio_mem_bbm_for_each_bb(_vm, _bb_id, _state) \
	for (_bb_id = vm->bbm.first_bb_id; \
	     _bb_id < vm->bbm.next_bb_id && _vm->bbm.bb_count[_state]; \
	     _bb_id++) \
		if (virtio_mem_bbm_get_bb_state(_vm, _bb_id) == _state)

#define virtio_mem_bbm_for_each_bb_rev(_vm, _bb_id, _state) \
	for (_bb_id = vm->bbm.next_bb_id - 1; \
	     _bb_id >= vm->bbm.first_bb_id && _vm->bbm.bb_count[_state]; \
	     _bb_id--) \
		if (virtio_mem_bbm_get_bb_state(_vm, _bb_id) == _state)

 
static void virtio_mem_sbm_set_mb_state(struct virtio_mem *vm,
					unsigned long mb_id, uint8_t state)
{
	const unsigned long idx = mb_id - vm->sbm.first_mb_id;
	uint8_t old_state;

	old_state = vm->sbm.mb_states[idx];
	vm->sbm.mb_states[idx] = state;

	BUG_ON(vm->sbm.mb_count[old_state] == 0);
	vm->sbm.mb_count[old_state]--;
	vm->sbm.mb_count[state]++;
}

 
static uint8_t virtio_mem_sbm_get_mb_state(struct virtio_mem *vm,
					   unsigned long mb_id)
{
	const unsigned long idx = mb_id - vm->sbm.first_mb_id;

	return vm->sbm.mb_states[idx];
}

 
static int virtio_mem_sbm_mb_states_prepare_next_mb(struct virtio_mem *vm)
{
	int old_pages = PFN_UP(vm->sbm.next_mb_id - vm->sbm.first_mb_id);
	int new_pages = PFN_UP(vm->sbm.next_mb_id - vm->sbm.first_mb_id + 1);
	uint8_t *new_array;

	if (vm->sbm.mb_states && old_pages == new_pages)
		return 0;

	new_array = vzalloc(new_pages * PAGE_SIZE);
	if (!new_array)
		return -ENOMEM;

	mutex_lock(&vm->hotplug_mutex);
	if (vm->sbm.mb_states)
		memcpy(new_array, vm->sbm.mb_states, old_pages * PAGE_SIZE);
	vfree(vm->sbm.mb_states);
	vm->sbm.mb_states = new_array;
	mutex_unlock(&vm->hotplug_mutex);

	return 0;
}

#define virtio_mem_sbm_for_each_mb(_vm, _mb_id, _state) \
	for (_mb_id = _vm->sbm.first_mb_id; \
	     _mb_id < _vm->sbm.next_mb_id && _vm->sbm.mb_count[_state]; \
	     _mb_id++) \
		if (virtio_mem_sbm_get_mb_state(_vm, _mb_id) == _state)

#define virtio_mem_sbm_for_each_mb_rev(_vm, _mb_id, _state) \
	for (_mb_id = _vm->sbm.next_mb_id - 1; \
	     _mb_id >= _vm->sbm.first_mb_id && _vm->sbm.mb_count[_state]; \
	     _mb_id--) \
		if (virtio_mem_sbm_get_mb_state(_vm, _mb_id) == _state)

 
static int virtio_mem_sbm_sb_state_bit_nr(struct virtio_mem *vm,
					  unsigned long mb_id, int sb_id)
{
	return (mb_id - vm->sbm.first_mb_id) * vm->sbm.sbs_per_mb + sb_id;
}

 
static void virtio_mem_sbm_set_sb_plugged(struct virtio_mem *vm,
					  unsigned long mb_id, int sb_id,
					  int count)
{
	const int bit = virtio_mem_sbm_sb_state_bit_nr(vm, mb_id, sb_id);

	__bitmap_set(vm->sbm.sb_states, bit, count);
}

 
static void virtio_mem_sbm_set_sb_unplugged(struct virtio_mem *vm,
					    unsigned long mb_id, int sb_id,
					    int count)
{
	const int bit = virtio_mem_sbm_sb_state_bit_nr(vm, mb_id, sb_id);

	__bitmap_clear(vm->sbm.sb_states, bit, count);
}

 
static bool virtio_mem_sbm_test_sb_plugged(struct virtio_mem *vm,
					   unsigned long mb_id, int sb_id,
					   int count)
{
	const int bit = virtio_mem_sbm_sb_state_bit_nr(vm, mb_id, sb_id);

	if (count == 1)
		return test_bit(bit, vm->sbm.sb_states);

	 
	return find_next_zero_bit(vm->sbm.sb_states, bit + count, bit) >=
	       bit + count;
}

 
static bool virtio_mem_sbm_test_sb_unplugged(struct virtio_mem *vm,
					     unsigned long mb_id, int sb_id,
					     int count)
{
	const int bit = virtio_mem_sbm_sb_state_bit_nr(vm, mb_id, sb_id);

	 
	return find_next_bit(vm->sbm.sb_states, bit + count, bit) >=
	       bit + count;
}

 
static int virtio_mem_sbm_first_unplugged_sb(struct virtio_mem *vm,
					    unsigned long mb_id)
{
	const int bit = virtio_mem_sbm_sb_state_bit_nr(vm, mb_id, 0);

	return find_next_zero_bit(vm->sbm.sb_states,
				  bit + vm->sbm.sbs_per_mb, bit) - bit;
}

 
static int virtio_mem_sbm_sb_states_prepare_next_mb(struct virtio_mem *vm)
{
	const unsigned long old_nb_mb = vm->sbm.next_mb_id - vm->sbm.first_mb_id;
	const unsigned long old_nb_bits = old_nb_mb * vm->sbm.sbs_per_mb;
	const unsigned long new_nb_bits = (old_nb_mb + 1) * vm->sbm.sbs_per_mb;
	int old_pages = PFN_UP(BITS_TO_LONGS(old_nb_bits) * sizeof(long));
	int new_pages = PFN_UP(BITS_TO_LONGS(new_nb_bits) * sizeof(long));
	unsigned long *new_bitmap, *old_bitmap;

	if (vm->sbm.sb_states && old_pages == new_pages)
		return 0;

	new_bitmap = vzalloc(new_pages * PAGE_SIZE);
	if (!new_bitmap)
		return -ENOMEM;

	mutex_lock(&vm->hotplug_mutex);
	if (vm->sbm.sb_states)
		memcpy(new_bitmap, vm->sbm.sb_states, old_pages * PAGE_SIZE);

	old_bitmap = vm->sbm.sb_states;
	vm->sbm.sb_states = new_bitmap;
	mutex_unlock(&vm->hotplug_mutex);

	vfree(old_bitmap);
	return 0;
}

 
static bool virtio_mem_could_add_memory(struct virtio_mem *vm, uint64_t size)
{
	if (WARN_ON_ONCE(size > vm->offline_threshold))
		return false;

	return atomic64_read(&vm->offline_size) + size <= vm->offline_threshold;
}

 
static int virtio_mem_add_memory(struct virtio_mem *vm, uint64_t addr,
				 uint64_t size)
{
	int rc;

	 
	if (!vm->resource_name) {
		vm->resource_name = kstrdup_const("System RAM (virtio_mem)",
						  GFP_KERNEL);
		if (!vm->resource_name)
			return -ENOMEM;
	}

	dev_dbg(&vm->vdev->dev, "adding memory: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);
	 
	atomic64_add(size, &vm->offline_size);
	rc = add_memory_driver_managed(vm->mgid, addr, size, vm->resource_name,
				       MHP_MERGE_RESOURCE | MHP_NID_IS_MGID);
	if (rc) {
		atomic64_sub(size, &vm->offline_size);
		dev_warn(&vm->vdev->dev, "adding memory failed: %d\n", rc);
		 
	}
	return rc;
}

 
static int virtio_mem_sbm_add_mb(struct virtio_mem *vm, unsigned long mb_id)
{
	const uint64_t addr = virtio_mem_mb_id_to_phys(mb_id);
	const uint64_t size = memory_block_size_bytes();

	return virtio_mem_add_memory(vm, addr, size);
}

 
static int virtio_mem_bbm_add_bb(struct virtio_mem *vm, unsigned long bb_id)
{
	const uint64_t addr = virtio_mem_bb_id_to_phys(vm, bb_id);
	const uint64_t size = vm->bbm.bb_size;

	return virtio_mem_add_memory(vm, addr, size);
}

 
static int virtio_mem_remove_memory(struct virtio_mem *vm, uint64_t addr,
				    uint64_t size)
{
	int rc;

	dev_dbg(&vm->vdev->dev, "removing memory: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);
	rc = remove_memory(addr, size);
	if (!rc) {
		atomic64_sub(size, &vm->offline_size);
		 
		virtio_mem_retry(vm);
	} else {
		dev_dbg(&vm->vdev->dev, "removing memory failed: %d\n", rc);
	}
	return rc;
}

 
static int virtio_mem_sbm_remove_mb(struct virtio_mem *vm, unsigned long mb_id)
{
	const uint64_t addr = virtio_mem_mb_id_to_phys(mb_id);
	const uint64_t size = memory_block_size_bytes();

	return virtio_mem_remove_memory(vm, addr, size);
}

 
static int virtio_mem_offline_and_remove_memory(struct virtio_mem *vm,
						uint64_t addr,
						uint64_t size)
{
	int rc;

	dev_dbg(&vm->vdev->dev,
		"offlining and removing memory: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);

	rc = offline_and_remove_memory(addr, size);
	if (!rc) {
		atomic64_sub(size, &vm->offline_size);
		 
		virtio_mem_retry(vm);
		return 0;
	}
	dev_dbg(&vm->vdev->dev, "offlining and removing memory failed: %d\n", rc);
	 
	WARN_ON_ONCE(rc != -ENOMEM && rc != -EBUSY);
	return rc == -ENOMEM ? -ENOMEM : -EBUSY;
}

 
static int virtio_mem_sbm_offline_and_remove_mb(struct virtio_mem *vm,
						unsigned long mb_id)
{
	const uint64_t addr = virtio_mem_mb_id_to_phys(mb_id);
	const uint64_t size = memory_block_size_bytes();

	return virtio_mem_offline_and_remove_memory(vm, addr, size);
}

 
static int virtio_mem_sbm_try_remove_unplugged_mb(struct virtio_mem *vm,
						  unsigned long mb_id)
{
	int rc;

	 
	if (!virtio_mem_sbm_test_sb_unplugged(vm, mb_id, 0, vm->sbm.sbs_per_mb))
		return 0;

	 
	mutex_unlock(&vm->hotplug_mutex);
	rc = virtio_mem_sbm_offline_and_remove_mb(vm, mb_id);
	mutex_lock(&vm->hotplug_mutex);
	if (!rc)
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_UNUSED);
	return rc;
}

 
static int virtio_mem_bbm_offline_and_remove_bb(struct virtio_mem *vm,
						unsigned long bb_id)
{
	const uint64_t addr = virtio_mem_bb_id_to_phys(vm, bb_id);
	const uint64_t size = vm->bbm.bb_size;

	return virtio_mem_offline_and_remove_memory(vm, addr, size);
}

 
static void virtio_mem_retry(struct virtio_mem *vm)
{
	unsigned long flags;

	spin_lock_irqsave(&vm->removal_lock, flags);
	if (!vm->removing)
		queue_work(system_freezable_wq, &vm->wq);
	spin_unlock_irqrestore(&vm->removal_lock, flags);
}

static int virtio_mem_translate_node_id(struct virtio_mem *vm, uint16_t node_id)
{
	int node = NUMA_NO_NODE;

#if defined(CONFIG_ACPI_NUMA)
	if (virtio_has_feature(vm->vdev, VIRTIO_MEM_F_ACPI_PXM))
		node = pxm_to_node(node_id);
#endif
	return node;
}

 
static bool virtio_mem_overlaps_range(struct virtio_mem *vm, uint64_t start,
				      uint64_t size)
{
	return start < vm->addr + vm->region_size && vm->addr < start + size;
}

 
static bool virtio_mem_contains_range(struct virtio_mem *vm, uint64_t start,
				      uint64_t size)
{
	return start >= vm->addr && start + size <= vm->addr + vm->region_size;
}

static int virtio_mem_sbm_notify_going_online(struct virtio_mem *vm,
					      unsigned long mb_id)
{
	switch (virtio_mem_sbm_get_mb_state(vm, mb_id)) {
	case VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL:
	case VIRTIO_MEM_SBM_MB_OFFLINE:
		return NOTIFY_OK;
	default:
		break;
	}
	dev_warn_ratelimited(&vm->vdev->dev,
			     "memory block onlining denied\n");
	return NOTIFY_BAD;
}

static void virtio_mem_sbm_notify_offline(struct virtio_mem *vm,
					  unsigned long mb_id)
{
	switch (virtio_mem_sbm_get_mb_state(vm, mb_id)) {
	case VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL:
	case VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL:
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL);
		break;
	case VIRTIO_MEM_SBM_MB_KERNEL:
	case VIRTIO_MEM_SBM_MB_MOVABLE:
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_OFFLINE);
		break;
	default:
		BUG();
		break;
	}
}

static void virtio_mem_sbm_notify_online(struct virtio_mem *vm,
					 unsigned long mb_id,
					 unsigned long start_pfn)
{
	const bool is_movable = is_zone_movable_page(pfn_to_page(start_pfn));
	int new_state;

	switch (virtio_mem_sbm_get_mb_state(vm, mb_id)) {
	case VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL:
		new_state = VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL;
		if (is_movable)
			new_state = VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL;
		break;
	case VIRTIO_MEM_SBM_MB_OFFLINE:
		new_state = VIRTIO_MEM_SBM_MB_KERNEL;
		if (is_movable)
			new_state = VIRTIO_MEM_SBM_MB_MOVABLE;
		break;
	default:
		BUG();
		break;
	}
	virtio_mem_sbm_set_mb_state(vm, mb_id, new_state);
}

static void virtio_mem_sbm_notify_going_offline(struct virtio_mem *vm,
						unsigned long mb_id)
{
	const unsigned long nr_pages = PFN_DOWN(vm->sbm.sb_size);
	unsigned long pfn;
	int sb_id;

	for (sb_id = 0; sb_id < vm->sbm.sbs_per_mb; sb_id++) {
		if (virtio_mem_sbm_test_sb_plugged(vm, mb_id, sb_id, 1))
			continue;
		pfn = PFN_DOWN(virtio_mem_mb_id_to_phys(mb_id) +
			       sb_id * vm->sbm.sb_size);
		virtio_mem_fake_offline_going_offline(pfn, nr_pages);
	}
}

static void virtio_mem_sbm_notify_cancel_offline(struct virtio_mem *vm,
						 unsigned long mb_id)
{
	const unsigned long nr_pages = PFN_DOWN(vm->sbm.sb_size);
	unsigned long pfn;
	int sb_id;

	for (sb_id = 0; sb_id < vm->sbm.sbs_per_mb; sb_id++) {
		if (virtio_mem_sbm_test_sb_plugged(vm, mb_id, sb_id, 1))
			continue;
		pfn = PFN_DOWN(virtio_mem_mb_id_to_phys(mb_id) +
			       sb_id * vm->sbm.sb_size);
		virtio_mem_fake_offline_cancel_offline(pfn, nr_pages);
	}
}

static void virtio_mem_bbm_notify_going_offline(struct virtio_mem *vm,
						unsigned long bb_id,
						unsigned long pfn,
						unsigned long nr_pages)
{
	 
	if (virtio_mem_bbm_get_bb_state(vm, bb_id) !=
	    VIRTIO_MEM_BBM_BB_FAKE_OFFLINE)
		return;
	virtio_mem_fake_offline_going_offline(pfn, nr_pages);
}

static void virtio_mem_bbm_notify_cancel_offline(struct virtio_mem *vm,
						 unsigned long bb_id,
						 unsigned long pfn,
						 unsigned long nr_pages)
{
	if (virtio_mem_bbm_get_bb_state(vm, bb_id) !=
	    VIRTIO_MEM_BBM_BB_FAKE_OFFLINE)
		return;
	virtio_mem_fake_offline_cancel_offline(pfn, nr_pages);
}

 
static int virtio_mem_memory_notifier_cb(struct notifier_block *nb,
					 unsigned long action, void *arg)
{
	struct virtio_mem *vm = container_of(nb, struct virtio_mem,
					     memory_notifier);
	struct memory_notify *mhp = arg;
	const unsigned long start = PFN_PHYS(mhp->start_pfn);
	const unsigned long size = PFN_PHYS(mhp->nr_pages);
	int rc = NOTIFY_OK;
	unsigned long id;

	if (!virtio_mem_overlaps_range(vm, start, size))
		return NOTIFY_DONE;

	if (vm->in_sbm) {
		id = virtio_mem_phys_to_mb_id(start);
		 
		if (WARN_ON_ONCE(size != memory_block_size_bytes() ||
				 !IS_ALIGNED(start, memory_block_size_bytes())))
			return NOTIFY_BAD;
	} else {
		id = virtio_mem_phys_to_bb_id(vm, start);
		 
		if (WARN_ON_ONCE(id != virtio_mem_phys_to_bb_id(vm, start + size - 1)))
			return NOTIFY_BAD;
	}

	 
	lockdep_off();

	switch (action) {
	case MEM_GOING_OFFLINE:
		mutex_lock(&vm->hotplug_mutex);
		if (vm->removing) {
			rc = notifier_from_errno(-EBUSY);
			mutex_unlock(&vm->hotplug_mutex);
			break;
		}
		vm->hotplug_active = true;
		if (vm->in_sbm)
			virtio_mem_sbm_notify_going_offline(vm, id);
		else
			virtio_mem_bbm_notify_going_offline(vm, id,
							    mhp->start_pfn,
							    mhp->nr_pages);
		break;
	case MEM_GOING_ONLINE:
		mutex_lock(&vm->hotplug_mutex);
		if (vm->removing) {
			rc = notifier_from_errno(-EBUSY);
			mutex_unlock(&vm->hotplug_mutex);
			break;
		}
		vm->hotplug_active = true;
		if (vm->in_sbm)
			rc = virtio_mem_sbm_notify_going_online(vm, id);
		break;
	case MEM_OFFLINE:
		if (vm->in_sbm)
			virtio_mem_sbm_notify_offline(vm, id);

		atomic64_add(size, &vm->offline_size);
		 
		if (!unplug_online)
			virtio_mem_retry(vm);

		vm->hotplug_active = false;
		mutex_unlock(&vm->hotplug_mutex);
		break;
	case MEM_ONLINE:
		if (vm->in_sbm)
			virtio_mem_sbm_notify_online(vm, id, mhp->start_pfn);

		atomic64_sub(size, &vm->offline_size);
		 
		if (!atomic_read(&vm->wq_active) &&
		    virtio_mem_could_add_memory(vm, vm->offline_threshold / 2))
			virtio_mem_retry(vm);

		vm->hotplug_active = false;
		mutex_unlock(&vm->hotplug_mutex);
		break;
	case MEM_CANCEL_OFFLINE:
		if (!vm->hotplug_active)
			break;
		if (vm->in_sbm)
			virtio_mem_sbm_notify_cancel_offline(vm, id);
		else
			virtio_mem_bbm_notify_cancel_offline(vm, id,
							     mhp->start_pfn,
							     mhp->nr_pages);
		vm->hotplug_active = false;
		mutex_unlock(&vm->hotplug_mutex);
		break;
	case MEM_CANCEL_ONLINE:
		if (!vm->hotplug_active)
			break;
		vm->hotplug_active = false;
		mutex_unlock(&vm->hotplug_mutex);
		break;
	default:
		break;
	}

	lockdep_on();

	return rc;
}

 
static void virtio_mem_set_fake_offline(unsigned long pfn,
					unsigned long nr_pages, bool onlined)
{
	page_offline_begin();
	for (; nr_pages--; pfn++) {
		struct page *page = pfn_to_page(pfn);

		__SetPageOffline(page);
		if (!onlined) {
			SetPageDirty(page);
			 
			ClearPageReserved(page);
		}
	}
	page_offline_end();
}

 
static void virtio_mem_clear_fake_offline(unsigned long pfn,
					  unsigned long nr_pages, bool onlined)
{
	for (; nr_pages--; pfn++) {
		struct page *page = pfn_to_page(pfn);

		__ClearPageOffline(page);
		if (!onlined)
			ClearPageDirty(page);
	}
}

 
static void virtio_mem_fake_online(unsigned long pfn, unsigned long nr_pages)
{
	unsigned long order = MAX_ORDER;
	unsigned long i;

	 
	while (!IS_ALIGNED(pfn | nr_pages, 1 << order))
		order--;

	for (i = 0; i < nr_pages; i += 1 << order) {
		struct page *page = pfn_to_page(pfn + i);

		 
		if (PageDirty(page)) {
			virtio_mem_clear_fake_offline(pfn + i, 1 << order, false);
			generic_online_page(page, order);
		} else {
			virtio_mem_clear_fake_offline(pfn + i, 1 << order, true);
			free_contig_range(pfn + i, 1 << order);
			adjust_managed_page_count(page, 1 << order);
		}
	}
}

 
static int virtio_mem_fake_offline(struct virtio_mem *vm, unsigned long pfn,
				   unsigned long nr_pages)
{
	const bool is_movable = is_zone_movable_page(pfn_to_page(pfn));
	int rc, retry_count;

	 
	for (retry_count = 0; retry_count < 5; retry_count++) {
		 
		if (atomic_read(&vm->config_changed))
			return -EAGAIN;

		rc = alloc_contig_range(pfn, pfn + nr_pages, MIGRATE_MOVABLE,
					GFP_KERNEL);
		if (rc == -ENOMEM)
			 
			return rc;
		else if (rc && !is_movable)
			break;
		else if (rc)
			continue;

		virtio_mem_set_fake_offline(pfn, nr_pages, true);
		adjust_managed_page_count(pfn_to_page(pfn), -nr_pages);
		return 0;
	}

	return -EBUSY;
}

 
static void virtio_mem_fake_offline_going_offline(unsigned long pfn,
						  unsigned long nr_pages)
{
	struct page *page;
	unsigned long i;

	 
	adjust_managed_page_count(pfn_to_page(pfn), nr_pages);
	 
	for (i = 0; i < nr_pages; i++) {
		page = pfn_to_page(pfn + i);
		if (WARN_ON(!page_ref_dec_and_test(page)))
			dump_page(page, "fake-offline page referenced");
	}
}

 
static void virtio_mem_fake_offline_cancel_offline(unsigned long pfn,
						   unsigned long nr_pages)
{
	unsigned long i;

	 
	adjust_managed_page_count(pfn_to_page(pfn), -nr_pages);
	for (i = 0; i < nr_pages; i++)
		page_ref_inc(pfn_to_page(pfn + i));
}

static void virtio_mem_online_page(struct virtio_mem *vm,
				   struct page *page, unsigned int order)
{
	const unsigned long start = page_to_phys(page);
	const unsigned long end = start + PFN_PHYS(1 << order);
	unsigned long addr, next, id, sb_id, count;
	bool do_online;

	 
	for (addr = start; addr < end; ) {
		next = addr + PFN_PHYS(1 << order);

		if (vm->in_sbm) {
			id = virtio_mem_phys_to_mb_id(addr);
			sb_id = virtio_mem_phys_to_sb_id(vm, addr);
			count = virtio_mem_phys_to_sb_id(vm, next - 1) - sb_id + 1;

			if (virtio_mem_sbm_test_sb_plugged(vm, id, sb_id, count)) {
				 
				do_online = true;
			} else if (count == 1 ||
				   virtio_mem_sbm_test_sb_unplugged(vm, id, sb_id, count)) {
				 
				do_online = false;
			} else {
				 
				order = ilog2(vm->sbm.sb_size) - PAGE_SHIFT;
				do_online = virtio_mem_sbm_test_sb_plugged(vm, id, sb_id, 1);
				continue;
			}
		} else {
			 
			id = virtio_mem_phys_to_bb_id(vm, addr);
			do_online = virtio_mem_bbm_get_bb_state(vm, id) !=
				    VIRTIO_MEM_BBM_BB_FAKE_OFFLINE;
		}

		if (do_online)
			generic_online_page(pfn_to_page(PFN_DOWN(addr)), order);
		else
			virtio_mem_set_fake_offline(PFN_DOWN(addr), 1 << order,
						    false);
		addr = next;
	}
}

static void virtio_mem_online_page_cb(struct page *page, unsigned int order)
{
	const unsigned long addr = page_to_phys(page);
	struct virtio_mem *vm;

	rcu_read_lock();
	list_for_each_entry_rcu(vm, &virtio_mem_devices, next) {
		 
		if (!virtio_mem_contains_range(vm, addr, PFN_PHYS(1 << order)))
			continue;

		 
		rcu_read_unlock();

		virtio_mem_online_page(vm, page, order);
		return;
	}
	rcu_read_unlock();

	 
	generic_online_page(page, order);
}

static uint64_t virtio_mem_send_request(struct virtio_mem *vm,
					const struct virtio_mem_req *req)
{
	struct scatterlist *sgs[2], sg_req, sg_resp;
	unsigned int len;
	int rc;

	 
	vm->req = *req;

	 
	sg_init_one(&sg_req, &vm->req, sizeof(vm->req));
	sgs[0] = &sg_req;

	 
	sg_init_one(&sg_resp, &vm->resp, sizeof(vm->resp));
	sgs[1] = &sg_resp;

	rc = virtqueue_add_sgs(vm->vq, sgs, 1, 1, vm, GFP_KERNEL);
	if (rc < 0)
		return rc;

	virtqueue_kick(vm->vq);

	 
	wait_event(vm->host_resp, virtqueue_get_buf(vm->vq, &len));

	return virtio16_to_cpu(vm->vdev, vm->resp.type);
}

static int virtio_mem_send_plug_request(struct virtio_mem *vm, uint64_t addr,
					uint64_t size)
{
	const uint64_t nb_vm_blocks = size / vm->device_block_size;
	const struct virtio_mem_req req = {
		.type = cpu_to_virtio16(vm->vdev, VIRTIO_MEM_REQ_PLUG),
		.u.plug.addr = cpu_to_virtio64(vm->vdev, addr),
		.u.plug.nb_blocks = cpu_to_virtio16(vm->vdev, nb_vm_blocks),
	};
	int rc = -ENOMEM;

	if (atomic_read(&vm->config_changed))
		return -EAGAIN;

	dev_dbg(&vm->vdev->dev, "plugging memory: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);

	switch (virtio_mem_send_request(vm, &req)) {
	case VIRTIO_MEM_RESP_ACK:
		vm->plugged_size += size;
		return 0;
	case VIRTIO_MEM_RESP_NACK:
		rc = -EAGAIN;
		break;
	case VIRTIO_MEM_RESP_BUSY:
		rc = -ETXTBSY;
		break;
	case VIRTIO_MEM_RESP_ERROR:
		rc = -EINVAL;
		break;
	default:
		break;
	}

	dev_dbg(&vm->vdev->dev, "plugging memory failed: %d\n", rc);
	return rc;
}

static int virtio_mem_send_unplug_request(struct virtio_mem *vm, uint64_t addr,
					  uint64_t size)
{
	const uint64_t nb_vm_blocks = size / vm->device_block_size;
	const struct virtio_mem_req req = {
		.type = cpu_to_virtio16(vm->vdev, VIRTIO_MEM_REQ_UNPLUG),
		.u.unplug.addr = cpu_to_virtio64(vm->vdev, addr),
		.u.unplug.nb_blocks = cpu_to_virtio16(vm->vdev, nb_vm_blocks),
	};
	int rc = -ENOMEM;

	if (atomic_read(&vm->config_changed))
		return -EAGAIN;

	dev_dbg(&vm->vdev->dev, "unplugging memory: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);

	switch (virtio_mem_send_request(vm, &req)) {
	case VIRTIO_MEM_RESP_ACK:
		vm->plugged_size -= size;
		return 0;
	case VIRTIO_MEM_RESP_BUSY:
		rc = -ETXTBSY;
		break;
	case VIRTIO_MEM_RESP_ERROR:
		rc = -EINVAL;
		break;
	default:
		break;
	}

	dev_dbg(&vm->vdev->dev, "unplugging memory failed: %d\n", rc);
	return rc;
}

static int virtio_mem_send_unplug_all_request(struct virtio_mem *vm)
{
	const struct virtio_mem_req req = {
		.type = cpu_to_virtio16(vm->vdev, VIRTIO_MEM_REQ_UNPLUG_ALL),
	};
	int rc = -ENOMEM;

	dev_dbg(&vm->vdev->dev, "unplugging all memory");

	switch (virtio_mem_send_request(vm, &req)) {
	case VIRTIO_MEM_RESP_ACK:
		vm->unplug_all_required = false;
		vm->plugged_size = 0;
		 
		atomic_set(&vm->config_changed, 1);
		return 0;
	case VIRTIO_MEM_RESP_BUSY:
		rc = -ETXTBSY;
		break;
	default:
		break;
	}

	dev_dbg(&vm->vdev->dev, "unplugging all memory failed: %d\n", rc);
	return rc;
}

 
static int virtio_mem_sbm_plug_sb(struct virtio_mem *vm, unsigned long mb_id,
				  int sb_id, int count)
{
	const uint64_t addr = virtio_mem_mb_id_to_phys(mb_id) +
			      sb_id * vm->sbm.sb_size;
	const uint64_t size = count * vm->sbm.sb_size;
	int rc;

	rc = virtio_mem_send_plug_request(vm, addr, size);
	if (!rc)
		virtio_mem_sbm_set_sb_plugged(vm, mb_id, sb_id, count);
	return rc;
}

 
static int virtio_mem_sbm_unplug_sb(struct virtio_mem *vm, unsigned long mb_id,
				    int sb_id, int count)
{
	const uint64_t addr = virtio_mem_mb_id_to_phys(mb_id) +
			      sb_id * vm->sbm.sb_size;
	const uint64_t size = count * vm->sbm.sb_size;
	int rc;

	rc = virtio_mem_send_unplug_request(vm, addr, size);
	if (!rc)
		virtio_mem_sbm_set_sb_unplugged(vm, mb_id, sb_id, count);
	return rc;
}

 
static int virtio_mem_bbm_unplug_bb(struct virtio_mem *vm, unsigned long bb_id)
{
	const uint64_t addr = virtio_mem_bb_id_to_phys(vm, bb_id);
	const uint64_t size = vm->bbm.bb_size;

	return virtio_mem_send_unplug_request(vm, addr, size);
}

 
static int virtio_mem_bbm_plug_bb(struct virtio_mem *vm, unsigned long bb_id)
{
	const uint64_t addr = virtio_mem_bb_id_to_phys(vm, bb_id);
	const uint64_t size = vm->bbm.bb_size;

	return virtio_mem_send_plug_request(vm, addr, size);
}

 
static int virtio_mem_sbm_unplug_any_sb_raw(struct virtio_mem *vm,
					    unsigned long mb_id, uint64_t *nb_sb)
{
	int sb_id, count;
	int rc;

	sb_id = vm->sbm.sbs_per_mb - 1;
	while (*nb_sb) {
		 
		while (sb_id >= 0 &&
		       virtio_mem_sbm_test_sb_unplugged(vm, mb_id, sb_id, 1))
			sb_id--;
		if (sb_id < 0)
			break;
		 
		count = 1;
		while (count < *nb_sb && sb_id > 0 &&
		       virtio_mem_sbm_test_sb_plugged(vm, mb_id, sb_id - 1, 1)) {
			count++;
			sb_id--;
		}

		rc = virtio_mem_sbm_unplug_sb(vm, mb_id, sb_id, count);
		if (rc)
			return rc;
		*nb_sb -= count;
		sb_id--;
	}

	return 0;
}

 
static int virtio_mem_sbm_unplug_mb(struct virtio_mem *vm, unsigned long mb_id)
{
	uint64_t nb_sb = vm->sbm.sbs_per_mb;

	return virtio_mem_sbm_unplug_any_sb_raw(vm, mb_id, &nb_sb);
}

 
static int virtio_mem_sbm_prepare_next_mb(struct virtio_mem *vm,
					  unsigned long *mb_id)
{
	int rc;

	if (vm->sbm.next_mb_id > vm->sbm.last_usable_mb_id)
		return -ENOSPC;

	 
	rc = virtio_mem_sbm_mb_states_prepare_next_mb(vm);
	if (rc)
		return rc;

	 
	rc = virtio_mem_sbm_sb_states_prepare_next_mb(vm);
	if (rc)
		return rc;

	vm->sbm.mb_count[VIRTIO_MEM_SBM_MB_UNUSED]++;
	*mb_id = vm->sbm.next_mb_id++;
	return 0;
}

 
static int virtio_mem_sbm_plug_and_add_mb(struct virtio_mem *vm,
					  unsigned long mb_id, uint64_t *nb_sb)
{
	const int count = min_t(int, *nb_sb, vm->sbm.sbs_per_mb);
	int rc;

	if (WARN_ON_ONCE(!count))
		return -EINVAL;

	 
	rc = virtio_mem_sbm_plug_sb(vm, mb_id, 0, count);
	if (rc)
		return rc;

	 
	if (count == vm->sbm.sbs_per_mb)
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_OFFLINE);
	else
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL);

	 
	rc = virtio_mem_sbm_add_mb(vm, mb_id);
	if (rc) {
		int new_state = VIRTIO_MEM_SBM_MB_UNUSED;

		if (virtio_mem_sbm_unplug_sb(vm, mb_id, 0, count))
			new_state = VIRTIO_MEM_SBM_MB_PLUGGED;
		virtio_mem_sbm_set_mb_state(vm, mb_id, new_state);
		return rc;
	}

	*nb_sb -= count;
	return 0;
}

 
static int virtio_mem_sbm_plug_any_sb(struct virtio_mem *vm,
				      unsigned long mb_id, uint64_t *nb_sb)
{
	const int old_state = virtio_mem_sbm_get_mb_state(vm, mb_id);
	unsigned long pfn, nr_pages;
	int sb_id, count;
	int rc;

	if (WARN_ON_ONCE(!*nb_sb))
		return -EINVAL;

	while (*nb_sb) {
		sb_id = virtio_mem_sbm_first_unplugged_sb(vm, mb_id);
		if (sb_id >= vm->sbm.sbs_per_mb)
			break;
		count = 1;
		while (count < *nb_sb &&
		       sb_id + count < vm->sbm.sbs_per_mb &&
		       !virtio_mem_sbm_test_sb_plugged(vm, mb_id, sb_id + count, 1))
			count++;

		rc = virtio_mem_sbm_plug_sb(vm, mb_id, sb_id, count);
		if (rc)
			return rc;
		*nb_sb -= count;
		if (old_state == VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL)
			continue;

		 
		pfn = PFN_DOWN(virtio_mem_mb_id_to_phys(mb_id) +
			       sb_id * vm->sbm.sb_size);
		nr_pages = PFN_DOWN(count * vm->sbm.sb_size);
		virtio_mem_fake_online(pfn, nr_pages);
	}

	if (virtio_mem_sbm_test_sb_plugged(vm, mb_id, 0, vm->sbm.sbs_per_mb))
		virtio_mem_sbm_set_mb_state(vm, mb_id, old_state - 1);

	return 0;
}

static int virtio_mem_sbm_plug_request(struct virtio_mem *vm, uint64_t diff)
{
	const int mb_states[] = {
		VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL,
		VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL,
		VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL,
	};
	uint64_t nb_sb = diff / vm->sbm.sb_size;
	unsigned long mb_id;
	int rc, i;

	if (!nb_sb)
		return 0;

	 
	mutex_lock(&vm->hotplug_mutex);

	for (i = 0; i < ARRAY_SIZE(mb_states); i++) {
		virtio_mem_sbm_for_each_mb(vm, mb_id, mb_states[i]) {
			rc = virtio_mem_sbm_plug_any_sb(vm, mb_id, &nb_sb);
			if (rc || !nb_sb)
				goto out_unlock;
			cond_resched();
		}
	}

	 
	mutex_unlock(&vm->hotplug_mutex);

	 
	virtio_mem_sbm_for_each_mb(vm, mb_id, VIRTIO_MEM_SBM_MB_UNUSED) {
		if (!virtio_mem_could_add_memory(vm, memory_block_size_bytes()))
			return -ENOSPC;

		rc = virtio_mem_sbm_plug_and_add_mb(vm, mb_id, &nb_sb);
		if (rc || !nb_sb)
			return rc;
		cond_resched();
	}

	 
	while (nb_sb) {
		if (!virtio_mem_could_add_memory(vm, memory_block_size_bytes()))
			return -ENOSPC;

		rc = virtio_mem_sbm_prepare_next_mb(vm, &mb_id);
		if (rc)
			return rc;
		rc = virtio_mem_sbm_plug_and_add_mb(vm, mb_id, &nb_sb);
		if (rc)
			return rc;
		cond_resched();
	}

	return 0;
out_unlock:
	mutex_unlock(&vm->hotplug_mutex);
	return rc;
}

 
static int virtio_mem_bbm_plug_and_add_bb(struct virtio_mem *vm,
					  unsigned long bb_id)
{
	int rc;

	if (WARN_ON_ONCE(virtio_mem_bbm_get_bb_state(vm, bb_id) !=
			 VIRTIO_MEM_BBM_BB_UNUSED))
		return -EINVAL;

	rc = virtio_mem_bbm_plug_bb(vm, bb_id);
	if (rc)
		return rc;
	virtio_mem_bbm_set_bb_state(vm, bb_id, VIRTIO_MEM_BBM_BB_ADDED);

	rc = virtio_mem_bbm_add_bb(vm, bb_id);
	if (rc) {
		if (!virtio_mem_bbm_unplug_bb(vm, bb_id))
			virtio_mem_bbm_set_bb_state(vm, bb_id,
						    VIRTIO_MEM_BBM_BB_UNUSED);
		else
			 
			virtio_mem_bbm_set_bb_state(vm, bb_id,
						    VIRTIO_MEM_BBM_BB_PLUGGED);
		return rc;
	}
	return 0;
}

 
static int virtio_mem_bbm_prepare_next_bb(struct virtio_mem *vm,
					  unsigned long *bb_id)
{
	int rc;

	if (vm->bbm.next_bb_id > vm->bbm.last_usable_bb_id)
		return -ENOSPC;

	 
	rc = virtio_mem_bbm_bb_states_prepare_next_bb(vm);
	if (rc)
		return rc;

	vm->bbm.bb_count[VIRTIO_MEM_BBM_BB_UNUSED]++;
	*bb_id = vm->bbm.next_bb_id;
	vm->bbm.next_bb_id++;
	return 0;
}

static int virtio_mem_bbm_plug_request(struct virtio_mem *vm, uint64_t diff)
{
	uint64_t nb_bb = diff / vm->bbm.bb_size;
	unsigned long bb_id;
	int rc;

	if (!nb_bb)
		return 0;

	 
	virtio_mem_bbm_for_each_bb(vm, bb_id, VIRTIO_MEM_BBM_BB_UNUSED) {
		if (!virtio_mem_could_add_memory(vm, vm->bbm.bb_size))
			return -ENOSPC;

		rc = virtio_mem_bbm_plug_and_add_bb(vm, bb_id);
		if (!rc)
			nb_bb--;
		if (rc || !nb_bb)
			return rc;
		cond_resched();
	}

	 
	while (nb_bb) {
		if (!virtio_mem_could_add_memory(vm, vm->bbm.bb_size))
			return -ENOSPC;

		rc = virtio_mem_bbm_prepare_next_bb(vm, &bb_id);
		if (rc)
			return rc;
		rc = virtio_mem_bbm_plug_and_add_bb(vm, bb_id);
		if (!rc)
			nb_bb--;
		if (rc)
			return rc;
		cond_resched();
	}

	return 0;
}

 
static int virtio_mem_plug_request(struct virtio_mem *vm, uint64_t diff)
{
	if (vm->in_sbm)
		return virtio_mem_sbm_plug_request(vm, diff);
	return virtio_mem_bbm_plug_request(vm, diff);
}

 
static int virtio_mem_sbm_unplug_any_sb_offline(struct virtio_mem *vm,
						unsigned long mb_id,
						uint64_t *nb_sb)
{
	int rc;

	rc = virtio_mem_sbm_unplug_any_sb_raw(vm, mb_id, nb_sb);

	 
	if (!virtio_mem_sbm_test_sb_plugged(vm, mb_id, 0, vm->sbm.sbs_per_mb))
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL);
	if (rc)
		return rc;

	if (virtio_mem_sbm_test_sb_unplugged(vm, mb_id, 0, vm->sbm.sbs_per_mb)) {
		 
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_UNUSED);

		mutex_unlock(&vm->hotplug_mutex);
		rc = virtio_mem_sbm_remove_mb(vm, mb_id);
		BUG_ON(rc);
		mutex_lock(&vm->hotplug_mutex);
	}
	return 0;
}

 
static int virtio_mem_sbm_unplug_sb_online(struct virtio_mem *vm,
					   unsigned long mb_id, int sb_id,
					   int count)
{
	const unsigned long nr_pages = PFN_DOWN(vm->sbm.sb_size) * count;
	const int old_state = virtio_mem_sbm_get_mb_state(vm, mb_id);
	unsigned long start_pfn;
	int rc;

	start_pfn = PFN_DOWN(virtio_mem_mb_id_to_phys(mb_id) +
			     sb_id * vm->sbm.sb_size);

	rc = virtio_mem_fake_offline(vm, start_pfn, nr_pages);
	if (rc)
		return rc;

	 
	rc = virtio_mem_sbm_unplug_sb(vm, mb_id, sb_id, count);
	if (rc) {
		 
		virtio_mem_fake_online(start_pfn, nr_pages);
		return rc;
	}

	switch (old_state) {
	case VIRTIO_MEM_SBM_MB_KERNEL:
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL);
		break;
	case VIRTIO_MEM_SBM_MB_MOVABLE:
		virtio_mem_sbm_set_mb_state(vm, mb_id,
					    VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL);
		break;
	}

	return 0;
}

 
static int virtio_mem_sbm_unplug_any_sb_online(struct virtio_mem *vm,
					       unsigned long mb_id,
					       uint64_t *nb_sb)
{
	int rc, sb_id;

	 
	if (*nb_sb >= vm->sbm.sbs_per_mb &&
	    virtio_mem_sbm_test_sb_plugged(vm, mb_id, 0, vm->sbm.sbs_per_mb)) {
		rc = virtio_mem_sbm_unplug_sb_online(vm, mb_id, 0,
						     vm->sbm.sbs_per_mb);
		if (!rc) {
			*nb_sb -= vm->sbm.sbs_per_mb;
			goto unplugged;
		} else if (rc != -EBUSY)
			return rc;
	}

	 
	for (sb_id = vm->sbm.sbs_per_mb - 1; sb_id >= 0 && *nb_sb; sb_id--) {
		 
		while (sb_id >= 0 &&
		       !virtio_mem_sbm_test_sb_plugged(vm, mb_id, sb_id, 1))
			sb_id--;
		if (sb_id < 0)
			break;

		rc = virtio_mem_sbm_unplug_sb_online(vm, mb_id, sb_id, 1);
		if (rc == -EBUSY)
			continue;
		else if (rc)
			return rc;
		*nb_sb -= 1;
	}

unplugged:
	rc = virtio_mem_sbm_try_remove_unplugged_mb(vm, mb_id);
	if (rc)
		vm->sbm.have_unplugged_mb = 1;
	 
	return 0;
}

 
static int virtio_mem_sbm_unplug_any_sb(struct virtio_mem *vm,
					unsigned long mb_id,
					uint64_t *nb_sb)
{
	const int old_state = virtio_mem_sbm_get_mb_state(vm, mb_id);

	switch (old_state) {
	case VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL:
	case VIRTIO_MEM_SBM_MB_KERNEL:
	case VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL:
	case VIRTIO_MEM_SBM_MB_MOVABLE:
		return virtio_mem_sbm_unplug_any_sb_online(vm, mb_id, nb_sb);
	case VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL:
	case VIRTIO_MEM_SBM_MB_OFFLINE:
		return virtio_mem_sbm_unplug_any_sb_offline(vm, mb_id, nb_sb);
	}
	return -EINVAL;
}

static int virtio_mem_sbm_unplug_request(struct virtio_mem *vm, uint64_t diff)
{
	const int mb_states[] = {
		VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL,
		VIRTIO_MEM_SBM_MB_OFFLINE,
		VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL,
		VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL,
		VIRTIO_MEM_SBM_MB_MOVABLE,
		VIRTIO_MEM_SBM_MB_KERNEL,
	};
	uint64_t nb_sb = diff / vm->sbm.sb_size;
	unsigned long mb_id;
	int rc, i;

	if (!nb_sb)
		return 0;

	 
	mutex_lock(&vm->hotplug_mutex);

	 
	for (i = 0; i < ARRAY_SIZE(mb_states); i++) {
		virtio_mem_sbm_for_each_mb_rev(vm, mb_id, mb_states[i]) {
			rc = virtio_mem_sbm_unplug_any_sb(vm, mb_id, &nb_sb);
			if (rc || !nb_sb)
				goto out_unlock;
			mutex_unlock(&vm->hotplug_mutex);
			cond_resched();
			mutex_lock(&vm->hotplug_mutex);
		}
		if (!unplug_online && i == 1) {
			mutex_unlock(&vm->hotplug_mutex);
			return 0;
		}
	}

	mutex_unlock(&vm->hotplug_mutex);
	return nb_sb ? -EBUSY : 0;
out_unlock:
	mutex_unlock(&vm->hotplug_mutex);
	return rc;
}

 
static int virtio_mem_bbm_offline_remove_and_unplug_bb(struct virtio_mem *vm,
						       unsigned long bb_id)
{
	const unsigned long start_pfn = PFN_DOWN(virtio_mem_bb_id_to_phys(vm, bb_id));
	const unsigned long nr_pages = PFN_DOWN(vm->bbm.bb_size);
	unsigned long end_pfn = start_pfn + nr_pages;
	unsigned long pfn;
	struct page *page;
	int rc;

	if (WARN_ON_ONCE(virtio_mem_bbm_get_bb_state(vm, bb_id) !=
			 VIRTIO_MEM_BBM_BB_ADDED))
		return -EINVAL;

	 
	mutex_lock(&vm->hotplug_mutex);
	virtio_mem_bbm_set_bb_state(vm, bb_id, VIRTIO_MEM_BBM_BB_FAKE_OFFLINE);

	for (pfn = start_pfn; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
		page = pfn_to_online_page(pfn);
		if (!page)
			continue;

		rc = virtio_mem_fake_offline(vm, pfn, PAGES_PER_SECTION);
		if (rc) {
			end_pfn = pfn;
			goto rollback;
		}
	}
	mutex_unlock(&vm->hotplug_mutex);

	rc = virtio_mem_bbm_offline_and_remove_bb(vm, bb_id);
	if (rc) {
		mutex_lock(&vm->hotplug_mutex);
		goto rollback;
	}

	rc = virtio_mem_bbm_unplug_bb(vm, bb_id);
	if (rc)
		virtio_mem_bbm_set_bb_state(vm, bb_id,
					    VIRTIO_MEM_BBM_BB_PLUGGED);
	else
		virtio_mem_bbm_set_bb_state(vm, bb_id,
					    VIRTIO_MEM_BBM_BB_UNUSED);
	return rc;

rollback:
	for (pfn = start_pfn; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
		page = pfn_to_online_page(pfn);
		if (!page)
			continue;
		virtio_mem_fake_online(pfn, PAGES_PER_SECTION);
	}
	virtio_mem_bbm_set_bb_state(vm, bb_id, VIRTIO_MEM_BBM_BB_ADDED);
	mutex_unlock(&vm->hotplug_mutex);
	return rc;
}

 
static bool virtio_mem_bbm_bb_is_offline(struct virtio_mem *vm,
					 unsigned long bb_id)
{
	const unsigned long start_pfn = PFN_DOWN(virtio_mem_bb_id_to_phys(vm, bb_id));
	const unsigned long nr_pages = PFN_DOWN(vm->bbm.bb_size);
	unsigned long pfn;

	for (pfn = start_pfn; pfn < start_pfn + nr_pages;
	     pfn += PAGES_PER_SECTION) {
		if (pfn_to_online_page(pfn))
			return false;
	}

	return true;
}

 
static bool virtio_mem_bbm_bb_is_movable(struct virtio_mem *vm,
					 unsigned long bb_id)
{
	const unsigned long start_pfn = PFN_DOWN(virtio_mem_bb_id_to_phys(vm, bb_id));
	const unsigned long nr_pages = PFN_DOWN(vm->bbm.bb_size);
	struct page *page;
	unsigned long pfn;

	for (pfn = start_pfn; pfn < start_pfn + nr_pages;
	     pfn += PAGES_PER_SECTION) {
		page = pfn_to_online_page(pfn);
		if (!page)
			continue;
		if (page_zonenum(page) != ZONE_MOVABLE)
			return false;
	}

	return true;
}

static int virtio_mem_bbm_unplug_request(struct virtio_mem *vm, uint64_t diff)
{
	uint64_t nb_bb = diff / vm->bbm.bb_size;
	uint64_t bb_id;
	int rc, i;

	if (!nb_bb)
		return 0;

	 
	for (i = 0; i < 3; i++) {
		virtio_mem_bbm_for_each_bb_rev(vm, bb_id, VIRTIO_MEM_BBM_BB_ADDED) {
			cond_resched();

			 
			if (i == 0 && !virtio_mem_bbm_bb_is_offline(vm, bb_id))
				continue;
			if (i == 1 && !virtio_mem_bbm_bb_is_movable(vm, bb_id))
				continue;
			rc = virtio_mem_bbm_offline_remove_and_unplug_bb(vm, bb_id);
			if (rc == -EBUSY)
				continue;
			if (!rc)
				nb_bb--;
			if (rc || !nb_bb)
				return rc;
		}
		if (i == 0 && !unplug_online)
			return 0;
	}

	return nb_bb ? -EBUSY : 0;
}

 
static int virtio_mem_unplug_request(struct virtio_mem *vm, uint64_t diff)
{
	if (vm->in_sbm)
		return virtio_mem_sbm_unplug_request(vm, diff);
	return virtio_mem_bbm_unplug_request(vm, diff);
}

 
static int virtio_mem_cleanup_pending_mb(struct virtio_mem *vm)
{
	unsigned long id;
	int rc = 0;

	if (!vm->in_sbm) {
		virtio_mem_bbm_for_each_bb(vm, id,
					   VIRTIO_MEM_BBM_BB_PLUGGED) {
			rc = virtio_mem_bbm_unplug_bb(vm, id);
			if (rc)
				return rc;
			virtio_mem_bbm_set_bb_state(vm, id,
						    VIRTIO_MEM_BBM_BB_UNUSED);
		}
		return 0;
	}

	virtio_mem_sbm_for_each_mb(vm, id, VIRTIO_MEM_SBM_MB_PLUGGED) {
		rc = virtio_mem_sbm_unplug_mb(vm, id);
		if (rc)
			return rc;
		virtio_mem_sbm_set_mb_state(vm, id,
					    VIRTIO_MEM_SBM_MB_UNUSED);
	}

	if (!vm->sbm.have_unplugged_mb)
		return 0;

	 
	vm->sbm.have_unplugged_mb = false;

	mutex_lock(&vm->hotplug_mutex);
	virtio_mem_sbm_for_each_mb(vm, id, VIRTIO_MEM_SBM_MB_MOVABLE_PARTIAL)
		rc |= virtio_mem_sbm_try_remove_unplugged_mb(vm, id);
	virtio_mem_sbm_for_each_mb(vm, id, VIRTIO_MEM_SBM_MB_KERNEL_PARTIAL)
		rc |= virtio_mem_sbm_try_remove_unplugged_mb(vm, id);
	virtio_mem_sbm_for_each_mb(vm, id, VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL)
		rc |= virtio_mem_sbm_try_remove_unplugged_mb(vm, id);
	mutex_unlock(&vm->hotplug_mutex);

	if (rc)
		vm->sbm.have_unplugged_mb = true;
	 
	return 0;
}

 
static void virtio_mem_refresh_config(struct virtio_mem *vm)
{
	const struct range pluggable_range = mhp_get_pluggable_range(true);
	uint64_t new_plugged_size, usable_region_size, end_addr;

	 
	virtio_cread_le(vm->vdev, struct virtio_mem_config, plugged_size,
			&new_plugged_size);
	if (WARN_ON_ONCE(new_plugged_size != vm->plugged_size))
		vm->plugged_size = new_plugged_size;

	 
	virtio_cread_le(vm->vdev, struct virtio_mem_config,
			usable_region_size, &usable_region_size);
	end_addr = min(vm->addr + usable_region_size - 1,
		       pluggable_range.end);

	if (vm->in_sbm) {
		vm->sbm.last_usable_mb_id = virtio_mem_phys_to_mb_id(end_addr);
		if (!IS_ALIGNED(end_addr + 1, memory_block_size_bytes()))
			vm->sbm.last_usable_mb_id--;
	} else {
		vm->bbm.last_usable_bb_id = virtio_mem_phys_to_bb_id(vm,
								     end_addr);
		if (!IS_ALIGNED(end_addr + 1, vm->bbm.bb_size))
			vm->bbm.last_usable_bb_id--;
	}
	 

	 
	virtio_cread_le(vm->vdev, struct virtio_mem_config, requested_size,
			&vm->requested_size);

	dev_info(&vm->vdev->dev, "plugged size: 0x%llx", vm->plugged_size);
	dev_info(&vm->vdev->dev, "requested size: 0x%llx", vm->requested_size);
}

 
static void virtio_mem_run_wq(struct work_struct *work)
{
	struct virtio_mem *vm = container_of(work, struct virtio_mem, wq);
	uint64_t diff;
	int rc;

	if (unlikely(vm->in_kdump)) {
		dev_warn_once(&vm->vdev->dev,
			     "unexpected workqueue run in kdump kernel\n");
		return;
	}

	hrtimer_cancel(&vm->retry_timer);

	if (vm->broken)
		return;

	atomic_set(&vm->wq_active, 1);
retry:
	rc = 0;

	 
	if (unlikely(vm->unplug_all_required))
		rc = virtio_mem_send_unplug_all_request(vm);

	if (atomic_read(&vm->config_changed)) {
		atomic_set(&vm->config_changed, 0);
		virtio_mem_refresh_config(vm);
	}

	 
	if (!rc)
		rc = virtio_mem_cleanup_pending_mb(vm);

	if (!rc && vm->requested_size != vm->plugged_size) {
		if (vm->requested_size > vm->plugged_size) {
			diff = vm->requested_size - vm->plugged_size;
			rc = virtio_mem_plug_request(vm, diff);
		} else {
			diff = vm->plugged_size - vm->requested_size;
			rc = virtio_mem_unplug_request(vm, diff);
		}
	}

	 
	if (!rc && vm->in_sbm && vm->sbm.have_unplugged_mb)
		rc = -EBUSY;

	switch (rc) {
	case 0:
		vm->retry_timer_ms = VIRTIO_MEM_RETRY_TIMER_MIN_MS;
		break;
	case -ENOSPC:
		 
		break;
	case -ETXTBSY:
		 
	case -EBUSY:
		 
	case -ENOMEM:
		 
		hrtimer_start(&vm->retry_timer, ms_to_ktime(vm->retry_timer_ms),
			      HRTIMER_MODE_REL);
		break;
	case -EAGAIN:
		 
		goto retry;
	default:
		 
		dev_err(&vm->vdev->dev,
			"unknown error, marking device broken: %d\n", rc);
		vm->broken = true;
	}

	atomic_set(&vm->wq_active, 0);
}

static enum hrtimer_restart virtio_mem_timer_expired(struct hrtimer *timer)
{
	struct virtio_mem *vm = container_of(timer, struct virtio_mem,
					     retry_timer);

	virtio_mem_retry(vm);
	vm->retry_timer_ms = min_t(unsigned int, vm->retry_timer_ms * 2,
				   VIRTIO_MEM_RETRY_TIMER_MAX_MS);
	return HRTIMER_NORESTART;
}

static void virtio_mem_handle_response(struct virtqueue *vq)
{
	struct virtio_mem *vm = vq->vdev->priv;

	wake_up(&vm->host_resp);
}

static int virtio_mem_init_vq(struct virtio_mem *vm)
{
	struct virtqueue *vq;

	vq = virtio_find_single_vq(vm->vdev, virtio_mem_handle_response,
				   "guest-request");
	if (IS_ERR(vq))
		return PTR_ERR(vq);
	vm->vq = vq;

	return 0;
}

static int virtio_mem_init_hotplug(struct virtio_mem *vm)
{
	const struct range pluggable_range = mhp_get_pluggable_range(true);
	uint64_t unit_pages, sb_size, addr;
	int rc;

	 
	if (!IS_ALIGNED(vm->addr, memory_block_size_bytes()))
		dev_warn(&vm->vdev->dev,
			 "The alignment of the physical start address can make some memory unusable.\n");
	if (!IS_ALIGNED(vm->addr + vm->region_size, memory_block_size_bytes()))
		dev_warn(&vm->vdev->dev,
			 "The alignment of the physical end address can make some memory unusable.\n");
	if (vm->addr < pluggable_range.start ||
	    vm->addr + vm->region_size - 1 > pluggable_range.end)
		dev_warn(&vm->vdev->dev,
			 "Some device memory is not addressable/pluggable. This can make some memory unusable.\n");

	 
	vm->offline_threshold = max_t(uint64_t, 2 * memory_block_size_bytes(),
				      VIRTIO_MEM_DEFAULT_OFFLINE_THRESHOLD);

	 
	sb_size = PAGE_SIZE * pageblock_nr_pages;
	sb_size = max_t(uint64_t, vm->device_block_size, sb_size);

	if (sb_size < memory_block_size_bytes() && !force_bbm) {
		 
		vm->in_sbm = true;
		vm->sbm.sb_size = sb_size;
		vm->sbm.sbs_per_mb = memory_block_size_bytes() /
				     vm->sbm.sb_size;

		 
		addr = max_t(uint64_t, vm->addr, pluggable_range.start) +
		       memory_block_size_bytes() - 1;
		vm->sbm.first_mb_id = virtio_mem_phys_to_mb_id(addr);
		vm->sbm.next_mb_id = vm->sbm.first_mb_id;
	} else {
		 
		vm->bbm.bb_size = max_t(uint64_t, vm->device_block_size,
					memory_block_size_bytes());

		if (bbm_block_size) {
			if (!is_power_of_2(bbm_block_size)) {
				dev_warn(&vm->vdev->dev,
					 "bbm_block_size is not a power of 2");
			} else if (bbm_block_size < vm->bbm.bb_size) {
				dev_warn(&vm->vdev->dev,
					 "bbm_block_size is too small");
			} else {
				vm->bbm.bb_size = bbm_block_size;
			}
		}

		 
		addr = max_t(uint64_t, vm->addr, pluggable_range.start) +
		       vm->bbm.bb_size - 1;
		vm->bbm.first_bb_id = virtio_mem_phys_to_bb_id(vm, addr);
		vm->bbm.next_bb_id = vm->bbm.first_bb_id;

		 
		vm->offline_threshold = max_t(uint64_t, 2 * vm->bbm.bb_size,
					      vm->offline_threshold);
	}

	dev_info(&vm->vdev->dev, "memory block size: 0x%lx",
		 memory_block_size_bytes());
	if (vm->in_sbm)
		dev_info(&vm->vdev->dev, "subblock size: 0x%llx",
			 (unsigned long long)vm->sbm.sb_size);
	else
		dev_info(&vm->vdev->dev, "big block size: 0x%llx",
			 (unsigned long long)vm->bbm.bb_size);

	 
	rc = virtio_mem_create_resource(vm);
	if (rc)
		return rc;

	 
	if (vm->in_sbm)
		unit_pages = PHYS_PFN(memory_block_size_bytes());
	else
		unit_pages = PHYS_PFN(vm->bbm.bb_size);
	rc = memory_group_register_dynamic(vm->nid, unit_pages);
	if (rc < 0)
		goto out_del_resource;
	vm->mgid = rc;

	 
	if (vm->plugged_size) {
		vm->unplug_all_required = true;
		dev_info(&vm->vdev->dev, "unplugging all memory is required\n");
	}

	 
	vm->memory_notifier.notifier_call = virtio_mem_memory_notifier_cb;
	rc = register_memory_notifier(&vm->memory_notifier);
	if (rc)
		goto out_unreg_group;
	rc = register_virtio_mem_device(vm);
	if (rc)
		goto out_unreg_mem;

	return 0;
out_unreg_mem:
	unregister_memory_notifier(&vm->memory_notifier);
out_unreg_group:
	memory_group_unregister(vm->mgid);
out_del_resource:
	virtio_mem_delete_resource(vm);
	return rc;
}

#ifdef CONFIG_PROC_VMCORE
static int virtio_mem_send_state_request(struct virtio_mem *vm, uint64_t addr,
					 uint64_t size)
{
	const uint64_t nb_vm_blocks = size / vm->device_block_size;
	const struct virtio_mem_req req = {
		.type = cpu_to_virtio16(vm->vdev, VIRTIO_MEM_REQ_STATE),
		.u.state.addr = cpu_to_virtio64(vm->vdev, addr),
		.u.state.nb_blocks = cpu_to_virtio16(vm->vdev, nb_vm_blocks),
	};
	int rc = -ENOMEM;

	dev_dbg(&vm->vdev->dev, "requesting state: 0x%llx - 0x%llx\n", addr,
		addr + size - 1);

	switch (virtio_mem_send_request(vm, &req)) {
	case VIRTIO_MEM_RESP_ACK:
		return virtio16_to_cpu(vm->vdev, vm->resp.u.state.state);
	case VIRTIO_MEM_RESP_ERROR:
		rc = -EINVAL;
		break;
	default:
		break;
	}

	dev_dbg(&vm->vdev->dev, "requesting state failed: %d\n", rc);
	return rc;
}

static bool virtio_mem_vmcore_pfn_is_ram(struct vmcore_cb *cb,
					 unsigned long pfn)
{
	struct virtio_mem *vm = container_of(cb, struct virtio_mem,
					     vmcore_cb);
	uint64_t addr = PFN_PHYS(pfn);
	bool is_ram;
	int rc;

	if (!virtio_mem_contains_range(vm, addr, PAGE_SIZE))
		return true;
	if (!vm->plugged_size)
		return false;

	 
	mutex_lock(&vm->hotplug_mutex);

	addr = ALIGN_DOWN(addr, vm->device_block_size);
	if (addr != vm->last_block_addr) {
		rc = virtio_mem_send_state_request(vm, addr,
						   vm->device_block_size);
		 
		if (rc == VIRTIO_MEM_STATE_PLUGGED)
			vm->last_block_plugged = true;
		else
			vm->last_block_plugged = false;
		vm->last_block_addr = addr;
	}

	is_ram = vm->last_block_plugged;
	mutex_unlock(&vm->hotplug_mutex);
	return is_ram;
}
#endif  

static int virtio_mem_init_kdump(struct virtio_mem *vm)
{
#ifdef CONFIG_PROC_VMCORE
	dev_info(&vm->vdev->dev, "memory hot(un)plug disabled in kdump kernel\n");
	vm->vmcore_cb.pfn_is_ram = virtio_mem_vmcore_pfn_is_ram;
	register_vmcore_cb(&vm->vmcore_cb);
	return 0;
#else  
	dev_warn(&vm->vdev->dev, "disabled in kdump kernel without vmcore\n");
	return -EBUSY;
#endif  
}

static int virtio_mem_init(struct virtio_mem *vm)
{
	uint16_t node_id;

	if (!vm->vdev->config->get) {
		dev_err(&vm->vdev->dev, "config access disabled\n");
		return -EINVAL;
	}

	 
	virtio_cread_le(vm->vdev, struct virtio_mem_config, plugged_size,
			&vm->plugged_size);
	virtio_cread_le(vm->vdev, struct virtio_mem_config, block_size,
			&vm->device_block_size);
	virtio_cread_le(vm->vdev, struct virtio_mem_config, node_id,
			&node_id);
	vm->nid = virtio_mem_translate_node_id(vm, node_id);
	virtio_cread_le(vm->vdev, struct virtio_mem_config, addr, &vm->addr);
	virtio_cread_le(vm->vdev, struct virtio_mem_config, region_size,
			&vm->region_size);

	 
	if (vm->nid == NUMA_NO_NODE)
		vm->nid = memory_add_physaddr_to_nid(vm->addr);

	dev_info(&vm->vdev->dev, "start address: 0x%llx", vm->addr);
	dev_info(&vm->vdev->dev, "region size: 0x%llx", vm->region_size);
	dev_info(&vm->vdev->dev, "device block size: 0x%llx",
		 (unsigned long long)vm->device_block_size);
	if (vm->nid != NUMA_NO_NODE && IS_ENABLED(CONFIG_NUMA))
		dev_info(&vm->vdev->dev, "nid: %d", vm->nid);

	 
	if (vm->in_kdump)
		return virtio_mem_init_kdump(vm);
	return virtio_mem_init_hotplug(vm);
}

static int virtio_mem_create_resource(struct virtio_mem *vm)
{
	 
	const char *name = kstrdup(dev_name(&vm->vdev->dev), GFP_KERNEL);

	if (!name)
		return -ENOMEM;

	 
	vm->parent_resource = __request_mem_region(vm->addr, vm->region_size,
						   name, IORESOURCE_SYSTEM_RAM |
						   IORESOURCE_EXCLUSIVE);
	if (!vm->parent_resource) {
		kfree(name);
		dev_warn(&vm->vdev->dev, "could not reserve device region\n");
		dev_info(&vm->vdev->dev,
			 "reloading the driver is not supported\n");
		return -EBUSY;
	}

	 
	vm->parent_resource->flags &= ~IORESOURCE_BUSY;
	return 0;
}

static void virtio_mem_delete_resource(struct virtio_mem *vm)
{
	const char *name;

	if (!vm->parent_resource)
		return;

	name = vm->parent_resource->name;
	release_resource(vm->parent_resource);
	kfree(vm->parent_resource);
	kfree(name);
	vm->parent_resource = NULL;
}

static int virtio_mem_range_has_system_ram(struct resource *res, void *arg)
{
	return 1;
}

static bool virtio_mem_has_memory_added(struct virtio_mem *vm)
{
	const unsigned long flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	return walk_iomem_res_desc(IORES_DESC_NONE, flags, vm->addr,
				   vm->addr + vm->region_size, NULL,
				   virtio_mem_range_has_system_ram) == 1;
}

static int virtio_mem_probe(struct virtio_device *vdev)
{
	struct virtio_mem *vm;
	int rc;

	BUILD_BUG_ON(sizeof(struct virtio_mem_req) != 24);
	BUILD_BUG_ON(sizeof(struct virtio_mem_resp) != 10);

	vdev->priv = vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	init_waitqueue_head(&vm->host_resp);
	vm->vdev = vdev;
	INIT_WORK(&vm->wq, virtio_mem_run_wq);
	mutex_init(&vm->hotplug_mutex);
	INIT_LIST_HEAD(&vm->next);
	spin_lock_init(&vm->removal_lock);
	hrtimer_init(&vm->retry_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vm->retry_timer.function = virtio_mem_timer_expired;
	vm->retry_timer_ms = VIRTIO_MEM_RETRY_TIMER_MIN_MS;
	vm->in_kdump = is_kdump_kernel();

	 
	rc = virtio_mem_init_vq(vm);
	if (rc)
		goto out_free_vm;

	 
	rc = virtio_mem_init(vm);
	if (rc)
		goto out_del_vq;

	virtio_device_ready(vdev);

	 
	if (!vm->in_kdump) {
		atomic_set(&vm->config_changed, 1);
		queue_work(system_freezable_wq, &vm->wq);
	}

	return 0;
out_del_vq:
	vdev->config->del_vqs(vdev);
out_free_vm:
	kfree(vm);
	vdev->priv = NULL;

	return rc;
}

static void virtio_mem_deinit_hotplug(struct virtio_mem *vm)
{
	unsigned long mb_id;
	int rc;

	 
	mutex_lock(&vm->hotplug_mutex);
	spin_lock_irq(&vm->removal_lock);
	vm->removing = true;
	spin_unlock_irq(&vm->removal_lock);
	mutex_unlock(&vm->hotplug_mutex);

	 
	cancel_work_sync(&vm->wq);
	hrtimer_cancel(&vm->retry_timer);

	if (vm->in_sbm) {
		 
		virtio_mem_sbm_for_each_mb(vm, mb_id,
					   VIRTIO_MEM_SBM_MB_OFFLINE_PARTIAL) {
			rc = virtio_mem_sbm_remove_mb(vm, mb_id);
			BUG_ON(rc);
			virtio_mem_sbm_set_mb_state(vm, mb_id,
						    VIRTIO_MEM_SBM_MB_UNUSED);
		}
		 
	}

	 
	unregister_virtio_mem_device(vm);
	unregister_memory_notifier(&vm->memory_notifier);

	 
	if (virtio_mem_has_memory_added(vm)) {
		dev_warn(&vm->vdev->dev,
			 "device still has system memory added\n");
	} else {
		virtio_mem_delete_resource(vm);
		kfree_const(vm->resource_name);
		memory_group_unregister(vm->mgid);
	}

	 
	if (vm->in_sbm) {
		vfree(vm->sbm.mb_states);
		vfree(vm->sbm.sb_states);
	} else {
		vfree(vm->bbm.bb_states);
	}
}

static void virtio_mem_deinit_kdump(struct virtio_mem *vm)
{
#ifdef CONFIG_PROC_VMCORE
	unregister_vmcore_cb(&vm->vmcore_cb);
#endif  
}

static void virtio_mem_remove(struct virtio_device *vdev)
{
	struct virtio_mem *vm = vdev->priv;

	if (vm->in_kdump)
		virtio_mem_deinit_kdump(vm);
	else
		virtio_mem_deinit_hotplug(vm);

	 
	virtio_reset_device(vdev);
	vdev->config->del_vqs(vdev);

	kfree(vm);
	vdev->priv = NULL;
}

static void virtio_mem_config_changed(struct virtio_device *vdev)
{
	struct virtio_mem *vm = vdev->priv;

	if (unlikely(vm->in_kdump))
		return;

	atomic_set(&vm->config_changed, 1);
	virtio_mem_retry(vm);
}

#ifdef CONFIG_PM_SLEEP
static int virtio_mem_freeze(struct virtio_device *vdev)
{
	 
	dev_err(&vdev->dev, "save/restore not supported.\n");
	return -EPERM;
}

static int virtio_mem_restore(struct virtio_device *vdev)
{
	return -EPERM;
}
#endif

static unsigned int virtio_mem_features[] = {
#if defined(CONFIG_NUMA) && defined(CONFIG_ACPI_NUMA)
	VIRTIO_MEM_F_ACPI_PXM,
#endif
	VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE,
};

static const struct virtio_device_id virtio_mem_id_table[] = {
	{ VIRTIO_ID_MEM, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_mem_driver = {
	.feature_table = virtio_mem_features,
	.feature_table_size = ARRAY_SIZE(virtio_mem_features),
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
	.id_table = virtio_mem_id_table,
	.probe = virtio_mem_probe,
	.remove = virtio_mem_remove,
	.config_changed = virtio_mem_config_changed,
#ifdef CONFIG_PM_SLEEP
	.freeze	=	virtio_mem_freeze,
	.restore =	virtio_mem_restore,
#endif
};

module_virtio_driver(virtio_mem_driver);
MODULE_DEVICE_TABLE(virtio, virtio_mem_id_table);
MODULE_AUTHOR("David Hildenbrand <david@redhat.com>");
MODULE_DESCRIPTION("Virtio-mem driver");
MODULE_LICENSE("GPL");
