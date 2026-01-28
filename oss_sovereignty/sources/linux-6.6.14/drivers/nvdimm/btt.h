


#ifndef _LINUX_BTT_H
#define _LINUX_BTT_H

#include <linux/types.h>

#define BTT_SIG_LEN 16
#define BTT_SIG "BTT_ARENA_INFO\0"
#define MAP_ENT_SIZE 4
#define MAP_TRIM_SHIFT 31
#define MAP_TRIM_MASK (1 << MAP_TRIM_SHIFT)
#define MAP_ERR_SHIFT 30
#define MAP_ERR_MASK (1 << MAP_ERR_SHIFT)
#define MAP_LBA_MASK (~((1 << MAP_TRIM_SHIFT) | (1 << MAP_ERR_SHIFT)))
#define MAP_ENT_NORMAL 0xC0000000
#define LOG_GRP_SIZE sizeof(struct log_group)
#define LOG_ENT_SIZE sizeof(struct log_entry)
#define ARENA_MIN_SIZE (1UL << 24)	
#define ARENA_MAX_SIZE (1ULL << 39)	
#define RTT_VALID (1UL << 31)
#define RTT_INVALID 0
#define BTT_PG_SIZE 4096
#define BTT_DEFAULT_NFREE ND_MAX_LANES
#define LOG_SEQ_INIT 1

#define IB_FLAG_ERROR 0x00000001
#define IB_FLAG_ERROR_MASK 0x00000001

#define ent_lba(ent) (ent & MAP_LBA_MASK)
#define ent_e_flag(ent) (!!(ent & MAP_ERR_MASK))
#define ent_z_flag(ent) (!!(ent & MAP_TRIM_MASK))
#define set_e_flag(ent) (ent |= MAP_ERR_MASK)

#define ent_normal(ent) (ent_e_flag(ent) && ent_z_flag(ent))

enum btt_init_state {
	INIT_UNCHECKED = 0,
	INIT_NOTFOUND,
	INIT_READY
};



struct log_entry {
	__le32 lba;
	__le32 old_map;
	__le32 new_map;
	__le32 seq;
};

struct log_group {
	struct log_entry ent[4];
};

struct btt_sb {
	u8 signature[BTT_SIG_LEN];
	u8 uuid[16];
	u8 parent_uuid[16];
	__le32 flags;
	__le16 version_major;
	__le16 version_minor;
	__le32 external_lbasize;
	__le32 external_nlba;
	__le32 internal_lbasize;
	__le32 internal_nlba;
	__le32 nfree;
	__le32 infosize;
	__le64 nextoff;
	__le64 dataoff;
	__le64 mapoff;
	__le64 logoff;
	__le64 info2off;
	u8 padding[3968];
	__le64 checksum;
};

struct free_entry {
	u32 block;
	u8 sub;
	u8 seq;
	u8 has_err;
};

struct aligned_lock {
	union {
		spinlock_t lock;
		u8 cacheline_padding[L1_CACHE_BYTES];
	};
};


struct arena_info {
	u64 size;			
	u64 external_lba_start;
	u32 internal_nlba;
	u32 internal_lbasize;
	u32 external_nlba;
	u32 external_lbasize;
	u32 nfree;
	u16 version_major;
	u16 version_minor;
	u32 sector_size;
	
	u64 nextoff;
	u64 infooff;
	u64 dataoff;
	u64 mapoff;
	u64 logoff;
	u64 info2off;
	
	struct free_entry *freelist;
	u32 *rtt;
	struct aligned_lock *map_locks;
	struct nd_btt *nd_btt;
	struct list_head list;
	struct dentry *debugfs_dir;
	
	u32 flags;
	struct mutex err_lock;
	int log_index[2];
};

struct badblocks;


struct btt {
	struct gendisk *btt_disk;
	struct list_head arena_list;
	struct dentry *debugfs_dir;
	struct nd_btt *nd_btt;
	u64 nlba;
	unsigned long long rawsize;
	u32 lbasize;
	u32 sector_size;
	struct nd_region *nd_region;
	struct mutex init_lock;
	int init_state;
	int num_arenas;
	struct badblocks *phys_bb;
};

bool nd_btt_arena_is_valid(struct nd_btt *nd_btt, struct btt_sb *super);
int nd_btt_version(struct nd_btt *nd_btt, struct nd_namespace_common *ndns,
		struct btt_sb *btt_sb);

#endif
