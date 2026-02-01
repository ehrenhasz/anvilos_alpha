 
 

 

#include <sys/zfs_context.h>
#include <sys/vdev_impl.h>
#include <sys/spa_impl.h>
#include <sys/zio.h>
#include <sys/avl.h>
#include <sys/dsl_pool.h>
#include <sys/metaslab_impl.h>
#include <sys/spa.h>
#include <sys/abd.h>

 

 
uint_t zfs_vdev_max_active = 1000;

 
static uint_t zfs_vdev_sync_read_min_active = 10;
static uint_t zfs_vdev_sync_read_max_active = 10;
static uint_t zfs_vdev_sync_write_min_active = 10;
static uint_t zfs_vdev_sync_write_max_active = 10;
static uint_t zfs_vdev_async_read_min_active = 1;
  uint_t zfs_vdev_async_read_max_active = 3;
static uint_t zfs_vdev_async_write_min_active = 2;
  uint_t zfs_vdev_async_write_max_active = 10;
static uint_t zfs_vdev_scrub_min_active = 1;
static uint_t zfs_vdev_scrub_max_active = 3;
static uint_t zfs_vdev_removal_min_active = 1;
static uint_t zfs_vdev_removal_max_active = 2;
static uint_t zfs_vdev_initializing_min_active = 1;
static uint_t zfs_vdev_initializing_max_active = 1;
static uint_t zfs_vdev_trim_min_active = 1;
static uint_t zfs_vdev_trim_max_active = 2;
static uint_t zfs_vdev_rebuild_min_active = 1;
static uint_t zfs_vdev_rebuild_max_active = 3;

 
uint_t zfs_vdev_async_write_active_min_dirty_percent = 30;
uint_t zfs_vdev_async_write_active_max_dirty_percent = 60;

 
static uint_t zfs_vdev_nia_delay = 5;

 
static uint_t zfs_vdev_nia_credit = 5;

 
static uint_t zfs_vdev_aggregation_limit = 1 << 20;
static uint_t zfs_vdev_aggregation_limit_non_rotating = SPA_OLD_MAXBLOCKSIZE;
static uint_t zfs_vdev_read_gap_limit = 32 << 10;
static uint_t zfs_vdev_write_gap_limit = 4 << 10;

 
#ifdef _KERNEL
uint_t zfs_vdev_queue_depth_pct = 1000;
#else
uint_t zfs_vdev_queue_depth_pct = 300;
#endif

 
uint_t zfs_vdev_def_queue_depth = 32;

static int
vdev_queue_offset_compare(const void *x1, const void *x2)
{
	const zio_t *z1 = (const zio_t *)x1;
	const zio_t *z2 = (const zio_t *)x2;

	int cmp = TREE_CMP(z1->io_offset, z2->io_offset);

	if (likely(cmp))
		return (cmp);

	return (TREE_PCMP(z1, z2));
}

#define	VDQ_T_SHIFT 29

static int
vdev_queue_to_compare(const void *x1, const void *x2)
{
	const zio_t *z1 = (const zio_t *)x1;
	const zio_t *z2 = (const zio_t *)x2;

	int tcmp = TREE_CMP(z1->io_timestamp >> VDQ_T_SHIFT,
	    z2->io_timestamp >> VDQ_T_SHIFT);
	int ocmp = TREE_CMP(z1->io_offset, z2->io_offset);
	int cmp = tcmp ? tcmp : ocmp;

	if (likely(cmp | (z1->io_queue_state == ZIO_QS_NONE)))
		return (cmp);

	return (TREE_PCMP(z1, z2));
}

static inline boolean_t
vdev_queue_class_fifo(zio_priority_t p)
{
	return (p == ZIO_PRIORITY_SYNC_READ || p == ZIO_PRIORITY_SYNC_WRITE ||
	    p == ZIO_PRIORITY_TRIM);
}

static void
vdev_queue_class_add(vdev_queue_t *vq, zio_t *zio)
{
	zio_priority_t p = zio->io_priority;
	vq->vq_cqueued |= 1U << p;
	if (vdev_queue_class_fifo(p)) {
		list_insert_tail(&vq->vq_class[p].vqc_list, zio);
		vq->vq_class[p].vqc_list_numnodes++;
	}
	else
		avl_add(&vq->vq_class[p].vqc_tree, zio);
}

