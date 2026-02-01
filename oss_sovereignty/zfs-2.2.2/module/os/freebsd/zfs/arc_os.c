 

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/spa_impl.h>
#include <sys/counter.h>
#include <sys/zio_compress.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/arc_os.h>
#include <sys/zfs_refcount.h>
#include <sys/vdev.h>
#include <sys/vdev_trim.h>
#include <sys/vdev_impl.h>
#include <sys/dsl_pool.h>
#include <sys/zio_checksum.h>
#include <sys/multilist.h>
#include <sys/abd.h>
#include <sys/zil.h>
#include <sys/fm/fs/zfs.h>
#include <sys/eventhandler.h>
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/zthr.h>
#include <zfs_fletcher.h>
#include <sys/arc_impl.h>
#include <sys/sdt.h>
#include <sys/aggsum.h>
#include <sys/vnode.h>
#include <cityhash.h>
#include <machine/vmparam.h>
#include <sys/vm.h>
#include <sys/vmmeter.h>

extern struct vfsops zfs_vfsops;

uint_t zfs_arc_free_target = 0;

static void
arc_free_target_init(void *unused __unused)
{
	zfs_arc_free_target = vm_cnt.v_free_target;
}
SYSINIT(arc_free_target_init, SI_SUB_KTHREAD_PAGE, SI_ORDER_ANY,
    arc_free_target_init, NULL);

 
ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, free_target,
    param_set_arc_free_target, 0, CTLFLAG_RW,
	"Desired number of free pages below which ARC triggers reclaim");
ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, no_grow_shift,
    param_set_arc_no_grow_shift, 0, ZMOD_RW,
	"log2(fraction of ARC which must be free to allow growing)");

int64_t
arc_available_memory(void)
{
	int64_t lowest = INT64_MAX;
	int64_t n __unused;

	 
	n = PAGESIZE * ((int64_t)freemem - zfs_arc_free_target);
	if (n < lowest) {
		lowest = n;
	}
#if defined(__i386) || !defined(UMA_MD_SMALL_ALLOC)
	 
	n = uma_avail() - (long)(uma_limit() / 4);
	if (n < lowest) {
		lowest = n;
	}
#endif

	DTRACE_PROBE1(arc__available_memory, int64_t, lowest);
	return (lowest);
}

 
uint64_t
arc_default_max(uint64_t min, uint64_t allmem)
{
	uint64_t size;

	if (allmem >= 1 << 30)
		size = allmem - (1 << 30);
	else
		size = min;
	return (MAX(allmem * 5 / 8, size));
}

uint64_t
arc_all_memory(void)
{
	return (ptob(physmem));
}

int
arc_memory_throttle(spa_t *spa, uint64_t reserve, uint64_t txg)
{
	return (0);
}

uint64_t
arc_free_memory(void)
{
	return (ptob(freemem));
}

static eventhandler_tag arc_event_lowmem = NULL;

static void
arc_lowmem(void *arg __unused, int howto __unused)
{
	int64_t free_memory, to_free;

	arc_no_grow = B_TRUE;
	arc_warm = B_TRUE;
	arc_growtime = gethrtime() + SEC2NSEC(arc_grow_retry);
	free_memory = arc_available_memory();
	int64_t can_free = arc_c - arc_c_min;
	if (can_free <= 0)
		return;
	to_free = (can_free >> arc_shrink_shift) - MIN(free_memory, 0);
	DTRACE_PROBE2(arc__needfree, int64_t, free_memory, int64_t, to_free);
	arc_reduce_target_size(to_free);

	 
	if (curproc == pageproc)
		arc_wait_for_eviction(to_free, B_FALSE);
}

void
arc_lowmem_init(void)
{
	arc_event_lowmem = EVENTHANDLER_REGISTER(vm_lowmem, arc_lowmem, NULL,
	    EVENTHANDLER_PRI_FIRST);
}

void
arc_lowmem_fini(void)
{
	if (arc_event_lowmem != NULL)
		EVENTHANDLER_DEREGISTER(vm_lowmem, arc_event_lowmem);
}

void
arc_register_hotplug(void)
{
}

void
arc_unregister_hotplug(void)
{
}
