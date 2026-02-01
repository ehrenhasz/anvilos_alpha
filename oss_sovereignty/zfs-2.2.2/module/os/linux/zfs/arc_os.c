 
 

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/spa_impl.h>
#include <sys/zio_compress.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/zfs_refcount.h>
#include <sys/vdev.h>
#include <sys/vdev_trim.h>
#include <sys/vdev_impl.h>
#include <sys/dsl_pool.h>
#include <sys/multilist.h>
#include <sys/abd.h>
#include <sys/zil.h>
#include <sys/fm/fs/zfs.h>
#ifdef _KERNEL
#include <sys/shrinker.h>
#include <sys/vmsystm.h>
#include <sys/zpl.h>
#include <linux/page_compat.h>
#include <linux/notifier.h>
#include <linux/memory.h>
#endif
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/zthr.h>
#include <zfs_fletcher.h>
#include <sys/arc_impl.h>
#include <sys/trace_zfs.h>
#include <sys/aggsum.h>

 
int zfs_arc_shrinker_limit = 10000;

#ifdef CONFIG_MEMORY_HOTPLUG
static struct notifier_block arc_hotplug_callback_mem_nb;
#endif

 
uint64_t
arc_default_max(uint64_t min, uint64_t allmem)
{
	 
	return (MAX(allmem / 2, min));
}

#ifdef _KERNEL
 
uint64_t
arc_all_memory(void)
{
#ifdef CONFIG_HIGHMEM
	return (ptob(zfs_totalram_pages - zfs_totalhigh_pages));
#else
	return (ptob(zfs_totalram_pages));
#endif  
}

 
uint64_t
arc_free_memory(void)
{
#ifdef CONFIG_HIGHMEM
	struct sysinfo si;
	si_meminfo(&si);
	return (ptob(si.freeram - si.freehigh));
#else
	return (ptob(nr_free_pages() +
	    nr_inactive_file_pages()));
#endif  
}

 
int64_t
arc_available_memory(void)
{
	return (arc_free_memory() - arc_sys_free);
}

static uint64_t
arc_evictable_memory(void)
{
	int64_t asize = aggsum_value(&arc_sums.arcstat_size);
	uint64_t arc_clean =
	    zfs_refcount_count(&arc_mru->arcs_esize[ARC_BUFC_DATA]) +
	    zfs_refcount_count(&arc_mru->arcs_esize[ARC_BUFC_METADATA]) +
	    zfs_refcount_count(&arc_mfu->arcs_esize[ARC_BUFC_DATA]) +
	    zfs_refcount_count(&arc_mfu->arcs_esize[ARC_BUFC_METADATA]);
	uint64_t arc_dirty = MAX((int64_t)asize - (int64_t)arc_clean, 0);

	 
	uint64_t min = (ptob(nr_file_pages()) / 100) * zfs_arc_pc_percent;
	min = MAX(arc_c_min, MIN(arc_c_max, min));

	if (arc_dirty >= min)
		return (arc_clean);

	return (MAX((int64_t)asize - (int64_t)min, 0));
}

 
static unsigned long
arc_shrinker_count(struct shrinker *shrink, struct shrink_control *sc)
{
	 
	if (!(sc->gfp_mask & __GFP_FS)) {
		return (0);
	}

	 
	int64_t limit = zfs_arc_shrinker_limit != 0 ?
	    zfs_arc_shrinker_limit : INT64_MAX;
	return (MIN(limit, btop((int64_t)arc_evictable_memory())));
}

static unsigned long
arc_shrinker_scan(struct shrinker *shrink, struct shrink_control *sc)
{
	ASSERT((sc->gfp_mask & __GFP_FS) != 0);

	 
	if (unlikely(arc_warm == B_FALSE))
		arc_warm = B_TRUE;

	 
	arc_reduce_target_size(ptob(sc->nr_to_scan));
	arc_wait_for_eviction(ptob(sc->nr_to_scan), B_FALSE);
	if (current->reclaim_state != NULL)
#ifdef	HAVE_RECLAIM_STATE_RECLAIMED
		current->reclaim_state->reclaimed += sc->nr_to_scan;
#else
		current->reclaim_state->reclaimed_slab += sc->nr_to_scan;
#endif

	 
	arc_no_grow = B_TRUE;

	 
	if (current_is_kswapd()) {
		ARCSTAT_BUMP(arcstat_memory_indirect_count);
	} else {
		ARCSTAT_BUMP(arcstat_memory_direct_count);
	}

	return (sc->nr_to_scan);
}

SPL_SHRINKER_DECLARE(arc_shrinker,
    arc_shrinker_count, arc_shrinker_scan, DEFAULT_SEEKS);

