
 

#define FSCACHE_DEBUG_LEVEL COOKIE
#include <linux/module.h>
#include <linux/slab.h>
#include "internal.h"

struct kmem_cache *fscache_cookie_jar;

static void fscache_cookie_lru_timed_out(struct timer_list *timer);
static void fscache_cookie_lru_worker(struct work_struct *work);
static void fscache_cookie_worker(struct work_struct *work);
static void fscache_unhash_cookie(struct fscache_cookie *cookie);
static void fscache_perform_invalidation(struct fscache_cookie *cookie);

#define fscache_cookie_hash_shift 15
static struct hlist_bl_head fscache_cookie_hash[1 << fscache_cookie_hash_shift];
static LIST_HEAD(fscache_cookies);
static DEFINE_RWLOCK(fscache_cookies_lock);
static LIST_HEAD(fscache_cookie_lru);
static DEFINE_SPINLOCK(fscache_cookie_lru_lock);
DEFINE_TIMER(fscache_cookie_lru_timer, fscache_cookie_lru_timed_out);
static DECLARE_WORK(fscache_cookie_lru_work, fscache_cookie_lru_worker);
static const char fscache_cookie_states[FSCACHE_COOKIE_STATE__NR] = "-LCAIFUWRD";
static unsigned int fscache_lru_cookie_timeout = 10 * HZ;

void fscache_print_cookie(struct fscache_cookie *cookie, char prefix)
{
	const u8 *k;

	pr_err("%c-cookie c=%08x [fl=%lx na=%u nA=%u s=%c]\n",
	       prefix,
	       cookie->debug_id,
	       cookie->flags,
	       atomic_read(&cookie->n_active),
	       atomic_read(&cookie->n_accesses),
	       fscache_cookie_states[cookie->state]);
	pr_err("%c-cookie V=%08x [%s]\n",
	       prefix,
	       cookie->volume->debug_id,
	       cookie->volume->key);

	k = (cookie->key_len <= sizeof(cookie->inline_key)) ?
		cookie->inline_key : cookie->key;
	pr_err("%c-key=[%u] '%*phN'\n", prefix, cookie->key_len, cookie->key_len, k);
}

static void fscache_free_cookie(struct fscache_cookie *cookie)
{
	if (WARN_ON_ONCE(!list_empty(&cookie->commit_link))) {
		spin_lock(&fscache_cookie_lru_lock);
		list_del_init(&cookie->commit_link);
		spin_unlock(&fscache_cookie_lru_lock);
		fscache_stat_d(&fscache_n_cookies_lru);
		fscache_stat(&fscache_n_cookies_lru_removed);
	}

	if (WARN_ON_ONCE(test_bit(FSCACHE_COOKIE_IS_HASHED, &cookie->flags))) {
		fscache_print_cookie(cookie, 'F');
		return;
	}

	write_lock(&fscache_cookies_lock);
	list_del(&cookie->proc_link);
	write_unlock(&fscache_cookies_lock);
	if (cookie->aux_len > sizeof(cookie->inline_aux))
		kfree(cookie->aux);
	if (cookie->key_len > sizeof(cookie->inline_key))
		kfree(cookie->key);
	fscache_stat_d(&fscache_n_cookies);
	kmem_cache_free(fscache_cookie_jar, cookie);
}

static void __fscache_queue_cookie(struct fscache_cookie *cookie)
{
	if (!queue_work(fscache_wq, &cookie->work))
		fscache_put_cookie(cookie, fscache_cookie_put_over_queued);
}

static void fscache_queue_cookie(struct fscache_cookie *cookie,
				 enum fscache_cookie_trace where)
{
	fscache_get_cookie(cookie, where);
	__fscache_queue_cookie(cookie);
}

 
static void fscache_init_access_gate(struct fscache_cookie *cookie)
{
	int n_accesses;

	n_accesses = atomic_read(&cookie->n_accesses);
	trace_fscache_access(cookie->debug_id, refcount_read(&cookie->ref),
			     n_accesses, fscache_access_cache_pin);
	set_bit(FSCACHE_COOKIE_NO_ACCESS_WAKE, &cookie->flags);
}

 
void fscache_end_cookie_access(struct fscache_cookie *cookie,
			       enum fscache_access_trace why)
{
	int n_accesses;

	smp_mb__before_atomic();
	n_accesses = atomic_dec_return(&cookie->n_accesses);
	trace_fscache_access(cookie->debug_id, refcount_read(&cookie->ref),
			     n_accesses, why);
	if (n_accesses == 0 &&
	    !test_bit(FSCACHE_COOKIE_NO_ACCESS_WAKE, &cookie->flags))
		fscache_queue_cookie(cookie, fscache_cookie_get_end_access);
}
EXPORT_SYMBOL(fscache_end_cookie_access);

 
static void __fscache_begin_cookie_access(struct fscache_cookie *cookie,
					  enum fscache_access_trace why)
{
	int n_accesses;

