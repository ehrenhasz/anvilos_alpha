


#ifndef BTRFS_VOLUMES_H
#define BTRFS_VOLUMES_H

#include <linux/sort.h>
#include <linux/btrfs.h>
#include "async-thread.h"
#include "messages.h"
#include "tree-checker.h"
#include "rcu-string.h"

#define BTRFS_MAX_DATA_CHUNK_SIZE	(10ULL * SZ_1G)

extern struct mutex uuid_mutex;

#define BTRFS_STRIPE_LEN		SZ_64K
#define BTRFS_STRIPE_LEN_SHIFT		(16)
#define BTRFS_STRIPE_LEN_MASK		(BTRFS_STRIPE_LEN - 1)

static_assert(const_ilog2(BTRFS_STRIPE_LEN) == BTRFS_STRIPE_LEN_SHIFT);


#define const_ffs(n) (__builtin_ctzll(n) + 1)


static_assert(const_ffs(BTRFS_BLOCK_GROUP_RAID0) <
	      const_ffs(BTRFS_BLOCK_GROUP_PROFILE_MASK & ~BTRFS_BLOCK_GROUP_RAID0));
static_assert(const_ilog2(BTRFS_BLOCK_GROUP_RAID0) >
	      ilog2(BTRFS_BLOCK_GROUP_TYPE_MASK));


#define BTRFS_BG_FLAG_TO_INDEX(profile)					\
	ilog2((profile) >> (ilog2(BTRFS_BLOCK_GROUP_RAID0) - 1))

enum btrfs_raid_types {
	
	BTRFS_RAID_SINGLE  = 0,

	BTRFS_RAID_RAID0   = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID0),
	BTRFS_RAID_RAID1   = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID1),
	BTRFS_RAID_DUP	   = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_DUP),
	BTRFS_RAID_RAID10  = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID10),
	BTRFS_RAID_RAID5   = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID5),
	BTRFS_RAID_RAID6   = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID6),
	BTRFS_RAID_RAID1C3 = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID1C3),
	BTRFS_RAID_RAID1C4 = BTRFS_BG_FLAG_TO_INDEX(BTRFS_BLOCK_GROUP_RAID1C4),

	BTRFS_NR_RAID_TYPES
};


#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
#include <linux/seqlock.h>
#define __BTRFS_NEED_DEVICE_DATA_ORDERED
#define btrfs_device_data_ordered_init(device)	\
	seqcount_init(&device->data_seqcount)
#else
#define btrfs_device_data_ordered_init(device) do { } while (0)
#endif

#define BTRFS_DEV_STATE_WRITEABLE	(0)
#define BTRFS_DEV_STATE_IN_FS_METADATA	(1)
#define BTRFS_DEV_STATE_MISSING		(2)
#define BTRFS_DEV_STATE_REPLACE_TGT	(3)
#define BTRFS_DEV_STATE_FLUSH_SENT	(4)
#define BTRFS_DEV_STATE_NO_READA	(5)

struct btrfs_zoned_device_info;

struct btrfs_device {
	struct list_head dev_list; 
	struct list_head dev_alloc_list; 
	struct list_head post_commit_list; 
	struct btrfs_fs_devices *fs_devices;
	struct btrfs_fs_info *fs_info;

	struct rcu_string __rcu *name;

	u64 generation;

	struct block_device *bdev;

	struct btrfs_zoned_device_info *zone_info;

	
	void *holder;

	
	dev_t devt;
	unsigned long dev_state;
	blk_status_t last_flush_error;

#ifdef __BTRFS_NEED_DEVICE_DATA_ORDERED
	seqcount_t data_seqcount;
#endif

	
	u64 devid;

	
	u64 total_bytes;

	
	u64 disk_total_bytes;

	
	u64 bytes_used;

	
	u32 io_align;

	
	u32 io_width;
	