static void
vdev_queue_class_remove(vdev_queue_t *vq, zio_t *zio)
{
	zio_priority_t p = zio->io_priority;
	uint32_t empty;
	if (vdev_queue_class_fifo(p)) {
		list_t *list = &vq->vq_class[p].vqc_list;
		list_remove(list, zio);
		empty = list_is_empty(list);
		vq->vq_class[p].vqc_list_numnodes--;
	} else {
		avl_tree_t *tree = &vq->vq_class[p].vqc_tree;
		avl_remove(tree, zio);
		empty = avl_is_empty(tree);
	}
	vq->vq_cqueued &= ~(empty << p);
}

static uint_t
vdev_queue_class_min_active(vdev_queue_t *vq, zio_priority_t p)
{
	switch (p) {
	case ZIO_PRIORITY_SYNC_READ:
		return (zfs_vdev_sync_read_min_active);
	case ZIO_PRIORITY_SYNC_WRITE:
		return (zfs_vdev_sync_write_min_active);
	case ZIO_PRIORITY_ASYNC_READ:
		return (zfs_vdev_async_read_min_active);
	case ZIO_PRIORITY_ASYNC_WRITE:
		return (zfs_vdev_async_write_min_active);
	case ZIO_PRIORITY_SCRUB:
		return (vq->vq_ia_active == 0 ? zfs_vdev_scrub_min_active :
		    MIN(vq->vq_nia_credit, zfs_vdev_scrub_min_active));
	case ZIO_PRIORITY_REMOVAL:
		return (vq->vq_ia_active == 0 ? zfs_vdev_removal_min_active :
		    MIN(vq->vq_nia_credit, zfs_vdev_removal_min_active));
	case ZIO_PRIORITY_INITIALIZING:
		return (vq->vq_ia_active == 0 ?zfs_vdev_initializing_min_active:
		    MIN(vq->vq_nia_credit, zfs_vdev_initializing_min_active));
	case ZIO_PRIORITY_TRIM:
		return (zfs_vdev_trim_min_active);
	case ZIO_PRIORITY_REBUILD:
		return (vq->vq_ia_active == 0 ? zfs_vdev_rebuild_min_active :
		    MIN(vq->vq_nia_credit, zfs_vdev_rebuild_min_active));
	default:
		panic("invalid priority %u", p);
		return (0);
	}
}

static uint_t
vdev_queue_max_async_writes(spa_t *spa)
{
	uint_t writes;
	uint64_t dirty = 0;
	dsl_pool_t *dp = spa_get_dsl(spa);
	uint64_t min_bytes = zfs_dirty_data_max *
	    zfs_vdev_async_write_active_min_dirty_percent / 100;
	uint64_t max_bytes = zfs_dirty_data_max *
	    zfs_vdev_async_write_active_max_dirty_percent / 100;

	 
	if (dp == NULL)
		return (zfs_vdev_async_write_max_active);

	 
	dirty = dp->dp_dirty_total;
	if (dirty > max_bytes || spa_has_pending_synctask(spa))
		return (zfs_vdev_async_write_max_active);

	if (dirty < min_bytes)
		return (zfs_vdev_async_write_min_active);

	 
	writes = (dirty - min_bytes) *
	    (zfs_vdev_async_write_max_active -
	    zfs_vdev_async_write_min_active) /
	    (max_bytes - min_bytes) +
	    zfs_vdev_async_write_min_active;
	ASSERT3U(writes, >=, zfs_vdev_async_write_min_active);
	ASSERT3U(writes, <=, zfs_vdev_async_write_max_active);
	return (writes);
}