	n_accesses = atomic_inc_return(&cookie->n_accesses);
	smp_mb__after_atomic();  
	trace_fscache_access(cookie->debug_id, refcount_read(&cookie->ref),
			     n_accesses, why);
}

 
bool fscache_begin_cookie_access(struct fscache_cookie *cookie,
				 enum fscache_access_trace why)
{
	if (!test_bit(FSCACHE_COOKIE_IS_CACHING, &cookie->flags))
		return false;
	__fscache_begin_cookie_access(cookie, why);
	if (!test_bit(FSCACHE_COOKIE_IS_CACHING, &cookie->flags) ||
	    !fscache_cache_is_live(cookie->volume->cache)) {
		fscache_end_cookie_access(cookie, fscache_access_unlive);
		return false;
	}
	return true;
}

static inline void wake_up_cookie_state(struct fscache_cookie *cookie)
{
	 
	smp_mb();
	wake_up_var(&cookie->state);
}

 
static void __fscache_set_cookie_state(struct fscache_cookie *cookie,
				       enum fscache_cookie_state state)
{
	smp_store_release(&cookie->state, state);
}

static void fscache_set_cookie_state(struct fscache_cookie *cookie,
				     enum fscache_cookie_state state)
{
	spin_lock(&cookie->lock);
	__fscache_set_cookie_state(cookie, state);
	spin_unlock(&cookie->lock);
	wake_up_cookie_state(cookie);
}

 
void fscache_cookie_lookup_negative(struct fscache_cookie *cookie)
{
	set_bit(FSCACHE_COOKIE_NO_DATA_TO_READ, &cookie->flags);
	fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_CREATING);
}
EXPORT_SYMBOL(fscache_cookie_lookup_negative);

 
void fscache_resume_after_invalidation(struct fscache_cookie *cookie)
{
	fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_ACTIVE);
}
EXPORT_SYMBOL(fscache_resume_after_invalidation);

 
void fscache_caching_failed(struct fscache_cookie *cookie)
{
	clear_bit(FSCACHE_COOKIE_IS_CACHING, &cookie->flags);
	fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_FAILED);
	trace_fscache_cookie(cookie->debug_id, refcount_read(&cookie->ref),
				fscache_cookie_failed);
}
EXPORT_SYMBOL(fscache_caching_failed);

 
static int fscache_set_key(struct fscache_cookie *cookie,
			   const void *index_key, size_t index_key_len)
{
	void *buf;
	size_t buf_size;

	buf_size = round_up(index_key_len, sizeof(__le32));

	if (index_key_len > sizeof(cookie->inline_key)) {
		buf = kzalloc(buf_size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		cookie->key = buf;
	} else {
		buf = cookie->inline_key;
	}

	memcpy(buf, index_key, index_key_len);
	cookie->key_hash = fscache_hash(cookie->volume->key_hash,
					buf, buf_size);
	return 0;
}

static bool fscache_cookie_same(const struct fscache_cookie *a,
				const struct fscache_cookie *b)
{
	const void *ka, *kb;

	if (a->key_hash	!= b->key_hash ||
	    a->volume	!= b->volume ||
	    a->key_len	!= b->key_len)
		return false;

	if (a->key_len <= sizeof(a->inline_key)) {
		ka = &a->inline_key;
		kb = &b->inline_key;
	} else {
		ka = a->key;
		kb = b->key;
	}
	return memcmp(ka, kb, a->key_len) == 0;
}

static atomic_t fscache_cookie_debug_id = ATOMIC_INIT(1);

 
static struct fscache_cookie *fscache_alloc_cookie(
	struct fscache_volume *volume,
	u8 advice,
	const void *index_key, size_t index_key_len,
	const void *aux_data, size_t aux_data_len,
	loff_t object_size)
{
	struct fscache_cookie *cookie;

	 
	cookie = kmem_cache_zalloc(fscache_cookie_jar, GFP_KERNEL);
	if (!cookie)
		return NULL;
	fscache_stat(&fscache_n_cookies);

	cookie->volume		= volume;
	cookie->advice		= advice;
	cookie->key_len		= index_key_len;
	cookie->aux_len		= aux_data_len;
	cookie->object_size	= object_size;
	if (object_size == 0)
		__set_bit(FSCACHE_COOKIE_NO_DATA_TO_READ, &cookie->flags);

	if (fscache_set_key(cookie, index_key, index_key_len) < 0)
		goto nomem;

	if (cookie->aux_len <= sizeof(cookie->inline_aux)) {
		memcpy(cookie->inline_aux, aux_data, cookie->aux_len);
	} else {
		cookie->aux = kmemdup(aux_data, cookie->aux_len, GFP_KERNEL);
		if (!cookie->aux)
			goto nomem;
	}

	refcount_set(&cookie->ref, 1);
	cookie->debug_id = atomic_inc_return(&fscache_cookie_debug_id);
	spin_lock_init(&cookie->lock);
	INIT_LIST_HEAD(&cookie->commit_link);
	INIT_WORK(&cookie->work, fscache_cookie_worker);
	__fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_QUIESCENT);

	write_lock(&fscache_cookies_lock);
	list_add_tail(&cookie->proc_link, &fscache_cookies);
	write_unlock(&fscache_cookies_lock);
	fscache_see_cookie(cookie, fscache_cookie_new_acquire);
	return cookie;

nomem:
	fscache_free_cookie(cookie);
	return NULL;
}