	u64 type;

	
	u32 sector_size;

	
	u8 uuid[BTRFS_UUID_SIZE];

	
	u64 commit_total_bytes;

	
	u64 commit_bytes_used;

	
	struct bio flush_bio;
	struct completion flush_wait;

	
	struct scrub_ctx *scrub_ctx;

	
	int dev_stats_valid;

	
	atomic_t dev_stats_ccnt;
	atomic_t dev_stat_values[BTRFS_DEV_STAT_VALUES_MAX];

	struct extent_io_tree alloc_state;

	struct completion kobj_unregister;
	
	struct kobject devid_kobj;

	
	u64 scrub_speed_max;
};


struct btrfs_swapfile_pin {
	struct rb_node node;
	void *ptr;
	struct inode *inode;
	
	bool is_block_group;
	
	int bg_extent_count;
};


#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
#define BTRFS_DEVICE_GETSET_FUNCS(name)					\
static inline u64							\
btrfs_device_get_##name(const struct btrfs_device *dev)			\
{									\
	u64 size;							\
	unsigned int seq;						\
									\
	do {								\
		seq = read_seqcount_begin(&dev->data_seqcount);		\
		size = dev->name;					\
	} while (read_seqcount_retry(&dev->data_seqcount, seq));	\
	return size;							\
}									\
									\
static inline void							\
btrfs_device_set_##name(struct btrfs_device *dev, u64 size)		\
{									\
	preempt_disable();						\
	write_seqcount_begin(&dev->data_seqcount);			\
	dev->name = size;						\
	write_seqcount_end(&dev->data_seqcount);			\
	preempt_enable();						\
}
#elif BITS_PER_LONG==32 && defined(CONFIG_PREEMPTION)
#define BTRFS_DEVICE_GETSET_FUNCS(name)					\
static inline u64							\
btrfs_device_get_##name(const struct btrfs_device *dev)			\
{									\
	u64 size;							\
									\
	preempt_disable();						\
	size = dev->name;						\
	preempt_enable();						\
	return size;							\
}									\
									\
static inline void							\
btrfs_device_set_##name(struct btrfs_device *dev, u64 size)		\
{									\
	preempt_disable();						\
	dev->name = size;						\
	preempt_enable();						\
}
#else
#define BTRFS_DEVICE_GETSET_FUNCS(name)					\
static inline u64							\
btrfs_device_get_##name(const struct btrfs_device *dev)			\
{									\
	return dev->name;						\
}									\
									\
static inline void							\
btrfs_device_set_##name(struct btrfs_device *dev, u64 size)		\
{									\
	dev->name = size;						\
}
#endif

BTRFS_DEVICE_GETSET_FUNCS(total_bytes);
BTRFS_DEVICE_GETSET_FUNCS(disk_total_bytes);
BTRFS_DEVICE_GETSET_FUNCS(bytes_used);

enum btrfs_chunk_allocation_policy {
	BTRFS_CHUNK_ALLOC_REGULAR,
	BTRFS_CHUNK_ALLOC_ZONED,
};


enum btrfs_read_policy {
	
	BTRFS_READ_POLICY_PID,
	BTRFS_NR_READ_POLICY,
};

struct btrfs_fs_devices {
	u8 fsid[BTRFS_FSID_SIZE]; 

	
	u8 metadata_uuid[BTRFS_FSID_SIZE];

	struct list_head fs_list;

	
	u64 num_devices;

	
	u64 open_devices;

	
	u64 rw_devices;

	
	u64 missing_devices;
	u64 total_rw_bytes;

	
	u64 total_devices;

	
	u64 latest_generation;

	
	struct btrfs_device *latest_dev;

	
	struct mutex device_list_mutex;

	
	struct list_head devices;

	
	struct list_head alloc_list;

	struct list_head seed_list;

	
	int opened;

	
	bool rotating;
	
	bool discardable;
	bool fsid_change;
	
	bool seeding;

	struct btrfs_fs_info *fs_info;
	
	struct kobject fsid_kobj;
	struct kobject *devices_kobj;
	struct kobject *devinfo_kobj;
	struct completion kobj_unregister;