static uint_t
vdev_queue_class_max_active(vdev_queue_t *vq, zio_priority_t p)
{
	switch (p) {
	case ZIO_PRIORITY_SYNC_READ:
		return (zfs_vdev_sync_read_max_active);
	case ZIO_PRIORITY_SYNC_WRITE:
		return (zfs_vdev_sync_write_max_active);
	case ZIO_PRIORITY_ASYNC_READ:
		return (zfs_vdev_async_read_max_active);
	case ZIO_PRIORITY_ASYNC_WRITE:
		return (vdev_queue_max_async_writes(vq->vq_vdev->vdev_spa));
	case ZIO_PRIORITY_SCRUB:
		if (vq->vq_ia_active > 0) {
			return (MIN(vq->vq_nia_credit,
			    zfs_vdev_scrub_min_active));
		} else if (vq->vq_nia_credit < zfs_vdev_nia_delay)
			return (MAX(1, zfs_vdev_scrub_min_active));
		return (zfs_vdev_scrub_max_active);
	case ZIO_PRIORITY_REMOVAL:
		if (vq->vq_ia_active > 0) {
			return (MIN(vq->vq_nia_credit,
			    zfs_vdev_removal_min_active));
		} else if (vq->vq_nia_credit < zfs_vdev_nia_delay)
			return (MAX(1, zfs_vdev_removal_min_active));
		return (zfs_vdev_removal_max_active);
	case ZIO_PRIORITY_INITIALIZING:
		if (vq->vq_ia_active > 0) {
			return (MIN(vq->vq_nia_credit,
			    zfs_vdev_initializing_min_active));
		} else if (vq->vq_nia_credit < zfs_vdev_nia_delay)
			return (MAX(1, zfs_vdev_initializing_min_active));
		return (zfs_vdev_initializing_max_active);
	case ZIO_PRIORITY_TRIM:
		return (zfs_vdev_trim_max_active);
	case ZIO_PRIORITY_REBUILD:
		if (vq->vq_ia_active > 0) {
			return (MIN(vq->vq_nia_credit,
			    zfs_vdev_rebuild_min_active));
		} else if (vq->vq_nia_credit < zfs_vdev_nia_delay)
			return (MAX(1, zfs_vdev_rebuild_min_active));
		return (zfs_vdev_rebuild_max_active);
	default:
		panic("invalid priority %u", p);
		return (0);
	}
}

 
static zio_priority_t
vdev_queue_class_to_issue(vdev_queue_t *vq)
{
	uint32_t cq = vq->vq_cqueued;
	zio_priority_t p, p1;

	if (cq == 0 || vq->vq_active >= zfs_vdev_max_active)
		return (ZIO_PRIORITY_NUM_QUEUEABLE);

	 
	p1 = vq->vq_last_prio + 1;
	if (p1 >= ZIO_PRIORITY_NUM_QUEUEABLE)
		p1 = 0;
	for (p = p1; p < ZIO_PRIORITY_NUM_QUEUEABLE; p++) {
		if ((cq & (1U << p)) != 0 && vq->vq_cactive[p] <
		    vdev_queue_class_min_active(vq, p))
			goto found;
	}
	for (p = 0; p < p1; p++) {
		if ((cq & (1U << p)) != 0 && vq->vq_cactive[p] <
		    vdev_queue_class_min_active(vq, p))
			goto found;
	}

	 
	for (p = 0; p < ZIO_PRIORITY_NUM_QUEUEABLE; p++) {
		if ((cq & (1U << p)) != 0 && vq->vq_cactive[p] <
		    vdev_queue_class_max_active(vq, p))
			break;
	}

found:
	vq->vq_last_prio = p;
	return (p);
}

void
vdev_queue_init(vdev_t *vd)
{
	vdev_queue_t *vq = &vd->vdev_queue;
	zio_priority_t p;

	vq->vq_vdev = vd;

	for (p = 0; p < ZIO_PRIORITY_NUM_QUEUEABLE; p++) {
		if (vdev_queue_class_fifo(p)) {
			list_create(&vq->vq_class[p].vqc_list,
			    sizeof (zio_t),
			    offsetof(struct zio, io_queue_node.l));
		} else {
			avl_create(&vq->vq_class[p].vqc_tree,
			    vdev_queue_to_compare, sizeof (zio_t),
			    offsetof(struct zio, io_queue_node.a));
		}
	}
	avl_create(&vq->vq_read_offset_tree,
	    vdev_queue_offset_compare, sizeof (zio_t),
	    offsetof(struct zio, io_offset_node));
	avl_create(&vq->vq_write_offset_tree,
	    vdev_queue_offset_compare, sizeof (zio_t),
	    offsetof(struct zio, io_offset_node));

	vq->vq_last_offset = 0;
	list_create(&vq->vq_active_list, sizeof (struct zio),
	    offsetof(struct zio, io_queue_node.l));
	mutex_init(&vq->vq_lock, NULL, MUTEX_DEFAULT, NULL);
}