static inline bool fscache_cookie_is_dropped(struct fscache_cookie *cookie)
{
	return READ_ONCE(cookie->state) == FSCACHE_COOKIE_STATE_DROPPED;
}

static void fscache_wait_on_collision(struct fscache_cookie *candidate,
				      struct fscache_cookie *wait_for)
{
	enum fscache_cookie_state *statep = &wait_for->state;

	wait_var_event_timeout(statep, fscache_cookie_is_dropped(wait_for),
			       20 * HZ);
	if (!fscache_cookie_is_dropped(wait_for)) {
		pr_notice("Potential collision c=%08x old: c=%08x",
			  candidate->debug_id, wait_for->debug_id);
		wait_var_event(statep, fscache_cookie_is_dropped(wait_for));
	}
}

 
static bool fscache_hash_cookie(struct fscache_cookie *candidate)
{
	struct fscache_cookie *cursor, *wait_for = NULL;
	struct hlist_bl_head *h;
	struct hlist_bl_node *p;
	unsigned int bucket;

	bucket = candidate->key_hash & (ARRAY_SIZE(fscache_cookie_hash) - 1);
	h = &fscache_cookie_hash[bucket];

	hlist_bl_lock(h);
	hlist_bl_for_each_entry(cursor, p, h, hash_link) {
		if (fscache_cookie_same(candidate, cursor)) {
			if (!test_bit(FSCACHE_COOKIE_RELINQUISHED, &cursor->flags))
				goto collision;
			wait_for = fscache_get_cookie(cursor,
						      fscache_cookie_get_hash_collision);
			break;
		}
	}

	fscache_get_volume(candidate->volume, fscache_volume_get_cookie);
	atomic_inc(&candidate->volume->n_cookies);
	hlist_bl_add_head(&candidate->hash_link, h);
	set_bit(FSCACHE_COOKIE_IS_HASHED, &candidate->flags);
	hlist_bl_unlock(h);

	if (wait_for) {
		fscache_wait_on_collision(candidate, wait_for);
		fscache_put_cookie(wait_for, fscache_cookie_put_hash_collision);
	}
	return true;

collision:
	trace_fscache_cookie(cursor->debug_id, refcount_read(&cursor->ref),
			     fscache_cookie_collision);
	pr_err("Duplicate cookie detected\n");
	fscache_print_cookie(cursor, 'O');
	fscache_print_cookie(candidate, 'N');
	hlist_bl_unlock(h);
	return false;
}

 
struct fscache_cookie *__fscache_acquire_cookie(
	struct fscache_volume *volume,
	u8 advice,
	const void *index_key, size_t index_key_len,
	const void *aux_data, size_t aux_data_len,
	loff_t object_size)
{
	struct fscache_cookie *cookie;

	_enter("V=%x", volume->debug_id);

	if (!index_key || !index_key_len || index_key_len > 255 || aux_data_len > 255)
		return NULL;
	if (!aux_data || !aux_data_len) {
		aux_data = NULL;
		aux_data_len = 0;
	}

	fscache_stat(&fscache_n_acquires);

	cookie = fscache_alloc_cookie(volume, advice,
				      index_key, index_key_len,
				      aux_data, aux_data_len,
				      object_size);
	if (!cookie) {
		fscache_stat(&fscache_n_acquires_oom);
		return NULL;
	}

	if (!fscache_hash_cookie(cookie)) {
		fscache_see_cookie(cookie, fscache_cookie_discard);
		fscache_free_cookie(cookie);
		return NULL;
	}

	trace_fscache_acquire(cookie);
	fscache_stat(&fscache_n_acquires_ok);
	_leave(" = c=%08x", cookie->debug_id);
	return cookie;
}
EXPORT_SYMBOL(__fscache_acquire_cookie);

 
static void fscache_prepare_to_write(struct fscache_cookie *cookie)
{
	cookie->volume->cache->ops->prepare_to_write(cookie);
}

 
static void fscache_perform_lookup(struct fscache_cookie *cookie)
{
	enum fscache_access_trace trace = fscache_access_lookup_cookie_end_failed;
	bool need_withdraw = false;

	_enter("");

	if (!cookie->volume->cache_priv) {
		fscache_create_volume(cookie->volume, true);
		if (!cookie->volume->cache_priv) {
			fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_QUIESCENT);
			goto out;
		}
	}

	if (!cookie->volume->cache->ops->lookup_cookie(cookie)) {
		if (cookie->state != FSCACHE_COOKIE_STATE_FAILED)
			fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_QUIESCENT);
		need_withdraw = true;
		_leave(" [fail]");
		goto out;
	}

	fscache_see_cookie(cookie, fscache_cookie_see_active);
	spin_lock(&cookie->lock);
	if (test_and_clear_bit(FSCACHE_COOKIE_DO_INVALIDATE, &cookie->flags))
		__fscache_set_cookie_state(cookie,
					   FSCACHE_COOKIE_STATE_INVALIDATING);
	else
		__fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_ACTIVE);
	spin_unlock(&cookie->lock);
	wake_up_cookie_state(cookie);
	trace = fscache_access_lookup_cookie_end;

