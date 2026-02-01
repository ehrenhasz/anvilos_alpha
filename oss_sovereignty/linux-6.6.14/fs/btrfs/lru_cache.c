

#include <linux/mm.h>
#include "lru_cache.h"
#include "messages.h"

 
void btrfs_lru_cache_init(struct btrfs_lru_cache *cache, unsigned int max_size)
{
	INIT_LIST_HEAD(&cache->lru_list);
	mt_init(&cache->entries);
	cache->size = 0;
	cache->max_size = max_size;
}

static struct btrfs_lru_cache_entry *match_entry(struct list_head *head, u64 key,
						 u64 gen)
{
	struct btrfs_lru_cache_entry *entry;

	list_for_each_entry(entry, head, list) {
		if (entry->key == key && entry->gen == gen)
			return entry;
	}

	return NULL;
}

 
struct btrfs_lru_cache_entry *btrfs_lru_cache_lookup(struct btrfs_lru_cache *cache,
						     u64 key, u64 gen)
{
	struct list_head *head;
	struct btrfs_lru_cache_entry *entry;

	head = mtree_load(&cache->entries, key);
	if (!head)
		return NULL;

	entry = match_entry(head, key, gen);
	if (entry)
		list_move_tail(&entry->lru_list, &cache->lru_list);

	return entry;
}

 
void btrfs_lru_cache_remove(struct btrfs_lru_cache *cache,
			    struct btrfs_lru_cache_entry *entry)
{
	struct list_head *prev = entry->list.prev;

	ASSERT(cache->size > 0);
	ASSERT(!mtree_empty(&cache->entries));

	list_del(&entry->list);
	list_del(&entry->lru_list);

	if (list_empty(prev)) {
		struct list_head *head;

		 
		head = mtree_erase(&cache->entries, entry->key);
		ASSERT(head == prev);
		kfree(head);
	}

	kfree(entry);
	cache->size--;
}

 
int btrfs_lru_cache_store(struct btrfs_lru_cache *cache,
			  struct btrfs_lru_cache_entry *new_entry,
			  gfp_t gfp)
{
	const u64 key = new_entry->key;
	struct list_head *head;
	int ret;

	head = kmalloc(sizeof(*head), gfp);
	if (!head)
		return -ENOMEM;

	ret = mtree_insert(&cache->entries, key, head, gfp);
	if (ret == 0) {
		INIT_LIST_HEAD(head);
		list_add_tail(&new_entry->list, head);
	} else if (ret == -EEXIST) {
		kfree(head);
		head = mtree_load(&cache->entries, key);
		ASSERT(head != NULL);
		if (match_entry(head, key, new_entry->gen) != NULL)
			return -EEXIST;
		list_add_tail(&new_entry->list, head);
	} else if (ret < 0) {
		kfree(head);
		return ret;
	}

	if (cache->max_size > 0 && cache->size == cache->max_size) {
		struct btrfs_lru_cache_entry *lru_entry;

		lru_entry = list_first_entry(&cache->lru_list,
					     struct btrfs_lru_cache_entry,
					     lru_list);
		btrfs_lru_cache_remove(cache, lru_entry);
	}

	list_add_tail(&new_entry->lru_list, &cache->lru_list);
	cache->size++;

	return 0;
}

 
void btrfs_lru_cache_clear(struct btrfs_lru_cache *cache)
{
	struct btrfs_lru_cache_entry *entry;
	struct btrfs_lru_cache_entry *tmp;

	list_for_each_entry_safe(entry, tmp, &cache->lru_list, lru_list)
		btrfs_lru_cache_remove(cache, entry);

	ASSERT(cache->size == 0);
	ASSERT(mtree_empty(&cache->entries));
}