void
vdev_queue_fini(vdev_t *vd)
{
	vdev_queue_t *vq = &vd->vdev_queue;

	for (zio_priority_t p = 0; p < ZIO_PRIORITY_NUM_QUEUEABLE; p++) {
		if (vdev_queue_class_fifo(p))
			list_destroy(&vq->vq_class[p].vqc_list);
		else
			avl_destroy(&vq->vq_class[p].vqc_tree);
	}
	avl_destroy(&vq->vq_read_offset_tree);
	avl_destroy(&vq->vq_write_offset_tree);

	list_destroy(&vq->vq_active_list);
	mutex_destroy(&vq->vq_lock);
}

static void
vdev_queue_io_add(vdev_queue_t *vq, zio_t *zio)
{
	zio->io_queue_state = ZIO_QS_QUEUED;
	vdev_queue_class_add(vq, zio);
	if (zio->io_type == ZIO_TYPE_READ)
		avl_add(&vq->vq_read_offset_tree, zio);
	else if (zio->io_type == ZIO_TYPE_WRITE)
		avl_add(&vq->vq_write_offset_tree, zio);
}

static void
vdev_queue_io_remove(vdev_queue_t *vq, zio_t *zio)
{
	vdev_queue_class_remove(vq, zio);
	if (zio->io_type == ZIO_TYPE_READ)
		avl_remove(&vq->vq_read_offset_tree, zio);
	else if (zio->io_type == ZIO_TYPE_WRITE)
		avl_remove(&vq->vq_write_offset_tree, zio);
	zio->io_queue_state = ZIO_QS_NONE;
}

static boolean_t
vdev_queue_is_interactive(zio_priority_t p)
{
	switch (p) {
	case ZIO_PRIORITY_SCRUB:
	case ZIO_PRIORITY_REMOVAL:
	case ZIO_PRIORITY_INITIALIZING:
	case ZIO_PRIORITY_REBUILD:
		return (B_FALSE);
	default:
		return (B_TRUE);
	}
}

static void
vdev_queue_pending_add(vdev_queue_t *vq, zio_t *zio)
{
	ASSERT(MUTEX_HELD(&vq->vq_lock));
	ASSERT3U(zio->io_priority, <, ZIO_PRIORITY_NUM_QUEUEABLE);
	vq->vq_cactive[zio->io_priority]++;
	vq->vq_active++;
	if (vdev_queue_is_interactive(zio->io_priority)) {
		if (++vq->vq_ia_active == 1)
			vq->vq_nia_credit = 1;
	} else if (vq->vq_ia_active > 0) {
		vq->vq_nia_credit--;
	}
	zio->io_queue_state = ZIO_QS_ACTIVE;
	list_insert_tail(&vq->vq_active_list, zio);
}

static void
vdev_queue_pending_remove(vdev_queue_t *vq, zio_t *zio)
{
	ASSERT(MUTEX_HELD(&vq->vq_lock));
	ASSERT3U(zio->io_priority, <, ZIO_PRIORITY_NUM_QUEUEABLE);
	vq->vq_cactive[zio->io_priority]--;
	vq->vq_active--;
	if (vdev_queue_is_interactive(zio->io_priority)) {
		if (--vq->vq_ia_active == 0)
			vq->vq_nia_credit = 0;
		else
			vq->vq_nia_credit = zfs_vdev_nia_credit;
	} else if (vq->vq_ia_active == 0)
		vq->vq_nia_credit++;
	list_remove(&vq->vq_active_list, zio);
	zio->io_queue_state = ZIO_QS_NONE;
}

