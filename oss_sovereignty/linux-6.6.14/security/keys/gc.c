
 

#include <linux/slab.h>
#include <linux/security.h>
#include <keys/keyring-type.h>
#include "internal.h"

 
unsigned key_gc_delay = 5 * 60;

 
static void key_garbage_collector(struct work_struct *work);
DECLARE_WORK(key_gc_work, key_garbage_collector);

 
static void key_gc_timer_func(struct timer_list *);
static DEFINE_TIMER(key_gc_timer, key_gc_timer_func);

static time64_t key_gc_next_run = TIME64_MAX;
static struct key_type *key_gc_dead_keytype;

static unsigned long key_gc_flags;
#define KEY_GC_KEY_EXPIRED	0	 
#define KEY_GC_REAP_KEYTYPE	1	 
#define KEY_GC_REAPING_KEYTYPE	2	 


 
struct key_type key_type_dead = {
	.name = ".dead",
};

 
void key_schedule_gc(time64_t gc_at)
{
	unsigned long expires;
	time64_t now = ktime_get_real_seconds();

	kenter("%lld", gc_at - now);

	if (gc_at <= now || test_bit(KEY_GC_REAP_KEYTYPE, &key_gc_flags)) {
		kdebug("IMMEDIATE");
		schedule_work(&key_gc_work);
	} else if (gc_at < key_gc_next_run) {
		kdebug("DEFERRED");
		key_gc_next_run = gc_at;
		expires = jiffies + (gc_at - now) * HZ;
		mod_timer(&key_gc_timer, expires);
	}
}

 
void key_set_expiry(struct key *key, time64_t expiry)
{
	key->expiry = expiry;
	if (expiry != TIME64_MAX) {
		if (!(key->type->flags & KEY_TYPE_INSTANT_REAP))
			expiry += key_gc_delay;
		key_schedule_gc(expiry);
	}
}

 
void key_schedule_gc_links(void)
{
	set_bit(KEY_GC_KEY_EXPIRED, &key_gc_flags);
	schedule_work(&key_gc_work);
}

 
static void key_gc_timer_func(struct timer_list *unused)
{
	kenter("");
	key_gc_next_run = TIME64_MAX;
	key_schedule_gc_links();
}

 
void key_gc_keytype(struct key_type *ktype)
{
	kenter("%s", ktype->name);

	key_gc_dead_keytype = ktype;
	set_bit(KEY_GC_REAPING_KEYTYPE, &key_gc_flags);
	smp_mb();
	set_bit(KEY_GC_REAP_KEYTYPE, &key_gc_flags);

	kdebug("schedule");
	schedule_work(&key_gc_work);

	kdebug("sleep");
	wait_on_bit(&key_gc_flags, KEY_GC_REAPING_KEYTYPE,
		    TASK_UNINTERRUPTIBLE);

	key_gc_dead_keytype = NULL;
	kleave("");
}

 
static noinline void key_gc_unused_keys(struct list_head *keys)
{
	while (!list_empty(keys)) {
		struct key *key =
			list_entry(keys->next, struct key, graveyard_link);
		short state = key->state;

		list_del(&key->graveyard_link);

		kdebug("- %u", key->serial);
		key_check(key);

#ifdef CONFIG_KEY_NOTIFICATIONS
		remove_watch_list(key->watchers, key->serial);
		key->watchers = NULL;
#endif

		 
		if (state == KEY_IS_POSITIVE && key->type->destroy)
			key->type->destroy(key);

		security_key_free(key);

		 
		if (test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
			spin_lock(&key->user->lock);
			key->user->qnkeys--;
			key->user->qnbytes -= key->quotalen;
			spin_unlock(&key->user->lock);
		}

		atomic_dec(&key->user->nkeys);
		if (state != KEY_IS_UNINSTANTIATED)
			atomic_dec(&key->user->nikeys);

		key_user_put(key->user);
		key_put_tag(key->domain_tag);
		kfree(key->description);

		memzero_explicit(key, sizeof(*key));
		kmem_cache_free(key_jar, key);
	}
}

 
static void key_garbage_collector(struct work_struct *work)
{
	static LIST_HEAD(graveyard);
	static u8 gc_state;		 
#define KEY_GC_REAP_AGAIN	0x01	 
#define KEY_GC_REAPING_LINKS	0x02	 
#define KEY_GC_REAPING_DEAD_1	0x10	 
#define KEY_GC_REAPING_DEAD_2	0x20	 
#define KEY_GC_REAPING_DEAD_3	0x40	 
#define KEY_GC_FOUND_DEAD_KEY	0x80	 

	struct rb_node *cursor;
	struct key *key;
	time64_t new_timer, limit, expiry;

	kenter("[%lx,%x]", key_gc_flags, gc_state);

	limit = ktime_get_real_seconds();

	 
	gc_state &= KEY_GC_REAPING_DEAD_1 | KEY_GC_REAPING_DEAD_2;
	gc_state <<= 1;
	if (test_and_clear_bit(KEY_GC_KEY_EXPIRED, &key_gc_flags))
		gc_state |= KEY_GC_REAPING_LINKS;

	if (test_and_clear_bit(KEY_GC_REAP_KEYTYPE, &key_gc_flags))
		gc_state |= KEY_GC_REAPING_DEAD_1;
	kdebug("new pass %x", gc_state);

	new_timer = TIME64_MAX;

	 
	spin_lock(&key_serial_lock);
	cursor = rb_first(&key_serial_tree);

continue_scanning:
	while (cursor) {
		key = rb_entry(cursor, struct key, serial_node);
		cursor = rb_next(cursor);

		if (refcount_read(&key->usage) == 0)
			goto found_unreferenced_key;

		if (unlikely(gc_state & KEY_GC_REAPING_DEAD_1)) {
			if (key->type == key_gc_dead_keytype) {
				gc_state |= KEY_GC_FOUND_DEAD_KEY;
				set_bit(KEY_FLAG_DEAD, &key->flags);
				key->perm = 0;
				goto skip_dead_key;
			} else if (key->type == &key_type_keyring &&
				   key->restrict_link) {
				goto found_restricted_keyring;
			}
		}

		expiry = key->expiry;
		if (expiry != TIME64_MAX) {
			if (!(key->type->flags & KEY_TYPE_INSTANT_REAP))
				expiry += key_gc_delay;
			if (expiry > limit && expiry < new_timer) {
				kdebug("will expire %x in %lld",
				       key_serial(key), key->expiry - limit);
				new_timer = key->expiry;
			}
		}

		if (unlikely(gc_state & KEY_GC_REAPING_DEAD_2))
			if (key->type == key_gc_dead_keytype)
				gc_state |= KEY_GC_FOUND_DEAD_KEY;

		if ((gc_state & KEY_GC_REAPING_LINKS) ||
		    unlikely(gc_state & KEY_GC_REAPING_DEAD_2)) {
			if (key->type == &key_type_keyring)
				goto found_keyring;
		}

		if (unlikely(gc_state & KEY_GC_REAPING_DEAD_3))
			if (key->type == key_gc_dead_keytype)
				goto destroy_dead_key;

	skip_dead_key:
		if (spin_is_contended(&key_serial_lock) || need_resched())
			goto contended;
	}

contended:
	spin_unlock(&key_serial_lock);

maybe_resched:
	if (cursor) {
		cond_resched();
		spin_lock(&key_serial_lock);
		goto continue_scanning;
	}

	 
	kdebug("pass complete");

	if (new_timer != TIME64_MAX) {
		new_timer += key_gc_delay;
		key_schedule_gc(new_timer);
	}

	if (unlikely(gc_state & KEY_GC_REAPING_DEAD_2) ||
	    !list_empty(&graveyard)) {
		 
		kdebug("gc sync");
		synchronize_rcu();
	}

	if (!list_empty(&graveyard)) {
		kdebug("gc keys");
		key_gc_unused_keys(&graveyard);
	}

	if (unlikely(gc_state & (KEY_GC_REAPING_DEAD_1 |
				 KEY_GC_REAPING_DEAD_2))) {
		if (!(gc_state & KEY_GC_FOUND_DEAD_KEY)) {
			 
			kdebug("dead short");
			gc_state &= ~(KEY_GC_REAPING_DEAD_1 | KEY_GC_REAPING_DEAD_2);
			gc_state |= KEY_GC_REAPING_DEAD_3;
		} else {
			gc_state |= KEY_GC_REAP_AGAIN;
		}
	}

	if (unlikely(gc_state & KEY_GC_REAPING_DEAD_3)) {
		kdebug("dead wake");
		smp_mb();
		clear_bit(KEY_GC_REAPING_KEYTYPE, &key_gc_flags);
		wake_up_bit(&key_gc_flags, KEY_GC_REAPING_KEYTYPE);
	}

	if (gc_state & KEY_GC_REAP_AGAIN)
		schedule_work(&key_gc_work);
	kleave(" [end %x]", gc_state);
	return;

	 
found_unreferenced_key:
	kdebug("unrefd key %d", key->serial);
	rb_erase(&key->serial_node, &key_serial_tree);
	spin_unlock(&key_serial_lock);

	list_add_tail(&key->graveyard_link, &graveyard);
	gc_state |= KEY_GC_REAP_AGAIN;
	goto maybe_resched;

	 
found_restricted_keyring:
	spin_unlock(&key_serial_lock);
	keyring_restriction_gc(key, key_gc_dead_keytype);
	goto maybe_resched;

	 
found_keyring:
	spin_unlock(&key_serial_lock);
	keyring_gc(key, limit);
	goto maybe_resched;

	 
destroy_dead_key:
	spin_unlock(&key_serial_lock);
	kdebug("destroy key %d", key->serial);
	down_write(&key->sem);
	key->type = &key_type_dead;
	if (key_gc_dead_keytype->destroy)
		key_gc_dead_keytype->destroy(key);
	memset(&key->payload, KEY_DESTROY, sizeof(key->payload));
	up_write(&key->sem);
	goto maybe_resched;
}