out:
	fscache_end_cookie_access(cookie, trace);
	if (need_withdraw)
		fscache_withdraw_cookie(cookie);
	fscache_end_volume_access(cookie->volume, cookie, trace);
}

 
static bool fscache_begin_lookup(struct fscache_cookie *cookie, bool will_modify)
{
	if (will_modify) {
		set_bit(FSCACHE_COOKIE_LOCAL_WRITE, &cookie->flags);
		set_bit(FSCACHE_COOKIE_DO_PREP_TO_WRITE, &cookie->flags);
	}
	if (!fscache_begin_volume_access(cookie->volume, cookie,
					 fscache_access_lookup_cookie))
		return false;

	__fscache_begin_cookie_access(cookie, fscache_access_lookup_cookie);
	__fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_LOOKING_UP);
	set_bit(FSCACHE_COOKIE_IS_CACHING, &cookie->flags);
	set_bit(FSCACHE_COOKIE_HAS_BEEN_CACHED, &cookie->flags);
	return true;
}

 
void __fscache_use_cookie(struct fscache_cookie *cookie, bool will_modify)
{
	enum fscache_cookie_state state;
	bool queue = false;
	int n_active;

	_enter("c=%08x", cookie->debug_id);

	if (WARN(test_bit(FSCACHE_COOKIE_RELINQUISHED, &cookie->flags),
		 "Trying to use relinquished cookie\n"))
		return;

	spin_lock(&cookie->lock);

	n_active = atomic_inc_return(&cookie->n_active);
	trace_fscache_active(cookie->debug_id, refcount_read(&cookie->ref),
			     n_active, atomic_read(&cookie->n_accesses),
			     will_modify ?
			     fscache_active_use_modify : fscache_active_use);

again:
	state = fscache_cookie_state(cookie);
	switch (state) {
	case FSCACHE_COOKIE_STATE_QUIESCENT:
		queue = fscache_begin_lookup(cookie, will_modify);
		break;

	case FSCACHE_COOKIE_STATE_LOOKING_UP:
	case FSCACHE_COOKIE_STATE_CREATING:
		if (will_modify)
			set_bit(FSCACHE_COOKIE_LOCAL_WRITE, &cookie->flags);
		break;
	case FSCACHE_COOKIE_STATE_ACTIVE:
	case FSCACHE_COOKIE_STATE_INVALIDATING:
		if (will_modify &&
		    !test_and_set_bit(FSCACHE_COOKIE_LOCAL_WRITE, &cookie->flags)) {
			set_bit(FSCACHE_COOKIE_DO_PREP_TO_WRITE, &cookie->flags);
			queue = true;
		}
		 
		if (test_and_clear_bit(FSCACHE_COOKIE_DO_LRU_DISCARD, &cookie->flags))
			fscache_see_cookie(cookie, fscache_cookie_see_lru_discard_clear);
		break;

	case FSCACHE_COOKIE_STATE_FAILED:
	case FSCACHE_COOKIE_STATE_WITHDRAWING:
		break;

	case FSCACHE_COOKIE_STATE_LRU_DISCARDING:
		spin_unlock(&cookie->lock);
		wait_var_event(&cookie->state,
			       fscache_cookie_state(cookie) !=
			       FSCACHE_COOKIE_STATE_LRU_DISCARDING);
		spin_lock(&cookie->lock);
		goto again;

	case FSCACHE_COOKIE_STATE_DROPPED:
	case FSCACHE_COOKIE_STATE_RELINQUISHING:
		WARN(1, "Can't use cookie in state %u\n", state);
		break;
	}

	spin_unlock(&cookie->lock);
	if (queue)
		fscache_queue_cookie(cookie, fscache_cookie_get_use_work);
	_leave("");
}
EXPORT_SYMBOL(__fscache_use_cookie);

