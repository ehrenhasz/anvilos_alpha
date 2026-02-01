
 

 

#define pr_fmt(fmt) "blk-crypto: " fmt

#include <linux/blk-crypto-profile.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/wait.h>
#include <linux/blkdev.h>
#include <linux/blk-integrity.h>
#include "blk-crypto-internal.h"

struct blk_crypto_keyslot {
	atomic_t slot_refs;
	struct list_head idle_slot_node;
	struct hlist_node hash_node;
	const struct blk_crypto_key *key;
	struct blk_crypto_profile *profile;
};

static inline void blk_crypto_hw_enter(struct blk_crypto_profile *profile)
{
	 
	if (profile->dev)
		pm_runtime_get_sync(profile->dev);
	down_write(&profile->lock);
}

static inline void blk_crypto_hw_exit(struct blk_crypto_profile *profile)
{
	up_write(&profile->lock);
	if (profile->dev)
		pm_runtime_put_sync(profile->dev);
}

 
int blk_crypto_profile_init(struct blk_crypto_profile *profile,
			    unsigned int num_slots)
{
	unsigned int slot;
	unsigned int i;
	unsigned int slot_hashtable_size;

	memset(profile, 0, sizeof(*profile));

	 
	lockdep_register_key(&profile->lockdep_key);
	__init_rwsem(&profile->lock, "&profile->lock", &profile->lockdep_key);

	if (num_slots == 0)
		return 0;

	 

	profile->slots = kvcalloc(num_slots, sizeof(profile->slots[0]),
				  GFP_KERNEL);
	if (!profile->slots)
		goto err_destroy;

	profile->num_slots = num_slots;

	init_waitqueue_head(&profile->idle_slots_wait_queue);
	INIT_LIST_HEAD(&profile->idle_slots);

	for (slot = 0; slot < num_slots; slot++) {
		profile->slots[slot].profile = profile;
		list_add_tail(&profile->slots[slot].idle_slot_node,
			      &profile->idle_slots);
	}

	spin_lock_init(&profile->idle_slots_lock);

	slot_hashtable_size = roundup_pow_of_two(num_slots);
	 
	if (slot_hashtable_size < 2)
		slot_hashtable_size = 2;

	profile->log_slot_ht_size = ilog2(slot_hashtable_size);
	profile->slot_hashtable =
		kvmalloc_array(slot_hashtable_size,
			       sizeof(profile->slot_hashtable[0]), GFP_KERNEL);
	if (!profile->slot_hashtable)
		goto err_destroy;
	for (i = 0; i < slot_hashtable_size; i++)
		INIT_HLIST_HEAD(&profile->slot_hashtable[i]);

	return 0;

err_destroy:
	blk_crypto_profile_destroy(profile);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(blk_crypto_profile_init);

static void blk_crypto_profile_destroy_callback(void *profile)
{
	blk_crypto_profile_destroy(profile);
}

 
int devm_blk_crypto_profile_init(struct device *dev,
				 struct blk_crypto_profile *profile,
				 unsigned int num_slots)
{
	int err = blk_crypto_profile_init(profile, num_slots);

	if (err)
		return err;

	return devm_add_action_or_reset(dev,
					blk_crypto_profile_destroy_callback,
					profile);
}
EXPORT_SYMBOL_GPL(devm_blk_crypto_profile_init);

static inline struct hlist_head *
blk_crypto_hash_bucket_for_key(struct blk_crypto_profile *profile,
			       const struct blk_crypto_key *key)
{
	return &profile->slot_hashtable[
			hash_ptr(key, profile->log_slot_ht_size)];
}

static void
blk_crypto_remove_slot_from_lru_list(struct blk_crypto_keyslot *slot)
{
	struct blk_crypto_profile *profile = slot->profile;
	unsigned long flags;

	spin_lock_irqsave(&profile->idle_slots_lock, flags);
	list_del(&slot->idle_slot_node);
	spin_unlock_irqrestore(&profile->idle_slots_lock, flags);
}

static struct blk_crypto_keyslot *
blk_crypto_find_keyslot(struct blk_crypto_profile *profile,
			const struct blk_crypto_key *key)
{
	const struct hlist_head *head =
		blk_crypto_hash_bucket_for_key(profile, key);
	struct blk_crypto_keyslot *slotp;

	hlist_for_each_entry(slotp, head, hash_node) {
		if (slotp->key == key)
			return slotp;
	}
	return NULL;
}

static struct blk_crypto_keyslot *
blk_crypto_find_and_grab_keyslot(struct blk_crypto_profile *profile,
				 const struct blk_crypto_key *key)
{
	struct blk_crypto_keyslot *slot;

	slot = blk_crypto_find_keyslot(profile, key);
	if (!slot)
		return NULL;
	if (atomic_inc_return(&slot->slot_refs) == 1) {
		 
		blk_crypto_remove_slot_from_lru_list(slot);
	}
	return slot;
}

 
unsigned int blk_crypto_keyslot_index(struct blk_crypto_keyslot *slot)
{
	return slot - slot->profile->slots;
}
EXPORT_SYMBOL_GPL(blk_crypto_keyslot_index);

 
blk_status_t blk_crypto_get_keyslot(struct blk_crypto_profile *profile,
				    const struct blk_crypto_key *key,
				    struct blk_crypto_keyslot **slot_ptr)
{
	struct blk_crypto_keyslot *slot;
	int slot_idx;
	int err;

	*slot_ptr = NULL;

	 
	if (profile->num_slots == 0)
		return BLK_STS_OK;

	down_read(&profile->lock);
	slot = blk_crypto_find_and_grab_keyslot(profile, key);
	up_read(&profile->lock);
	if (slot)
		goto success;

	for (;;) {
		blk_crypto_hw_enter(profile);
		slot = blk_crypto_find_and_grab_keyslot(profile, key);
		if (slot) {
			blk_crypto_hw_exit(profile);
			goto success;
		}

		 
		if (!list_empty(&profile->idle_slots))
			break;

		blk_crypto_hw_exit(profile);
		wait_event(profile->idle_slots_wait_queue,
			   !list_empty(&profile->idle_slots));
	}

	slot = list_first_entry(&profile->idle_slots, struct blk_crypto_keyslot,
				idle_slot_node);
	slot_idx = blk_crypto_keyslot_index(slot);

	err = profile->ll_ops.keyslot_program(profile, key, slot_idx);
	if (err) {
		wake_up(&profile->idle_slots_wait_queue);
		blk_crypto_hw_exit(profile);
		return errno_to_blk_status(err);
	}

	 
	if (slot->key)
		hlist_del(&slot->hash_node);
	slot->key = key;
	hlist_add_head(&slot->hash_node,
		       blk_crypto_hash_bucket_for_key(profile, key));

	atomic_set(&slot->slot_refs, 1);

	blk_crypto_remove_slot_from_lru_list(slot);

	blk_crypto_hw_exit(profile);
success:
	*slot_ptr = slot;
	return BLK_STS_OK;
}

 
void blk_crypto_put_keyslot(struct blk_crypto_keyslot *slot)
{
	struct blk_crypto_profile *profile = slot->profile;
	unsigned long flags;

	if (atomic_dec_and_lock_irqsave(&slot->slot_refs,
					&profile->idle_slots_lock, flags)) {
		list_add_tail(&slot->idle_slot_node, &profile->idle_slots);
		spin_unlock_irqrestore(&profile->idle_slots_lock, flags);
		wake_up(&profile->idle_slots_wait_queue);
	}
}

 
bool __blk_crypto_cfg_supported(struct blk_crypto_profile *profile,
				const struct blk_crypto_config *cfg)
{
	if (!profile)
		return false;
	if (!(profile->modes_supported[cfg->crypto_mode] & cfg->data_unit_size))
		return false;
	if (profile->max_dun_bytes_supported < cfg->dun_bytes)
		return false;
	return true;
}

 
int __blk_crypto_evict_key(struct blk_crypto_profile *profile,
			   const struct blk_crypto_key *key)
{
	struct blk_crypto_keyslot *slot;
	int err;

	if (profile->num_slots == 0) {
		if (profile->ll_ops.keyslot_evict) {
			blk_crypto_hw_enter(profile);
			err = profile->ll_ops.keyslot_evict(profile, key, -1);
			blk_crypto_hw_exit(profile);
			return err;
		}
		return 0;
	}

	blk_crypto_hw_enter(profile);
	slot = blk_crypto_find_keyslot(profile, key);
	if (!slot) {
		 
		err = 0;
		goto out;
	}

	if (WARN_ON_ONCE(atomic_read(&slot->slot_refs) != 0)) {
		 
		err = -EBUSY;
		goto out_remove;
	}
	err = profile->ll_ops.keyslot_evict(profile, key,
					    blk_crypto_keyslot_index(slot));
out_remove:
	 
	hlist_del(&slot->hash_node);
	slot->key = NULL;
out:
	blk_crypto_hw_exit(profile);
	return err;
}

 
void blk_crypto_reprogram_all_keys(struct blk_crypto_profile *profile)
{
	unsigned int slot;

	if (profile->num_slots == 0)
		return;

	 
	down_write(&profile->lock);
	for (slot = 0; slot < profile->num_slots; slot++) {
		const struct blk_crypto_key *key = profile->slots[slot].key;
		int err;

		if (!key)
			continue;

		err = profile->ll_ops.keyslot_program(profile, key, slot);
		WARN_ON(err);
	}
	up_write(&profile->lock);
}
EXPORT_SYMBOL_GPL(blk_crypto_reprogram_all_keys);

void blk_crypto_profile_destroy(struct blk_crypto_profile *profile)
{
	if (!profile)
		return;
	lockdep_unregister_key(&profile->lockdep_key);
	kvfree(profile->slot_hashtable);
	kvfree_sensitive(profile->slots,
			 sizeof(profile->slots[0]) * profile->num_slots);
	memzero_explicit(profile, sizeof(*profile));
}
EXPORT_SYMBOL_GPL(blk_crypto_profile_destroy);

bool blk_crypto_register(struct blk_crypto_profile *profile,
			 struct request_queue *q)
{
	if (blk_integrity_queue_supports_integrity(q)) {
		pr_warn("Integrity and hardware inline encryption are not supported together. Disabling hardware inline encryption.\n");
		return false;
	}
	q->crypto_profile = profile;
	return true;
}
EXPORT_SYMBOL_GPL(blk_crypto_register);

 
void blk_crypto_intersect_capabilities(struct blk_crypto_profile *parent,
				       const struct blk_crypto_profile *child)
{
	if (child) {
		unsigned int i;

		parent->max_dun_bytes_supported =
			min(parent->max_dun_bytes_supported,
			    child->max_dun_bytes_supported);
		for (i = 0; i < ARRAY_SIZE(child->modes_supported); i++)
			parent->modes_supported[i] &= child->modes_supported[i];
	} else {
		parent->max_dun_bytes_supported = 0;
		memset(parent->modes_supported, 0,
		       sizeof(parent->modes_supported));
	}
}
EXPORT_SYMBOL_GPL(blk_crypto_intersect_capabilities);

 
bool blk_crypto_has_capabilities(const struct blk_crypto_profile *target,
				 const struct blk_crypto_profile *reference)
{
	int i;

	if (!reference)
		return true;

	if (!target)
		return false;

	for (i = 0; i < ARRAY_SIZE(target->modes_supported); i++) {
		if (reference->modes_supported[i] & ~target->modes_supported[i])
			return false;
	}

	if (reference->max_dun_bytes_supported >
	    target->max_dun_bytes_supported)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(blk_crypto_has_capabilities);

 
void blk_crypto_update_capabilities(struct blk_crypto_profile *dst,
				    const struct blk_crypto_profile *src)
{
	memcpy(dst->modes_supported, src->modes_supported,
	       sizeof(dst->modes_supported));

	dst->max_dun_bytes_supported = src->max_dun_bytes_supported;
}
EXPORT_SYMBOL_GPL(blk_crypto_update_capabilities);
