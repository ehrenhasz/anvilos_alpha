
 

#include "peerlookup.h"
#include "peer.h"
#include "noise.h"

static struct hlist_head *pubkey_bucket(struct pubkey_hashtable *table,
					const u8 pubkey[NOISE_PUBLIC_KEY_LEN])
{
	 
	const u64 hash = siphash(pubkey, NOISE_PUBLIC_KEY_LEN, &table->key);

	return &table->hashtable[hash & (HASH_SIZE(table->hashtable) - 1)];
}

struct pubkey_hashtable *wg_pubkey_hashtable_alloc(void)
{
	struct pubkey_hashtable *table = kvmalloc(sizeof(*table), GFP_KERNEL);

	if (!table)
		return NULL;

	get_random_bytes(&table->key, sizeof(table->key));
	hash_init(table->hashtable);
	mutex_init(&table->lock);
	return table;
}

void wg_pubkey_hashtable_add(struct pubkey_hashtable *table,
			     struct wg_peer *peer)
{
	mutex_lock(&table->lock);
	hlist_add_head_rcu(&peer->pubkey_hash,
			   pubkey_bucket(table, peer->handshake.remote_static));
	mutex_unlock(&table->lock);
}

void wg_pubkey_hashtable_remove(struct pubkey_hashtable *table,
				struct wg_peer *peer)
{
	mutex_lock(&table->lock);
	hlist_del_init_rcu(&peer->pubkey_hash);
	mutex_unlock(&table->lock);
}

 
struct wg_peer *
wg_pubkey_hashtable_lookup(struct pubkey_hashtable *table,
			   const u8 pubkey[NOISE_PUBLIC_KEY_LEN])
{
	struct wg_peer *iter_peer, *peer = NULL;

	rcu_read_lock_bh();
	hlist_for_each_entry_rcu_bh(iter_peer, pubkey_bucket(table, pubkey),
				    pubkey_hash) {
		if (!memcmp(pubkey, iter_peer->handshake.remote_static,
			    NOISE_PUBLIC_KEY_LEN)) {
			peer = iter_peer;
			break;
		}
	}
	peer = wg_peer_get_maybe_zero(peer);
	rcu_read_unlock_bh();
	return peer;
}

static struct hlist_head *index_bucket(struct index_hashtable *table,
				       const __le32 index)
{
	 
	return &table->hashtable[(__force u32)index &
				 (HASH_SIZE(table->hashtable) - 1)];
}

struct index_hashtable *wg_index_hashtable_alloc(void)
{
	struct index_hashtable *table = kvmalloc(sizeof(*table), GFP_KERNEL);

	if (!table)
		return NULL;

	hash_init(table->hashtable);
	spin_lock_init(&table->lock);
	return table;
}

 

__le32 wg_index_hashtable_insert(struct index_hashtable *table,
				 struct index_hashtable_entry *entry)
{
	struct index_hashtable_entry *existing_entry;

	spin_lock_bh(&table->lock);
	hlist_del_init_rcu(&entry->index_hash);
	spin_unlock_bh(&table->lock);

	rcu_read_lock_bh();

search_unused_slot:
	 
	entry->index = (__force __le32)get_random_u32();
	hlist_for_each_entry_rcu_bh(existing_entry,
				    index_bucket(table, entry->index),
				    index_hash) {
		if (existing_entry->index == entry->index)
			 
			goto search_unused_slot;
	}

	 
	spin_lock_bh(&table->lock);
	hlist_for_each_entry_rcu_bh(existing_entry,
				    index_bucket(table, entry->index),
				    index_hash) {
		if (existing_entry->index == entry->index) {
			spin_unlock_bh(&table->lock);
			 
			goto search_unused_slot;
		}
	}
	 
	hlist_add_head_rcu(&entry->index_hash,
			   index_bucket(table, entry->index));
	spin_unlock_bh(&table->lock);

	rcu_read_unlock_bh();

	return entry->index;
}

bool wg_index_hashtable_replace(struct index_hashtable *table,
				struct index_hashtable_entry *old,
				struct index_hashtable_entry *new)
{
	bool ret;

	spin_lock_bh(&table->lock);
	ret = !hlist_unhashed(&old->index_hash);
	if (unlikely(!ret))
		goto out;

	new->index = old->index;
	hlist_replace_rcu(&old->index_hash, &new->index_hash);

	 
	INIT_HLIST_NODE(&old->index_hash);
out:
	spin_unlock_bh(&table->lock);
	return ret;
}

void wg_index_hashtable_remove(struct index_hashtable *table,
			       struct index_hashtable_entry *entry)
{
	spin_lock_bh(&table->lock);
	hlist_del_init_rcu(&entry->index_hash);
	spin_unlock_bh(&table->lock);
}

 
struct index_hashtable_entry *
wg_index_hashtable_lookup(struct index_hashtable *table,
			  const enum index_hashtable_type type_mask,
			  const __le32 index, struct wg_peer **peer)
{
	struct index_hashtable_entry *iter_entry, *entry = NULL;

	rcu_read_lock_bh();
	hlist_for_each_entry_rcu_bh(iter_entry, index_bucket(table, index),
				    index_hash) {
		if (iter_entry->index == index) {
			if (likely(iter_entry->type & type_mask))
				entry = iter_entry;
			break;
		}
	}
	if (likely(entry)) {
		entry->peer = wg_peer_get_maybe_zero(entry->peer);
		if (likely(entry->peer))
			*peer = entry->peer;
		else
			entry = NULL;
	}
	rcu_read_unlock_bh();
	return entry;
}