static void fscache_unuse_cookie_locked(struct fscache_cookie *cookie)
{
	clear_bit(FSCACHE_COOKIE_DISABLED, &cookie->flags);
	if (!test_bit(FSCACHE_COOKIE_IS_CACHING, &cookie->flags))
		return;

	cookie->unused_at = jiffies;
	spin_lock(&fscache_cookie_lru_lock);
	if (list_empty(&cookie->commit_link)) {
		fscache_get_cookie(cookie, fscache_cookie_get_lru);
		fscache_stat(&fscache_n_cookies_lru);
	}
	list_move_tail(&cookie->commit_link, &fscache_cookie_lru);

	spin_unlock(&fscache_cookie_lru_lock);
	timer_reduce(&fscache_cookie_lru_timer,
		     jiffies + fscache_lru_cookie_timeout);
}

 
void __fscache_unuse_cookie(struct fscache_cookie *cookie,
			    const void *aux_data, const loff_t *object_size)
{
	unsigned int debug_id = cookie->debug_id;
	unsigned int r = refcount_read(&cookie->ref);
	unsigned int a = atomic_read(&cookie->n_accesses);
	unsigned int c;

	if (aux_data || object_size)
		__fscache_update_cookie(cookie, aux_data, object_size);

	 
	c = atomic_fetch_add_unless(&cookie->n_active, -1, 1);
	if (c != 1) {
		trace_fscache_active(debug_id, r, c - 1, a, fscache_active_unuse);
		return;
	}

	spin_lock(&cookie->lock);
	r = refcount_read(&cookie->ref);
	a = atomic_read(&cookie->n_accesses);
	c = atomic_dec_return(&cookie->n_active);
	trace_fscache_active(debug_id, r, c, a, fscache_active_unuse);
	if (c == 0)
		fscache_unuse_cookie_locked(cookie);
	spin_unlock(&cookie->lock);
}
EXPORT_SYMBOL(__fscache_unuse_cookie);

 
static void fscache_cookie_state_machine(struct fscache_cookie *cookie)
{
	enum fscache_cookie_state state;
	bool wake = false;

	_enter("c=%x", cookie->debug_id);

again:
	spin_lock(&cookie->lock);
again_locked:
	state = cookie->state;
	switch (state) {
	case FSCACHE_COOKIE_STATE_QUIESCENT:
		 

		if (atomic_read(&cookie->n_accesses) == 0 &&
		    test_bit(FSCACHE_COOKIE_DO_RELINQUISH, &cookie->flags)) {
			__fscache_set_cookie_state(cookie,
						   FSCACHE_COOKIE_STATE_RELINQUISHING);
			wake = true;
			goto again_locked;
		}
		break;

	case FSCACHE_COOKIE_STATE_LOOKING_UP:
		spin_unlock(&cookie->lock);
		fscache_init_access_gate(cookie);
		fscache_perform_lookup(cookie);
		goto again;

	case FSCACHE_COOKIE_STATE_INVALIDATING:
		spin_unlock(&cookie->lock);
		fscache_perform_invalidation(cookie);
		goto again;

	case FSCACHE_COOKIE_STATE_ACTIVE:
		if (test_and_clear_bit(FSCACHE_COOKIE_DO_PREP_TO_WRITE, &cookie->flags)) {
			spin_unlock(&cookie->lock);
			fscache_prepare_to_write(cookie);
			spin_lock(&cookie->lock);
		}
		if (test_bit(FSCACHE_COOKIE_DO_LRU_DISCARD, &cookie->flags)) {
			__fscache_set_cookie_state(cookie,
						   FSCACHE_COOKIE_STATE_LRU_DISCARDING);
			wake = true;
			goto again_locked;
		}
		fallthrough;

	case FSCACHE_COOKIE_STATE_FAILED:
		if (test_and_clear_bit(FSCACHE_COOKIE_DO_INVALIDATE, &cookie->flags))
			fscache_end_cookie_access(cookie, fscache_access_invalidate_cookie_end);

		if (atomic_read(&cookie->n_accesses) != 0)
			break;
		if (test_bit(FSCACHE_COOKIE_DO_RELINQUISH, &cookie->flags)) {
			__fscache_set_cookie_state(cookie,
						   FSCACHE_COOKIE_STATE_RELINQUISHING);
			wake = true;
			goto again_locked;
		}
		if (test_bit(FSCACHE_COOKIE_DO_WITHDRAW, &cookie->flags)) {
			__fscache_set_cookie_state(cookie,
						   FSCACHE_COOKIE_STATE_WITHDRAWING);
			wake = true;
			goto again_locked;
		}
		break;

	case FSCACHE_COOKIE_STATE_LRU_DISCARDING:
	case FSCACHE_COOKIE_STATE_RELINQUISHING:
	case FSCACHE_COOKIE_STATE_WITHDRAWING:
		if (cookie->cache_priv) {
			spin_unlock(&cookie->lock);
			cookie->volume->cache->ops->withdraw_cookie(cookie);
			spin_lock(&cookie->lock);
		}

		if (test_and_clear_bit(FSCACHE_COOKIE_DO_INVALIDATE, &cookie->flags))
			fscache_end_cookie_access(cookie, fscache_access_invalidate_cookie_end);

		switch (state) {
		case FSCACHE_COOKIE_STATE_RELINQUISHING:
			fscache_see_cookie(cookie, fscache_cookie_see_relinquish);
			fscache_unhash_cookie(cookie);
			__fscache_set_cookie_state(cookie,
						   FSCACHE_COOKIE_STATE_DROPPED);
			wake = true;
			goto out;
		case FSCACHE_COOKIE_STATE_LRU_DISCARDING:
			fscache_see_cookie(cookie, fscache_cookie_see_lru_discard);
			break;
		case FSCACHE_COOKIE_STATE_WITHDRAWING:
			fscache_see_cookie(cookie, fscache_cookie_see_withdraw);
			break;
		default:
			BUG();
		}

		clear_bit(FSCACHE_COOKIE_NEEDS_UPDATE, &cookie->flags);
		clear_bit(FSCACHE_COOKIE_DO_WITHDRAW, &cookie->flags);
		clear_bit(FSCACHE_COOKIE_DO_LRU_DISCARD, &cookie->flags);
		clear_bit(FSCACHE_COOKIE_DO_PREP_TO_WRITE, &cookie->flags);
		set_bit(FSCACHE_COOKIE_NO_DATA_TO_READ, &cookie->flags);
		__fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_QUIESCENT);
		wake = true;
		goto again_locked;

	case FSCACHE_COOKIE_STATE_DROPPED:
		break;

	default:
		WARN_ONCE(1, "Cookie %x in unexpected state %u\n",
			  cookie->debug_id, state);
		break;
	}