static void
vdev_queue_agg_io_done(zio_t *aio)
{
	abd_free(aio->io_abd);
}

 
#define	IO_SPAN(fio, lio) ((lio)->io_offset + (lio)->io_size - (fio)->io_offset)
#define	IO_GAP(fio, lio) (-IO_SPAN(lio, fio))

 
static zio_t *
vdev_queue_aggregate(vdev_queue_t *vq, zio_t *zio)
{
	zio_t *first, *last, *aio, *dio, *mandatory, *nio;
	uint64_t maxgap = 0;
	uint64_t size;
	uint64_t limit;
	boolean_t stretch = B_FALSE;
	uint64_t next_offset;
	abd_t *abd;
	avl_tree_t *t;

	 
	if (zio->io_type == ZIO_TYPE_TRIM)
		return (NULL);

	if (zio->io_flags & ZIO_FLAG_DONT_AGGREGATE)
		return (NULL);

	if (vq->vq_vdev->vdev_nonrot)
		limit = zfs_vdev_aggregation_limit_non_rotating;
	else
		limit = zfs_vdev_aggregation_limit;
	if (limit == 0)
		return (NULL);
	limit = MIN(limit, SPA_MAXBLOCKSIZE);

	 
	ASSERT(vq->vq_vdev->vdev_ops != &vdev_draid_spare_ops);

	first = last = zio;

	if (zio->io_type == ZIO_TYPE_READ) {
		maxgap = zfs_vdev_read_gap_limit;
		t = &vq->vq_read_offset_tree;
	} else {
		ASSERT3U(zio->io_type, ==, ZIO_TYPE_WRITE);
		t = &vq->vq_write_offset_tree;
	}

	 

	 
	mandatory = (first->io_flags & ZIO_FLAG_OPTIONAL) ? NULL : first;

	 
	zio_flag_t flags = zio->io_flags & ZIO_FLAG_AGG_INHERIT;
	while ((dio = AVL_PREV(t, first)) != NULL &&
	    (dio->io_flags & ZIO_FLAG_AGG_INHERIT) == flags &&
	    IO_SPAN(dio, last) <= limit &&
	    IO_GAP(dio, first) <= maxgap &&
	    dio->io_type == zio->io_type) {
		first = dio;
		if (mandatory == NULL && !(first->io_flags & ZIO_FLAG_OPTIONAL))
			mandatory = first;
	}

	 
	while ((first->io_flags & ZIO_FLAG_OPTIONAL) && first != last) {
		first = AVL_NEXT(t, first);
		ASSERT(first != NULL);
	}


	 
	while ((dio = AVL_NEXT(t, last)) != NULL &&
	    (dio->io_flags & ZIO_FLAG_AGG_INHERIT) == flags &&
	    (IO_SPAN(first, dio) <= limit ||
	    (dio->io_flags & ZIO_FLAG_OPTIONAL)) &&
	    IO_SPAN(first, dio) <= SPA_MAXBLOCKSIZE &&
	    IO_GAP(last, dio) <= maxgap &&
	    dio->io_type == zio->io_type) {
		last = dio;
		if (!(last->io_flags & ZIO_FLAG_OPTIONAL))
			mandatory = last;
	}

	 
	if (zio->io_type == ZIO_TYPE_WRITE && mandatory != NULL) {
		zio_t *nio = last;
		while ((dio = AVL_NEXT(t, nio)) != NULL &&
		    IO_GAP(nio, dio) == 0 &&
		    IO_GAP(mandatory, dio) <= zfs_vdev_write_gap_limit) {
			nio = dio;
			if (!(nio->io_flags & ZIO_FLAG_OPTIONAL)) {
				stretch = B_TRUE;
				break;
			}
		}
	}

	if (stretch) {
		 
		dio = AVL_NEXT(t, last);
		ASSERT3P(dio, !=, NULL);
		dio->io_flags &= ~ZIO_FLAG_OPTIONAL;
	} else {
		 
		while (last != mandatory && last != first) {
			ASSERT(last->io_flags & ZIO_FLAG_OPTIONAL);
			last = AVL_PREV(t, last);
			ASSERT(last != NULL);
		}
	}

	if (first == last)
		return (NULL);

	size = IO_SPAN(first, last);
	ASSERT3U(size, <=, SPA_MAXBLOCKSIZE);

	abd = abd_alloc_gang();
	if (abd == NULL)
		return (NULL);

	aio = zio_vdev_delegated_io(first->io_vd, first->io_offset,
	    abd, size, first->io_type, zio->io_priority,
	    flags | ZIO_FLAG_DONT_QUEUE, vdev_queue_agg_io_done, NULL);
	aio->io_timestamp = first->io_timestamp;

	nio = first;
	next_offset = first->io_offset;
	do {
		dio = nio;
		nio = AVL_NEXT(t, dio);
		ASSERT3P(dio, !=, NULL);
		zio_add_child(dio, aio);
		vdev_queue_io_remove(vq, dio);

		if (dio->io_offset != next_offset) {
			 
			ASSERT3U(dio->io_type, ==, ZIO_TYPE_READ);
			ASSERT3U(dio->io_offset, >, next_offset);
			abd = abd_alloc_for_io(
			    dio->io_offset - next_offset, B_TRUE);
			abd_gang_add(aio->io_abd, abd, B_TRUE);
		}
		if (dio->io_abd &&
		    (dio->io_size != abd_get_size(dio->io_abd))) {
			 
			ASSERT3U(abd_get_size(dio->io_abd), >, dio->io_size);
			abd = abd_get_offset_size(dio->io_abd, 0, dio->io_size);
			abd_gang_add(aio->io_abd, abd, B_TRUE);
		} else {
			if (dio->io_flags & ZIO_FLAG_NODATA) {
				 
				ASSERT3U(dio->io_type, ==, ZIO_TYPE_WRITE);
				ASSERT3P(dio->io_abd, ==, NULL);
				abd_gang_add(aio->io_abd,
				    abd_get_zeros(dio->io_size), B_TRUE);
			} else {
				 
				abd_gang_add(aio->io_abd, dio->io_abd,
				    B_FALSE);
			}
		}
		next_offset = dio->io_offset + dio->io_size;
	} while (dio != last);
	ASSERT3U(abd_get_size(aio->io_abd), ==, aio->io_size);

	 
	return (aio);
}

