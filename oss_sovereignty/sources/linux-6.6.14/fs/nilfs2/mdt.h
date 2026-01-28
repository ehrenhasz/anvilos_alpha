


#ifndef _NILFS_MDT_H
#define _NILFS_MDT_H

#include <linux/buffer_head.h>
#include <linux/blockgroup_lock.h>
#include "nilfs.h"
#include "page.h"


struct nilfs_shadow_map {
	struct nilfs_bmap_store bmap_store;
	struct inode *inode;
	struct list_head frozen_buffers;
};


struct nilfs_mdt_info {
	struct rw_semaphore	mi_sem;
	struct blockgroup_lock *mi_bgl;
	unsigned int		mi_entry_size;
	unsigned int		mi_first_entry_offset;
	unsigned long		mi_entries_per_block;
	struct nilfs_palloc_cache *mi_palloc_cache;
	struct nilfs_shadow_map *mi_shadow;
	unsigned long		mi_blocks_per_group;
	unsigned long		mi_blocks_per_desc_block;
};

static inline struct nilfs_mdt_info *NILFS_MDT(const struct inode *inode)
{
	return inode->i_private;
}

static inline int nilfs_is_metadata_file_inode(const struct inode *inode)
{
	return inode->i_private != NULL;
}


#define NILFS_MDT_GFP      (__GFP_RECLAIM | __GFP_IO | __GFP_HIGHMEM)

int nilfs_mdt_get_block(struct inode *, unsigned long, int,
			void (*init_block)(struct inode *,
					   struct buffer_head *, void *),
			struct buffer_head **);
int nilfs_mdt_find_block(struct inode *inode, unsigned long start,
			 unsigned long end, unsigned long *blkoff,
			 struct buffer_head **out_bh);
int nilfs_mdt_delete_block(struct inode *, unsigned long);
int nilfs_mdt_forget_block(struct inode *, unsigned long);
int nilfs_mdt_fetch_dirty(struct inode *);

int nilfs_mdt_init(struct inode *inode, gfp_t gfp_mask, size_t objsz);
void nilfs_mdt_clear(struct inode *inode);
void nilfs_mdt_destroy(struct inode *inode);

void nilfs_mdt_set_entry_size(struct inode *, unsigned int, unsigned int);

int nilfs_mdt_setup_shadow_map(struct inode *inode,
			       struct nilfs_shadow_map *shadow);
int nilfs_mdt_save_to_shadow_map(struct inode *inode);
void nilfs_mdt_restore_from_shadow_map(struct inode *inode);
void nilfs_mdt_clear_shadow_map(struct inode *inode);
int nilfs_mdt_freeze_buffer(struct inode *inode, struct buffer_head *bh);
struct buffer_head *nilfs_mdt_get_frozen_buffer(struct inode *inode,
						struct buffer_head *bh);

static inline void nilfs_mdt_mark_dirty(struct inode *inode)
{
	if (!test_bit(NILFS_I_DIRTY, &NILFS_I(inode)->i_state))
		set_bit(NILFS_I_DIRTY, &NILFS_I(inode)->i_state);
}

static inline void nilfs_mdt_clear_dirty(struct inode *inode)
{
	clear_bit(NILFS_I_DIRTY, &NILFS_I(inode)->i_state);
}

static inline __u64 nilfs_mdt_cno(struct inode *inode)
{
	return ((struct the_nilfs *)inode->i_sb->s_fs_info)->ns_cno;
}

static inline spinlock_t *
nilfs_mdt_bgl_lock(struct inode *inode, unsigned int block_group)
{
	return bgl_lock_ptr(NILFS_MDT(inode)->mi_bgl, block_group);
}

#endif 