out:
	spin_unlock(&cookie->lock);
	if (wake)
		wake_up_cookie_state(cookie);
	_leave("");
}

static void fscache_cookie_worker(struct work_struct *work)
{
	struct fscache_cookie *cookie = container_of(work, struct fscache_cookie, work);

	fscache_see_cookie(cookie, fscache_cookie_see_work);
	fscache_cookie_state_machine(cookie);
	fscache_put_cookie(cookie, fscache_cookie_put_work);
}

 
static void __fscache_withdraw_cookie(struct fscache_cookie *cookie)
{
	int n_accesses;
	bool unpinned;

	unpinned = test_and_clear_bit(FSCACHE_COOKIE_NO_ACCESS_WAKE, &cookie->flags);

	 
	n_accesses = atomic_read(&cookie->n_accesses);
	if (unpinned)
		trace_fscache_access(cookie->debug_id, refcount_read(&cookie->ref),
				     n_accesses, fscache_access_cache_unpin);
	if (n_accesses == 0)
		fscache_queue_cookie(cookie, fscache_cookie_get_end_access);
}

static void fscache_cookie_lru_do_one(struct fscache_cookie *cookie)
{
	fscache_see_cookie(cookie, fscache_cookie_see_lru_do_one);

	spin_lock(&cookie->lock);
	if (cookie->state != FSCACHE_COOKIE_STATE_ACTIVE ||
	    time_before(jiffies, cookie->unused_at + fscache_lru_cookie_timeout) ||
	    atomic_read(&cookie->n_active) > 0) {
		spin_unlock(&cookie->lock);
		fscache_stat(&fscache_n_cookies_lru_removed);
	} else {
		set_bit(FSCACHE_COOKIE_DO_LRU_DISCARD, &cookie->flags);
		spin_unlock(&cookie->lock);
		fscache_stat(&fscache_n_cookies_lru_expired);
		_debug("lru c=%x", cookie->debug_id);
		__fscache_withdraw_cookie(cookie);
	}

	fscache_put_cookie(cookie, fscache_cookie_put_lru);
}

static void fscache_cookie_lru_worker(struct work_struct *work)
{
	struct fscache_cookie *cookie;
	unsigned long unused_at;

	spin_lock(&fscache_cookie_lru_lock);

	while (!list_empty(&fscache_cookie_lru)) {
		cookie = list_first_entry(&fscache_cookie_lru,
					  struct fscache_cookie, commit_link);
		unused_at = cookie->unused_at + fscache_lru_cookie_timeout;
		if (time_before(jiffies, unused_at)) {
			timer_reduce(&fscache_cookie_lru_timer, unused_at);
			break;
		}

		list_del_init(&cookie->commit_link);
		fscache_stat_d(&fscache_n_cookies_lru);
		spin_unlock(&fscache_cookie_lru_lock);
		fscache_cookie_lru_do_one(cookie);
		spin_lock(&fscache_cookie_lru_lock);
	}

	spin_unlock(&fscache_cookie_lru_lock);
}

