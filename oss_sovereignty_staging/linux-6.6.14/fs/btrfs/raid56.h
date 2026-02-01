 
 

#ifndef BTRFS_RAID56_H
#define BTRFS_RAID56_H

#include <linux/workqueue.h>
#include "volumes.h"

enum btrfs_rbio_ops {
	BTRFS_RBIO_WRITE,
	BTRFS_RBIO_READ_REBUILD,
	BTRFS_RBIO_PARITY_SCRUB,
};

struct btrfs_raid_bio {
	struct btrfs_io_context *bioc;

	 
	struct list_head hash_list;

	 
	struct list_head stripe_cache;

	 
	struct work_struct work;

	 
	struct bio_list bio_list;
	spinlock_t bio_list_lock;

	 
	struct list_head plug_list;

	 
	unsigned long flags;

	 
	enum btrfs_rbio_ops operation;

	 
	u16 nr_pages;

	 
	u16 nr_sectors;

	 
	u8 nr_data;

	 
	u8 real_stripes;

	 
	u8 stripe_npages;

	 
	u8 stripe_nsectors;

	 
	u8 scrubp;

	 
	int bio_list_bytes;

	refcount_t refs;

	atomic_t stripes_pending;

	wait_queue_head_t io_wait;

	 
	unsigned long dbitmap;

	 
	unsigned long finish_pbitmap;

	 

	 
	struct page **stripe_pages;

	 
	struct sector_ptr *bio_sectors;

	 
	struct sector_ptr *stripe_sectors;

	 
	void **finish_pointers;

	 
	unsigned long *error_bitmap;

	 
	u8 *csum_buf;

	 
	unsigned long *csum_bitmap;
};

 
struct raid56_bio_trace_info {
	u64 devid;

	 
	u32 offset;

	 
	u8 stripe_nr;
};

static inline int nr_data_stripes(const struct map_lookup *map)
{
	return map->num_stripes - btrfs_nr_parity_stripes(map->type);
}

static inline int nr_bioc_data_stripes(const struct btrfs_io_context *bioc)
{
	return bioc->num_stripes - btrfs_nr_parity_stripes(bioc->map_type);
}

#define RAID5_P_STRIPE ((u64)-2)
#define RAID6_Q_STRIPE ((u64)-1)

#define is_parity_stripe(x) (((x) == RAID5_P_STRIPE) ||		\
			     ((x) == RAID6_Q_STRIPE))

struct btrfs_device;

void raid56_parity_recover(struct bio *bio, struct btrfs_io_context *bioc,
			   int mirror_num);
void raid56_parity_write(struct bio *bio, struct btrfs_io_context *bioc);

struct btrfs_raid_bio *raid56_parity_alloc_scrub_rbio(struct bio *bio,
				struct btrfs_io_context *bioc,
				struct btrfs_device *scrub_dev,
				unsigned long *dbitmap, int stripe_nsectors);
void raid56_parity_submit_scrub_rbio(struct btrfs_raid_bio *rbio);

void raid56_parity_cache_data_pages(struct btrfs_raid_bio *rbio,
				    struct page **data_pages, u64 data_logical);

int btrfs_alloc_stripe_hash_table(struct btrfs_fs_info *info);
void btrfs_free_stripe_hash_table(struct btrfs_fs_info *info);

#endif