static zio_t *
vdev_queue_io_to_issue(vdev_queue_t *vq)
{
	zio_t *zio, *aio;
	zio_priority_t p;
	avl_index_t idx;
	avl_tree_t *tree;

again:
	ASSERT(MUTEX_HELD(&vq->vq_lock));

	p = vdev_queue_class_to_issue(vq);

	if (p == ZIO_PRIORITY_NUM_QUEUEABLE) {
		 
		return (NULL);
	}

	if (vdev_queue_class_fifo(p)) {
		zio = list_head(&vq->vq_class[p].vqc_list);
	} else {
		 
		tree = &vq->vq_class[p].vqc_tree;
		zio = aio = avl_first(tree);
		if (zio->io_offset < vq->vq_last_offset) {
			vq->vq_io_search.io_timestamp = zio->io_timestamp;
			vq->vq_io_search.io_offset = vq->vq_last_offset;
			zio = avl_find(tree, &vq->vq_io_search, &idx);
			if (zio == NULL) {
				zio = avl_nearest(tree, idx, AVL_AFTER);
				if (zio == NULL ||
				    (zio->io_timestamp >> VDQ_T_SHIFT) !=
				    (aio->io_timestamp >> VDQ_T_SHIFT))
					zio = aio;
			}
		}
	}
	ASSERT3U(zio->io_priority, ==, p);

	aio = vdev_queue_aggregate(vq, zio);
	if (aio != NULL) {
		zio = aio;
	} else {
		vdev_queue_io_remove(vq, zio);

		 
		if (zio->io_flags & ZIO_FLAG_NODATA) {
			mutex_exit(&vq->vq_lock);
			zio_vdev_io_bypass(zio);
			zio_execute(zio);
			mutex_enter(&vq->vq_lock);
			goto again;
		}
	}

	vdev_queue_pending_add(vq, zio);
	vq->vq_last_offset = zio->io_offset + zio->io_size;

	return (zio);
}