static void fscache_cookie_lru_timed_out(struct timer_list *timer)
{
	queue_work(fscache_wq, &fscache_cookie_lru_work);
}

static void fscache_cookie_drop_from_lru(struct fscache_cookie *cookie)
{
	bool need_put = false;

	if (!list_empty(&cookie->commit_link)) {
		spin_lock(&fscache_cookie_lru_lock);
		if (!list_empty(&cookie->commit_link)) {
			list_del_init(&cookie->commit_link);
			fscache_stat_d(&fscache_n_cookies_lru);
			fscache_stat(&fscache_n_cookies_lru_dropped);
			need_put = true;
		}
		spin_unlock(&fscache_cookie_lru_lock);
		if (need_put)
			fscache_put_cookie(cookie, fscache_cookie_put_lru);
	}
}

 
static void fscache_unhash_cookie(struct fscache_cookie *cookie)
{
	struct hlist_bl_head *h;
	unsigned int bucket;

	bucket = cookie->key_hash & (ARRAY_SIZE(fscache_cookie_hash) - 1);
	h = &fscache_cookie_hash[bucket];

	hlist_bl_lock(h);
	hlist_bl_del(&cookie->hash_link);
	clear_bit(FSCACHE_COOKIE_IS_HASHED, &cookie->flags);
	hlist_bl_unlock(h);
	fscache_stat(&fscache_n_relinquishes_dropped);
}

static void fscache_drop_withdraw_cookie(struct fscache_cookie *cookie)
{
	fscache_cookie_drop_from_lru(cookie);
	__fscache_withdraw_cookie(cookie);
}

 
void fscache_withdraw_cookie(struct fscache_cookie *cookie)
{
	set_bit(FSCACHE_COOKIE_DO_WITHDRAW, &cookie->flags);
	fscache_drop_withdraw_cookie(cookie);
}
EXPORT_SYMBOL(fscache_withdraw_cookie);

 
void __fscache_relinquish_cookie(struct fscache_cookie *cookie, bool retire)
{
	fscache_stat(&fscache_n_relinquishes);
	if (retire)
		fscache_stat(&fscache_n_relinquishes_retire);

	_enter("c=%08x{%d},%d",
	       cookie->debug_id, atomic_read(&cookie->n_active), retire);

	if (WARN(test_and_set_bit(FSCACHE_COOKIE_RELINQUISHED, &cookie->flags),
		 "Cookie c=%x already relinquished\n", cookie->debug_id))
		return;

	if (retire)
		set_bit(FSCACHE_COOKIE_RETIRED, &cookie->flags);
	trace_fscache_relinquish(cookie, retire);

	ASSERTCMP(atomic_read(&cookie->n_active), ==, 0);
	ASSERTCMP(atomic_read(&cookie->volume->n_cookies), >, 0);
	atomic_dec(&cookie->volume->n_cookies);

	if (test_bit(FSCACHE_COOKIE_HAS_BEEN_CACHED, &cookie->flags)) {
		set_bit(FSCACHE_COOKIE_DO_RELINQUISH, &cookie->flags);
		fscache_drop_withdraw_cookie(cookie);
	} else {
		fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_DROPPED);
		fscache_unhash_cookie(cookie);
	}
	fscache_put_cookie(cookie, fscache_cookie_put_relinquish);
}
EXPORT_SYMBOL(__fscache_relinquish_cookie);

 
void fscache_put_cookie(struct fscache_cookie *cookie,
			enum fscache_cookie_trace where)
{
	struct fscache_volume *volume = cookie->volume;
	unsigned int cookie_debug_id = cookie->debug_id;
	bool zero;
	int ref;

	zero = __refcount_dec_and_test(&cookie->ref, &ref);
	trace_fscache_cookie(cookie_debug_id, ref - 1, where);
	if (zero) {
		fscache_free_cookie(cookie);
		fscache_put_volume(volume, fscache_volume_put_cookie);
	}
}
EXPORT_SYMBOL(fscache_put_cookie);

 
struct fscache_cookie *fscache_get_cookie(struct fscache_cookie *cookie,
					  enum fscache_cookie_trace where)
{
	int ref;