	enum btrfs_chunk_allocation_policy chunk_alloc_policy;

	
	enum btrfs_read_policy read_policy;
};

#define BTRFS_MAX_DEVS(info) ((BTRFS_MAX_ITEM_SIZE(info)	\
			- sizeof(struct btrfs_chunk))		\
			/ sizeof(struct btrfs_stripe) + 1)

#define BTRFS_MAX_DEVS_SYS_CHUNK ((BTRFS_SYSTEM_CHUNK_ARRAY_SIZE	\
				- 2 * sizeof(struct btrfs_disk_key)	\
				- 2 * sizeof(struct btrfs_chunk))	\
				/ sizeof(struct btrfs_stripe) + 1)

struct btrfs_io_stripe {
	struct btrfs_device *dev;
	union {
		
		u64 physical;
		
		struct btrfs_io_context *bioc;
	};
};

struct btrfs_discard_stripe {
	struct btrfs_device *dev;
	u64 physical;
	u64 length;
};


struct btrfs_io_context {
	refcount_t refs;
	struct btrfs_fs_info *fs_info;
	u64 map_type; 
	struct bio *orig_bio;
	atomic_t error;
	u16 max_errors;

	
	u16 num_stripes;

	
	u16 mirror_num;

	
	u16 replace_nr_stripes;
	s16 replace_stripe_src;
	
	u64 full_stripe_logical;
	struct btrfs_io_stripe stripes[];
};

struct btrfs_device_info {
	struct btrfs_device *dev;
	u64 dev_offset;
	u64 max_avail;
	u64 total_avail;
};

struct btrfs_raid_attr {
	u8 sub_stripes;		
	u8 dev_stripes;		
	u8 devs_max;		
	u8 devs_min;		
	u8 tolerated_failures;	
	u8 devs_increment;	
	u8 ncopies;		
	u8 nparity;		
	u8 mindev_error;	
	const char raid_name[8]; 
	u64 bg_flag;		
};

extern const struct btrfs_raid_attr btrfs_raid_array[BTRFS_NR_RAID_TYPES];

struct map_lookup {
	u64 type;
	int io_align;
	int io_width;
	int num_stripes;
	int sub_stripes;
	int verified_stripes; 
	struct btrfs_io_stripe stripes[];
};

#define map_lookup_size(n) (sizeof(struct map_lookup) + \
			    (sizeof(struct btrfs_io_stripe) * (n)))

struct btrfs_balance_args;
struct btrfs_balance_progress;
struct btrfs_balance_control {
	struct btrfs_balance_args data;
	struct btrfs_balance_args meta;
	struct btrfs_balance_args sys;

	u64 flags;

	struct btrfs_balance_progress stat;
};


struct btrfs_dev_lookup_args {
	u64 devid;
	u8 *uuid;
	u8 *fsid;
	bool missing;
};


#define BTRFS_DEV_LOOKUP_ARGS_INIT { .devid = (u64)-1 }

#define BTRFS_DEV_LOOKUP_ARGS(name) \
	struct btrfs_dev_lookup_args name = BTRFS_DEV_LOOKUP_ARGS_INIT

enum btrfs_map_op {
	BTRFS_MAP_READ,
	BTRFS_MAP_WRITE,
	BTRFS_MAP_GET_READ_MIRRORS,
};

static inline enum btrfs_map_op btrfs_op(struct bio *bio)
{
	switch (bio_op(bio)) {
	case REQ_OP_WRITE:
	case REQ_OP_ZONE_APPEND:
		return BTRFS_MAP_WRITE;
	default:
		WARN_ON_ONCE(1);
		fallthrough;
	case REQ_OP_READ:
		return BTRFS_MAP_READ;
	}
}

static inline unsigned long btrfs_chunk_item_size(int num_stripes)
{
	ASSERT(num_stripes);
	return sizeof(struct btrfs_chunk) +
		sizeof(struct btrfs_stripe) * (num_stripes - 1);
}