zio_t *
vdev_queue_io(zio_t *zio)
{
	vdev_queue_t *vq = &zio->io_vd->vdev_queue;
	zio_t *dio, *nio;
	zio_link_t *zl = NULL;

	if (zio->io_flags & ZIO_FLAG_DONT_QUEUE)
		return (zio);

	 
	if (zio->io_type == ZIO_TYPE_READ) {
		ASSERT(zio->io_priority != ZIO_PRIORITY_TRIM);

		if (zio->io_priority != ZIO_PRIORITY_SYNC_READ &&
		    zio->io_priority != ZIO_PRIORITY_ASYNC_READ &&
		    zio->io_priority != ZIO_PRIORITY_SCRUB &&
		    zio->io_priority != ZIO_PRIORITY_REMOVAL &&
		    zio->io_priority != ZIO_PRIORITY_INITIALIZING &&
		    zio->io_priority != ZIO_PRIORITY_REBUILD) {
			zio->io_priority = ZIO_PRIORITY_ASYNC_READ;
		}
	} else if (zio->io_type == ZIO_TYPE_WRITE) {
		ASSERT(zio->io_priority != ZIO_PRIORITY_TRIM);

		if (zio->io_priority != ZIO_PRIORITY_SYNC_WRITE &&
		    zio->io_priority != ZIO_PRIORITY_ASYNC_WRITE &&
		    zio->io_priority != ZIO_PRIORITY_REMOVAL &&
		    zio->io_priority != ZIO_PRIORITY_INITIALIZING &&
		    zio->io_priority != ZIO_PRIORITY_REBUILD) {
			zio->io_priority = ZIO_PRIORITY_ASYNC_WRITE;
		}
	} else {
		ASSERT(zio->io_type == ZIO_TYPE_TRIM);
		ASSERT(zio->io_priority == ZIO_PRIORITY_TRIM);
	}

	zio->io_flags |= ZIO_FLAG_DONT_QUEUE;
	zio->io_timestamp = gethrtime();

	mutex_enter(&vq->vq_lock);
	vdev_queue_io_add(vq, zio);
	nio = vdev_queue_io_to_issue(vq);
	mutex_exit(&vq->vq_lock);

	if (nio == NULL)
		return (NULL);

	if (nio->io_done == vdev_queue_agg_io_done) {
		while ((dio = zio_walk_parents(nio, &zl)) != NULL) {
			ASSERT3U(dio->io_type, ==, nio->io_type);
			zio_vdev_io_bypass(dio);
			zio_execute(dio);
		}
		zio_nowait(nio);
		return (NULL);
	}

	return (nio);
}

void
vdev_queue_io_done(zio_t *zio)
{
	vdev_queue_t *vq = &zio->io_vd->vdev_queue;
	zio_t *dio, *nio;
	zio_link_t *zl = NULL;

	hrtime_t now = gethrtime();
	vq->vq_io_complete_ts = now;
	vq->vq_io_delta_ts = zio->io_delta = now - zio->io_timestamp;

	mutex_enter(&vq->vq_lock);
	vdev_queue_pending_remove(vq, zio);

	while ((nio = vdev_queue_io_to_issue(vq)) != NULL) {
		mutex_exit(&vq->vq_lock);
		if (nio->io_done == vdev_queue_agg_io_done) {
			while ((dio = zio_walk_parents(nio, &zl)) != NULL) {
				ASSERT3U(dio->io_type, ==, nio->io_type);
				zio_vdev_io_bypass(dio);
				zio_execute(dio);
			}
			zio_nowait(nio);
		} else {
			zio_vdev_io_reissue(nio);
			zio_execute(nio);
		}
		mutex_enter(&vq->vq_lock);
	}

	mutex_exit(&vq->vq_lock);
}