	__refcount_inc(&cookie->ref, &ref);
	trace_fscache_cookie(cookie->debug_id, ref + 1, where);
	return cookie;
}
EXPORT_SYMBOL(fscache_get_cookie);

 
static void fscache_perform_invalidation(struct fscache_cookie *cookie)
{
	if (!cookie->volume->cache->ops->invalidate_cookie(cookie))
		fscache_caching_failed(cookie);
	fscache_end_cookie_access(cookie, fscache_access_invalidate_cookie_end);
}

 
void __fscache_invalidate(struct fscache_cookie *cookie,
			  const void *aux_data, loff_t new_size,
			  unsigned int flags)
{
	bool is_caching;

	_enter("c=%x", cookie->debug_id);

	fscache_stat(&fscache_n_invalidates);

	if (WARN(test_bit(FSCACHE_COOKIE_RELINQUISHED, &cookie->flags),
		 "Trying to invalidate relinquished cookie\n"))
		return;

	if ((flags & FSCACHE_INVAL_DIO_WRITE) &&
	    test_and_set_bit(FSCACHE_COOKIE_DISABLED, &cookie->flags))
		return;

	spin_lock(&cookie->lock);
	set_bit(FSCACHE_COOKIE_NO_DATA_TO_READ, &cookie->flags);
	fscache_update_aux(cookie, aux_data, &new_size);
	cookie->inval_counter++;
	trace_fscache_invalidate(cookie, new_size);

	switch (cookie->state) {
	case FSCACHE_COOKIE_STATE_INVALIDATING:  
	default:
		spin_unlock(&cookie->lock);
		_leave(" [no %u]", cookie->state);
		return;

	case FSCACHE_COOKIE_STATE_LOOKING_UP:
		if (!test_and_set_bit(FSCACHE_COOKIE_DO_INVALIDATE, &cookie->flags))
			__fscache_begin_cookie_access(cookie, fscache_access_invalidate_cookie);
		fallthrough;
	case FSCACHE_COOKIE_STATE_CREATING:
		spin_unlock(&cookie->lock);
		_leave(" [look %x]", cookie->inval_counter);
		return;

	case FSCACHE_COOKIE_STATE_ACTIVE:
		is_caching = fscache_begin_cookie_access(
			cookie, fscache_access_invalidate_cookie);
		if (is_caching)
			__fscache_set_cookie_state(cookie, FSCACHE_COOKIE_STATE_INVALIDATING);
		spin_unlock(&cookie->lock);
		wake_up_cookie_state(cookie);

		if (is_caching)
			fscache_queue_cookie(cookie, fscache_cookie_get_inval_work);
		_leave(" [inv]");
		return;
	}
}
EXPORT_SYMBOL(__fscache_invalidate);

#ifdef CONFIG_PROC_FS
 
static int fscache_cookies_seq_show(struct seq_file *m, void *v)
{
	struct fscache_cookie *cookie;
	unsigned int keylen = 0, auxlen = 0;
	u8 *p;

	if (v == &fscache_cookies) {
		seq_puts(m,
			 "COOKIE   VOLUME   REF ACT ACC S FL DEF             \n"
			 "======== ======== === === === = == ================\n"
			 );
		return 0;
	}

	cookie = list_entry(v, struct fscache_cookie, proc_link);

	seq_printf(m,
		   "%08x %08x %3d %3d %3d %c %02lx",
		   cookie->debug_id,
		   cookie->volume->debug_id,
		   refcount_read(&cookie->ref),
		   atomic_read(&cookie->n_active),
		   atomic_read(&cookie->n_accesses),
		   fscache_cookie_states[cookie->state],
		   cookie->flags);

	keylen = cookie->key_len;
	auxlen = cookie->aux_len;

	if (keylen > 0 || auxlen > 0) {
		seq_puts(m, " ");
		p = keylen <= sizeof(cookie->inline_key) ?
			cookie->inline_key : cookie->key;
		for (; keylen > 0; keylen--)
			seq_printf(m, "%02x", *p++);
		if (auxlen > 0) {
			seq_puts(m, ", ");
			p = auxlen <= sizeof(cookie->inline_aux) ?
				cookie->inline_aux : cookie->aux;
			for (; auxlen > 0; auxlen--)
				seq_printf(m, "%02x", *p++);
		}
	}

	seq_puts(m, "\n");
	return 0;
}

static void *fscache_cookies_seq_start(struct seq_file *m, loff_t *_pos)
	__acquires(fscache_cookies_lock)
{
	read_lock(&fscache_cookies_lock);
	return seq_list_start_head(&fscache_cookies, *_pos);
}

static void *fscache_cookies_seq_next(struct seq_file *m, void *v, loff_t *_pos)
{
	return seq_list_next(v, &fscache_cookies, _pos);
}

static void fscache_cookies_seq_stop(struct seq_file *m, void *v)
	__releases(rcu)
{
	read_unlock(&fscache_cookies_lock);
}


const struct seq_operations fscache_cookies_seq_ops = {
	.start  = fscache_cookies_seq_start,
	.next   = fscache_cookies_seq_next,
	.stop   = fscache_cookies_seq_stop,
	.show   = fscache_cookies_seq_show,
};
#endif
