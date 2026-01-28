

#ifndef BTRFS_LRU_CACHE_H
#define BTRFS_LRU_CACHE_H

#include <linux/maple_tree.h>
#include <linux/list.h>


struct btrfs_lru_cache_entry {
	struct list_head lru_list;
	u64 key;
	
	u64 gen;
	
	struct list_head list;
};

struct btrfs_lru_cache {
	struct list_head lru_list;
	struct maple_tree entries;
	
	unsigned int size;
	
	unsigned int max_size;
};

#define btrfs_lru_cache_for_each_entry_safe(cache, entry, tmp)		\
	list_for_each_entry_safe_reverse((entry), (tmp), &(cache)->lru_list, lru_list)

static inline unsigned int btrfs_lru_cache_size(const struct btrfs_lru_cache *cache)
{
	return cache->size;
}

static inline struct btrfs_lru_cache_entry *btrfs_lru_cache_lru_entry(
					      struct btrfs_lru_cache *cache)
{
	return list_first_entry_or_null(&cache->lru_list,
					struct btrfs_lru_cache_entry, lru_list);
}

void btrfs_lru_cache_init(struct btrfs_lru_cache *cache, unsigned int max_size);
struct btrfs_lru_cache_entry *btrfs_lru_cache_lookup(struct btrfs_lru_cache *cache,
						     u64 key, u64 gen);
int btrfs_lru_cache_store(struct btrfs_lru_cache *cache,
			  struct btrfs_lru_cache_entry *new_entry,
			  gfp_t gfp);
void btrfs_lru_cache_remove(struct btrfs_lru_cache *cache,
			    struct btrfs_lru_cache_entry *entry);
void btrfs_lru_cache_clear(struct btrfs_lru_cache *cache);

#endif