void
vdev_queue_change_io_priority(zio_t *zio, zio_priority_t priority)
{
	vdev_queue_t *vq = &zio->io_vd->vdev_queue;

	 
	if (zio->io_priority == ZIO_PRIORITY_NOW)
		return;

	ASSERT3U(zio->io_priority, <, ZIO_PRIORITY_NUM_QUEUEABLE);
	ASSERT3U(priority, <, ZIO_PRIORITY_NUM_QUEUEABLE);

	if (zio->io_type == ZIO_TYPE_READ) {
		if (priority != ZIO_PRIORITY_SYNC_READ &&
		    priority != ZIO_PRIORITY_ASYNC_READ &&
		    priority != ZIO_PRIORITY_SCRUB)
			priority = ZIO_PRIORITY_ASYNC_READ;
	} else {
		ASSERT(zio->io_type == ZIO_TYPE_WRITE);
		if (priority != ZIO_PRIORITY_SYNC_WRITE &&
		    priority != ZIO_PRIORITY_ASYNC_WRITE)
			priority = ZIO_PRIORITY_ASYNC_WRITE;
	}

	mutex_enter(&vq->vq_lock);

	 
	if (zio->io_queue_state == ZIO_QS_QUEUED) {
		vdev_queue_class_remove(vq, zio);
		zio->io_priority = priority;
		vdev_queue_class_add(vq, zio);
	} else if (zio->io_queue_state == ZIO_QS_NONE) {
		zio->io_priority = priority;
	}

	mutex_exit(&vq->vq_lock);
}

 
uint32_t
vdev_queue_length(vdev_t *vd)
{
	return (vd->vdev_queue.vq_active);
}

uint64_t
vdev_queue_last_offset(vdev_t *vd)
{
	return (vd->vdev_queue.vq_last_offset);
}

uint64_t
vdev_queue_class_length(vdev_t *vd, zio_priority_t p)
{
	vdev_queue_t *vq = &vd->vdev_queue;
	if (vdev_queue_class_fifo(p))
		return (vq->vq_class[p].vqc_list_numnodes);
	else
		return (avl_numnodes(&vq->vq_class[p].vqc_tree));
}

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, aggregation_limit, UINT, ZMOD_RW,
	"Max vdev I/O aggregation size");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, aggregation_limit_non_rotating, UINT,
	ZMOD_RW, "Max vdev I/O aggregation size for non-rotating media");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, read_gap_limit, UINT, ZMOD_RW,
	"Aggregate read I/O over gap");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, write_gap_limit, UINT, ZMOD_RW,
	"Aggregate write I/O over gap");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, max_active, UINT, ZMOD_RW,
	"Maximum number of active I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_write_active_max_dirty_percent,
	UINT, ZMOD_RW, "Async write concurrency max threshold");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_write_active_min_dirty_percent,
	UINT, ZMOD_RW, "Async write concurrency min threshold");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_read_max_active, UINT, ZMOD_RW,
	"Max active async read I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_read_min_active, UINT, ZMOD_RW,
	"Min active async read I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_write_max_active, UINT, ZMOD_RW,
	"Max active async write I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, async_write_min_active, UINT, ZMOD_RW,
	"Min active async write I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, initializing_max_active, UINT, ZMOD_RW,
	"Max active initializing I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, initializing_min_active, UINT, ZMOD_RW,
	"Min active initializing I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, removal_max_active, UINT, ZMOD_RW,
	"Max active removal I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, removal_min_active, UINT, ZMOD_RW,
	"Min active removal I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, scrub_max_active, UINT, ZMOD_RW,
	"Max active scrub I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, scrub_min_active, UINT, ZMOD_RW,
	"Min active scrub I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, sync_read_max_active, UINT, ZMOD_RW,
	"Max active sync read I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, sync_read_min_active, UINT, ZMOD_RW,
	"Min active sync read I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, sync_write_max_active, UINT, ZMOD_RW,
	"Max active sync write I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, sync_write_min_active, UINT, ZMOD_RW,
	"Min active sync write I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, trim_max_active, UINT, ZMOD_RW,
	"Max active trim/discard I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, trim_min_active, UINT, ZMOD_RW,
	"Min active trim/discard I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, rebuild_max_active, UINT, ZMOD_RW,
	"Max active rebuild I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, rebuild_min_active, UINT, ZMOD_RW,
	"Min active rebuild I/Os per vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, nia_credit, UINT, ZMOD_RW,
	"Number of non-interactive I/Os to allow in sequence");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, nia_delay, UINT, ZMOD_RW,
	"Number of non-interactive I/Os before _max_active");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, queue_depth_pct, UINT, ZMOD_RW,
	"Queue depth percentage for each top-level vdev");

ZFS_MODULE_PARAM(zfs_vdev, zfs_vdev_, def_queue_depth, UINT, ZMOD_RW,
	"Default queue depth for each allocator");
