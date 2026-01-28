#ifndef _NET_BATMAN_ADV_HASH_H_
#define _NET_BATMAN_ADV_HASH_H_
#include "main.h"
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/types.h>
typedef bool (*batadv_hashdata_compare_cb)(const struct hlist_node *,
					   const void *);
typedef u32 (*batadv_hashdata_choose_cb)(const void *, u32);
typedef void (*batadv_hashdata_free_cb)(struct hlist_node *, void *);
struct batadv_hashtable {
	struct hlist_head *table;
	spinlock_t *list_locks;
	u32 size;
	atomic_t generation;
};
struct batadv_hashtable *batadv_hash_new(u32 size);
void batadv_hash_set_lock_class(struct batadv_hashtable *hash,
				struct lock_class_key *key);
void batadv_hash_destroy(struct batadv_hashtable *hash);
static inline int batadv_hash_add(struct batadv_hashtable *hash,
				  batadv_hashdata_compare_cb compare,
				  batadv_hashdata_choose_cb choose,
				  const void *data,
				  struct hlist_node *data_node)
{
	u32 index;
	int ret = -1;
	struct hlist_head *head;
	struct hlist_node *node;
	spinlock_t *list_lock;  
	if (!hash)
		goto out;
	index = choose(data, hash->size);
	head = &hash->table[index];
	list_lock = &hash->list_locks[index];
	spin_lock_bh(list_lock);
	hlist_for_each(node, head) {
		if (!compare(node, data))
			continue;
		ret = 1;
		goto unlock;
	}
	hlist_add_head_rcu(data_node, head);
	atomic_inc(&hash->generation);
	ret = 0;
unlock:
	spin_unlock_bh(list_lock);
out:
	return ret;
}
static inline void *batadv_hash_remove(struct batadv_hashtable *hash,
				       batadv_hashdata_compare_cb compare,
				       batadv_hashdata_choose_cb choose,
				       void *data)
{
	u32 index;
	struct hlist_node *node;
	struct hlist_head *head;
	void *data_save = NULL;
	index = choose(data, hash->size);
	head = &hash->table[index];
	spin_lock_bh(&hash->list_locks[index]);
	hlist_for_each(node, head) {
		if (!compare(node, data))
			continue;
		data_save = node;
		hlist_del_rcu(node);
		atomic_inc(&hash->generation);
		break;
	}
	spin_unlock_bh(&hash->list_locks[index]);
	return data_save;
}
#endif  
