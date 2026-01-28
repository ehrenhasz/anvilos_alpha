#include <sys/rrwlock.h>
#include <sys/trace_zfs.h>
uint_t rrw_tsd_key;
typedef struct rrw_node {
	struct rrw_node *rn_next;
	rrwlock_t *rn_rrl;
	const void *rn_tag;
} rrw_node_t;
static rrw_node_t *
rrn_find(rrwlock_t *rrl)
{
	rrw_node_t *rn;
	if (zfs_refcount_count(&rrl->rr_linked_rcount) == 0)
		return (NULL);
	for (rn = tsd_get(rrw_tsd_key); rn != NULL; rn = rn->rn_next) {
		if (rn->rn_rrl == rrl)
			return (rn);
	}
	return (NULL);
}
static void
rrn_add(rrwlock_t *rrl, const void *tag)
{
	rrw_node_t *rn;
	rn = kmem_alloc(sizeof (*rn), KM_SLEEP);
	rn->rn_rrl = rrl;
	rn->rn_next = tsd_get(rrw_tsd_key);
	rn->rn_tag = tag;
	VERIFY(tsd_set(rrw_tsd_key, rn) == 0);
}
static boolean_t
rrn_find_and_remove(rrwlock_t *rrl, const void *tag)
{
	rrw_node_t *rn;
	rrw_node_t *prev = NULL;
	if (zfs_refcount_count(&rrl->rr_linked_rcount) == 0)
		return (B_FALSE);
	for (rn = tsd_get(rrw_tsd_key); rn != NULL; rn = rn->rn_next) {
		if (rn->rn_rrl == rrl && rn->rn_tag == tag) {
			if (prev)
				prev->rn_next = rn->rn_next;
			else
				VERIFY(tsd_set(rrw_tsd_key, rn->rn_next) == 0);
			kmem_free(rn, sizeof (*rn));
			return (B_TRUE);
		}
		prev = rn;
	}
	return (B_FALSE);
}
void
rrw_init(rrwlock_t *rrl, boolean_t track_all)
{
	mutex_init(&rrl->rr_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&rrl->rr_cv, NULL, CV_DEFAULT, NULL);
	rrl->rr_writer = NULL;
	zfs_refcount_create(&rrl->rr_anon_rcount);
	zfs_refcount_create(&rrl->rr_linked_rcount);
	rrl->rr_writer_wanted = B_FALSE;
	rrl->rr_track_all = track_all;
}
void
rrw_destroy(rrwlock_t *rrl)
{
	mutex_destroy(&rrl->rr_lock);
	cv_destroy(&rrl->rr_cv);
	ASSERT(rrl->rr_writer == NULL);
	zfs_refcount_destroy(&rrl->rr_anon_rcount);
	zfs_refcount_destroy(&rrl->rr_linked_rcount);
}
static void
rrw_enter_read_impl(rrwlock_t *rrl, boolean_t prio, const void *tag)
{
	mutex_enter(&rrl->rr_lock);
#if !defined(ZFS_DEBUG) && defined(_KERNEL)
	if (rrl->rr_writer == NULL && !rrl->rr_writer_wanted &&
	    !rrl->rr_track_all) {
		rrl->rr_anon_rcount.rc_count++;
		mutex_exit(&rrl->rr_lock);
		return;
	}
	DTRACE_PROBE(zfs__rrwfastpath__rdmiss);
#endif
	ASSERT(rrl->rr_writer != curthread);
	ASSERT(zfs_refcount_count(&rrl->rr_anon_rcount) >= 0);
	while (rrl->rr_writer != NULL || (rrl->rr_writer_wanted &&
	    zfs_refcount_is_zero(&rrl->rr_anon_rcount) && !prio &&
	    rrn_find(rrl) == NULL))
		cv_wait(&rrl->rr_cv, &rrl->rr_lock);
	if (rrl->rr_writer_wanted || rrl->rr_track_all) {
		rrn_add(rrl, tag);
		(void) zfs_refcount_add(&rrl->rr_linked_rcount, tag);
	} else {
		(void) zfs_refcount_add(&rrl->rr_anon_rcount, tag);
	}
	ASSERT(rrl->rr_writer == NULL);
	mutex_exit(&rrl->rr_lock);
}
void
rrw_enter_read(rrwlock_t *rrl, const void *tag)
{
	rrw_enter_read_impl(rrl, B_FALSE, tag);
}
void
rrw_enter_read_prio(rrwlock_t *rrl, const void *tag)
{
	rrw_enter_read_impl(rrl, B_TRUE, tag);
}
void
rrw_enter_write(rrwlock_t *rrl)
{
	mutex_enter(&rrl->rr_lock);
	ASSERT(rrl->rr_writer != curthread);
	while (zfs_refcount_count(&rrl->rr_anon_rcount) > 0 ||
	    zfs_refcount_count(&rrl->rr_linked_rcount) > 0 ||
	    rrl->rr_writer != NULL) {
		rrl->rr_writer_wanted = B_TRUE;
		cv_wait(&rrl->rr_cv, &rrl->rr_lock);
	}
	rrl->rr_writer_wanted = B_FALSE;
	rrl->rr_writer = curthread;
	mutex_exit(&rrl->rr_lock);
}
void
rrw_enter(rrwlock_t *rrl, krw_t rw, const void *tag)
{
	if (rw == RW_READER)
		rrw_enter_read(rrl, tag);
	else
		rrw_enter_write(rrl);
}
void
rrw_exit(rrwlock_t *rrl, const void *tag)
{
	mutex_enter(&rrl->rr_lock);
#if !defined(ZFS_DEBUG) && defined(_KERNEL)
	if (!rrl->rr_writer && rrl->rr_linked_rcount.rc_count == 0) {
		rrl->rr_anon_rcount.rc_count--;
		if (rrl->rr_anon_rcount.rc_count == 0)
			cv_broadcast(&rrl->rr_cv);
		mutex_exit(&rrl->rr_lock);
		return;
	}
	DTRACE_PROBE(zfs__rrwfastpath__exitmiss);
#endif
	ASSERT(!zfs_refcount_is_zero(&rrl->rr_anon_rcount) ||
	    !zfs_refcount_is_zero(&rrl->rr_linked_rcount) ||
	    rrl->rr_writer != NULL);
	if (rrl->rr_writer == NULL) {
		int64_t count;
		if (rrn_find_and_remove(rrl, tag)) {
			count = zfs_refcount_remove(
			    &rrl->rr_linked_rcount, tag);
		} else {
			ASSERT(!rrl->rr_track_all);
			count = zfs_refcount_remove(&rrl->rr_anon_rcount, tag);
		}
		if (count == 0)
			cv_broadcast(&rrl->rr_cv);
	} else {
		ASSERT(rrl->rr_writer == curthread);
		ASSERT(zfs_refcount_is_zero(&rrl->rr_anon_rcount) &&
		    zfs_refcount_is_zero(&rrl->rr_linked_rcount));
		rrl->rr_writer = NULL;
		cv_broadcast(&rrl->rr_cv);
	}
	mutex_exit(&rrl->rr_lock);
}
boolean_t
rrw_held(rrwlock_t *rrl, krw_t rw)
{
	boolean_t held;
	mutex_enter(&rrl->rr_lock);
	if (rw == RW_WRITER) {
		held = (rrl->rr_writer == curthread);
	} else {
		held = (!zfs_refcount_is_zero(&rrl->rr_anon_rcount) ||
		    rrn_find(rrl) != NULL);
	}
	mutex_exit(&rrl->rr_lock);
	return (held);
}
void
rrw_tsd_destroy(void *arg)
{
	rrw_node_t *rn = arg;
	if (rn != NULL) {
		panic("thread %p terminating with rrw lock %p held",
		    (void *)curthread, (void *)rn->rn_rrl);
	}
}
void
rrm_init(rrmlock_t *rrl, boolean_t track_all)
{
	int i;
	for (i = 0; i < RRM_NUM_LOCKS; i++)
		rrw_init(&rrl->locks[i], track_all);
}
void
rrm_destroy(rrmlock_t *rrl)
{
	int i;
	for (i = 0; i < RRM_NUM_LOCKS; i++)
		rrw_destroy(&rrl->locks[i]);
}
void
rrm_enter(rrmlock_t *rrl, krw_t rw, const void *tag)
{
	if (rw == RW_READER)
		rrm_enter_read(rrl, tag);
	else
		rrm_enter_write(rrl);
}
#define	RRM_TD_LOCK()	(((uint32_t)(uintptr_t)(curthread)) % RRM_NUM_LOCKS)
void
rrm_enter_read(rrmlock_t *rrl, const void *tag)
{
	rrw_enter_read(&rrl->locks[RRM_TD_LOCK()], tag);
}
void
rrm_enter_write(rrmlock_t *rrl)
{
	int i;
	for (i = 0; i < RRM_NUM_LOCKS; i++)
		rrw_enter_write(&rrl->locks[i]);
}
void
rrm_exit(rrmlock_t *rrl, const void *tag)
{
	int i;
	if (rrl->locks[0].rr_writer == curthread) {
		for (i = 0; i < RRM_NUM_LOCKS; i++)
			rrw_exit(&rrl->locks[i], tag);
	} else {
		rrw_exit(&rrl->locks[RRM_TD_LOCK()], tag);
	}
}
boolean_t
rrm_held(rrmlock_t *rrl, krw_t rw)
{
	if (rw == RW_WRITER) {
		return (rrw_held(&rrl->locks[0], rw));
	} else {
		return (rrw_held(&rrl->locks[RRM_TD_LOCK()], rw));
	}
}