int
arc_memory_throttle(spa_t *spa, uint64_t reserve, uint64_t txg)
{
	uint64_t free_memory = arc_free_memory();

	if (free_memory > arc_all_memory() * arc_lotsfree_percent / 100)
		return (0);

	if (txg > spa->spa_lowmem_last_txg) {
		spa->spa_lowmem_last_txg = txg;
		spa->spa_lowmem_page_load = 0;
	}
	 
	if (current_is_kswapd()) {
		if (spa->spa_lowmem_page_load >
		    MAX(arc_sys_free / 4, free_memory) / 4) {
			DMU_TX_STAT_BUMP(dmu_tx_memory_reclaim);
			return (SET_ERROR(ERESTART));
		}
		 
		atomic_add_64(&spa->spa_lowmem_page_load, reserve / 8);
		return (0);
	} else if (spa->spa_lowmem_page_load > 0 && arc_reclaim_needed()) {
		 
		ARCSTAT_INCR(arcstat_memory_throttle_count, 1);
		DMU_TX_STAT_BUMP(dmu_tx_memory_reclaim);
		return (SET_ERROR(EAGAIN));
	}
	spa->spa_lowmem_page_load = 0;
	return (0);
}

static void
arc_set_sys_free(uint64_t allmem)
{
	 

	 
	long wmark = 4 * int_sqrt(allmem/1024) * 1024;

	 
	wmark = MAX(wmark, 128 * 1024);
	wmark = MIN(wmark, 64 * 1024 * 1024);

	 
	wmark += wmark * 150 / 100;

	 
	arc_sys_free = wmark * 3 + allmem / 32;
}

void
arc_lowmem_init(void)
{
	uint64_t allmem = arc_all_memory();

	 
	spl_register_shrinker(&arc_shrinker);
	arc_set_sys_free(allmem);
}

void
arc_lowmem_fini(void)
{
	spl_unregister_shrinker(&arc_shrinker);
}

int
param_set_arc_u64(const char *buf, zfs_kernel_param_t *kp)
{
	int error;

	error = spl_param_set_u64(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));

	arc_tuning_update(B_TRUE);

	return (0);
}

int
param_set_arc_min(const char *buf, zfs_kernel_param_t *kp)
{
	return (param_set_arc_u64(buf, kp));
}

int
param_set_arc_max(const char *buf, zfs_kernel_param_t *kp)
{
	return (param_set_arc_u64(buf, kp));
}

int
param_set_arc_int(const char *buf, zfs_kernel_param_t *kp)
{
	int error;

	error = param_set_int(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));

	arc_tuning_update(B_TRUE);

	return (0);
}

#ifdef CONFIG_MEMORY_HOTPLUG
static int
arc_hotplug_callback(struct notifier_block *self, unsigned long action,
    void *arg)
{
	(void) self, (void) arg;
	uint64_t allmem = arc_all_memory();
	if (action != MEM_ONLINE)
		return (NOTIFY_OK);

	arc_set_limits(allmem);

#ifdef __LP64__
	if (zfs_dirty_data_max_max == 0)
		zfs_dirty_data_max_max = MIN(4ULL * 1024 * 1024 * 1024,
		    allmem * zfs_dirty_data_max_max_percent / 100);
#else
	if (zfs_dirty_data_max_max == 0)
		zfs_dirty_data_max_max = MIN(1ULL * 1024 * 1024 * 1024,
		    allmem * zfs_dirty_data_max_max_percent / 100);
#endif

	arc_set_sys_free(allmem);
	return (NOTIFY_OK);
}
#endif

void
arc_register_hotplug(void)
{
#ifdef CONFIG_MEMORY_HOTPLUG
	arc_hotplug_callback_mem_nb.notifier_call = arc_hotplug_callback;
	 
	arc_hotplug_callback_mem_nb.priority = 100;
	register_memory_notifier(&arc_hotplug_callback_mem_nb);
#endif
}

void
arc_unregister_hotplug(void)
{
#ifdef CONFIG_MEMORY_HOTPLUG
	unregister_memory_notifier(&arc_hotplug_callback_mem_nb);
#endif
}
#else  
int64_t
arc_available_memory(void)
{
	int64_t lowest = INT64_MAX;

	 
	if (random_in_range(100) == 0)
		lowest = -1024;

	return (lowest);
}

int
arc_memory_throttle(spa_t *spa, uint64_t reserve, uint64_t txg)
{
	(void) spa, (void) reserve, (void) txg;
	return (0);
}

uint64_t
arc_all_memory(void)
{
	return (ptob(physmem) / 2);
}

uint64_t
arc_free_memory(void)
{
	return (random_in_range(arc_all_memory() * 20 / 100));
}

void
arc_register_hotplug(void)
{
}

void
arc_unregister_hotplug(void)
{
}
#endif  

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, shrinker_limit, INT, ZMOD_RW,
	"Limit on number of pages that ARC shrinker can reclaim at once");