static inline u64 btrfs_stripe_nr_to_offset(u32 stripe_nr)
{
	return (u64)stripe_nr << BTRFS_STRIPE_LEN_SHIFT;
}

void btrfs_get_bioc(struct btrfs_io_context *bioc);
void btrfs_put_bioc(struct btrfs_io_context *bioc);
int btrfs_map_block(struct btrfs_fs_info *fs_info, enum btrfs_map_op op,
		    u64 logical, u64 *length,
		    struct btrfs_io_context **bioc_ret,
		    struct btrfs_io_stripe *smap, int *mirror_num_ret,
		    int need_raid_map);
int btrfs_map_repair_block(struct btrfs_fs_info *fs_info,
			   struct btrfs_io_stripe *smap, u64 logical,
			   u32 length, int mirror_num);
struct btrfs_discard_stripe *btrfs_map_discard(struct btrfs_fs_info *fs_info,
					       u64 logical, u64 *length_ret,
					       u32 *num_stripes);
int btrfs_read_sys_array(struct btrfs_fs_info *fs_info);
int btrfs_read_chunk_tree(struct btrfs_fs_info *fs_info);
struct btrfs_block_group *btrfs_create_chunk(struct btrfs_trans_handle *trans,
					    u64 type);
void btrfs_mapping_tree_free(struct extent_map_tree *tree);
int btrfs_open_devices(struct btrfs_fs_devices *fs_devices,
		       blk_mode_t flags, void *holder);
struct btrfs_device *btrfs_scan_one_device(const char *path, blk_mode_t flags);
int btrfs_forget_devices(dev_t devt);
void btrfs_close_devices(struct btrfs_fs_devices *fs_devices);
void btrfs_free_extra_devids(struct btrfs_fs_devices *fs_devices);
void btrfs_assign_next_active_device(struct btrfs_device *device,
				     struct btrfs_device *this_dev);
struct btrfs_device *btrfs_find_device_by_devspec(struct btrfs_fs_info *fs_info,
						  u64 devid,
						  const char *devpath);
int btrfs_get_dev_args_from_path(struct btrfs_fs_info *fs_info,
				 struct btrfs_dev_lookup_args *args,
				 const char *path);
struct btrfs_device *btrfs_alloc_device(struct btrfs_fs_info *fs_info,
					const u64 *devid, const u8 *uuid,
					const char *path);
void btrfs_put_dev_args_from_path(struct btrfs_dev_lookup_args *args);
int btrfs_rm_device(struct btrfs_fs_info *fs_info,
		    struct btrfs_dev_lookup_args *args,
		    struct block_device **bdev, void **holder);
void __exit btrfs_cleanup_fs_uuids(void);
int btrfs_num_copies(struct btrfs_fs_info *fs_info, u64 logical, u64 len);
int btrfs_grow_device(struct btrfs_trans_handle *trans,
		      struct btrfs_device *device, u64 new_size);
struct btrfs_device *btrfs_find_device(const struct btrfs_fs_devices *fs_devices,
				       const struct btrfs_dev_lookup_args *args);
int btrfs_shrink_device(struct btrfs_device *device, u64 new_size);
int btrfs_init_new_device(struct btrfs_fs_info *fs_info, const char *path);
int btrfs_balance(struct btrfs_fs_info *fs_info,
		  struct btrfs_balance_control *bctl,
		  struct btrfs_ioctl_balance_args *bargs);
void btrfs_describe_block_groups(u64 flags, char *buf, u32 size_buf);
int btrfs_resume_balance_async(struct btrfs_fs_info *fs_info);
int btrfs_recover_balance(struct btrfs_fs_info *fs_info);
int btrfs_pause_balance(struct btrfs_fs_info *fs_info);
int btrfs_relocate_chunk(struct btrfs_fs_info *fs_info, u64 chunk_offset);
int btrfs_cancel_balance(struct btrfs_fs_info *fs_info);
int btrfs_create_uuid_tree(struct btrfs_fs_info *fs_info);
int btrfs_uuid_scan_kthread(void *data);
bool btrfs_chunk_writeable(struct btrfs_fs_info *fs_info, u64 chunk_offset);
void btrfs_dev_stat_inc_and_print(struct btrfs_device *dev, int index);
int btrfs_get_dev_stats(struct btrfs_fs_info *fs_info,
			struct btrfs_ioctl_get_dev_stats *stats);
