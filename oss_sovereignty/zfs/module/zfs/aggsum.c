#include <sys/zfs_context.h>
#include <sys/aggsum.h>
static uint_t aggsum_borrow_shift = 4;
void
aggsum_init(aggsum_t *as, uint64_t value)
{
	memset(as, 0, sizeof (*as));
	as->as_lower_bound = as->as_upper_bound = value;
	mutex_init(&as->as_lock, NULL, MUTEX_DEFAULT, NULL);
	as->as_bucketshift = highbit64(boot_ncpus / 6) / 2;
	as->as_numbuckets = ((boot_ncpus - 1) >> as->as_bucketshift) + 1;
	as->as_buckets = kmem_zalloc(as->as_numbuckets *
	    sizeof (aggsum_bucket_t), KM_SLEEP);
	for (int i = 0; i < as->as_numbuckets; i++) {
		mutex_init(&as->as_buckets[i].asc_lock,
		    NULL, MUTEX_DEFAULT, NULL);
	}
}
void
aggsum_fini(aggsum_t *as)
{
	for (int i = 0; i < as->as_numbuckets; i++)
		mutex_destroy(&as->as_buckets[i].asc_lock);
	kmem_free(as->as_buckets, as->as_numbuckets * sizeof (aggsum_bucket_t));
	mutex_destroy(&as->as_lock);
}
int64_t
aggsum_lower_bound(aggsum_t *as)
{
	return (atomic_load_64((volatile uint64_t *)&as->as_lower_bound));
}
uint64_t
aggsum_upper_bound(aggsum_t *as)
{
	return (atomic_load_64(&as->as_upper_bound));
}
uint64_t
aggsum_value(aggsum_t *as)
{
	int64_t lb;
	uint64_t ub;
	mutex_enter(&as->as_lock);
	lb = as->as_lower_bound;
	ub = as->as_upper_bound;
	if (lb == ub) {
		for (int i = 0; i < as->as_numbuckets; i++) {
			ASSERT0(as->as_buckets[i].asc_delta);
			ASSERT0(as->as_buckets[i].asc_borrowed);
		}
		mutex_exit(&as->as_lock);
		return (lb);
	}
	for (int i = 0; i < as->as_numbuckets; i++) {
		struct aggsum_bucket *asb = &as->as_buckets[i];
		if (asb->asc_borrowed == 0)
			continue;
		mutex_enter(&asb->asc_lock);
		lb += asb->asc_delta + asb->asc_borrowed;
		ub += asb->asc_delta - asb->asc_borrowed;
		asb->asc_delta = 0;
		asb->asc_borrowed = 0;
		mutex_exit(&asb->asc_lock);
	}
	ASSERT3U(lb, ==, ub);
	atomic_store_64((volatile uint64_t *)&as->as_lower_bound, lb);
	atomic_store_64(&as->as_upper_bound, lb);
	mutex_exit(&as->as_lock);
	return (lb);
}
void
aggsum_add(aggsum_t *as, int64_t delta)
{
	struct aggsum_bucket *asb;
	int64_t borrow;
	asb = &as->as_buckets[(CPU_SEQID_UNSTABLE >> as->as_bucketshift) %
	    as->as_numbuckets];
	mutex_enter(&asb->asc_lock);
	if (asb->asc_delta + delta <= (int64_t)asb->asc_borrowed &&
	    asb->asc_delta + delta >= -(int64_t)asb->asc_borrowed) {
		asb->asc_delta += delta;
		mutex_exit(&asb->asc_lock);
		return;
	}
	mutex_exit(&asb->asc_lock);
	borrow = (delta < 0 ? -delta : delta);
	borrow <<= aggsum_borrow_shift + as->as_bucketshift;
	mutex_enter(&as->as_lock);
	if (borrow >= asb->asc_borrowed)
		borrow -= asb->asc_borrowed;
	else
		borrow = (borrow - (int64_t)asb->asc_borrowed) / 4;
	mutex_enter(&asb->asc_lock);
	delta += asb->asc_delta;
	asb->asc_delta = 0;
	asb->asc_borrowed += borrow;
	mutex_exit(&asb->asc_lock);
	atomic_store_64((volatile uint64_t *)&as->as_lower_bound,
	    as->as_lower_bound + delta - borrow);
	atomic_store_64(&as->as_upper_bound,
	    as->as_upper_bound + delta + borrow);
	mutex_exit(&as->as_lock);
}
int
aggsum_compare(aggsum_t *as, uint64_t target)
{
	int64_t lb;
	uint64_t ub;
	int i;
	if (atomic_load_64(&as->as_upper_bound) < target)
		return (-1);
	lb = atomic_load_64((volatile uint64_t *)&as->as_lower_bound);
	if (lb > 0 && (uint64_t)lb > target)
		return (1);
	mutex_enter(&as->as_lock);
	lb = as->as_lower_bound;
	ub = as->as_upper_bound;
	for (i = 0; i < as->as_numbuckets; i++) {
		struct aggsum_bucket *asb = &as->as_buckets[i];
		if (asb->asc_borrowed == 0)
			continue;
		mutex_enter(&asb->asc_lock);
		lb += asb->asc_delta + asb->asc_borrowed;
		ub += asb->asc_delta - asb->asc_borrowed;
		asb->asc_delta = 0;
		asb->asc_borrowed = 0;
		mutex_exit(&asb->asc_lock);
		if (ub < target || (lb > 0 && (uint64_t)lb > target))
			break;
	}
	if (i >= as->as_numbuckets)
		ASSERT3U(lb, ==, ub);
	atomic_store_64((volatile uint64_t *)&as->as_lower_bound, lb);
	atomic_store_64(&as->as_upper_bound, ub);
	mutex_exit(&as->as_lock);
	return (ub < target ? -1 : (uint64_t)lb > target ? 1 : 0);
}