int btrfs_init_devices_late(struct btrfs_fs_info *fs_info);
int btrfs_init_dev_stats(struct btrfs_fs_info *fs_info);
int btrfs_run_dev_stats(struct btrfs_trans_handle *trans);
void btrfs_rm_dev_replace_remove_srcdev(struct btrfs_device *srcdev);
void btrfs_rm_dev_replace_free_srcdev(struct btrfs_device *srcdev);
void btrfs_destroy_dev_replace_tgtdev(struct btrfs_device *tgtdev);
int btrfs_is_parity_mirror(struct btrfs_fs_info *fs_info,
			   u64 logical, u64 len);
unsigned long btrfs_full_stripe_len(struct btrfs_fs_info *fs_info,
				    u64 logical);
u64 btrfs_calc_stripe_length(const struct extent_map *em);
int btrfs_nr_parity_stripes(u64 type);
int btrfs_chunk_alloc_add_chunk_item(struct btrfs_trans_handle *trans,
				     struct btrfs_block_group *bg);
int btrfs_remove_chunk(struct btrfs_trans_handle *trans, u64 chunk_offset);
struct extent_map *btrfs_get_chunk_map(struct btrfs_fs_info *fs_info,
				       u64 logical, u64 length);
void btrfs_release_disk_super(struct btrfs_super_block *super);

static inline void btrfs_dev_stat_inc(struct btrfs_device *dev,
				      int index)
{
	atomic_inc(dev->dev_stat_values + index);
	
	smp_mb__before_atomic();
	atomic_inc(&dev->dev_stats_ccnt);
}

static inline int btrfs_dev_stat_read(struct btrfs_device *dev,
				      int index)
{
	return atomic_read(dev->dev_stat_values + index);
}

static inline int btrfs_dev_stat_read_and_reset(struct btrfs_device *dev,
						int index)
{
	int ret;

	ret = atomic_xchg(dev->dev_stat_values + index, 0);
	
	atomic_inc(&dev->dev_stats_ccnt);
	return ret;
}

static inline void btrfs_dev_stat_set(struct btrfs_device *dev,
				      int index, unsigned long val)
{
	atomic_set(dev->dev_stat_values + index, val);
	
	smp_mb__before_atomic();
	atomic_inc(&dev->dev_stats_ccnt);
}

static inline const char *btrfs_dev_name(const struct btrfs_device *device)
{
	if (!device || test_bit(BTRFS_DEV_STATE_MISSING, &device->dev_state))
		return "<missing disk>";
	else
		return rcu_str_deref(device->name);
}

void btrfs_commit_device_sizes(struct btrfs_transaction *trans);

struct list_head * __attribute_const__ btrfs_get_fs_uuids(void);
bool btrfs_check_rw_degradable(struct btrfs_fs_info *fs_info,
					struct btrfs_device *failing_dev);
void btrfs_scratch_superblocks(struct btrfs_fs_info *fs_info,
			       struct block_device *bdev,
			       const char *device_path);

enum btrfs_raid_types __attribute_const__ btrfs_bg_flags_to_raid_index(u64 flags);
int btrfs_bg_type_to_factor(u64 flags);
const char *btrfs_bg_type_to_raid_name(u64 flags);
int btrfs_verify_dev_extents(struct btrfs_fs_info *fs_info);
bool btrfs_repair_one_zone(struct btrfs_fs_info *fs_info, u64 logical);

bool btrfs_pinned_by_swapfile(struct btrfs_fs_info *fs_info, void *ptr);
u8 *btrfs_sb_fsid_ptr(struct btrfs_super_block *sb);

#endif
