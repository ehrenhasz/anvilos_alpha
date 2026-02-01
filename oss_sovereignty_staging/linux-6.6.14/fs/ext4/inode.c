
 

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/time.h>
#include <linux/highuid.h>
#include <linux/pagemap.h>
#include <linux/dax.h>
#include <linux/quotaops.h>
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/pagevec.h>
#include <linux/mpage.h>
#include <linux/namei.h>
#include <linux/uio.h>
#include <linux/bio.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/iomap.h>
#include <linux/iversion.h>

#include "ext4_jbd2.h"
#include "xattr.h"
#include "acl.h"
#include "truncate.h"

#include <trace/events/ext4.h>

static __u32 ext4_inode_csum(struct inode *inode, struct ext4_inode *raw,
			      struct ext4_inode_info *ei)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	__u32 csum;
	__u16 dummy_csum = 0;
	int offset = offsetof(struct ext4_inode, i_checksum_lo);
	unsigned int csum_size = sizeof(dummy_csum);

	csum = ext4_chksum(sbi, ei->i_csum_seed, (__u8 *)raw, offset);
	csum = ext4_chksum(sbi, csum, (__u8 *)&dummy_csum, csum_size);
	offset += csum_size;
	csum = ext4_chksum(sbi, csum, (__u8 *)raw + offset,
			   EXT4_GOOD_OLD_INODE_SIZE - offset);

	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE) {
		offset = offsetof(struct ext4_inode, i_checksum_hi);
		csum = ext4_chksum(sbi, csum, (__u8 *)raw +
				   EXT4_GOOD_OLD_INODE_SIZE,
				   offset - EXT4_GOOD_OLD_INODE_SIZE);
		if (EXT4_FITS_IN_INODE(raw, ei, i_checksum_hi)) {
			csum = ext4_chksum(sbi, csum, (__u8 *)&dummy_csum,
					   csum_size);
			offset += csum_size;
		}
		csum = ext4_chksum(sbi, csum, (__u8 *)raw + offset,
				   EXT4_INODE_SIZE(inode->i_sb) - offset);
	}

	return csum;
}

static int ext4_inode_csum_verify(struct inode *inode, struct ext4_inode *raw,
				  struct ext4_inode_info *ei)
{
	__u32 provided, calculated;

	if (EXT4_SB(inode->i_sb)->s_es->s_creator_os !=
	    cpu_to_le32(EXT4_OS_LINUX) ||
	    !ext4_has_metadata_csum(inode->i_sb))
		return 1;

	provided = le16_to_cpu(raw->i_checksum_lo);
	calculated = ext4_inode_csum(inode, raw, ei);
	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE &&
	    EXT4_FITS_IN_INODE(raw, ei, i_checksum_hi))
		provided |= ((__u32)le16_to_cpu(raw->i_checksum_hi)) << 16;
	else
		calculated &= 0xFFFF;

	return provided == calculated;
}

void ext4_inode_csum_set(struct inode *inode, struct ext4_inode *raw,
			 struct ext4_inode_info *ei)
{
	__u32 csum;

	if (EXT4_SB(inode->i_sb)->s_es->s_creator_os !=
	    cpu_to_le32(EXT4_OS_LINUX) ||
	    !ext4_has_metadata_csum(inode->i_sb))
		return;

	csum = ext4_inode_csum(inode, raw, ei);
	raw->i_checksum_lo = cpu_to_le16(csum & 0xFFFF);
	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE &&
	    EXT4_FITS_IN_INODE(raw, ei, i_checksum_hi))
		raw->i_checksum_hi = cpu_to_le16(csum >> 16);
}

static inline int ext4_begin_ordered_truncate(struct inode *inode,
					      loff_t new_size)
{
	trace_ext4_begin_ordered_truncate(inode, new_size);
	 
	if (!EXT4_I(inode)->jinode)
		return 0;
	return jbd2_journal_begin_ordered_truncate(EXT4_JOURNAL(inode),
						   EXT4_I(inode)->jinode,
						   new_size);
}

static int ext4_meta_trans_blocks(struct inode *inode, int lblocks,
				  int pextents);

 
int ext4_inode_is_fast_symlink(struct inode *inode)
{
	if (!(EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL)) {
		int ea_blocks = EXT4_I(inode)->i_file_acl ?
				EXT4_CLUSTER_SIZE(inode->i_sb) >> 9 : 0;

		if (ext4_has_inline_data(inode))
			return 0;

		return (S_ISLNK(inode->i_mode) && inode->i_blocks - ea_blocks == 0);
	}
	return S_ISLNK(inode->i_mode) && inode->i_size &&
	       (inode->i_size < EXT4_N_BLOCKS * 4);
}

 
void ext4_evict_inode(struct inode *inode)
{
	handle_t *handle;
	int err;
	 
	int extra_credits = 6;
	struct ext4_xattr_inode_array *ea_inode_array = NULL;
	bool freeze_protected = false;

	trace_ext4_evict_inode(inode);

	if (EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL)
		ext4_evict_ea_inode(inode);
	if (inode->i_nlink) {
		truncate_inode_pages_final(&inode->i_data);

		goto no_delete;
	}

	if (is_bad_inode(inode))
		goto no_delete;
	dquot_initialize(inode);

	if (ext4_should_order_data(inode))
		ext4_begin_ordered_truncate(inode, 0);
	truncate_inode_pages_final(&inode->i_data);

	 
	if (!list_empty_careful(&inode->i_io_list))
		inode_io_list_del(inode);

	 
	if (!ext4_journal_current_handle()) {
		sb_start_intwrite(inode->i_sb);
		freeze_protected = true;
	}

	if (!IS_NOQUOTA(inode))
		extra_credits += EXT4_MAXQUOTAS_DEL_BLOCKS(inode->i_sb);

	 
	handle = ext4_journal_start(inode, EXT4_HT_TRUNCATE,
			 ext4_blocks_for_truncate(inode) + extra_credits - 3);
	if (IS_ERR(handle)) {
		ext4_std_error(inode->i_sb, PTR_ERR(handle));
		 
		ext4_orphan_del(NULL, inode);
		if (freeze_protected)
			sb_end_intwrite(inode->i_sb);
		goto no_delete;
	}

	if (IS_SYNC(inode))
		ext4_handle_sync(handle);

	 
	if (ext4_inode_is_fast_symlink(inode))
		memset(EXT4_I(inode)->i_data, 0, sizeof(EXT4_I(inode)->i_data));
	inode->i_size = 0;
	err = ext4_mark_inode_dirty(handle, inode);
	if (err) {
		ext4_warning(inode->i_sb,
			     "couldn't mark inode dirty (err %d)", err);
		goto stop_handle;
	}
	if (inode->i_blocks) {
		err = ext4_truncate(inode);
		if (err) {
			ext4_error_err(inode->i_sb, -err,
				       "couldn't truncate inode %lu (err %d)",
				       inode->i_ino, err);
			goto stop_handle;
		}
	}

	 
	err = ext4_xattr_delete_inode(handle, inode, &ea_inode_array,
				      extra_credits);
	if (err) {
		ext4_warning(inode->i_sb, "xattr delete (err %d)", err);
stop_handle:
		ext4_journal_stop(handle);
		ext4_orphan_del(NULL, inode);
		if (freeze_protected)
			sb_end_intwrite(inode->i_sb);
		ext4_xattr_inode_array_free(ea_inode_array);
		goto no_delete;
	}

	 
	ext4_orphan_del(handle, inode);
	EXT4_I(inode)->i_dtime	= (__u32)ktime_get_real_seconds();

	 
	if (ext4_mark_inode_dirty(handle, inode))
		 
		ext4_clear_inode(inode);
	else
		ext4_free_inode(handle, inode);
	ext4_journal_stop(handle);
	if (freeze_protected)
		sb_end_intwrite(inode->i_sb);
	ext4_xattr_inode_array_free(ea_inode_array);
	return;
no_delete:
	 
	WARN_ON_ONCE(!list_empty_careful(&inode->i_io_list));

	if (!list_empty(&EXT4_I(inode)->i_fc_list))
		ext4_fc_mark_ineligible(inode->i_sb, EXT4_FC_REASON_NOMEM, NULL);
	ext4_clear_inode(inode);	 
}

#ifdef CONFIG_QUOTA
qsize_t *ext4_get_reserved_space(struct inode *inode)
{
	return &EXT4_I(inode)->i_reserved_quota;
}
#endif

 
void ext4_da_update_reserve_space(struct inode *inode,
					int used, int quota_claim)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct ext4_inode_info *ei = EXT4_I(inode);

	spin_lock(&ei->i_block_reservation_lock);
	trace_ext4_da_update_reserve_space(inode, used, quota_claim);
	if (unlikely(used > ei->i_reserved_data_blocks)) {
		ext4_warning(inode->i_sb, "%s: ino %lu, used %d "
			 "with only %d reserved data blocks",
			 __func__, inode->i_ino, used,
			 ei->i_reserved_data_blocks);
		WARN_ON(1);
		used = ei->i_reserved_data_blocks;
	}

	 
	ei->i_reserved_data_blocks -= used;
	percpu_counter_sub(&sbi->s_dirtyclusters_counter, used);

	spin_unlock(&ei->i_block_reservation_lock);

	 
	if (quota_claim)
		dquot_claim_block(inode, EXT4_C2B(sbi, used));
	else {
		 
		dquot_release_reservation_block(inode, EXT4_C2B(sbi, used));
	}

	 
	if ((ei->i_reserved_data_blocks == 0) &&
	    !inode_is_open_for_write(inode))
		ext4_discard_preallocations(inode, 0);
}

static int __check_block_validity(struct inode *inode, const char *func,
				unsigned int line,
				struct ext4_map_blocks *map)
{
	if (ext4_has_feature_journal(inode->i_sb) &&
	    (inode->i_ino ==
	     le32_to_cpu(EXT4_SB(inode->i_sb)->s_es->s_journal_inum)))
		return 0;
	if (!ext4_inode_block_valid(inode, map->m_pblk, map->m_len)) {
		ext4_error_inode(inode, func, line, map->m_pblk,
				 "lblock %lu mapped to illegal pblock %llu "
				 "(length %d)", (unsigned long) map->m_lblk,
				 map->m_pblk, map->m_len);
		return -EFSCORRUPTED;
	}
	return 0;
}

int ext4_issue_zeroout(struct inode *inode, ext4_lblk_t lblk, ext4_fsblk_t pblk,
		       ext4_lblk_t len)
{
	int ret;

	if (IS_ENCRYPTED(inode) && S_ISREG(inode->i_mode))
		return fscrypt_zeroout_range(inode, lblk, pblk, len);

	ret = sb_issue_zeroout(inode->i_sb, pblk, len, GFP_NOFS);
	if (ret > 0)
		ret = 0;

	return ret;
}

#define check_block_validity(inode, map)	\
	__check_block_validity((inode), __func__, __LINE__, (map))

#ifdef ES_AGGRESSIVE_TEST
static void ext4_map_blocks_es_recheck(handle_t *handle,
				       struct inode *inode,
				       struct ext4_map_blocks *es_map,
				       struct ext4_map_blocks *map,
				       int flags)
{
	int retval;

	map->m_flags = 0;
	 
	down_read(&EXT4_I(inode)->i_data_sem);
	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)) {
		retval = ext4_ext_map_blocks(handle, inode, map, 0);
	} else {
		retval = ext4_ind_map_blocks(handle, inode, map, 0);
	}
	up_read((&EXT4_I(inode)->i_data_sem));

	 
	if (es_map->m_lblk != map->m_lblk ||
	    es_map->m_flags != map->m_flags ||
	    es_map->m_pblk != map->m_pblk) {
		printk("ES cache assertion failed for inode: %lu "
		       "es_cached ex [%d/%d/%llu/%x] != "
		       "found ex [%d/%d/%llu/%x] retval %d flags %x\n",
		       inode->i_ino, es_map->m_lblk, es_map->m_len,
		       es_map->m_pblk, es_map->m_flags, map->m_lblk,
		       map->m_len, map->m_pblk, map->m_flags,
		       retval, flags);
	}
}
#endif  

 
int ext4_map_blocks(handle_t *handle, struct inode *inode,
		    struct ext4_map_blocks *map, int flags)
{
	struct extent_status es;
	int retval;
	int ret = 0;
#ifdef ES_AGGRESSIVE_TEST
	struct ext4_map_blocks orig_map;

	memcpy(&orig_map, map, sizeof(*map));
#endif

	map->m_flags = 0;
	ext_debug(inode, "flag 0x%x, max_blocks %u, logical block %lu\n",
		  flags, map->m_len, (unsigned long) map->m_lblk);

	 
	if (unlikely(map->m_len > INT_MAX))
		map->m_len = INT_MAX;

	 
	if (unlikely(map->m_lblk >= EXT_MAX_BLOCKS))
		return -EFSCORRUPTED;

	 
	if (!(EXT4_SB(inode->i_sb)->s_mount_state & EXT4_FC_REPLAY) &&
	    ext4_es_lookup_extent(inode, map->m_lblk, NULL, &es)) {
		if (ext4_es_is_written(&es) || ext4_es_is_unwritten(&es)) {
			map->m_pblk = ext4_es_pblock(&es) +
					map->m_lblk - es.es_lblk;
			map->m_flags |= ext4_es_is_written(&es) ?
					EXT4_MAP_MAPPED : EXT4_MAP_UNWRITTEN;
			retval = es.es_len - (map->m_lblk - es.es_lblk);
			if (retval > map->m_len)
				retval = map->m_len;
			map->m_len = retval;
		} else if (ext4_es_is_delayed(&es) || ext4_es_is_hole(&es)) {
			map->m_pblk = 0;
			retval = es.es_len - (map->m_lblk - es.es_lblk);
			if (retval > map->m_len)
				retval = map->m_len;
			map->m_len = retval;
			retval = 0;
		} else {
			BUG();
		}

		if (flags & EXT4_GET_BLOCKS_CACHED_NOWAIT)
			return retval;
#ifdef ES_AGGRESSIVE_TEST
		ext4_map_blocks_es_recheck(handle, inode, map,
					   &orig_map, flags);
#endif
		goto found;
	}
	 
	if (flags & EXT4_GET_BLOCKS_CACHED_NOWAIT)
		return 0;

	 
	down_read(&EXT4_I(inode)->i_data_sem);
	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)) {
		retval = ext4_ext_map_blocks(handle, inode, map, 0);
	} else {
		retval = ext4_ind_map_blocks(handle, inode, map, 0);
	}
	if (retval > 0) {
		unsigned int status;

		if (unlikely(retval != map->m_len)) {
			ext4_warning(inode->i_sb,
				     "ES len assertion failed for inode "
				     "%lu: retval %d != map->m_len %d",
				     inode->i_ino, retval, map->m_len);
			WARN_ON(1);
		}

		status = map->m_flags & EXT4_MAP_UNWRITTEN ?
				EXTENT_STATUS_UNWRITTEN : EXTENT_STATUS_WRITTEN;
		if (!(flags & EXT4_GET_BLOCKS_DELALLOC_RESERVE) &&
		    !(status & EXTENT_STATUS_WRITTEN) &&
		    ext4_es_scan_range(inode, &ext4_es_is_delayed, map->m_lblk,
				       map->m_lblk + map->m_len - 1))
			status |= EXTENT_STATUS_DELAYED;
		ext4_es_insert_extent(inode, map->m_lblk, map->m_len,
				      map->m_pblk, status);
	}
	up_read((&EXT4_I(inode)->i_data_sem));

found:
	if (retval > 0 && map->m_flags & EXT4_MAP_MAPPED) {
		ret = check_block_validity(inode, map);
		if (ret != 0)
			return ret;
	}

	 
	if ((flags & EXT4_GET_BLOCKS_CREATE) == 0)
		return retval;

	 
	if (retval > 0 && map->m_flags & EXT4_MAP_MAPPED)
		 
		if (!(flags & EXT4_GET_BLOCKS_CONVERT_UNWRITTEN))
			return retval;

	 
	map->m_flags &= ~EXT4_MAP_FLAGS;

	 
	down_write(&EXT4_I(inode)->i_data_sem);

	 
	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)) {
		retval = ext4_ext_map_blocks(handle, inode, map, flags);
	} else {
		retval = ext4_ind_map_blocks(handle, inode, map, flags);

		if (retval > 0 && map->m_flags & EXT4_MAP_NEW) {
			 
			ext4_clear_inode_state(inode, EXT4_STATE_EXT_MIGRATE);
		}
	}

	if (retval > 0) {
		unsigned int status;

		if (unlikely(retval != map->m_len)) {
			ext4_warning(inode->i_sb,
				     "ES len assertion failed for inode "
				     "%lu: retval %d != map->m_len %d",
				     inode->i_ino, retval, map->m_len);
			WARN_ON(1);
		}

		 
		if (flags & EXT4_GET_BLOCKS_ZERO &&
		    map->m_flags & EXT4_MAP_MAPPED &&
		    map->m_flags & EXT4_MAP_NEW) {
			ret = ext4_issue_zeroout(inode, map->m_lblk,
						 map->m_pblk, map->m_len);
			if (ret) {
				retval = ret;
				goto out_sem;
			}
		}

		 
		if ((flags & EXT4_GET_BLOCKS_PRE_IO) &&
		    ext4_es_lookup_extent(inode, map->m_lblk, NULL, &es)) {
			if (ext4_es_is_written(&es))
				goto out_sem;
		}
		status = map->m_flags & EXT4_MAP_UNWRITTEN ?
				EXTENT_STATUS_UNWRITTEN : EXTENT_STATUS_WRITTEN;
		if (!(flags & EXT4_GET_BLOCKS_DELALLOC_RESERVE) &&
		    !(status & EXTENT_STATUS_WRITTEN) &&
		    ext4_es_scan_range(inode, &ext4_es_is_delayed, map->m_lblk,
				       map->m_lblk + map->m_len - 1))
			status |= EXTENT_STATUS_DELAYED;
		ext4_es_insert_extent(inode, map->m_lblk, map->m_len,
				      map->m_pblk, status);
	}

out_sem:
	up_write((&EXT4_I(inode)->i_data_sem));
	if (retval > 0 && map->m_flags & EXT4_MAP_MAPPED) {
		ret = check_block_validity(inode, map);
		if (ret != 0)
			return ret;

		 
		if (map->m_flags & EXT4_MAP_NEW &&
		    !(map->m_flags & EXT4_MAP_UNWRITTEN) &&
		    !(flags & EXT4_GET_BLOCKS_ZERO) &&
		    !ext4_is_quota_file(inode) &&
		    ext4_should_order_data(inode)) {
			loff_t start_byte =
				(loff_t)map->m_lblk << inode->i_blkbits;
			loff_t length = (loff_t)map->m_len << inode->i_blkbits;

			if (flags & EXT4_GET_BLOCKS_IO_SUBMIT)
				ret = ext4_jbd2_inode_add_wait(handle, inode,
						start_byte, length);
			else
				ret = ext4_jbd2_inode_add_write(handle, inode,
						start_byte, length);
			if (ret)
				return ret;
		}
	}
	if (retval > 0 && (map->m_flags & EXT4_MAP_UNWRITTEN ||
				map->m_flags & EXT4_MAP_MAPPED))
		ext4_fc_track_range(handle, inode, map->m_lblk,
					map->m_lblk + map->m_len - 1);
	if (retval < 0)
		ext_debug(inode, "failed with err %d\n", retval);
	return retval;
}

 
static void ext4_update_bh_state(struct buffer_head *bh, unsigned long flags)
{
	unsigned long old_state;
	unsigned long new_state;

	flags &= EXT4_MAP_FLAGS;

	 
	if (!bh->b_page) {
		bh->b_state = (bh->b_state & ~EXT4_MAP_FLAGS) | flags;
		return;
	}
	 
	old_state = READ_ONCE(bh->b_state);
	do {
		new_state = (old_state & ~EXT4_MAP_FLAGS) | flags;
	} while (unlikely(!try_cmpxchg(&bh->b_state, &old_state, new_state)));
}

static int _ext4_get_block(struct inode *inode, sector_t iblock,
			   struct buffer_head *bh, int flags)
{
	struct ext4_map_blocks map;
	int ret = 0;

	if (ext4_has_inline_data(inode))
		return -ERANGE;

	map.m_lblk = iblock;
	map.m_len = bh->b_size >> inode->i_blkbits;

	ret = ext4_map_blocks(ext4_journal_current_handle(), inode, &map,
			      flags);
	if (ret > 0) {
		map_bh(bh, inode->i_sb, map.m_pblk);
		ext4_update_bh_state(bh, map.m_flags);
		bh->b_size = inode->i_sb->s_blocksize * map.m_len;
		ret = 0;
	} else if (ret == 0) {
		 
		bh->b_size = inode->i_sb->s_blocksize * map.m_len;
	}
	return ret;
}

int ext4_get_block(struct inode *inode, sector_t iblock,
		   struct buffer_head *bh, int create)
{
	return _ext4_get_block(inode, iblock, bh,
			       create ? EXT4_GET_BLOCKS_CREATE : 0);
}

 
int ext4_get_block_unwritten(struct inode *inode, sector_t iblock,
			     struct buffer_head *bh_result, int create)
{
	int ret = 0;

	ext4_debug("ext4_get_block_unwritten: inode %lu, create flag %d\n",
		   inode->i_ino, create);
	ret = _ext4_get_block(inode, iblock, bh_result,
			       EXT4_GET_BLOCKS_CREATE_UNWRIT_EXT);

	 
	if (ret == 0 && buffer_unwritten(bh_result))
		set_buffer_new(bh_result);

	return ret;
}

 
#define DIO_MAX_BLOCKS 4096

 
struct buffer_head *ext4_getblk(handle_t *handle, struct inode *inode,
				ext4_lblk_t block, int map_flags)
{
	struct ext4_map_blocks map;
	struct buffer_head *bh;
	int create = map_flags & EXT4_GET_BLOCKS_CREATE;
	bool nowait = map_flags & EXT4_GET_BLOCKS_CACHED_NOWAIT;
	int err;

	ASSERT((EXT4_SB(inode->i_sb)->s_mount_state & EXT4_FC_REPLAY)
		    || handle != NULL || create == 0);
	ASSERT(create == 0 || !nowait);

	map.m_lblk = block;
	map.m_len = 1;
	err = ext4_map_blocks(handle, inode, &map, map_flags);

	if (err == 0)
		return create ? ERR_PTR(-ENOSPC) : NULL;
	if (err < 0)
		return ERR_PTR(err);

	if (nowait)
		return sb_find_get_block(inode->i_sb, map.m_pblk);

	bh = sb_getblk(inode->i_sb, map.m_pblk);
	if (unlikely(!bh))
		return ERR_PTR(-ENOMEM);
	if (map.m_flags & EXT4_MAP_NEW) {
		ASSERT(create != 0);
		ASSERT((EXT4_SB(inode->i_sb)->s_mount_state & EXT4_FC_REPLAY)
			    || (handle != NULL));

		 
		lock_buffer(bh);
		BUFFER_TRACE(bh, "call get_create_access");
		err = ext4_journal_get_create_access(handle, inode->i_sb, bh,
						     EXT4_JTR_NONE);
		if (unlikely(err)) {
			unlock_buffer(bh);
			goto errout;
		}
		if (!buffer_uptodate(bh)) {
			memset(bh->b_data, 0, inode->i_sb->s_blocksize);
			set_buffer_uptodate(bh);
		}
		unlock_buffer(bh);
		BUFFER_TRACE(bh, "call ext4_handle_dirty_metadata");
		err = ext4_handle_dirty_metadata(handle, inode, bh);
		if (unlikely(err))
			goto errout;
	} else
		BUFFER_TRACE(bh, "not a new buffer");
	return bh;
errout:
	brelse(bh);
	return ERR_PTR(err);
}

struct buffer_head *ext4_bread(handle_t *handle, struct inode *inode,
			       ext4_lblk_t block, int map_flags)
{
	struct buffer_head *bh;
	int ret;

	bh = ext4_getblk(handle, inode, block, map_flags);
	if (IS_ERR(bh))
		return bh;
	if (!bh || ext4_buffer_uptodate(bh))
		return bh;

	ret = ext4_read_bh_lock(bh, REQ_META | REQ_PRIO, true);
	if (ret) {
		put_bh(bh);
		return ERR_PTR(ret);
	}
	return bh;
}

 
int ext4_bread_batch(struct inode *inode, ext4_lblk_t block, int bh_count,
		     bool wait, struct buffer_head **bhs)
{
	int i, err;

	for (i = 0; i < bh_count; i++) {
		bhs[i] = ext4_getblk(NULL, inode, block + i, 0  );
		if (IS_ERR(bhs[i])) {
			err = PTR_ERR(bhs[i]);
			bh_count = i;
			goto out_brelse;
		}
	}

	for (i = 0; i < bh_count; i++)
		 
		if (bhs[i] && !ext4_buffer_uptodate(bhs[i]))
			ext4_read_bh_lock(bhs[i], REQ_META | REQ_PRIO, false);

	if (!wait)
		return 0;

	for (i = 0; i < bh_count; i++)
		if (bhs[i])
			wait_on_buffer(bhs[i]);

	for (i = 0; i < bh_count; i++) {
		if (bhs[i] && !buffer_uptodate(bhs[i])) {
			err = -EIO;
			goto out_brelse;
		}
	}
	return 0;

out_brelse:
	for (i = 0; i < bh_count; i++) {
		brelse(bhs[i]);
		bhs[i] = NULL;
	}
	return err;
}

int ext4_walk_page_buffers(handle_t *handle, struct inode *inode,
			   struct buffer_head *head,
			   unsigned from,
			   unsigned to,
			   int *partial,
			   int (*fn)(handle_t *handle, struct inode *inode,
				     struct buffer_head *bh))
{
	struct buffer_head *bh;
	unsigned block_start, block_end;
	unsigned blocksize = head->b_size;
	int err, ret = 0;
	struct buffer_head *next;

	for (bh = head, block_start = 0;
	     ret == 0 && (bh != head || !block_start);
	     block_start = block_end, bh = next) {
		next = bh->b_this_page;
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (partial && !buffer_uptodate(bh))
				*partial = 1;
			continue;
		}
		err = (*fn)(handle, inode, bh);
		if (!ret)
			ret = err;
	}
	return ret;
}

 
static int ext4_dirty_journalled_data(handle_t *handle, struct buffer_head *bh)
{
	folio_mark_dirty(bh->b_folio);
	return ext4_handle_dirty_metadata(handle, NULL, bh);
}

int do_journal_get_write_access(handle_t *handle, struct inode *inode,
				struct buffer_head *bh)
{
	int dirty = buffer_dirty(bh);
	int ret;

	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	 
	if (dirty)
		clear_buffer_dirty(bh);
	BUFFER_TRACE(bh, "get write access");
	ret = ext4_journal_get_write_access(handle, inode->i_sb, bh,
					    EXT4_JTR_NONE);
	if (!ret && dirty)
		ret = ext4_dirty_journalled_data(handle, bh);
	return ret;
}

#ifdef CONFIG_FS_ENCRYPTION
static int ext4_block_write_begin(struct folio *folio, loff_t pos, unsigned len,
				  get_block_t *get_block)
{
	unsigned from = pos & (PAGE_SIZE - 1);
	unsigned to = from + len;
	struct inode *inode = folio->mapping->host;
	unsigned block_start, block_end;
	sector_t block;
	int err = 0;
	unsigned blocksize = inode->i_sb->s_blocksize;
	unsigned bbits;
	struct buffer_head *bh, *head, *wait[2];
	int nr_wait = 0;
	int i;

	BUG_ON(!folio_test_locked(folio));
	BUG_ON(from > PAGE_SIZE);
	BUG_ON(to > PAGE_SIZE);
	BUG_ON(from > to);

	head = folio_buffers(folio);
	if (!head) {
		create_empty_buffers(&folio->page, blocksize, 0);
		head = folio_buffers(folio);
	}
	bbits = ilog2(blocksize);
	block = (sector_t)folio->index << (PAGE_SHIFT - bbits);

	for (bh = head, block_start = 0; bh != head || !block_start;
	    block++, block_start = block_end, bh = bh->b_this_page) {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (folio_test_uptodate(folio)) {
				set_buffer_uptodate(bh);
			}
			continue;
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);
		if (!buffer_mapped(bh)) {
			WARN_ON(bh->b_size != blocksize);
			err = get_block(inode, block, bh, 1);
			if (err)
				break;
			if (buffer_new(bh)) {
				if (folio_test_uptodate(folio)) {
					clear_buffer_new(bh);
					set_buffer_uptodate(bh);
					mark_buffer_dirty(bh);
					continue;
				}
				if (block_end > to || block_start < from)
					folio_zero_segments(folio, to,
							    block_end,
							    block_start, from);
				continue;
			}
		}
		if (folio_test_uptodate(folio)) {
			set_buffer_uptodate(bh);
			continue;
		}
		if (!buffer_uptodate(bh) && !buffer_delay(bh) &&
		    !buffer_unwritten(bh) &&
		    (block_start < from || block_end > to)) {
			ext4_read_bh_lock(bh, 0, false);
			wait[nr_wait++] = bh;
		}
	}
	 
	for (i = 0; i < nr_wait; i++) {
		wait_on_buffer(wait[i]);
		if (!buffer_uptodate(wait[i]))
			err = -EIO;
	}
	if (unlikely(err)) {
		folio_zero_new_buffers(folio, from, to);
	} else if (fscrypt_inode_uses_fs_layer_crypto(inode)) {
		for (i = 0; i < nr_wait; i++) {
			int err2;

			err2 = fscrypt_decrypt_pagecache_blocks(folio,
						blocksize, bh_offset(wait[i]));
			if (err2) {
				clear_buffer_uptodate(wait[i]);
				err = err2;
			}
		}
	}

	return err;
}
#endif

 
static int ext4_write_begin(struct file *file, struct address_space *mapping,
			    loff_t pos, unsigned len,
			    struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	int ret, needed_blocks;
	handle_t *handle;
	int retries = 0;
	struct folio *folio;
	pgoff_t index;
	unsigned from, to;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	trace_ext4_write_begin(inode, pos, len);
	 
	needed_blocks = ext4_writepage_trans_blocks(inode) + 1;
	index = pos >> PAGE_SHIFT;
	from = pos & (PAGE_SIZE - 1);
	to = from + len;

	if (ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA)) {
		ret = ext4_try_to_write_inline_data(mapping, inode, pos, len,
						    pagep);
		if (ret < 0)
			return ret;
		if (ret == 1)
			return 0;
	}

	 
retry_grab:
	folio = __filemap_get_folio(mapping, index, FGP_WRITEBEGIN,
					mapping_gfp_mask(mapping));
	if (IS_ERR(folio))
		return PTR_ERR(folio);
	 
	if (!folio_buffers(folio))
		create_empty_buffers(&folio->page, inode->i_sb->s_blocksize, 0);

	folio_unlock(folio);

retry_journal:
	handle = ext4_journal_start(inode, EXT4_HT_WRITE_PAGE, needed_blocks);
	if (IS_ERR(handle)) {
		folio_put(folio);
		return PTR_ERR(handle);
	}

	folio_lock(folio);
	if (folio->mapping != mapping) {
		 
		folio_unlock(folio);
		folio_put(folio);
		ext4_journal_stop(handle);
		goto retry_grab;
	}
	 
	folio_wait_stable(folio);

#ifdef CONFIG_FS_ENCRYPTION
	if (ext4_should_dioread_nolock(inode))
		ret = ext4_block_write_begin(folio, pos, len,
					     ext4_get_block_unwritten);
	else
		ret = ext4_block_write_begin(folio, pos, len, ext4_get_block);
#else
	if (ext4_should_dioread_nolock(inode))
		ret = __block_write_begin(&folio->page, pos, len,
					  ext4_get_block_unwritten);
	else
		ret = __block_write_begin(&folio->page, pos, len, ext4_get_block);
#endif
	if (!ret && ext4_should_journal_data(inode)) {
		ret = ext4_walk_page_buffers(handle, inode,
					     folio_buffers(folio), from, to,
					     NULL, do_journal_get_write_access);
	}

	if (ret) {
		bool extended = (pos + len > inode->i_size) &&
				!ext4_verity_in_progress(inode);

		folio_unlock(folio);
		 
		if (extended && ext4_can_truncate(inode))
			ext4_orphan_add(handle, inode);

		ext4_journal_stop(handle);
		if (extended) {
			ext4_truncate_failed_write(inode);
			 
			if (inode->i_nlink)
				ext4_orphan_del(NULL, inode);
		}

		if (ret == -ENOSPC &&
		    ext4_should_retry_alloc(inode->i_sb, &retries))
			goto retry_journal;
		folio_put(folio);
		return ret;
	}
	*pagep = &folio->page;
	return ret;
}

 
static int write_end_fn(handle_t *handle, struct inode *inode,
			struct buffer_head *bh)
{
	int ret;
	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	set_buffer_uptodate(bh);
	ret = ext4_dirty_journalled_data(handle, bh);
	clear_buffer_meta(bh);
	clear_buffer_prio(bh);
	return ret;
}

 
static int ext4_write_end(struct file *file,
			  struct address_space *mapping,
			  loff_t pos, unsigned len, unsigned copied,
			  struct page *page, void *fsdata)
{
	struct folio *folio = page_folio(page);
	handle_t *handle = ext4_journal_current_handle();
	struct inode *inode = mapping->host;
	loff_t old_size = inode->i_size;
	int ret = 0, ret2;
	int i_size_changed = 0;
	bool verity = ext4_verity_in_progress(inode);

	trace_ext4_write_end(inode, pos, len, copied);

	if (ext4_has_inline_data(inode) &&
	    ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA))
		return ext4_write_inline_data_end(inode, pos, len, copied,
						  folio);

	copied = block_write_end(file, mapping, pos, len, copied, page, fsdata);
	 
	if (!verity)
		i_size_changed = ext4_update_inode_size(inode, pos + copied);
	folio_unlock(folio);
	folio_put(folio);

	if (old_size < pos && !verity)
		pagecache_isize_extended(inode, old_size, pos);
	 
	if (i_size_changed)
		ret = ext4_mark_inode_dirty(handle, inode);

	if (pos + len > inode->i_size && !verity && ext4_can_truncate(inode))
		 
		ext4_orphan_add(handle, inode);

	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;

	if (pos + len > inode->i_size && !verity) {
		ext4_truncate_failed_write(inode);
		 
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}

	return ret ? ret : copied;
}

 
static void ext4_journalled_zero_new_buffers(handle_t *handle,
					    struct inode *inode,
					    struct folio *folio,
					    unsigned from, unsigned to)
{
	unsigned int block_start = 0, block_end;
	struct buffer_head *head, *bh;

	bh = head = folio_buffers(folio);
	do {
		block_end = block_start + bh->b_size;
		if (buffer_new(bh)) {
			if (block_end > from && block_start < to) {
				if (!folio_test_uptodate(folio)) {
					unsigned start, size;

					start = max(from, block_start);
					size = min(to, block_end) - start;

					folio_zero_range(folio, start, size);
					write_end_fn(handle, inode, bh);
				}
				clear_buffer_new(bh);
			}
		}
		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);
}

static int ext4_journalled_write_end(struct file *file,
				     struct address_space *mapping,
				     loff_t pos, unsigned len, unsigned copied,
				     struct page *page, void *fsdata)
{
	struct folio *folio = page_folio(page);
	handle_t *handle = ext4_journal_current_handle();
	struct inode *inode = mapping->host;
	loff_t old_size = inode->i_size;
	int ret = 0, ret2;
	int partial = 0;
	unsigned from, to;
	int size_changed = 0;
	bool verity = ext4_verity_in_progress(inode);

	trace_ext4_journalled_write_end(inode, pos, len, copied);
	from = pos & (PAGE_SIZE - 1);
	to = from + len;

	BUG_ON(!ext4_handle_valid(handle));

	if (ext4_has_inline_data(inode))
		return ext4_write_inline_data_end(inode, pos, len, copied,
						  folio);

	if (unlikely(copied < len) && !folio_test_uptodate(folio)) {
		copied = 0;
		ext4_journalled_zero_new_buffers(handle, inode, folio,
						 from, to);
	} else {
		if (unlikely(copied < len))
			ext4_journalled_zero_new_buffers(handle, inode, folio,
							 from + copied, to);
		ret = ext4_walk_page_buffers(handle, inode,
					     folio_buffers(folio),
					     from, from + copied, &partial,
					     write_end_fn);
		if (!partial)
			folio_mark_uptodate(folio);
	}
	if (!verity)
		size_changed = ext4_update_inode_size(inode, pos + copied);
	EXT4_I(inode)->i_datasync_tid = handle->h_transaction->t_tid;
	folio_unlock(folio);
	folio_put(folio);

	if (old_size < pos && !verity)
		pagecache_isize_extended(inode, old_size, pos);

	if (size_changed) {
		ret2 = ext4_mark_inode_dirty(handle, inode);
		if (!ret)
			ret = ret2;
	}

	if (pos + len > inode->i_size && !verity && ext4_can_truncate(inode))
		 
		ext4_orphan_add(handle, inode);

	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;
	if (pos + len > inode->i_size && !verity) {
		ext4_truncate_failed_write(inode);
		 
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}

	return ret ? ret : copied;
}

 
static int ext4_da_reserve_space(struct inode *inode)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct ext4_inode_info *ei = EXT4_I(inode);
	int ret;

	 
	ret = dquot_reserve_block(inode, EXT4_C2B(sbi, 1));
	if (ret)
		return ret;

	spin_lock(&ei->i_block_reservation_lock);
	if (ext4_claim_free_clusters(sbi, 1, 0)) {
		spin_unlock(&ei->i_block_reservation_lock);
		dquot_release_reservation_block(inode, EXT4_C2B(sbi, 1));
		return -ENOSPC;
	}
	ei->i_reserved_data_blocks++;
	trace_ext4_da_reserve_space(inode);
	spin_unlock(&ei->i_block_reservation_lock);

	return 0;        
}

void ext4_da_release_space(struct inode *inode, int to_free)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct ext4_inode_info *ei = EXT4_I(inode);

	if (!to_free)
		return;		 

	spin_lock(&EXT4_I(inode)->i_block_reservation_lock);

	trace_ext4_da_release_space(inode, to_free);
	if (unlikely(to_free > ei->i_reserved_data_blocks)) {
		 
		ext4_warning(inode->i_sb, "ext4_da_release_space: "
			 "ino %lu, to_free %d with only %d reserved "
			 "data blocks", inode->i_ino, to_free,
			 ei->i_reserved_data_blocks);
		WARN_ON(1);
		to_free = ei->i_reserved_data_blocks;
	}
	ei->i_reserved_data_blocks -= to_free;

	 
	percpu_counter_sub(&sbi->s_dirtyclusters_counter, to_free);

	spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);

	dquot_release_reservation_block(inode, EXT4_C2B(sbi, to_free));
}

 

struct mpage_da_data {
	 
	struct inode *inode;
	struct writeback_control *wbc;
	unsigned int can_map:1;	 

	 
	pgoff_t first_page;	 
	pgoff_t next_page;	 
	pgoff_t last_page;	 
	 
	struct ext4_map_blocks map;
	struct ext4_io_submit io_submit;	 
	unsigned int do_map:1;
	unsigned int scanned_until_end:1;
	unsigned int journalled_more_data:1;
};

static void mpage_release_unused_pages(struct mpage_da_data *mpd,
				       bool invalidate)
{
	unsigned nr, i;
	pgoff_t index, end;
	struct folio_batch fbatch;
	struct inode *inode = mpd->inode;
	struct address_space *mapping = inode->i_mapping;

	 
	if (mpd->first_page >= mpd->next_page)
		return;

	mpd->scanned_until_end = 0;
	index = mpd->first_page;
	end   = mpd->next_page - 1;
	if (invalidate) {
		ext4_lblk_t start, last;
		start = index << (PAGE_SHIFT - inode->i_blkbits);
		last = end << (PAGE_SHIFT - inode->i_blkbits);

		 
		down_write(&EXT4_I(inode)->i_data_sem);
		ext4_es_remove_extent(inode, start, last - start + 1);
		up_write(&EXT4_I(inode)->i_data_sem);
	}

	folio_batch_init(&fbatch);
	while (index <= end) {
		nr = filemap_get_folios(mapping, &index, end, &fbatch);
		if (nr == 0)
			break;
		for (i = 0; i < nr; i++) {
			struct folio *folio = fbatch.folios[i];

			if (folio->index < mpd->first_page)
				continue;
			if (folio_next_index(folio) - 1 > end)
				continue;
			BUG_ON(!folio_test_locked(folio));
			BUG_ON(folio_test_writeback(folio));
			if (invalidate) {
				if (folio_mapped(folio))
					folio_clear_dirty_for_io(folio);
				block_invalidate_folio(folio, 0,
						folio_size(folio));
				folio_clear_uptodate(folio);
			}
			folio_unlock(folio);
		}
		folio_batch_release(&fbatch);
	}
}

static void ext4_print_free_blocks(struct inode *inode)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct super_block *sb = inode->i_sb;
	struct ext4_inode_info *ei = EXT4_I(inode);

	ext4_msg(sb, KERN_CRIT, "Total free blocks count %lld",
	       EXT4_C2B(EXT4_SB(inode->i_sb),
			ext4_count_free_clusters(sb)));
	ext4_msg(sb, KERN_CRIT, "Free/Dirty block details");
	ext4_msg(sb, KERN_CRIT, "free_blocks=%lld",
	       (long long) EXT4_C2B(EXT4_SB(sb),
		percpu_counter_sum(&sbi->s_freeclusters_counter)));
	ext4_msg(sb, KERN_CRIT, "dirty_blocks=%lld",
	       (long long) EXT4_C2B(EXT4_SB(sb),
		percpu_counter_sum(&sbi->s_dirtyclusters_counter)));
	ext4_msg(sb, KERN_CRIT, "Block reservation details");
	ext4_msg(sb, KERN_CRIT, "i_reserved_data_blocks=%u",
		 ei->i_reserved_data_blocks);
	return;
}

 
static int ext4_insert_delayed_block(struct inode *inode, ext4_lblk_t lblk)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	int ret;
	bool allocated = false;

	 
	if (sbi->s_cluster_ratio == 1) {
		ret = ext4_da_reserve_space(inode);
		if (ret != 0)    
			return ret;
	} else {    
		if (!ext4_es_scan_clu(inode, &ext4_es_is_delonly, lblk)) {
			if (!ext4_es_scan_clu(inode,
					      &ext4_es_is_mapped, lblk)) {
				ret = ext4_clu_mapped(inode,
						      EXT4_B2C(sbi, lblk));
				if (ret < 0)
					return ret;
				if (ret == 0) {
					ret = ext4_da_reserve_space(inode);
					if (ret != 0)    
						return ret;
				} else {
					allocated = true;
				}
			} else {
				allocated = true;
			}
		}
	}

	ext4_es_insert_delayed_block(inode, lblk, allocated);
	return 0;
}

 
static int ext4_da_map_blocks(struct inode *inode, sector_t iblock,
			      struct ext4_map_blocks *map,
			      struct buffer_head *bh)
{
	struct extent_status es;
	int retval;
	sector_t invalid_block = ~((sector_t) 0xffff);
#ifdef ES_AGGRESSIVE_TEST
	struct ext4_map_blocks orig_map;

	memcpy(&orig_map, map, sizeof(*map));
#endif

	if (invalid_block < ext4_blocks_count(EXT4_SB(inode->i_sb)->s_es))
		invalid_block = ~0;

	map->m_flags = 0;
	ext_debug(inode, "max_blocks %u, logical block %lu\n", map->m_len,
		  (unsigned long) map->m_lblk);

	 
	if (ext4_es_lookup_extent(inode, iblock, NULL, &es)) {
		if (ext4_es_is_hole(&es)) {
			retval = 0;
			down_read(&EXT4_I(inode)->i_data_sem);
			goto add_delayed;
		}

		 
		if (ext4_es_is_delayed(&es) && !ext4_es_is_unwritten(&es)) {
			map_bh(bh, inode->i_sb, invalid_block);
			set_buffer_new(bh);
			set_buffer_delay(bh);
			return 0;
		}

		map->m_pblk = ext4_es_pblock(&es) + iblock - es.es_lblk;
		retval = es.es_len - (iblock - es.es_lblk);
		if (retval > map->m_len)
			retval = map->m_len;
		map->m_len = retval;
		if (ext4_es_is_written(&es))
			map->m_flags |= EXT4_MAP_MAPPED;
		else if (ext4_es_is_unwritten(&es))
			map->m_flags |= EXT4_MAP_UNWRITTEN;
		else
			BUG();

#ifdef ES_AGGRESSIVE_TEST
		ext4_map_blocks_es_recheck(NULL, inode, map, &orig_map, 0);
#endif
		return retval;
	}

	 
	down_read(&EXT4_I(inode)->i_data_sem);
	if (ext4_has_inline_data(inode))
		retval = 0;
	else if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		retval = ext4_ext_map_blocks(NULL, inode, map, 0);
	else
		retval = ext4_ind_map_blocks(NULL, inode, map, 0);

add_delayed:
	if (retval == 0) {
		int ret;

		 

		ret = ext4_insert_delayed_block(inode, map->m_lblk);
		if (ret != 0) {
			retval = ret;
			goto out_unlock;
		}

		map_bh(bh, inode->i_sb, invalid_block);
		set_buffer_new(bh);
		set_buffer_delay(bh);
	} else if (retval > 0) {
		unsigned int status;

		if (unlikely(retval != map->m_len)) {
			ext4_warning(inode->i_sb,
				     "ES len assertion failed for inode "
				     "%lu: retval %d != map->m_len %d",
				     inode->i_ino, retval, map->m_len);
			WARN_ON(1);
		}

		status = map->m_flags & EXT4_MAP_UNWRITTEN ?
				EXTENT_STATUS_UNWRITTEN : EXTENT_STATUS_WRITTEN;
		ext4_es_insert_extent(inode, map->m_lblk, map->m_len,
				      map->m_pblk, status);
	}

out_unlock:
	up_read((&EXT4_I(inode)->i_data_sem));

	return retval;
}

 
int ext4_da_get_block_prep(struct inode *inode, sector_t iblock,
			   struct buffer_head *bh, int create)
{
	struct ext4_map_blocks map;
	int ret = 0;

	BUG_ON(create == 0);
	BUG_ON(bh->b_size != inode->i_sb->s_blocksize);

	map.m_lblk = iblock;
	map.m_len = 1;

	 
	ret = ext4_da_map_blocks(inode, iblock, &map, bh);
	if (ret <= 0)
		return ret;

	map_bh(bh, inode->i_sb, map.m_pblk);
	ext4_update_bh_state(bh, map.m_flags);

	if (buffer_unwritten(bh)) {
		 
		set_buffer_new(bh);
		set_buffer_mapped(bh);
	}
	return 0;
}

static void mpage_folio_done(struct mpage_da_data *mpd, struct folio *folio)
{
	mpd->first_page += folio_nr_pages(folio);
	folio_unlock(folio);
}

static int mpage_submit_folio(struct mpage_da_data *mpd, struct folio *folio)
{
	size_t len;
	loff_t size;
	int err;

	BUG_ON(folio->index != mpd->first_page);
	folio_clear_dirty_for_io(folio);
	 
	size = i_size_read(mpd->inode);
	len = folio_size(folio);
	if (folio_pos(folio) + len > size &&
	    !ext4_verity_in_progress(mpd->inode))
		len = size & ~PAGE_MASK;
	err = ext4_bio_write_folio(&mpd->io_submit, folio, len);
	if (!err)
		mpd->wbc->nr_to_write--;

	return err;
}

#define BH_FLAGS (BIT(BH_Unwritten) | BIT(BH_Delay))

 
#define MAX_WRITEPAGES_EXTENT_LEN 2048

 
static bool mpage_add_bh_to_extent(struct mpage_da_data *mpd, ext4_lblk_t lblk,
				   struct buffer_head *bh)
{
	struct ext4_map_blocks *map = &mpd->map;

	 
	if (!buffer_dirty(bh) || !buffer_mapped(bh) ||
	    (!buffer_delay(bh) && !buffer_unwritten(bh))) {
		 
		if (map->m_len == 0)
			return true;
		return false;
	}

	 
	if (map->m_len == 0) {
		 
		if (!mpd->do_map)
			return false;
		map->m_lblk = lblk;
		map->m_len = 1;
		map->m_flags = bh->b_state & BH_FLAGS;
		return true;
	}

	 
	if (map->m_len >= MAX_WRITEPAGES_EXTENT_LEN)
		return false;

	 
	if (lblk == map->m_lblk + map->m_len &&
	    (bh->b_state & BH_FLAGS) == map->m_flags) {
		map->m_len++;
		return true;
	}
	return false;
}

 
static int mpage_process_page_bufs(struct mpage_da_data *mpd,
				   struct buffer_head *head,
				   struct buffer_head *bh,
				   ext4_lblk_t lblk)
{
	struct inode *inode = mpd->inode;
	int err;
	ext4_lblk_t blocks = (i_size_read(inode) + i_blocksize(inode) - 1)
							>> inode->i_blkbits;

	if (ext4_verity_in_progress(inode))
		blocks = EXT_MAX_BLOCKS;

	do {
		BUG_ON(buffer_locked(bh));

		if (lblk >= blocks || !mpage_add_bh_to_extent(mpd, lblk, bh)) {
			 
			if (mpd->map.m_len)
				return 0;
			 
			if (!mpd->do_map)
				return 0;
			 
			break;
		}
	} while (lblk++, (bh = bh->b_this_page) != head);
	 
	if (mpd->map.m_len == 0) {
		err = mpage_submit_folio(mpd, head->b_folio);
		if (err < 0)
			return err;
		mpage_folio_done(mpd, head->b_folio);
	}
	if (lblk >= blocks) {
		mpd->scanned_until_end = 1;
		return 0;
	}
	return 1;
}

 
static int mpage_process_folio(struct mpage_da_data *mpd, struct folio *folio,
			      ext4_lblk_t *m_lblk, ext4_fsblk_t *m_pblk,
			      bool *map_bh)
{
	struct buffer_head *head, *bh;
	ext4_io_end_t *io_end = mpd->io_submit.io_end;
	ext4_lblk_t lblk = *m_lblk;
	ext4_fsblk_t pblock = *m_pblk;
	int err = 0;
	int blkbits = mpd->inode->i_blkbits;
	ssize_t io_end_size = 0;
	struct ext4_io_end_vec *io_end_vec = ext4_last_io_end_vec(io_end);

	bh = head = folio_buffers(folio);
	do {
		if (lblk < mpd->map.m_lblk)
			continue;
		if (lblk >= mpd->map.m_lblk + mpd->map.m_len) {
			 
			mpd->map.m_len = 0;
			mpd->map.m_flags = 0;
			io_end_vec->size += io_end_size;

			err = mpage_process_page_bufs(mpd, head, bh, lblk);
			if (err > 0)
				err = 0;
			if (!err && mpd->map.m_len && mpd->map.m_lblk > lblk) {
				io_end_vec = ext4_alloc_io_end_vec(io_end);
				if (IS_ERR(io_end_vec)) {
					err = PTR_ERR(io_end_vec);
					goto out;
				}
				io_end_vec->offset = (loff_t)mpd->map.m_lblk << blkbits;
			}
			*map_bh = true;
			goto out;
		}
		if (buffer_delay(bh)) {
			clear_buffer_delay(bh);
			bh->b_blocknr = pblock++;
		}
		clear_buffer_unwritten(bh);
		io_end_size += (1 << blkbits);
	} while (lblk++, (bh = bh->b_this_page) != head);

	io_end_vec->size += io_end_size;
	*map_bh = false;
out:
	*m_lblk = lblk;
	*m_pblk = pblock;
	return err;
}

 
static int mpage_map_and_submit_buffers(struct mpage_da_data *mpd)
{
	struct folio_batch fbatch;
	unsigned nr, i;
	struct inode *inode = mpd->inode;
	int bpp_bits = PAGE_SHIFT - inode->i_blkbits;
	pgoff_t start, end;
	ext4_lblk_t lblk;
	ext4_fsblk_t pblock;
	int err;
	bool map_bh = false;

	start = mpd->map.m_lblk >> bpp_bits;
	end = (mpd->map.m_lblk + mpd->map.m_len - 1) >> bpp_bits;
	lblk = start << bpp_bits;
	pblock = mpd->map.m_pblk;

	folio_batch_init(&fbatch);
	while (start <= end) {
		nr = filemap_get_folios(inode->i_mapping, &start, end, &fbatch);
		if (nr == 0)
			break;
		for (i = 0; i < nr; i++) {
			struct folio *folio = fbatch.folios[i];

			err = mpage_process_folio(mpd, folio, &lblk, &pblock,
						 &map_bh);
			 
			if (err < 0 || map_bh)
				goto out;
			 
			err = mpage_submit_folio(mpd, folio);
			if (err < 0)
				goto out;
			mpage_folio_done(mpd, folio);
		}
		folio_batch_release(&fbatch);
	}
	 
	mpd->map.m_len = 0;
	mpd->map.m_flags = 0;
	return 0;
out:
	folio_batch_release(&fbatch);
	return err;
}

static int mpage_map_one_extent(handle_t *handle, struct mpage_da_data *mpd)
{
	struct inode *inode = mpd->inode;
	struct ext4_map_blocks *map = &mpd->map;
	int get_blocks_flags;
	int err, dioread_nolock;

	trace_ext4_da_write_pages_extent(inode, map);
	 
	get_blocks_flags = EXT4_GET_BLOCKS_CREATE |
			   EXT4_GET_BLOCKS_METADATA_NOFAIL |
			   EXT4_GET_BLOCKS_IO_SUBMIT;
	dioread_nolock = ext4_should_dioread_nolock(inode);
	if (dioread_nolock)
		get_blocks_flags |= EXT4_GET_BLOCKS_IO_CREATE_EXT;
	if (map->m_flags & BIT(BH_Delay))
		get_blocks_flags |= EXT4_GET_BLOCKS_DELALLOC_RESERVE;

	err = ext4_map_blocks(handle, inode, map, get_blocks_flags);
	if (err < 0)
		return err;
	if (dioread_nolock && (map->m_flags & EXT4_MAP_UNWRITTEN)) {
		if (!mpd->io_submit.io_end->handle &&
		    ext4_handle_valid(handle)) {
			mpd->io_submit.io_end->handle = handle->h_rsv_handle;
			handle->h_rsv_handle = NULL;
		}
		ext4_set_io_unwritten_flag(inode, mpd->io_submit.io_end);
	}

	BUG_ON(map->m_len == 0);
	return 0;
}

 
static int mpage_map_and_submit_extent(handle_t *handle,
				       struct mpage_da_data *mpd,
				       bool *give_up_on_write)
{
	struct inode *inode = mpd->inode;
	struct ext4_map_blocks *map = &mpd->map;
	int err;
	loff_t disksize;
	int progress = 0;
	ext4_io_end_t *io_end = mpd->io_submit.io_end;
	struct ext4_io_end_vec *io_end_vec;

	io_end_vec = ext4_alloc_io_end_vec(io_end);
	if (IS_ERR(io_end_vec))
		return PTR_ERR(io_end_vec);
	io_end_vec->offset = ((loff_t)map->m_lblk) << inode->i_blkbits;
	do {
		err = mpage_map_one_extent(handle, mpd);
		if (err < 0) {
			struct super_block *sb = inode->i_sb;

			if (ext4_forced_shutdown(sb))
				goto invalidate_dirty_pages;
			 
			if ((err == -ENOMEM) ||
			    (err == -ENOSPC && ext4_count_free_clusters(sb))) {
				if (progress)
					goto update_disksize;
				return err;
			}
			ext4_msg(sb, KERN_CRIT,
				 "Delayed block allocation failed for "
				 "inode %lu at logical offset %llu with"
				 " max blocks %u with error %d",
				 inode->i_ino,
				 (unsigned long long)map->m_lblk,
				 (unsigned)map->m_len, -err);
			ext4_msg(sb, KERN_CRIT,
				 "This should not happen!! Data will "
				 "be lost\n");
			if (err == -ENOSPC)
				ext4_print_free_blocks(inode);
		invalidate_dirty_pages:
			*give_up_on_write = true;
			return err;
		}
		progress = 1;
		 
		err = mpage_map_and_submit_buffers(mpd);
		if (err < 0)
			goto update_disksize;
	} while (map->m_len);

update_disksize:
	 
	disksize = ((loff_t)mpd->first_page) << PAGE_SHIFT;
	if (disksize > READ_ONCE(EXT4_I(inode)->i_disksize)) {
		int err2;
		loff_t i_size;

		down_write(&EXT4_I(inode)->i_data_sem);
		i_size = i_size_read(inode);
		if (disksize > i_size)
			disksize = i_size;
		if (disksize > EXT4_I(inode)->i_disksize)
			EXT4_I(inode)->i_disksize = disksize;
		up_write(&EXT4_I(inode)->i_data_sem);
		err2 = ext4_mark_inode_dirty(handle, inode);
		if (err2) {
			ext4_error_err(inode->i_sb, -err2,
				       "Failed to mark inode %lu dirty",
				       inode->i_ino);
		}
		if (!err)
			err = err2;
	}
	return err;
}

 
static int ext4_da_writepages_trans_blocks(struct inode *inode)
{
	int bpp = ext4_journal_blocks_per_page(inode);

	return ext4_meta_trans_blocks(inode,
				MAX_WRITEPAGES_EXTENT_LEN + bpp - 1, bpp);
}

static int ext4_journal_folio_buffers(handle_t *handle, struct folio *folio,
				     size_t len)
{
	struct buffer_head *page_bufs = folio_buffers(folio);
	struct inode *inode = folio->mapping->host;
	int ret, err;

	ret = ext4_walk_page_buffers(handle, inode, page_bufs, 0, len,
				     NULL, do_journal_get_write_access);
	err = ext4_walk_page_buffers(handle, inode, page_bufs, 0, len,
				     NULL, write_end_fn);
	if (ret == 0)
		ret = err;
	err = ext4_jbd2_inode_add_write(handle, inode, folio_pos(folio), len);
	if (ret == 0)
		ret = err;
	EXT4_I(inode)->i_datasync_tid = handle->h_transaction->t_tid;

	return ret;
}

static int mpage_journal_page_buffers(handle_t *handle,
				      struct mpage_da_data *mpd,
				      struct folio *folio)
{
	struct inode *inode = mpd->inode;
	loff_t size = i_size_read(inode);
	size_t len = folio_size(folio);

	folio_clear_checked(folio);
	mpd->wbc->nr_to_write--;

	if (folio_pos(folio) + len > size &&
	    !ext4_verity_in_progress(inode))
		len = size - folio_pos(folio);

	return ext4_journal_folio_buffers(handle, folio, len);
}

 
static int mpage_prepare_extent_to_map(struct mpage_da_data *mpd)
{
	struct address_space *mapping = mpd->inode->i_mapping;
	struct folio_batch fbatch;
	unsigned int nr_folios;
	pgoff_t index = mpd->first_page;
	pgoff_t end = mpd->last_page;
	xa_mark_t tag;
	int i, err = 0;
	int blkbits = mpd->inode->i_blkbits;
	ext4_lblk_t lblk;
	struct buffer_head *head;
	handle_t *handle = NULL;
	int bpp = ext4_journal_blocks_per_page(mpd->inode);

	if (mpd->wbc->sync_mode == WB_SYNC_ALL || mpd->wbc->tagged_writepages)
		tag = PAGECACHE_TAG_TOWRITE;
	else
		tag = PAGECACHE_TAG_DIRTY;

	mpd->map.m_len = 0;
	mpd->next_page = index;
	if (ext4_should_journal_data(mpd->inode)) {
		handle = ext4_journal_start(mpd->inode, EXT4_HT_WRITE_PAGE,
					    bpp);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
	}
	folio_batch_init(&fbatch);
	while (index <= end) {
		nr_folios = filemap_get_folios_tag(mapping, &index, end,
				tag, &fbatch);
		if (nr_folios == 0)
			break;

		for (i = 0; i < nr_folios; i++) {
			struct folio *folio = fbatch.folios[i];

			 
			if (mpd->wbc->sync_mode == WB_SYNC_NONE &&
			    mpd->wbc->nr_to_write <=
			    mpd->map.m_len >> (PAGE_SHIFT - blkbits))
				goto out;

			 
			if (mpd->map.m_len > 0 && mpd->next_page != folio->index)
				goto out;

			if (handle) {
				err = ext4_journal_ensure_credits(handle, bpp,
								  0);
				if (err < 0)
					goto out;
			}

			folio_lock(folio);
			 
			if (!folio_test_dirty(folio) ||
			    (folio_test_writeback(folio) &&
			     (mpd->wbc->sync_mode == WB_SYNC_NONE)) ||
			    unlikely(folio->mapping != mapping)) {
				folio_unlock(folio);
				continue;
			}

			folio_wait_writeback(folio);
			BUG_ON(folio_test_writeback(folio));

			 
			if (!folio_buffers(folio)) {
				ext4_warning_inode(mpd->inode, "page %lu does not have buffers attached", folio->index);
				folio_clear_dirty(folio);
				folio_unlock(folio);
				continue;
			}

			if (mpd->map.m_len == 0)
				mpd->first_page = folio->index;
			mpd->next_page = folio_next_index(folio);
			 
			if (!mpd->can_map) {
				err = mpage_submit_folio(mpd, folio);
				if (err < 0)
					goto out;
				 
				if (folio_test_checked(folio)) {
					err = mpage_journal_page_buffers(handle,
						mpd, folio);
					if (err < 0)
						goto out;
					mpd->journalled_more_data = 1;
				}
				mpage_folio_done(mpd, folio);
			} else {
				 
				lblk = ((ext4_lblk_t)folio->index) <<
					(PAGE_SHIFT - blkbits);
				head = folio_buffers(folio);
				err = mpage_process_page_bufs(mpd, head, head,
						lblk);
				if (err <= 0)
					goto out;
				err = 0;
			}
		}
		folio_batch_release(&fbatch);
		cond_resched();
	}
	mpd->scanned_until_end = 1;
	if (handle)
		ext4_journal_stop(handle);
	return 0;
out:
	folio_batch_release(&fbatch);
	if (handle)
		ext4_journal_stop(handle);
	return err;
}

static int ext4_do_writepages(struct mpage_da_data *mpd)
{
	struct writeback_control *wbc = mpd->wbc;
	pgoff_t	writeback_index = 0;
	long nr_to_write = wbc->nr_to_write;
	int range_whole = 0;
	int cycled = 1;
	handle_t *handle = NULL;
	struct inode *inode = mpd->inode;
	struct address_space *mapping = inode->i_mapping;
	int needed_blocks, rsv_blocks = 0, ret = 0;
	struct ext4_sb_info *sbi = EXT4_SB(mapping->host->i_sb);
	struct blk_plug plug;
	bool give_up_on_write = false;

	trace_ext4_writepages(inode, wbc);

	 
	if (!mapping->nrpages || !mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		goto out_writepages;

	 
	if (unlikely(ext4_forced_shutdown(mapping->host->i_sb))) {
		ret = -EROFS;
		goto out_writepages;
	}

	 
	if (ext4_has_inline_data(inode)) {
		 
		handle = ext4_journal_start(inode, EXT4_HT_INODE, 1);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			goto out_writepages;
		}
		BUG_ON(ext4_test_inode_state(inode,
				EXT4_STATE_MAY_INLINE_DATA));
		ext4_destroy_inline_data(handle, inode);
		ext4_journal_stop(handle);
	}

	 
	if (ext4_should_journal_data(inode)) {
		mpd->can_map = 0;
		if (wbc->sync_mode == WB_SYNC_ALL)
			ext4_fc_commit(sbi->s_journal,
				       EXT4_I(inode)->i_datasync_tid);
	}
	mpd->journalled_more_data = 0;

	if (ext4_should_dioread_nolock(inode)) {
		 
		rsv_blocks = 1 + ext4_chunk_trans_blocks(inode,
						PAGE_SIZE >> inode->i_blkbits);
	}

	if (wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
		range_whole = 1;

	if (wbc->range_cyclic) {
		writeback_index = mapping->writeback_index;
		if (writeback_index)
			cycled = 0;
		mpd->first_page = writeback_index;
		mpd->last_page = -1;
	} else {
		mpd->first_page = wbc->range_start >> PAGE_SHIFT;
		mpd->last_page = wbc->range_end >> PAGE_SHIFT;
	}

	ext4_io_submit_init(&mpd->io_submit, wbc);
retry:
	if (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages)
		tag_pages_for_writeback(mapping, mpd->first_page,
					mpd->last_page);
	blk_start_plug(&plug);

	 
	mpd->do_map = 0;
	mpd->scanned_until_end = 0;
	mpd->io_submit.io_end = ext4_init_io_end(inode, GFP_KERNEL);
	if (!mpd->io_submit.io_end) {
		ret = -ENOMEM;
		goto unplug;
	}
	ret = mpage_prepare_extent_to_map(mpd);
	 
	mpage_release_unused_pages(mpd, false);
	 
	ext4_io_submit(&mpd->io_submit);
	ext4_put_io_end_defer(mpd->io_submit.io_end);
	mpd->io_submit.io_end = NULL;
	if (ret < 0)
		goto unplug;

	while (!mpd->scanned_until_end && wbc->nr_to_write > 0) {
		 
		mpd->io_submit.io_end = ext4_init_io_end(inode, GFP_KERNEL);
		if (!mpd->io_submit.io_end) {
			ret = -ENOMEM;
			break;
		}

		WARN_ON_ONCE(!mpd->can_map);
		 
		BUG_ON(ext4_should_journal_data(inode));
		needed_blocks = ext4_da_writepages_trans_blocks(inode);

		 
		handle = ext4_journal_start_with_reserve(inode,
				EXT4_HT_WRITE_PAGE, needed_blocks, rsv_blocks);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			ext4_msg(inode->i_sb, KERN_CRIT, "%s: jbd2_start: "
			       "%ld pages, ino %lu; err %d", __func__,
				wbc->nr_to_write, inode->i_ino, ret);
			 
			ext4_put_io_end(mpd->io_submit.io_end);
			mpd->io_submit.io_end = NULL;
			break;
		}
		mpd->do_map = 1;

		trace_ext4_da_write_pages(inode, mpd->first_page, wbc);
		ret = mpage_prepare_extent_to_map(mpd);
		if (!ret && mpd->map.m_len)
			ret = mpage_map_and_submit_extent(handle, mpd,
					&give_up_on_write);
		 
		if (!ext4_handle_valid(handle) || handle->h_sync == 0) {
			ext4_journal_stop(handle);
			handle = NULL;
			mpd->do_map = 0;
		}
		 
		mpage_release_unused_pages(mpd, give_up_on_write);
		 
		ext4_io_submit(&mpd->io_submit);

		 
		if (handle) {
			ext4_put_io_end_defer(mpd->io_submit.io_end);
			ext4_journal_stop(handle);
		} else
			ext4_put_io_end(mpd->io_submit.io_end);
		mpd->io_submit.io_end = NULL;

		if (ret == -ENOSPC && sbi->s_journal) {
			 
			jbd2_journal_force_commit_nested(sbi->s_journal);
			ret = 0;
			continue;
		}
		 
		if (ret)
			break;
	}
unplug:
	blk_finish_plug(&plug);
	if (!ret && !cycled && wbc->nr_to_write > 0) {
		cycled = 1;
		mpd->last_page = writeback_index - 1;
		mpd->first_page = 0;
		goto retry;
	}

	 
	if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0))
		 
		mapping->writeback_index = mpd->first_page;

out_writepages:
	trace_ext4_writepages_result(inode, wbc, ret,
				     nr_to_write - wbc->nr_to_write);
	return ret;
}

static int ext4_writepages(struct address_space *mapping,
			   struct writeback_control *wbc)
{
	struct super_block *sb = mapping->host->i_sb;
	struct mpage_da_data mpd = {
		.inode = mapping->host,
		.wbc = wbc,
		.can_map = 1,
	};
	int ret;
	int alloc_ctx;

	if (unlikely(ext4_forced_shutdown(sb)))
		return -EIO;

	alloc_ctx = ext4_writepages_down_read(sb);
	ret = ext4_do_writepages(&mpd);
	 
	if (!ret && mpd.journalled_more_data)
		ret = ext4_do_writepages(&mpd);
	ext4_writepages_up_read(sb, alloc_ctx);

	return ret;
}

int ext4_normal_submit_inode_data_buffers(struct jbd2_inode *jinode)
{
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_ALL,
		.nr_to_write = LONG_MAX,
		.range_start = jinode->i_dirty_start,
		.range_end = jinode->i_dirty_end,
	};
	struct mpage_da_data mpd = {
		.inode = jinode->i_vfs_inode,
		.wbc = &wbc,
		.can_map = 0,
	};
	return ext4_do_writepages(&mpd);
}

static int ext4_dax_writepages(struct address_space *mapping,
			       struct writeback_control *wbc)
{
	int ret;
	long nr_to_write = wbc->nr_to_write;
	struct inode *inode = mapping->host;
	int alloc_ctx;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	alloc_ctx = ext4_writepages_down_read(inode->i_sb);
	trace_ext4_writepages(inode, wbc);

	ret = dax_writeback_mapping_range(mapping,
					  EXT4_SB(inode->i_sb)->s_daxdev, wbc);
	trace_ext4_writepages_result(inode, wbc, ret,
				     nr_to_write - wbc->nr_to_write);
	ext4_writepages_up_read(inode->i_sb, alloc_ctx);
	return ret;
}

static int ext4_nonda_switch(struct super_block *sb)
{
	s64 free_clusters, dirty_clusters;
	struct ext4_sb_info *sbi = EXT4_SB(sb);

	 
	free_clusters =
		percpu_counter_read_positive(&sbi->s_freeclusters_counter);
	dirty_clusters =
		percpu_counter_read_positive(&sbi->s_dirtyclusters_counter);
	 
	if (dirty_clusters && (free_clusters < 2 * dirty_clusters))
		try_to_writeback_inodes_sb(sb, WB_REASON_FS_FREE_SPACE);

	if (2 * free_clusters < 3 * dirty_clusters ||
	    free_clusters < (dirty_clusters + EXT4_FREECLUSTERS_WATERMARK)) {
		 
		return 1;
	}
	return 0;
}

static int ext4_da_write_begin(struct file *file, struct address_space *mapping,
			       loff_t pos, unsigned len,
			       struct page **pagep, void **fsdata)
{
	int ret, retries = 0;
	struct folio *folio;
	pgoff_t index;
	struct inode *inode = mapping->host;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	index = pos >> PAGE_SHIFT;

	if (ext4_nonda_switch(inode->i_sb) || ext4_verity_in_progress(inode)) {
		*fsdata = (void *)FALL_BACK_TO_NONDELALLOC;
		return ext4_write_begin(file, mapping, pos,
					len, pagep, fsdata);
	}
	*fsdata = (void *)0;
	trace_ext4_da_write_begin(inode, pos, len);

	if (ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA)) {
		ret = ext4_da_write_inline_data_begin(mapping, inode, pos, len,
						      pagep, fsdata);
		if (ret < 0)
			return ret;
		if (ret == 1)
			return 0;
	}

retry:
	folio = __filemap_get_folio(mapping, index, FGP_WRITEBEGIN,
			mapping_gfp_mask(mapping));
	if (IS_ERR(folio))
		return PTR_ERR(folio);

	 
	folio_wait_stable(folio);

#ifdef CONFIG_FS_ENCRYPTION
	ret = ext4_block_write_begin(folio, pos, len, ext4_da_get_block_prep);
#else
	ret = __block_write_begin(&folio->page, pos, len, ext4_da_get_block_prep);
#endif
	if (ret < 0) {
		folio_unlock(folio);
		folio_put(folio);
		 
		if (pos + len > inode->i_size)
			ext4_truncate_failed_write(inode);

		if (ret == -ENOSPC &&
		    ext4_should_retry_alloc(inode->i_sb, &retries))
			goto retry;
		return ret;
	}

	*pagep = &folio->page;
	return ret;
}

 
static int ext4_da_should_update_i_disksize(struct folio *folio,
					    unsigned long offset)
{
	struct buffer_head *bh;
	struct inode *inode = folio->mapping->host;
	unsigned int idx;
	int i;

	bh = folio_buffers(folio);
	idx = offset >> inode->i_blkbits;

	for (i = 0; i < idx; i++)
		bh = bh->b_this_page;

	if (!buffer_mapped(bh) || (buffer_delay(bh)) || buffer_unwritten(bh))
		return 0;
	return 1;
}

static int ext4_da_do_write_end(struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page)
{
	struct inode *inode = mapping->host;
	loff_t old_size = inode->i_size;
	bool disksize_changed = false;
	loff_t new_i_size;

	 
	copied = block_write_end(NULL, mapping, pos, len, copied, page, NULL);
	new_i_size = pos + copied;

	 
	if (new_i_size > inode->i_size) {
		unsigned long end;

		i_size_write(inode, new_i_size);
		end = (new_i_size - 1) & (PAGE_SIZE - 1);
		if (copied && ext4_da_should_update_i_disksize(page_folio(page), end)) {
			ext4_update_i_disksize(inode, new_i_size);
			disksize_changed = true;
		}
	}

	unlock_page(page);
	put_page(page);

	if (old_size < pos)
		pagecache_isize_extended(inode, old_size, pos);

	if (disksize_changed) {
		handle_t *handle;

		handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		ext4_mark_inode_dirty(handle, inode);
		ext4_journal_stop(handle);
	}

	return copied;
}

static int ext4_da_write_end(struct file *file,
			     struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned copied,
			     struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	int write_mode = (int)(unsigned long)fsdata;
	struct folio *folio = page_folio(page);

	if (write_mode == FALL_BACK_TO_NONDELALLOC)
		return ext4_write_end(file, mapping, pos,
				      len, copied, &folio->page, fsdata);

	trace_ext4_da_write_end(inode, pos, len, copied);

	if (write_mode != CONVERT_INLINE_DATA &&
	    ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA) &&
	    ext4_has_inline_data(inode))
		return ext4_write_inline_data_end(inode, pos, len, copied,
						  folio);

	if (unlikely(copied < len) && !PageUptodate(page))
		copied = 0;

	return ext4_da_do_write_end(mapping, pos, len, copied, &folio->page);
}

 
int ext4_alloc_da_blocks(struct inode *inode)
{
	trace_ext4_alloc_da_blocks(inode);

	if (!EXT4_I(inode)->i_reserved_data_blocks)
		return 0;

	 
	return filemap_flush(inode->i_mapping);
}

 
static sector_t ext4_bmap(struct address_space *mapping, sector_t block)
{
	struct inode *inode = mapping->host;
	sector_t ret = 0;

	inode_lock_shared(inode);
	 
	if (ext4_has_inline_data(inode))
		goto out;

	if (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY) &&
	    (test_opt(inode->i_sb, DELALLOC) ||
	     ext4_should_journal_data(inode))) {
		 
		filemap_write_and_wait(mapping);
	}

	ret = iomap_bmap(mapping, block, &ext4_iomap_ops);

out:
	inode_unlock_shared(inode);
	return ret;
}

static int ext4_read_folio(struct file *file, struct folio *folio)
{
	int ret = -EAGAIN;
	struct inode *inode = folio->mapping->host;

	trace_ext4_read_folio(inode, folio);

	if (ext4_has_inline_data(inode))
		ret = ext4_readpage_inline(inode, folio);

	if (ret == -EAGAIN)
		return ext4_mpage_readpages(inode, NULL, folio);

	return ret;
}

static void ext4_readahead(struct readahead_control *rac)
{
	struct inode *inode = rac->mapping->host;

	 
	if (ext4_has_inline_data(inode))
		return;

	ext4_mpage_readpages(inode, rac, NULL);
}

static void ext4_invalidate_folio(struct folio *folio, size_t offset,
				size_t length)
{
	trace_ext4_invalidate_folio(folio, offset, length);

	 
	WARN_ON(folio_buffers(folio) && buffer_jbd(folio_buffers(folio)));

	block_invalidate_folio(folio, offset, length);
}

static int __ext4_journalled_invalidate_folio(struct folio *folio,
					    size_t offset, size_t length)
{
	journal_t *journal = EXT4_JOURNAL(folio->mapping->host);

	trace_ext4_journalled_invalidate_folio(folio, offset, length);

	 
	if (offset == 0 && length == folio_size(folio))
		folio_clear_checked(folio);

	return jbd2_journal_invalidate_folio(journal, folio, offset, length);
}

 
static void ext4_journalled_invalidate_folio(struct folio *folio,
					   size_t offset,
					   size_t length)
{
	WARN_ON(__ext4_journalled_invalidate_folio(folio, offset, length) < 0);
}

static bool ext4_release_folio(struct folio *folio, gfp_t wait)
{
	struct inode *inode = folio->mapping->host;
	journal_t *journal = EXT4_JOURNAL(inode);

	trace_ext4_release_folio(inode, folio);

	 
	if (folio_test_checked(folio))
		return false;
	if (journal)
		return jbd2_journal_try_to_free_buffers(journal, folio);
	else
		return try_to_free_buffers(folio);
}

static bool ext4_inode_datasync_dirty(struct inode *inode)
{
	journal_t *journal = EXT4_SB(inode->i_sb)->s_journal;

	if (journal) {
		if (jbd2_transaction_committed(journal,
			EXT4_I(inode)->i_datasync_tid))
			return false;
		if (test_opt2(inode->i_sb, JOURNAL_FAST_COMMIT))
			return !list_empty(&EXT4_I(inode)->i_fc_list);
		return true;
	}

	 
	if (!list_empty(&inode->i_mapping->private_list))
		return true;
	return inode->i_state & I_DIRTY_DATASYNC;
}

static void ext4_set_iomap(struct inode *inode, struct iomap *iomap,
			   struct ext4_map_blocks *map, loff_t offset,
			   loff_t length, unsigned int flags)
{
	u8 blkbits = inode->i_blkbits;

	 
	iomap->flags = 0;
	if (ext4_inode_datasync_dirty(inode) ||
	    offset + length > i_size_read(inode))
		iomap->flags |= IOMAP_F_DIRTY;

	if (map->m_flags & EXT4_MAP_NEW)
		iomap->flags |= IOMAP_F_NEW;

	if (flags & IOMAP_DAX)
		iomap->dax_dev = EXT4_SB(inode->i_sb)->s_daxdev;
	else
		iomap->bdev = inode->i_sb->s_bdev;
	iomap->offset = (u64) map->m_lblk << blkbits;
	iomap->length = (u64) map->m_len << blkbits;

	if ((map->m_flags & EXT4_MAP_MAPPED) &&
	    !ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		iomap->flags |= IOMAP_F_MERGED;

	 
	if (map->m_flags & EXT4_MAP_UNWRITTEN) {
		iomap->type = IOMAP_UNWRITTEN;
		iomap->addr = (u64) map->m_pblk << blkbits;
		if (flags & IOMAP_DAX)
			iomap->addr += EXT4_SB(inode->i_sb)->s_dax_part_off;
	} else if (map->m_flags & EXT4_MAP_MAPPED) {
		iomap->type = IOMAP_MAPPED;
		iomap->addr = (u64) map->m_pblk << blkbits;
		if (flags & IOMAP_DAX)
			iomap->addr += EXT4_SB(inode->i_sb)->s_dax_part_off;
	} else {
		iomap->type = IOMAP_HOLE;
		iomap->addr = IOMAP_NULL_ADDR;
	}
}

static int ext4_iomap_alloc(struct inode *inode, struct ext4_map_blocks *map,
			    unsigned int flags)
{
	handle_t *handle;
	u8 blkbits = inode->i_blkbits;
	int ret, dio_credits, m_flags = 0, retries = 0;

	 
	if (map->m_len > DIO_MAX_BLOCKS)
		map->m_len = DIO_MAX_BLOCKS;
	dio_credits = ext4_chunk_trans_blocks(inode, map->m_len);

retry:
	 
	handle = ext4_journal_start(inode, EXT4_HT_MAP_BLOCKS, dio_credits);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	 
	WARN_ON(!(flags & (IOMAP_DAX | IOMAP_DIRECT)));
	if (flags & IOMAP_DAX)
		m_flags = EXT4_GET_BLOCKS_CREATE_ZERO;
	 
	else if (((loff_t)map->m_lblk << blkbits) >= i_size_read(inode))
		m_flags = EXT4_GET_BLOCKS_CREATE;
	else if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		m_flags = EXT4_GET_BLOCKS_IO_CREATE_EXT;

	ret = ext4_map_blocks(handle, inode, map, m_flags);

	 
	if (!m_flags && !ret)
		ret = -ENOTBLK;

	ext4_journal_stop(handle);
	if (ret == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;

	return ret;
}


static int ext4_iomap_begin(struct inode *inode, loff_t offset, loff_t length,
		unsigned flags, struct iomap *iomap, struct iomap *srcmap)
{
	int ret;
	struct ext4_map_blocks map;
	u8 blkbits = inode->i_blkbits;

	if ((offset >> blkbits) > EXT4_MAX_LOGICAL_BLOCK)
		return -EINVAL;

	if (WARN_ON_ONCE(ext4_has_inline_data(inode)))
		return -ERANGE;

	 
	map.m_lblk = offset >> blkbits;
	map.m_len = min_t(loff_t, (offset + length - 1) >> blkbits,
			  EXT4_MAX_LOGICAL_BLOCK) - map.m_lblk + 1;

	if (flags & IOMAP_WRITE) {
		 
		if (offset + length <= i_size_read(inode)) {
			ret = ext4_map_blocks(NULL, inode, &map, 0);
			if (ret > 0 && (map.m_flags & EXT4_MAP_MAPPED))
				goto out;
		}
		ret = ext4_iomap_alloc(inode, &map, flags);
	} else {
		ret = ext4_map_blocks(NULL, inode, &map, 0);
	}

	if (ret < 0)
		return ret;
out:
	 
	map.m_len = fscrypt_limit_io_blocks(inode, map.m_lblk, map.m_len);

	ext4_set_iomap(inode, iomap, &map, offset, length, flags);

	return 0;
}

static int ext4_iomap_overwrite_begin(struct inode *inode, loff_t offset,
		loff_t length, unsigned flags, struct iomap *iomap,
		struct iomap *srcmap)
{
	int ret;

	 
	flags &= ~IOMAP_WRITE;
	ret = ext4_iomap_begin(inode, offset, length, flags, iomap, srcmap);
	WARN_ON_ONCE(!ret && iomap->type != IOMAP_MAPPED);
	return ret;
}

static int ext4_iomap_end(struct inode *inode, loff_t offset, loff_t length,
			  ssize_t written, unsigned flags, struct iomap *iomap)
{
	 
	if (flags & (IOMAP_WRITE | IOMAP_DIRECT) && written == 0)
		return -ENOTBLK;

	return 0;
}

const struct iomap_ops ext4_iomap_ops = {
	.iomap_begin		= ext4_iomap_begin,
	.iomap_end		= ext4_iomap_end,
};

const struct iomap_ops ext4_iomap_overwrite_ops = {
	.iomap_begin		= ext4_iomap_overwrite_begin,
	.iomap_end		= ext4_iomap_end,
};

static bool ext4_iomap_is_delalloc(struct inode *inode,
				   struct ext4_map_blocks *map)
{
	struct extent_status es;
	ext4_lblk_t offset = 0, end = map->m_lblk + map->m_len - 1;

	ext4_es_find_extent_range(inode, &ext4_es_is_delayed,
				  map->m_lblk, end, &es);

	if (!es.es_len || es.es_lblk > end)
		return false;

	if (es.es_lblk > map->m_lblk) {
		map->m_len = es.es_lblk - map->m_lblk;
		return false;
	}

	offset = map->m_lblk - es.es_lblk;
	map->m_len = es.es_len - offset;

	return true;
}

static int ext4_iomap_begin_report(struct inode *inode, loff_t offset,
				   loff_t length, unsigned int flags,
				   struct iomap *iomap, struct iomap *srcmap)
{
	int ret;
	bool delalloc = false;
	struct ext4_map_blocks map;
	u8 blkbits = inode->i_blkbits;

	if ((offset >> blkbits) > EXT4_MAX_LOGICAL_BLOCK)
		return -EINVAL;

	if (ext4_has_inline_data(inode)) {
		ret = ext4_inline_data_iomap(inode, iomap);
		if (ret != -EAGAIN) {
			if (ret == 0 && offset >= iomap->length)
				ret = -ENOENT;
			return ret;
		}
	}

	 
	map.m_lblk = offset >> blkbits;
	map.m_len = min_t(loff_t, (offset + length - 1) >> blkbits,
			  EXT4_MAX_LOGICAL_BLOCK) - map.m_lblk + 1;

	 
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

		if (offset >= sbi->s_bitmap_maxbytes) {
			map.m_flags = 0;
			goto set_iomap;
		}
	}

	ret = ext4_map_blocks(NULL, inode, &map, 0);
	if (ret < 0)
		return ret;
	if (ret == 0)
		delalloc = ext4_iomap_is_delalloc(inode, &map);

set_iomap:
	ext4_set_iomap(inode, iomap, &map, offset, length, flags);
	if (delalloc && iomap->type == IOMAP_HOLE)
		iomap->type = IOMAP_DELALLOC;

	return 0;
}

const struct iomap_ops ext4_iomap_report_ops = {
	.iomap_begin = ext4_iomap_begin_report,
};

 
static bool ext4_journalled_dirty_folio(struct address_space *mapping,
		struct folio *folio)
{
	WARN_ON_ONCE(!folio_buffers(folio));
	if (folio_maybe_dma_pinned(folio))
		folio_set_checked(folio);
	return filemap_dirty_folio(mapping, folio);
}

static bool ext4_dirty_folio(struct address_space *mapping, struct folio *folio)
{
	WARN_ON_ONCE(!folio_test_locked(folio) && !folio_test_dirty(folio));
	WARN_ON_ONCE(!folio_buffers(folio));
	return block_dirty_folio(mapping, folio);
}

static int ext4_iomap_swap_activate(struct swap_info_struct *sis,
				    struct file *file, sector_t *span)
{
	return iomap_swapfile_activate(sis, file, span,
				       &ext4_iomap_report_ops);
}

static const struct address_space_operations ext4_aops = {
	.read_folio		= ext4_read_folio,
	.readahead		= ext4_readahead,
	.writepages		= ext4_writepages,
	.write_begin		= ext4_write_begin,
	.write_end		= ext4_write_end,
	.dirty_folio		= ext4_dirty_folio,
	.bmap			= ext4_bmap,
	.invalidate_folio	= ext4_invalidate_folio,
	.release_folio		= ext4_release_folio,
	.direct_IO		= noop_direct_IO,
	.migrate_folio		= buffer_migrate_folio,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= ext4_iomap_swap_activate,
};

static const struct address_space_operations ext4_journalled_aops = {
	.read_folio		= ext4_read_folio,
	.readahead		= ext4_readahead,
	.writepages		= ext4_writepages,
	.write_begin		= ext4_write_begin,
	.write_end		= ext4_journalled_write_end,
	.dirty_folio		= ext4_journalled_dirty_folio,
	.bmap			= ext4_bmap,
	.invalidate_folio	= ext4_journalled_invalidate_folio,
	.release_folio		= ext4_release_folio,
	.direct_IO		= noop_direct_IO,
	.migrate_folio		= buffer_migrate_folio_norefs,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= ext4_iomap_swap_activate,
};

static const struct address_space_operations ext4_da_aops = {
	.read_folio		= ext4_read_folio,
	.readahead		= ext4_readahead,
	.writepages		= ext4_writepages,
	.write_begin		= ext4_da_write_begin,
	.write_end		= ext4_da_write_end,
	.dirty_folio		= ext4_dirty_folio,
	.bmap			= ext4_bmap,
	.invalidate_folio	= ext4_invalidate_folio,
	.release_folio		= ext4_release_folio,
	.direct_IO		= noop_direct_IO,
	.migrate_folio		= buffer_migrate_folio,
	.is_partially_uptodate  = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
	.swap_activate		= ext4_iomap_swap_activate,
};

static const struct address_space_operations ext4_dax_aops = {
	.writepages		= ext4_dax_writepages,
	.direct_IO		= noop_direct_IO,
	.dirty_folio		= noop_dirty_folio,
	.bmap			= ext4_bmap,
	.swap_activate		= ext4_iomap_swap_activate,
};

void ext4_set_aops(struct inode *inode)
{
	switch (ext4_inode_journal_mode(inode)) {
	case EXT4_INODE_ORDERED_DATA_MODE:
	case EXT4_INODE_WRITEBACK_DATA_MODE:
		break;
	case EXT4_INODE_JOURNAL_DATA_MODE:
		inode->i_mapping->a_ops = &ext4_journalled_aops;
		return;
	default:
		BUG();
	}
	if (IS_DAX(inode))
		inode->i_mapping->a_ops = &ext4_dax_aops;
	else if (test_opt(inode->i_sb, DELALLOC))
		inode->i_mapping->a_ops = &ext4_da_aops;
	else
		inode->i_mapping->a_ops = &ext4_aops;
}

static int __ext4_block_zero_page_range(handle_t *handle,
		struct address_space *mapping, loff_t from, loff_t length)
{
	ext4_fsblk_t index = from >> PAGE_SHIFT;
	unsigned offset = from & (PAGE_SIZE-1);
	unsigned blocksize, pos;
	ext4_lblk_t iblock;
	struct inode *inode = mapping->host;
	struct buffer_head *bh;
	struct folio *folio;
	int err = 0;

	folio = __filemap_get_folio(mapping, from >> PAGE_SHIFT,
				    FGP_LOCK | FGP_ACCESSED | FGP_CREAT,
				    mapping_gfp_constraint(mapping, ~__GFP_FS));
	if (IS_ERR(folio))
		return PTR_ERR(folio);

	blocksize = inode->i_sb->s_blocksize;

	iblock = index << (PAGE_SHIFT - inode->i_sb->s_blocksize_bits);

	bh = folio_buffers(folio);
	if (!bh) {
		create_empty_buffers(&folio->page, blocksize, 0);
		bh = folio_buffers(folio);
	}

	 
	pos = blocksize;
	while (offset >= pos) {
		bh = bh->b_this_page;
		iblock++;
		pos += blocksize;
	}
	if (buffer_freed(bh)) {
		BUFFER_TRACE(bh, "freed: skip");
		goto unlock;
	}
	if (!buffer_mapped(bh)) {
		BUFFER_TRACE(bh, "unmapped");
		ext4_get_block(inode, iblock, bh, 0);
		 
		if (!buffer_mapped(bh)) {
			BUFFER_TRACE(bh, "still unmapped");
			goto unlock;
		}
	}

	 
	if (folio_test_uptodate(folio))
		set_buffer_uptodate(bh);

	if (!buffer_uptodate(bh)) {
		err = ext4_read_bh_lock(bh, 0, true);
		if (err)
			goto unlock;
		if (fscrypt_inode_uses_fs_layer_crypto(inode)) {
			 
			BUG_ON(!fscrypt_has_encryption_key(inode));
			err = fscrypt_decrypt_pagecache_blocks(folio,
							       blocksize,
							       bh_offset(bh));
			if (err) {
				clear_buffer_uptodate(bh);
				goto unlock;
			}
		}
	}
	if (ext4_should_journal_data(inode)) {
		BUFFER_TRACE(bh, "get write access");
		err = ext4_journal_get_write_access(handle, inode->i_sb, bh,
						    EXT4_JTR_NONE);
		if (err)
			goto unlock;
	}
	folio_zero_range(folio, offset, length);
	BUFFER_TRACE(bh, "zeroed end of block");

	if (ext4_should_journal_data(inode)) {
		err = ext4_dirty_journalled_data(handle, bh);
	} else {
		err = 0;
		mark_buffer_dirty(bh);
		if (ext4_should_order_data(inode))
			err = ext4_jbd2_inode_add_write(handle, inode, from,
					length);
	}

unlock:
	folio_unlock(folio);
	folio_put(folio);
	return err;
}

 
static int ext4_block_zero_page_range(handle_t *handle,
		struct address_space *mapping, loff_t from, loff_t length)
{
	struct inode *inode = mapping->host;
	unsigned offset = from & (PAGE_SIZE-1);
	unsigned blocksize = inode->i_sb->s_blocksize;
	unsigned max = blocksize - (offset & (blocksize - 1));

	 
	if (length > max || length < 0)
		length = max;

	if (IS_DAX(inode)) {
		return dax_zero_range(inode, from, length, NULL,
				      &ext4_iomap_ops);
	}
	return __ext4_block_zero_page_range(handle, mapping, from, length);
}

 
static int ext4_block_truncate_page(handle_t *handle,
		struct address_space *mapping, loff_t from)
{
	unsigned offset = from & (PAGE_SIZE-1);
	unsigned length;
	unsigned blocksize;
	struct inode *inode = mapping->host;

	 
	if (IS_ENCRYPTED(inode) && !fscrypt_has_encryption_key(inode))
		return 0;

	blocksize = inode->i_sb->s_blocksize;
	length = blocksize - (offset & (blocksize - 1));

	return ext4_block_zero_page_range(handle, mapping, from, length);
}

int ext4_zero_partial_blocks(handle_t *handle, struct inode *inode,
			     loff_t lstart, loff_t length)
{
	struct super_block *sb = inode->i_sb;
	struct address_space *mapping = inode->i_mapping;
	unsigned partial_start, partial_end;
	ext4_fsblk_t start, end;
	loff_t byte_end = (lstart + length - 1);
	int err = 0;

	partial_start = lstart & (sb->s_blocksize - 1);
	partial_end = byte_end & (sb->s_blocksize - 1);

	start = lstart >> sb->s_blocksize_bits;
	end = byte_end >> sb->s_blocksize_bits;

	 
	if (start == end &&
	    (partial_start || (partial_end != sb->s_blocksize - 1))) {
		err = ext4_block_zero_page_range(handle, mapping,
						 lstart, length);
		return err;
	}
	 
	if (partial_start) {
		err = ext4_block_zero_page_range(handle, mapping,
						 lstart, sb->s_blocksize);
		if (err)
			return err;
	}
	 
	if (partial_end != sb->s_blocksize - 1)
		err = ext4_block_zero_page_range(handle, mapping,
						 byte_end - partial_end,
						 partial_end + 1);
	return err;
}

int ext4_can_truncate(struct inode *inode)
{
	if (S_ISREG(inode->i_mode))
		return 1;
	if (S_ISDIR(inode->i_mode))
		return 1;
	if (S_ISLNK(inode->i_mode))
		return !ext4_inode_is_fast_symlink(inode);
	return 0;
}

 
int ext4_update_disksize_before_punch(struct inode *inode, loff_t offset,
				      loff_t len)
{
	handle_t *handle;
	int ret;

	loff_t size = i_size_read(inode);

	WARN_ON(!inode_is_locked(inode));
	if (offset > size || offset + len < size)
		return 0;

	if (EXT4_I(inode)->i_disksize >= size)
		return 0;

	handle = ext4_journal_start(inode, EXT4_HT_MISC, 1);
	if (IS_ERR(handle))
		return PTR_ERR(handle);
	ext4_update_i_disksize(inode, size);
	ret = ext4_mark_inode_dirty(handle, inode);
	ext4_journal_stop(handle);

	return ret;
}

static void ext4_wait_dax_page(struct inode *inode)
{
	filemap_invalidate_unlock(inode->i_mapping);
	schedule();
	filemap_invalidate_lock(inode->i_mapping);
}

int ext4_break_layouts(struct inode *inode)
{
	struct page *page;
	int error;

	if (WARN_ON_ONCE(!rwsem_is_locked(&inode->i_mapping->invalidate_lock)))
		return -EINVAL;

	do {
		page = dax_layout_busy_page(inode->i_mapping);
		if (!page)
			return 0;

		error = ___wait_var_event(&page->_refcount,
				atomic_read(&page->_refcount) == 1,
				TASK_INTERRUPTIBLE, 0, 0,
				ext4_wait_dax_page(inode));
	} while (error == 0);

	return error;
}

 

int ext4_punch_hole(struct file *file, loff_t offset, loff_t length)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	ext4_lblk_t first_block, stop_block;
	struct address_space *mapping = inode->i_mapping;
	loff_t first_block_offset, last_block_offset, max_length;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	handle_t *handle;
	unsigned int credits;
	int ret = 0, ret2 = 0;

	trace_ext4_punch_hole(inode, offset, length, 0);

	 
	if (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY)) {
		ret = filemap_write_and_wait_range(mapping, offset,
						   offset + length - 1);
		if (ret)
			return ret;
	}

	inode_lock(inode);

	 
	if (offset >= inode->i_size)
		goto out_mutex;

	 
	if (offset + length > inode->i_size) {
		length = inode->i_size +
		   PAGE_SIZE - (inode->i_size & (PAGE_SIZE - 1)) -
		   offset;
	}

	 
	max_length = sbi->s_bitmap_maxbytes - inode->i_sb->s_blocksize;
	if (offset + length > max_length)
		length = max_length - offset;

	if (offset & (sb->s_blocksize - 1) ||
	    (offset + length) & (sb->s_blocksize - 1)) {
		 
		ret = ext4_inode_attach_jinode(inode);
		if (ret < 0)
			goto out_mutex;

	}

	 
	inode_dio_wait(inode);

	ret = file_modified(file);
	if (ret)
		goto out_mutex;

	 
	filemap_invalidate_lock(mapping);

	ret = ext4_break_layouts(inode);
	if (ret)
		goto out_dio;

	first_block_offset = round_up(offset, sb->s_blocksize);
	last_block_offset = round_down((offset + length), sb->s_blocksize) - 1;

	 
	if (last_block_offset > first_block_offset) {
		ret = ext4_update_disksize_before_punch(inode, offset, length);
		if (ret)
			goto out_dio;
		truncate_pagecache_range(inode, first_block_offset,
					 last_block_offset);
	}

	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		credits = ext4_writepage_trans_blocks(inode);
	else
		credits = ext4_blocks_for_truncate(inode);
	handle = ext4_journal_start(inode, EXT4_HT_TRUNCATE, credits);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		ext4_std_error(sb, ret);
		goto out_dio;
	}

	ret = ext4_zero_partial_blocks(handle, inode, offset,
				       length);
	if (ret)
		goto out_stop;

	first_block = (offset + sb->s_blocksize - 1) >>
		EXT4_BLOCK_SIZE_BITS(sb);
	stop_block = (offset + length) >> EXT4_BLOCK_SIZE_BITS(sb);

	 
	if (stop_block > first_block) {

		down_write(&EXT4_I(inode)->i_data_sem);
		ext4_discard_preallocations(inode, 0);

		ext4_es_remove_extent(inode, first_block,
				      stop_block - first_block);

		if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
			ret = ext4_ext_remove_space(inode, first_block,
						    stop_block - 1);
		else
			ret = ext4_ind_remove_space(handle, inode, first_block,
						    stop_block);

		up_write(&EXT4_I(inode)->i_data_sem);
	}
	ext4_fc_track_range(handle, inode, first_block, stop_block);
	if (IS_SYNC(inode))
		ext4_handle_sync(handle);

	inode->i_mtime = inode_set_ctime_current(inode);
	ret2 = ext4_mark_inode_dirty(handle, inode);
	if (unlikely(ret2))
		ret = ret2;
	if (ret >= 0)
		ext4_update_inode_fsync_trans(handle, inode, 1);
out_stop:
	ext4_journal_stop(handle);
out_dio:
	filemap_invalidate_unlock(mapping);
out_mutex:
	inode_unlock(inode);
	return ret;
}

int ext4_inode_attach_jinode(struct inode *inode)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	struct jbd2_inode *jinode;

	if (ei->jinode || !EXT4_SB(inode->i_sb)->s_journal)
		return 0;

	jinode = jbd2_alloc_inode(GFP_KERNEL);
	spin_lock(&inode->i_lock);
	if (!ei->jinode) {
		if (!jinode) {
			spin_unlock(&inode->i_lock);
			return -ENOMEM;
		}
		ei->jinode = jinode;
		jbd2_journal_init_jbd_inode(ei->jinode, inode);
		jinode = NULL;
	}
	spin_unlock(&inode->i_lock);
	if (unlikely(jinode != NULL))
		jbd2_free_inode(jinode);
	return 0;
}

 
int ext4_truncate(struct inode *inode)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	unsigned int credits;
	int err = 0, err2;
	handle_t *handle;
	struct address_space *mapping = inode->i_mapping;

	 
	if (!(inode->i_state & (I_NEW|I_FREEING)))
		WARN_ON(!inode_is_locked(inode));
	trace_ext4_truncate_enter(inode);

	if (!ext4_can_truncate(inode))
		goto out_trace;

	if (inode->i_size == 0 && !test_opt(inode->i_sb, NO_AUTO_DA_ALLOC))
		ext4_set_inode_state(inode, EXT4_STATE_DA_ALLOC_CLOSE);

	if (ext4_has_inline_data(inode)) {
		int has_inline = 1;

		err = ext4_inline_data_truncate(inode, &has_inline);
		if (err || has_inline)
			goto out_trace;
	}

	 
	if (inode->i_size & (inode->i_sb->s_blocksize - 1)) {
		err = ext4_inode_attach_jinode(inode);
		if (err)
			goto out_trace;
	}

	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		credits = ext4_writepage_trans_blocks(inode);
	else
		credits = ext4_blocks_for_truncate(inode);

	handle = ext4_journal_start(inode, EXT4_HT_TRUNCATE, credits);
	if (IS_ERR(handle)) {
		err = PTR_ERR(handle);
		goto out_trace;
	}

	if (inode->i_size & (inode->i_sb->s_blocksize - 1))
		ext4_block_truncate_page(handle, mapping, inode->i_size);

	 
	err = ext4_orphan_add(handle, inode);
	if (err)
		goto out_stop;

	down_write(&EXT4_I(inode)->i_data_sem);

	ext4_discard_preallocations(inode, 0);

	if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
		err = ext4_ext_truncate(handle, inode);
	else
		ext4_ind_truncate(handle, inode);

	up_write(&ei->i_data_sem);
	if (err)
		goto out_stop;

	if (IS_SYNC(inode))
		ext4_handle_sync(handle);

out_stop:
	 
	if (inode->i_nlink)
		ext4_orphan_del(handle, inode);

	inode->i_mtime = inode_set_ctime_current(inode);
	err2 = ext4_mark_inode_dirty(handle, inode);
	if (unlikely(err2 && !err))
		err = err2;
	ext4_journal_stop(handle);

out_trace:
	trace_ext4_truncate_exit(inode);
	return err;
}

static inline u64 ext4_inode_peek_iversion(const struct inode *inode)
{
	if (unlikely(EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL))
		return inode_peek_iversion_raw(inode);
	else
		return inode_peek_iversion(inode);
}

static int ext4_inode_blocks_set(struct ext4_inode *raw_inode,
				 struct ext4_inode_info *ei)
{
	struct inode *inode = &(ei->vfs_inode);
	u64 i_blocks = READ_ONCE(inode->i_blocks);
	struct super_block *sb = inode->i_sb;

	if (i_blocks <= ~0U) {
		 
		raw_inode->i_blocks_lo   = cpu_to_le32(i_blocks);
		raw_inode->i_blocks_high = 0;
		ext4_clear_inode_flag(inode, EXT4_INODE_HUGE_FILE);
		return 0;
	}

	 
	if (!ext4_has_feature_huge_file(sb))
		return -EFSCORRUPTED;

	if (i_blocks <= 0xffffffffffffULL) {
		 
		raw_inode->i_blocks_lo   = cpu_to_le32(i_blocks);
		raw_inode->i_blocks_high = cpu_to_le16(i_blocks >> 32);
		ext4_clear_inode_flag(inode, EXT4_INODE_HUGE_FILE);
	} else {
		ext4_set_inode_flag(inode, EXT4_INODE_HUGE_FILE);
		 
		i_blocks = i_blocks >> (inode->i_blkbits - 9);
		raw_inode->i_blocks_lo   = cpu_to_le32(i_blocks);
		raw_inode->i_blocks_high = cpu_to_le16(i_blocks >> 32);
	}
	return 0;
}

static int ext4_fill_raw_inode(struct inode *inode, struct ext4_inode *raw_inode)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	uid_t i_uid;
	gid_t i_gid;
	projid_t i_projid;
	int block;
	int err;

	err = ext4_inode_blocks_set(raw_inode, ei);

	raw_inode->i_mode = cpu_to_le16(inode->i_mode);
	i_uid = i_uid_read(inode);
	i_gid = i_gid_read(inode);
	i_projid = from_kprojid(&init_user_ns, ei->i_projid);
	if (!(test_opt(inode->i_sb, NO_UID32))) {
		raw_inode->i_uid_low = cpu_to_le16(low_16_bits(i_uid));
		raw_inode->i_gid_low = cpu_to_le16(low_16_bits(i_gid));
		 
		if (ei->i_dtime && list_empty(&ei->i_orphan)) {
			raw_inode->i_uid_high = 0;
			raw_inode->i_gid_high = 0;
		} else {
			raw_inode->i_uid_high =
				cpu_to_le16(high_16_bits(i_uid));
			raw_inode->i_gid_high =
				cpu_to_le16(high_16_bits(i_gid));
		}
	} else {
		raw_inode->i_uid_low = cpu_to_le16(fs_high2lowuid(i_uid));
		raw_inode->i_gid_low = cpu_to_le16(fs_high2lowgid(i_gid));
		raw_inode->i_uid_high = 0;
		raw_inode->i_gid_high = 0;
	}
	raw_inode->i_links_count = cpu_to_le16(inode->i_nlink);

	EXT4_INODE_SET_CTIME(inode, raw_inode);
	EXT4_INODE_SET_XTIME(i_mtime, inode, raw_inode);
	EXT4_INODE_SET_XTIME(i_atime, inode, raw_inode);
	EXT4_EINODE_SET_XTIME(i_crtime, ei, raw_inode);

	raw_inode->i_dtime = cpu_to_le32(ei->i_dtime);
	raw_inode->i_flags = cpu_to_le32(ei->i_flags & 0xFFFFFFFF);
	if (likely(!test_opt2(inode->i_sb, HURD_COMPAT)))
		raw_inode->i_file_acl_high =
			cpu_to_le16(ei->i_file_acl >> 32);
	raw_inode->i_file_acl_lo = cpu_to_le32(ei->i_file_acl);
	ext4_isize_set(raw_inode, ei->i_disksize);

	raw_inode->i_generation = cpu_to_le32(inode->i_generation);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode)) {
		if (old_valid_dev(inode->i_rdev)) {
			raw_inode->i_block[0] =
				cpu_to_le32(old_encode_dev(inode->i_rdev));
			raw_inode->i_block[1] = 0;
		} else {
			raw_inode->i_block[0] = 0;
			raw_inode->i_block[1] =
				cpu_to_le32(new_encode_dev(inode->i_rdev));
			raw_inode->i_block[2] = 0;
		}
	} else if (!ext4_has_inline_data(inode)) {
		for (block = 0; block < EXT4_N_BLOCKS; block++)
			raw_inode->i_block[block] = ei->i_data[block];
	}

	if (likely(!test_opt2(inode->i_sb, HURD_COMPAT))) {
		u64 ivers = ext4_inode_peek_iversion(inode);

		raw_inode->i_disk_version = cpu_to_le32(ivers);
		if (ei->i_extra_isize) {
			if (EXT4_FITS_IN_INODE(raw_inode, ei, i_version_hi))
				raw_inode->i_version_hi =
					cpu_to_le32(ivers >> 32);
			raw_inode->i_extra_isize =
				cpu_to_le16(ei->i_extra_isize);
		}
	}

	if (i_projid != EXT4_DEF_PROJID &&
	    !ext4_has_feature_project(inode->i_sb))
		err = err ?: -EFSCORRUPTED;

	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE &&
	    EXT4_FITS_IN_INODE(raw_inode, ei, i_projid))
		raw_inode->i_projid = cpu_to_le32(i_projid);

	ext4_inode_csum_set(inode, raw_inode, ei);
	return err;
}

 
static int __ext4_get_inode_loc(struct super_block *sb, unsigned long ino,
				struct inode *inode, struct ext4_iloc *iloc,
				ext4_fsblk_t *ret_block)
{
	struct ext4_group_desc	*gdp;
	struct buffer_head	*bh;
	ext4_fsblk_t		block;
	struct blk_plug		plug;
	int			inodes_per_block, inode_offset;

	iloc->bh = NULL;
	if (ino < EXT4_ROOT_INO ||
	    ino > le32_to_cpu(EXT4_SB(sb)->s_es->s_inodes_count))
		return -EFSCORRUPTED;

	iloc->block_group = (ino - 1) / EXT4_INODES_PER_GROUP(sb);
	gdp = ext4_get_group_desc(sb, iloc->block_group, NULL);
	if (!gdp)
		return -EIO;

	 
	inodes_per_block = EXT4_SB(sb)->s_inodes_per_block;
	inode_offset = ((ino - 1) %
			EXT4_INODES_PER_GROUP(sb));
	iloc->offset = (inode_offset % inodes_per_block) * EXT4_INODE_SIZE(sb);

	block = ext4_inode_table(sb, gdp);
	if ((block <= le32_to_cpu(EXT4_SB(sb)->s_es->s_first_data_block)) ||
	    (block >= ext4_blocks_count(EXT4_SB(sb)->s_es))) {
		ext4_error(sb, "Invalid inode table block %llu in "
			   "block_group %u", block, iloc->block_group);
		return -EFSCORRUPTED;
	}
	block += (inode_offset / inodes_per_block);

	bh = sb_getblk(sb, block);
	if (unlikely(!bh))
		return -ENOMEM;
	if (ext4_buffer_uptodate(bh))
		goto has_buffer;

	lock_buffer(bh);
	if (ext4_buffer_uptodate(bh)) {
		 
		unlock_buffer(bh);
		goto has_buffer;
	}

	 
	if (inode && !ext4_test_inode_state(inode, EXT4_STATE_XATTR)) {
		struct buffer_head *bitmap_bh;
		int i, start;

		start = inode_offset & ~(inodes_per_block - 1);

		 
		bitmap_bh = sb_getblk(sb, ext4_inode_bitmap(sb, gdp));
		if (unlikely(!bitmap_bh))
			goto make_io;

		 
		if (!buffer_uptodate(bitmap_bh)) {
			brelse(bitmap_bh);
			goto make_io;
		}
		for (i = start; i < start + inodes_per_block; i++) {
			if (i == inode_offset)
				continue;
			if (ext4_test_bit(i, bitmap_bh->b_data))
				break;
		}
		brelse(bitmap_bh);
		if (i == start + inodes_per_block) {
			struct ext4_inode *raw_inode =
				(struct ext4_inode *) (bh->b_data + iloc->offset);

			 
			memset(bh->b_data, 0, bh->b_size);
			if (!ext4_test_inode_state(inode, EXT4_STATE_NEW))
				ext4_fill_raw_inode(inode, raw_inode);
			set_buffer_uptodate(bh);
			unlock_buffer(bh);
			goto has_buffer;
		}
	}

make_io:
	 
	blk_start_plug(&plug);
	if (EXT4_SB(sb)->s_inode_readahead_blks) {
		ext4_fsblk_t b, end, table;
		unsigned num;
		__u32 ra_blks = EXT4_SB(sb)->s_inode_readahead_blks;

		table = ext4_inode_table(sb, gdp);
		 
		b = block & ~((ext4_fsblk_t) ra_blks - 1);
		if (table > b)
			b = table;
		end = b + ra_blks;
		num = EXT4_INODES_PER_GROUP(sb);
		if (ext4_has_group_desc_csum(sb))
			num -= ext4_itable_unused_count(sb, gdp);
		table += num / inodes_per_block;
		if (end > table)
			end = table;
		while (b <= end)
			ext4_sb_breadahead_unmovable(sb, b++);
	}

	 
	trace_ext4_load_inode(sb, ino);
	ext4_read_bh_nowait(bh, REQ_META | REQ_PRIO, NULL);
	blk_finish_plug(&plug);
	wait_on_buffer(bh);
	ext4_simulate_fail_bh(sb, bh, EXT4_SIM_INODE_EIO);
	if (!buffer_uptodate(bh)) {
		if (ret_block)
			*ret_block = block;
		brelse(bh);
		return -EIO;
	}
has_buffer:
	iloc->bh = bh;
	return 0;
}

static int __ext4_get_inode_loc_noinmem(struct inode *inode,
					struct ext4_iloc *iloc)
{
	ext4_fsblk_t err_blk = 0;
	int ret;

	ret = __ext4_get_inode_loc(inode->i_sb, inode->i_ino, NULL, iloc,
					&err_blk);

	if (ret == -EIO)
		ext4_error_inode_block(inode, err_blk, EIO,
					"unable to read itable block");

	return ret;
}

int ext4_get_inode_loc(struct inode *inode, struct ext4_iloc *iloc)
{
	ext4_fsblk_t err_blk = 0;
	int ret;

	ret = __ext4_get_inode_loc(inode->i_sb, inode->i_ino, inode, iloc,
					&err_blk);

	if (ret == -EIO)
		ext4_error_inode_block(inode, err_blk, EIO,
					"unable to read itable block");

	return ret;
}


int ext4_get_fc_inode_loc(struct super_block *sb, unsigned long ino,
			  struct ext4_iloc *iloc)
{
	return __ext4_get_inode_loc(sb, ino, NULL, iloc, NULL);
}

static bool ext4_should_enable_dax(struct inode *inode)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

	if (test_opt2(inode->i_sb, DAX_NEVER))
		return false;
	if (!S_ISREG(inode->i_mode))
		return false;
	if (ext4_should_journal_data(inode))
		return false;
	if (ext4_has_inline_data(inode))
		return false;
	if (ext4_test_inode_flag(inode, EXT4_INODE_ENCRYPT))
		return false;
	if (ext4_test_inode_flag(inode, EXT4_INODE_VERITY))
		return false;
	if (!test_bit(EXT4_FLAGS_BDEV_IS_DAX, &sbi->s_ext4_flags))
		return false;
	if (test_opt(inode->i_sb, DAX_ALWAYS))
		return true;

	return ext4_test_inode_flag(inode, EXT4_INODE_DAX);
}

void ext4_set_inode_flags(struct inode *inode, bool init)
{
	unsigned int flags = EXT4_I(inode)->i_flags;
	unsigned int new_fl = 0;

	WARN_ON_ONCE(IS_DAX(inode) && init);

	if (flags & EXT4_SYNC_FL)
		new_fl |= S_SYNC;
	if (flags & EXT4_APPEND_FL)
		new_fl |= S_APPEND;
	if (flags & EXT4_IMMUTABLE_FL)
		new_fl |= S_IMMUTABLE;
	if (flags & EXT4_NOATIME_FL)
		new_fl |= S_NOATIME;
	if (flags & EXT4_DIRSYNC_FL)
		new_fl |= S_DIRSYNC;

	 
	new_fl |= (inode->i_flags & S_DAX);
	if (init && ext4_should_enable_dax(inode))
		new_fl |= S_DAX;

	if (flags & EXT4_ENCRYPT_FL)
		new_fl |= S_ENCRYPTED;
	if (flags & EXT4_CASEFOLD_FL)
		new_fl |= S_CASEFOLD;
	if (flags & EXT4_VERITY_FL)
		new_fl |= S_VERITY;
	inode_set_flags(inode, new_fl,
			S_SYNC|S_APPEND|S_IMMUTABLE|S_NOATIME|S_DIRSYNC|S_DAX|
			S_ENCRYPTED|S_CASEFOLD|S_VERITY);
}

static blkcnt_t ext4_inode_blocks(struct ext4_inode *raw_inode,
				  struct ext4_inode_info *ei)
{
	blkcnt_t i_blocks ;
	struct inode *inode = &(ei->vfs_inode);
	struct super_block *sb = inode->i_sb;

	if (ext4_has_feature_huge_file(sb)) {
		 
		i_blocks = ((u64)le16_to_cpu(raw_inode->i_blocks_high)) << 32 |
					le32_to_cpu(raw_inode->i_blocks_lo);
		if (ext4_test_inode_flag(inode, EXT4_INODE_HUGE_FILE)) {
			 
			return i_blocks  << (inode->i_blkbits - 9);
		} else {
			return i_blocks;
		}
	} else {
		return le32_to_cpu(raw_inode->i_blocks_lo);
	}
}

static inline int ext4_iget_extra_inode(struct inode *inode,
					 struct ext4_inode *raw_inode,
					 struct ext4_inode_info *ei)
{
	__le32 *magic = (void *)raw_inode +
			EXT4_GOOD_OLD_INODE_SIZE + ei->i_extra_isize;

	if (EXT4_INODE_HAS_XATTR_SPACE(inode)  &&
	    *magic == cpu_to_le32(EXT4_XATTR_MAGIC)) {
		int err;

		ext4_set_inode_state(inode, EXT4_STATE_XATTR);
		err = ext4_find_inline_data_nolock(inode);
		if (!err && ext4_has_inline_data(inode))
			ext4_set_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA);
		return err;
	} else
		EXT4_I(inode)->i_inline_off = 0;
	return 0;
}

int ext4_get_projid(struct inode *inode, kprojid_t *projid)
{
	if (!ext4_has_feature_project(inode->i_sb))
		return -EOPNOTSUPP;
	*projid = EXT4_I(inode)->i_projid;
	return 0;
}

 
static inline void ext4_inode_set_iversion_queried(struct inode *inode, u64 val)
{
	if (unlikely(EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL))
		inode_set_iversion_raw(inode, val);
	else
		inode_set_iversion_queried(inode, val);
}

static const char *check_igot_inode(struct inode *inode, ext4_iget_flags flags)

{
	if (flags & EXT4_IGET_EA_INODE) {
		if (!(EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL))
			return "missing EA_INODE flag";
		if (ext4_test_inode_state(inode, EXT4_STATE_XATTR) ||
		    EXT4_I(inode)->i_file_acl)
			return "ea_inode with extended attributes";
	} else {
		if ((EXT4_I(inode)->i_flags & EXT4_EA_INODE_FL))
			return "unexpected EA_INODE flag";
	}
	if (is_bad_inode(inode) && !(flags & EXT4_IGET_BAD))
		return "unexpected bad inode w/o EXT4_IGET_BAD";
	return NULL;
}

struct inode *__ext4_iget(struct super_block *sb, unsigned long ino,
			  ext4_iget_flags flags, const char *function,
			  unsigned int line)
{
	struct ext4_iloc iloc;
	struct ext4_inode *raw_inode;
	struct ext4_inode_info *ei;
	struct ext4_super_block *es = EXT4_SB(sb)->s_es;
	struct inode *inode;
	const char *err_str;
	journal_t *journal = EXT4_SB(sb)->s_journal;
	long ret;
	loff_t size;
	int block;
	uid_t i_uid;
	gid_t i_gid;
	projid_t i_projid;

	if ((!(flags & EXT4_IGET_SPECIAL) &&
	     ((ino < EXT4_FIRST_INO(sb) && ino != EXT4_ROOT_INO) ||
	      ino == le32_to_cpu(es->s_usr_quota_inum) ||
	      ino == le32_to_cpu(es->s_grp_quota_inum) ||
	      ino == le32_to_cpu(es->s_prj_quota_inum) ||
	      ino == le32_to_cpu(es->s_orphan_file_inum))) ||
	    (ino < EXT4_ROOT_INO) ||
	    (ino > le32_to_cpu(es->s_inodes_count))) {
		if (flags & EXT4_IGET_HANDLE)
			return ERR_PTR(-ESTALE);
		__ext4_error(sb, function, line, false, EFSCORRUPTED, 0,
			     "inode #%lu: comm %s: iget: illegal inode #",
			     ino, current->comm);
		return ERR_PTR(-EFSCORRUPTED);
	}

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW)) {
		if ((err_str = check_igot_inode(inode, flags)) != NULL) {
			ext4_error_inode(inode, function, line, 0, err_str);
			iput(inode);
			return ERR_PTR(-EFSCORRUPTED);
		}
		return inode;
	}

	ei = EXT4_I(inode);
	iloc.bh = NULL;

	ret = __ext4_get_inode_loc_noinmem(inode, &iloc);
	if (ret < 0)
		goto bad_inode;
	raw_inode = ext4_raw_inode(&iloc);

	if ((flags & EXT4_IGET_HANDLE) &&
	    (raw_inode->i_links_count == 0) && (raw_inode->i_mode == 0)) {
		ret = -ESTALE;
		goto bad_inode;
	}

	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE) {
		ei->i_extra_isize = le16_to_cpu(raw_inode->i_extra_isize);
		if (EXT4_GOOD_OLD_INODE_SIZE + ei->i_extra_isize >
			EXT4_INODE_SIZE(inode->i_sb) ||
		    (ei->i_extra_isize & 3)) {
			ext4_error_inode(inode, function, line, 0,
					 "iget: bad extra_isize %u "
					 "(inode size %u)",
					 ei->i_extra_isize,
					 EXT4_INODE_SIZE(inode->i_sb));
			ret = -EFSCORRUPTED;
			goto bad_inode;
		}
	} else
		ei->i_extra_isize = 0;

	 
	if (ext4_has_metadata_csum(sb)) {
		struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
		__u32 csum;
		__le32 inum = cpu_to_le32(inode->i_ino);
		__le32 gen = raw_inode->i_generation;
		csum = ext4_chksum(sbi, sbi->s_csum_seed, (__u8 *)&inum,
				   sizeof(inum));
		ei->i_csum_seed = ext4_chksum(sbi, csum, (__u8 *)&gen,
					      sizeof(gen));
	}

	if ((!ext4_inode_csum_verify(inode, raw_inode, ei) ||
	    ext4_simulate_fail(sb, EXT4_SIM_INODE_CRC)) &&
	     (!(EXT4_SB(sb)->s_mount_state & EXT4_FC_REPLAY))) {
		ext4_error_inode_err(inode, function, line, 0,
				EFSBADCRC, "iget: checksum invalid");
		ret = -EFSBADCRC;
		goto bad_inode;
	}

	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	i_uid = (uid_t)le16_to_cpu(raw_inode->i_uid_low);
	i_gid = (gid_t)le16_to_cpu(raw_inode->i_gid_low);
	if (ext4_has_feature_project(sb) &&
	    EXT4_INODE_SIZE(sb) > EXT4_GOOD_OLD_INODE_SIZE &&
	    EXT4_FITS_IN_INODE(raw_inode, ei, i_projid))
		i_projid = (projid_t)le32_to_cpu(raw_inode->i_projid);
	else
		i_projid = EXT4_DEF_PROJID;

	if (!(test_opt(inode->i_sb, NO_UID32))) {
		i_uid |= le16_to_cpu(raw_inode->i_uid_high) << 16;
		i_gid |= le16_to_cpu(raw_inode->i_gid_high) << 16;
	}
	i_uid_write(inode, i_uid);
	i_gid_write(inode, i_gid);
	ei->i_projid = make_kprojid(&init_user_ns, i_projid);
	set_nlink(inode, le16_to_cpu(raw_inode->i_links_count));

	ext4_clear_state_flags(ei);	 
	ei->i_inline_off = 0;
	ei->i_dir_start_lookup = 0;
	ei->i_dtime = le32_to_cpu(raw_inode->i_dtime);
	 
	if (inode->i_nlink == 0) {
		if ((inode->i_mode == 0 || flags & EXT4_IGET_SPECIAL ||
		     !(EXT4_SB(inode->i_sb)->s_mount_state & EXT4_ORPHAN_FS)) &&
		    ino != EXT4_BOOT_LOADER_INO) {
			 
			if (flags & EXT4_IGET_SPECIAL) {
				ext4_error_inode(inode, function, line, 0,
						 "iget: special inode unallocated");
				ret = -EFSCORRUPTED;
			} else
				ret = -ESTALE;
			goto bad_inode;
		}
		 
	}
	ei->i_flags = le32_to_cpu(raw_inode->i_flags);
	ext4_set_inode_flags(inode, true);
	inode->i_blocks = ext4_inode_blocks(raw_inode, ei);
	ei->i_file_acl = le32_to_cpu(raw_inode->i_file_acl_lo);
	if (ext4_has_feature_64bit(sb))
		ei->i_file_acl |=
			((__u64)le16_to_cpu(raw_inode->i_file_acl_high)) << 32;
	inode->i_size = ext4_isize(sb, raw_inode);
	if ((size = i_size_read(inode)) < 0) {
		ext4_error_inode(inode, function, line, 0,
				 "iget: bad i_size value: %lld", size);
		ret = -EFSCORRUPTED;
		goto bad_inode;
	}
	 
	if (!ext4_has_feature_dir_index(sb) && ext4_has_metadata_csum(sb) &&
	    ext4_test_inode_flag(inode, EXT4_INODE_INDEX)) {
		ext4_error_inode(inode, function, line, 0,
			 "iget: Dir with htree data on filesystem without dir_index feature.");
		ret = -EFSCORRUPTED;
		goto bad_inode;
	}
	ei->i_disksize = inode->i_size;
#ifdef CONFIG_QUOTA
	ei->i_reserved_quota = 0;
#endif
	inode->i_generation = le32_to_cpu(raw_inode->i_generation);
	ei->i_block_group = iloc.block_group;
	ei->i_last_alloc_group = ~0;
	 
	for (block = 0; block < EXT4_N_BLOCKS; block++)
		ei->i_data[block] = raw_inode->i_block[block];
	INIT_LIST_HEAD(&ei->i_orphan);
	ext4_fc_init_inode(&ei->vfs_inode);

	 
	if (journal) {
		transaction_t *transaction;
		tid_t tid;

		read_lock(&journal->j_state_lock);
		if (journal->j_running_transaction)
			transaction = journal->j_running_transaction;
		else
			transaction = journal->j_committing_transaction;
		if (transaction)
			tid = transaction->t_tid;
		else
			tid = journal->j_commit_sequence;
		read_unlock(&journal->j_state_lock);
		ei->i_sync_tid = tid;
		ei->i_datasync_tid = tid;
	}

	if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE) {
		if (ei->i_extra_isize == 0) {
			 
			BUILD_BUG_ON(sizeof(struct ext4_inode) & 3);
			ei->i_extra_isize = sizeof(struct ext4_inode) -
					    EXT4_GOOD_OLD_INODE_SIZE;
		} else {
			ret = ext4_iget_extra_inode(inode, raw_inode, ei);
			if (ret)
				goto bad_inode;
		}
	}

	EXT4_INODE_GET_CTIME(inode, raw_inode);
	EXT4_INODE_GET_XTIME(i_mtime, inode, raw_inode);
	EXT4_INODE_GET_XTIME(i_atime, inode, raw_inode);
	EXT4_EINODE_GET_XTIME(i_crtime, ei, raw_inode);

	if (likely(!test_opt2(inode->i_sb, HURD_COMPAT))) {
		u64 ivers = le32_to_cpu(raw_inode->i_disk_version);

		if (EXT4_INODE_SIZE(inode->i_sb) > EXT4_GOOD_OLD_INODE_SIZE) {
			if (EXT4_FITS_IN_INODE(raw_inode, ei, i_version_hi))
				ivers |=
		    (__u64)(le32_to_cpu(raw_inode->i_version_hi)) << 32;
		}
		ext4_inode_set_iversion_queried(inode, ivers);
	}

	ret = 0;
	if (ei->i_file_acl &&
	    !ext4_inode_block_valid(inode, ei->i_file_acl, 1)) {
		ext4_error_inode(inode, function, line, 0,
				 "iget: bad extended attribute block %llu",
				 ei->i_file_acl);
		ret = -EFSCORRUPTED;
		goto bad_inode;
	} else if (!ext4_has_inline_data(inode)) {
		 
		if (!(EXT4_SB(sb)->s_mount_state & EXT4_FC_REPLAY) &&
			(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
			(S_ISLNK(inode->i_mode) &&
			!ext4_inode_is_fast_symlink(inode)))) {
			if (ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))
				ret = ext4_ext_check_inode(inode);
			else
				ret = ext4_ind_check_inode(inode);
		}
	}
	if (ret)
		goto bad_inode;

	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &ext4_file_inode_operations;
		inode->i_fop = &ext4_file_operations;
		ext4_set_aops(inode);
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &ext4_dir_inode_operations;
		inode->i_fop = &ext4_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		 
		if (IS_APPEND(inode) || IS_IMMUTABLE(inode)) {
			ext4_error_inode(inode, function, line, 0,
					 "iget: immutable or append flags "
					 "not allowed on symlinks");
			ret = -EFSCORRUPTED;
			goto bad_inode;
		}
		if (IS_ENCRYPTED(inode)) {
			inode->i_op = &ext4_encrypted_symlink_inode_operations;
		} else if (ext4_inode_is_fast_symlink(inode)) {
			inode->i_link = (char *)ei->i_data;
			inode->i_op = &ext4_fast_symlink_inode_operations;
			nd_terminate_link(ei->i_data, inode->i_size,
				sizeof(ei->i_data) - 1);
		} else {
			inode->i_op = &ext4_symlink_inode_operations;
		}
	} else if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode) ||
	      S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
		inode->i_op = &ext4_special_inode_operations;
		if (raw_inode->i_block[0])
			init_special_inode(inode, inode->i_mode,
			   old_decode_dev(le32_to_cpu(raw_inode->i_block[0])));
		else
			init_special_inode(inode, inode->i_mode,
			   new_decode_dev(le32_to_cpu(raw_inode->i_block[1])));
	} else if (ino == EXT4_BOOT_LOADER_INO) {
		make_bad_inode(inode);
	} else {
		ret = -EFSCORRUPTED;
		ext4_error_inode(inode, function, line, 0,
				 "iget: bogus i_mode (%o)", inode->i_mode);
		goto bad_inode;
	}
	if (IS_CASEFOLDED(inode) && !ext4_has_feature_casefold(inode->i_sb)) {
		ext4_error_inode(inode, function, line, 0,
				 "casefold flag without casefold feature");
		ret = -EFSCORRUPTED;
		goto bad_inode;
	}
	if ((err_str = check_igot_inode(inode, flags)) != NULL) {
		ext4_error_inode(inode, function, line, 0, err_str);
		ret = -EFSCORRUPTED;
		goto bad_inode;
	}

	brelse(iloc.bh);
	unlock_new_inode(inode);
	return inode;

bad_inode:
	brelse(iloc.bh);
	iget_failed(inode);
	return ERR_PTR(ret);
}

static void __ext4_update_other_inode_time(struct super_block *sb,
					   unsigned long orig_ino,
					   unsigned long ino,
					   struct ext4_inode *raw_inode)
{
	struct inode *inode;

	inode = find_inode_by_ino_rcu(sb, ino);
	if (!inode)
		return;

	if (!inode_is_dirtytime_only(inode))
		return;

	spin_lock(&inode->i_lock);
	if (inode_is_dirtytime_only(inode)) {
		struct ext4_inode_info	*ei = EXT4_I(inode);

		inode->i_state &= ~I_DIRTY_TIME;
		spin_unlock(&inode->i_lock);

		spin_lock(&ei->i_raw_lock);
		EXT4_INODE_SET_CTIME(inode, raw_inode);
		EXT4_INODE_SET_XTIME(i_mtime, inode, raw_inode);
		EXT4_INODE_SET_XTIME(i_atime, inode, raw_inode);
		ext4_inode_csum_set(inode, raw_inode, ei);
		spin_unlock(&ei->i_raw_lock);
		trace_ext4_other_inode_update_time(inode, orig_ino);
		return;
	}
	spin_unlock(&inode->i_lock);
}

 
static void ext4_update_other_inodes_time(struct super_block *sb,
					  unsigned long orig_ino, char *buf)
{
	unsigned long ino;
	int i, inodes_per_block = EXT4_SB(sb)->s_inodes_per_block;
	int inode_size = EXT4_INODE_SIZE(sb);

	 
	ino = ((orig_ino - 1) & ~(inodes_per_block - 1)) + 1;
	rcu_read_lock();
	for (i = 0; i < inodes_per_block; i++, ino++, buf += inode_size) {
		if (ino == orig_ino)
			continue;
		__ext4_update_other_inode_time(sb, orig_ino, ino,
					       (struct ext4_inode *)buf);
	}
	rcu_read_unlock();
}

 
static int ext4_do_update_inode(handle_t *handle,
				struct inode *inode,
				struct ext4_iloc *iloc)
{
	struct ext4_inode *raw_inode = ext4_raw_inode(iloc);
	struct ext4_inode_info *ei = EXT4_I(inode);
	struct buffer_head *bh = iloc->bh;
	struct super_block *sb = inode->i_sb;
	int err;
	int need_datasync = 0, set_large_file = 0;

	spin_lock(&ei->i_raw_lock);

	 
	if (ext4_test_inode_state(inode, EXT4_STATE_NEW))
		memset(raw_inode, 0, EXT4_SB(inode->i_sb)->s_inode_size);

	if (READ_ONCE(ei->i_disksize) != ext4_isize(inode->i_sb, raw_inode))
		need_datasync = 1;
	if (ei->i_disksize > 0x7fffffffULL) {
		if (!ext4_has_feature_large_file(sb) ||
		    EXT4_SB(sb)->s_es->s_rev_level == cpu_to_le32(EXT4_GOOD_OLD_REV))
			set_large_file = 1;
	}

	err = ext4_fill_raw_inode(inode, raw_inode);
	spin_unlock(&ei->i_raw_lock);
	if (err) {
		EXT4_ERROR_INODE(inode, "corrupted inode contents");
		goto out_brelse;
	}

	if (inode->i_sb->s_flags & SB_LAZYTIME)
		ext4_update_other_inodes_time(inode->i_sb, inode->i_ino,
					      bh->b_data);

	BUFFER_TRACE(bh, "call ext4_handle_dirty_metadata");
	err = ext4_handle_dirty_metadata(handle, NULL, bh);
	if (err)
		goto out_error;
	ext4_clear_inode_state(inode, EXT4_STATE_NEW);
	if (set_large_file) {
		BUFFER_TRACE(EXT4_SB(sb)->s_sbh, "get write access");
		err = ext4_journal_get_write_access(handle, sb,
						    EXT4_SB(sb)->s_sbh,
						    EXT4_JTR_NONE);
		if (err)
			goto out_error;
		lock_buffer(EXT4_SB(sb)->s_sbh);
		ext4_set_feature_large_file(sb);
		ext4_superblock_csum_set(sb);
		unlock_buffer(EXT4_SB(sb)->s_sbh);
		ext4_handle_sync(handle);
		err = ext4_handle_dirty_metadata(handle, NULL,
						 EXT4_SB(sb)->s_sbh);
	}
	ext4_update_inode_fsync_trans(handle, inode, need_datasync);
out_error:
	ext4_std_error(inode->i_sb, err);
out_brelse:
	brelse(bh);
	return err;
}

 
int ext4_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	int err;

	if (WARN_ON_ONCE(current->flags & PF_MEMALLOC))
		return 0;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	if (EXT4_SB(inode->i_sb)->s_journal) {
		if (ext4_journal_current_handle()) {
			ext4_debug("called recursively, non-PF_MEMALLOC!\n");
			dump_stack();
			return -EIO;
		}

		 
		if (wbc->sync_mode != WB_SYNC_ALL || wbc->for_sync)
			return 0;

		err = ext4_fc_commit(EXT4_SB(inode->i_sb)->s_journal,
						EXT4_I(inode)->i_sync_tid);
	} else {
		struct ext4_iloc iloc;

		err = __ext4_get_inode_loc_noinmem(inode, &iloc);
		if (err)
			return err;
		 
		if (wbc->sync_mode == WB_SYNC_ALL && !wbc->for_sync)
			sync_dirty_buffer(iloc.bh);
		if (buffer_req(iloc.bh) && !buffer_uptodate(iloc.bh)) {
			ext4_error_inode_block(inode, iloc.bh->b_blocknr, EIO,
					       "IO error syncing inode");
			err = -EIO;
		}
		brelse(iloc.bh);
	}
	return err;
}

 
static void ext4_wait_for_tail_page_commit(struct inode *inode)
{
	unsigned offset;
	journal_t *journal = EXT4_SB(inode->i_sb)->s_journal;
	tid_t commit_tid = 0;
	int ret;

	offset = inode->i_size & (PAGE_SIZE - 1);
	 
	if (!offset || offset > (PAGE_SIZE - i_blocksize(inode)))
		return;
	while (1) {
		struct folio *folio = filemap_lock_folio(inode->i_mapping,
				      inode->i_size >> PAGE_SHIFT);
		if (IS_ERR(folio))
			return;
		ret = __ext4_journalled_invalidate_folio(folio, offset,
						folio_size(folio) - offset);
		folio_unlock(folio);
		folio_put(folio);
		if (ret != -EBUSY)
			return;
		commit_tid = 0;
		read_lock(&journal->j_state_lock);
		if (journal->j_committing_transaction)
			commit_tid = journal->j_committing_transaction->t_tid;
		read_unlock(&journal->j_state_lock);
		if (commit_tid)
			jbd2_log_wait_commit(journal, commit_tid);
	}
}

 
int ext4_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		 struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	int error, rc = 0;
	int orphan = 0;
	const unsigned int ia_valid = attr->ia_valid;
	bool inc_ivers = true;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	if (unlikely(IS_IMMUTABLE(inode)))
		return -EPERM;

	if (unlikely(IS_APPEND(inode) &&
		     (ia_valid & (ATTR_MODE | ATTR_UID |
				  ATTR_GID | ATTR_TIMES_SET))))
		return -EPERM;

	error = setattr_prepare(idmap, dentry, attr);
	if (error)
		return error;

	error = fscrypt_prepare_setattr(dentry, attr);
	if (error)
		return error;

	error = fsverity_prepare_setattr(dentry, attr);
	if (error)
		return error;

	if (is_quota_modification(idmap, inode, attr)) {
		error = dquot_initialize(inode);
		if (error)
			return error;
	}

	if (i_uid_needs_update(idmap, attr, inode) ||
	    i_gid_needs_update(idmap, attr, inode)) {
		handle_t *handle;

		 
		handle = ext4_journal_start(inode, EXT4_HT_QUOTA,
			(EXT4_MAXQUOTAS_INIT_BLOCKS(inode->i_sb) +
			 EXT4_MAXQUOTAS_DEL_BLOCKS(inode->i_sb)) + 3);
		if (IS_ERR(handle)) {
			error = PTR_ERR(handle);
			goto err_out;
		}

		 
		down_read(&EXT4_I(inode)->xattr_sem);
		error = dquot_transfer(idmap, inode, attr);
		up_read(&EXT4_I(inode)->xattr_sem);

		if (error) {
			ext4_journal_stop(handle);
			return error;
		}
		 
		i_uid_update(idmap, attr, inode);
		i_gid_update(idmap, attr, inode);
		error = ext4_mark_inode_dirty(handle, inode);
		ext4_journal_stop(handle);
		if (unlikely(error)) {
			return error;
		}
	}

	if (attr->ia_valid & ATTR_SIZE) {
		handle_t *handle;
		loff_t oldsize = inode->i_size;
		loff_t old_disksize;
		int shrink = (attr->ia_size < inode->i_size);

		if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS))) {
			struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);

			if (attr->ia_size > sbi->s_bitmap_maxbytes) {
				return -EFBIG;
			}
		}
		if (!S_ISREG(inode->i_mode)) {
			return -EINVAL;
		}

		if (attr->ia_size == inode->i_size)
			inc_ivers = false;

		if (shrink) {
			if (ext4_should_order_data(inode)) {
				error = ext4_begin_ordered_truncate(inode,
							    attr->ia_size);
				if (error)
					goto err_out;
			}
			 
			inode_dio_wait(inode);
		}

		filemap_invalidate_lock(inode->i_mapping);

		rc = ext4_break_layouts(inode);
		if (rc) {
			filemap_invalidate_unlock(inode->i_mapping);
			goto err_out;
		}

		if (attr->ia_size != inode->i_size) {
			handle = ext4_journal_start(inode, EXT4_HT_INODE, 3);
			if (IS_ERR(handle)) {
				error = PTR_ERR(handle);
				goto out_mmap_sem;
			}
			if (ext4_handle_valid(handle) && shrink) {
				error = ext4_orphan_add(handle, inode);
				orphan = 1;
			}
			 
			if (!shrink)
				inode->i_mtime = inode_set_ctime_current(inode);

			if (shrink)
				ext4_fc_track_range(handle, inode,
					(attr->ia_size > 0 ? attr->ia_size - 1 : 0) >>
					inode->i_sb->s_blocksize_bits,
					EXT_MAX_BLOCKS - 1);
			else
				ext4_fc_track_range(
					handle, inode,
					(oldsize > 0 ? oldsize - 1 : oldsize) >>
					inode->i_sb->s_blocksize_bits,
					(attr->ia_size > 0 ? attr->ia_size - 1 : 0) >>
					inode->i_sb->s_blocksize_bits);

			down_write(&EXT4_I(inode)->i_data_sem);
			old_disksize = EXT4_I(inode)->i_disksize;
			EXT4_I(inode)->i_disksize = attr->ia_size;
			rc = ext4_mark_inode_dirty(handle, inode);
			if (!error)
				error = rc;
			 
			if (!error)
				i_size_write(inode, attr->ia_size);
			else
				EXT4_I(inode)->i_disksize = old_disksize;
			up_write(&EXT4_I(inode)->i_data_sem);
			ext4_journal_stop(handle);
			if (error)
				goto out_mmap_sem;
			if (!shrink) {
				pagecache_isize_extended(inode, oldsize,
							 inode->i_size);
			} else if (ext4_should_journal_data(inode)) {
				ext4_wait_for_tail_page_commit(inode);
			}
		}

		 
		truncate_pagecache(inode, inode->i_size);
		 
		if (attr->ia_size <= oldsize) {
			rc = ext4_truncate(inode);
			if (rc)
				error = rc;
		}
out_mmap_sem:
		filemap_invalidate_unlock(inode->i_mapping);
	}

	if (!error) {
		if (inc_ivers)
			inode_inc_iversion(inode);
		setattr_copy(idmap, inode, attr);
		mark_inode_dirty(inode);
	}

	 
	if (orphan && inode->i_nlink)
		ext4_orphan_del(NULL, inode);

	if (!error && (ia_valid & ATTR_MODE))
		rc = posix_acl_chmod(idmap, dentry, inode->i_mode);

err_out:
	if  (error)
		ext4_std_error(inode->i_sb, error);
	if (!error)
		error = rc;
	return error;
}

u32 ext4_dio_alignment(struct inode *inode)
{
	if (fsverity_active(inode))
		return 0;
	if (ext4_should_journal_data(inode))
		return 0;
	if (ext4_has_inline_data(inode))
		return 0;
	if (IS_ENCRYPTED(inode)) {
		if (!fscrypt_dio_supported(inode))
			return 0;
		return i_blocksize(inode);
	}
	return 1;  
}

int ext4_getattr(struct mnt_idmap *idmap, const struct path *path,
		 struct kstat *stat, u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	struct ext4_inode *raw_inode;
	struct ext4_inode_info *ei = EXT4_I(inode);
	unsigned int flags;

	if ((request_mask & STATX_BTIME) &&
	    EXT4_FITS_IN_INODE(raw_inode, ei, i_crtime)) {
		stat->result_mask |= STATX_BTIME;
		stat->btime.tv_sec = ei->i_crtime.tv_sec;
		stat->btime.tv_nsec = ei->i_crtime.tv_nsec;
	}

	 
	if ((request_mask & STATX_DIOALIGN) && S_ISREG(inode->i_mode)) {
		u32 dio_align = ext4_dio_alignment(inode);

		stat->result_mask |= STATX_DIOALIGN;
		if (dio_align == 1) {
			struct block_device *bdev = inode->i_sb->s_bdev;

			 
			stat->dio_mem_align = bdev_dma_alignment(bdev) + 1;
			stat->dio_offset_align = bdev_logical_block_size(bdev);
		} else {
			stat->dio_mem_align = dio_align;
			stat->dio_offset_align = dio_align;
		}
	}

	flags = ei->i_flags & EXT4_FL_USER_VISIBLE;
	if (flags & EXT4_APPEND_FL)
		stat->attributes |= STATX_ATTR_APPEND;
	if (flags & EXT4_COMPR_FL)
		stat->attributes |= STATX_ATTR_COMPRESSED;
	if (flags & EXT4_ENCRYPT_FL)
		stat->attributes |= STATX_ATTR_ENCRYPTED;
	if (flags & EXT4_IMMUTABLE_FL)
		stat->attributes |= STATX_ATTR_IMMUTABLE;
	if (flags & EXT4_NODUMP_FL)
		stat->attributes |= STATX_ATTR_NODUMP;
	if (flags & EXT4_VERITY_FL)
		stat->attributes |= STATX_ATTR_VERITY;

	stat->attributes_mask |= (STATX_ATTR_APPEND |
				  STATX_ATTR_COMPRESSED |
				  STATX_ATTR_ENCRYPTED |
				  STATX_ATTR_IMMUTABLE |
				  STATX_ATTR_NODUMP |
				  STATX_ATTR_VERITY);

	generic_fillattr(idmap, request_mask, inode, stat);
	return 0;
}

int ext4_file_getattr(struct mnt_idmap *idmap,
		      const struct path *path, struct kstat *stat,
		      u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	u64 delalloc_blocks;

	ext4_getattr(idmap, path, stat, request_mask, query_flags);

	 
	if (unlikely(ext4_has_inline_data(inode)))
		stat->blocks += (stat->size + 511) >> 9;

	 
	delalloc_blocks = EXT4_C2B(EXT4_SB(inode->i_sb),
				   EXT4_I(inode)->i_reserved_data_blocks);
	stat->blocks += delalloc_blocks << (inode->i_sb->s_blocksize_bits - 9);
	return 0;
}

static int ext4_index_trans_blocks(struct inode *inode, int lblocks,
				   int pextents)
{
	if (!(ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
		return ext4_ind_trans_blocks(inode, lblocks);
	return ext4_ext_index_trans_blocks(inode, pextents);
}

 
static int ext4_meta_trans_blocks(struct inode *inode, int lblocks,
				  int pextents)
{
	ext4_group_t groups, ngroups = ext4_get_groups_count(inode->i_sb);
	int gdpblocks;
	int idxblocks;
	int ret;

	 
	idxblocks = ext4_index_trans_blocks(inode, lblocks, pextents);

	ret = idxblocks;

	 
	groups = idxblocks + pextents;
	gdpblocks = groups;
	if (groups > ngroups)
		groups = ngroups;
	if (groups > EXT4_SB(inode->i_sb)->s_gdb_count)
		gdpblocks = EXT4_SB(inode->i_sb)->s_gdb_count;

	 
	ret += groups + gdpblocks;

	 
	ret += EXT4_META_TRANS_BLOCKS(inode->i_sb);

	return ret;
}

 
int ext4_writepage_trans_blocks(struct inode *inode)
{
	int bpp = ext4_journal_blocks_per_page(inode);
	int ret;

	ret = ext4_meta_trans_blocks(inode, bpp, bpp);

	 
	if (ext4_should_journal_data(inode))
		ret += bpp;
	return ret;
}

 
int ext4_chunk_trans_blocks(struct inode *inode, int nrblocks)
{
	return ext4_meta_trans_blocks(inode, nrblocks, 1);
}

 
int ext4_mark_iloc_dirty(handle_t *handle,
			 struct inode *inode, struct ext4_iloc *iloc)
{
	int err = 0;

	if (unlikely(ext4_forced_shutdown(inode->i_sb))) {
		put_bh(iloc->bh);
		return -EIO;
	}
	ext4_fc_track_inode(handle, inode);

	 
	get_bh(iloc->bh);

	 
	err = ext4_do_update_inode(handle, inode, iloc);
	put_bh(iloc->bh);
	return err;
}

 

int
ext4_reserve_inode_write(handle_t *handle, struct inode *inode,
			 struct ext4_iloc *iloc)
{
	int err;

	if (unlikely(ext4_forced_shutdown(inode->i_sb)))
		return -EIO;

	err = ext4_get_inode_loc(inode, iloc);
	if (!err) {
		BUFFER_TRACE(iloc->bh, "get_write_access");
		err = ext4_journal_get_write_access(handle, inode->i_sb,
						    iloc->bh, EXT4_JTR_NONE);
		if (err) {
			brelse(iloc->bh);
			iloc->bh = NULL;
		}
	}
	ext4_std_error(inode->i_sb, err);
	return err;
}

static int __ext4_expand_extra_isize(struct inode *inode,
				     unsigned int new_extra_isize,
				     struct ext4_iloc *iloc,
				     handle_t *handle, int *no_expand)
{
	struct ext4_inode *raw_inode;
	struct ext4_xattr_ibody_header *header;
	unsigned int inode_size = EXT4_INODE_SIZE(inode->i_sb);
	struct ext4_inode_info *ei = EXT4_I(inode);
	int error;

	 
	if ((EXT4_GOOD_OLD_INODE_SIZE + ei->i_extra_isize > inode_size) ||
	    (ei->i_extra_isize & 3)) {
		EXT4_ERROR_INODE(inode, "bad extra_isize %u (inode size %u)",
				 ei->i_extra_isize,
				 EXT4_INODE_SIZE(inode->i_sb));
		return -EFSCORRUPTED;
	}
	if ((new_extra_isize < ei->i_extra_isize) ||
	    (new_extra_isize < 4) ||
	    (new_extra_isize > inode_size - EXT4_GOOD_OLD_INODE_SIZE))
		return -EINVAL;	 

	raw_inode = ext4_raw_inode(iloc);

	header = IHDR(inode, raw_inode);

	 
	if (!ext4_test_inode_state(inode, EXT4_STATE_XATTR) ||
	    header->h_magic != cpu_to_le32(EXT4_XATTR_MAGIC)) {
		memset((void *)raw_inode + EXT4_GOOD_OLD_INODE_SIZE +
		       EXT4_I(inode)->i_extra_isize, 0,
		       new_extra_isize - EXT4_I(inode)->i_extra_isize);
		EXT4_I(inode)->i_extra_isize = new_extra_isize;
		return 0;
	}

	 
	if (dquot_initialize_needed(inode))
		return -EAGAIN;

	 
	error = ext4_expand_extra_isize_ea(inode, new_extra_isize,
					   raw_inode, handle);
	if (error) {
		 
		*no_expand = 1;
	}

	return error;
}

 
static int ext4_try_to_expand_extra_isize(struct inode *inode,
					  unsigned int new_extra_isize,
					  struct ext4_iloc iloc,
					  handle_t *handle)
{
	int no_expand;
	int error;

	if (ext4_test_inode_state(inode, EXT4_STATE_NO_EXPAND))
		return -EOVERFLOW;

	 
	if (ext4_journal_extend(handle,
				EXT4_DATA_TRANS_BLOCKS(inode->i_sb), 0) != 0)
		return -ENOSPC;

	if (ext4_write_trylock_xattr(inode, &no_expand) == 0)
		return -EBUSY;

	error = __ext4_expand_extra_isize(inode, new_extra_isize, &iloc,
					  handle, &no_expand);
	ext4_write_unlock_xattr(inode, &no_expand);

	return error;
}

int ext4_expand_extra_isize(struct inode *inode,
			    unsigned int new_extra_isize,
			    struct ext4_iloc *iloc)
{
	handle_t *handle;
	int no_expand;
	int error, rc;

	if (ext4_test_inode_state(inode, EXT4_STATE_NO_EXPAND)) {
		brelse(iloc->bh);
		return -EOVERFLOW;
	}

	handle = ext4_journal_start(inode, EXT4_HT_INODE,
				    EXT4_DATA_TRANS_BLOCKS(inode->i_sb));
	if (IS_ERR(handle)) {
		error = PTR_ERR(handle);
		brelse(iloc->bh);
		return error;
	}

	ext4_write_lock_xattr(inode, &no_expand);

	BUFFER_TRACE(iloc->bh, "get_write_access");
	error = ext4_journal_get_write_access(handle, inode->i_sb, iloc->bh,
					      EXT4_JTR_NONE);
	if (error) {
		brelse(iloc->bh);
		goto out_unlock;
	}

	error = __ext4_expand_extra_isize(inode, new_extra_isize, iloc,
					  handle, &no_expand);

	rc = ext4_mark_iloc_dirty(handle, inode, iloc);
	if (!error)
		error = rc;

out_unlock:
	ext4_write_unlock_xattr(inode, &no_expand);
	ext4_journal_stop(handle);
	return error;
}

 
int __ext4_mark_inode_dirty(handle_t *handle, struct inode *inode,
				const char *func, unsigned int line)
{
	struct ext4_iloc iloc;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	int err;

	might_sleep();
	trace_ext4_mark_inode_dirty(inode, _RET_IP_);
	err = ext4_reserve_inode_write(handle, inode, &iloc);
	if (err)
		goto out;

	if (EXT4_I(inode)->i_extra_isize < sbi->s_want_extra_isize)
		ext4_try_to_expand_extra_isize(inode, sbi->s_want_extra_isize,
					       iloc, handle);

	err = ext4_mark_iloc_dirty(handle, inode, &iloc);
out:
	if (unlikely(err))
		ext4_error_inode_err(inode, func, line, 0, err,
					"mark_inode_dirty error");
	return err;
}

 
void ext4_dirty_inode(struct inode *inode, int flags)
{
	handle_t *handle;

	handle = ext4_journal_start(inode, EXT4_HT_INODE, 2);
	if (IS_ERR(handle))
		return;
	ext4_mark_inode_dirty(handle, inode);
	ext4_journal_stop(handle);
}

int ext4_change_inode_journal_flag(struct inode *inode, int val)
{
	journal_t *journal;
	handle_t *handle;
	int err;
	int alloc_ctx;

	 

	journal = EXT4_JOURNAL(inode);
	if (!journal)
		return 0;
	if (is_journal_aborted(journal))
		return -EROFS;

	 
	inode_dio_wait(inode);

	 
	if (val) {
		filemap_invalidate_lock(inode->i_mapping);
		err = filemap_write_and_wait(inode->i_mapping);
		if (err < 0) {
			filemap_invalidate_unlock(inode->i_mapping);
			return err;
		}
	}

	alloc_ctx = ext4_writepages_down_write(inode->i_sb);
	jbd2_journal_lock_updates(journal);

	 

	if (val)
		ext4_set_inode_flag(inode, EXT4_INODE_JOURNAL_DATA);
	else {
		err = jbd2_journal_flush(journal, 0);
		if (err < 0) {
			jbd2_journal_unlock_updates(journal);
			ext4_writepages_up_write(inode->i_sb, alloc_ctx);
			return err;
		}
		ext4_clear_inode_flag(inode, EXT4_INODE_JOURNAL_DATA);
	}
	ext4_set_aops(inode);

	jbd2_journal_unlock_updates(journal);
	ext4_writepages_up_write(inode->i_sb, alloc_ctx);

	if (val)
		filemap_invalidate_unlock(inode->i_mapping);

	 

	handle = ext4_journal_start(inode, EXT4_HT_INODE, 1);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	ext4_fc_mark_ineligible(inode->i_sb,
		EXT4_FC_REASON_JOURNAL_FLAG_CHANGE, handle);
	err = ext4_mark_inode_dirty(handle, inode);
	ext4_handle_sync(handle);
	ext4_journal_stop(handle);
	ext4_std_error(inode->i_sb, err);

	return err;
}

static int ext4_bh_unmapped(handle_t *handle, struct inode *inode,
			    struct buffer_head *bh)
{
	return !buffer_mapped(bh);
}

vm_fault_t ext4_page_mkwrite(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct folio *folio = page_folio(vmf->page);
	loff_t size;
	unsigned long len;
	int err;
	vm_fault_t ret;
	struct file *file = vma->vm_file;
	struct inode *inode = file_inode(file);
	struct address_space *mapping = inode->i_mapping;
	handle_t *handle;
	get_block_t *get_block;
	int retries = 0;

	if (unlikely(IS_IMMUTABLE(inode)))
		return VM_FAULT_SIGBUS;

	sb_start_pagefault(inode->i_sb);
	file_update_time(vma->vm_file);

	filemap_invalidate_lock_shared(mapping);

	err = ext4_convert_inline_data(inode);
	if (err)
		goto out_ret;

	 
	if (ext4_should_journal_data(inode))
		goto retry_alloc;

	 
	if (test_opt(inode->i_sb, DELALLOC) &&
	    !ext4_nonda_switch(inode->i_sb)) {
		do {
			err = block_page_mkwrite(vma, vmf,
						   ext4_da_get_block_prep);
		} while (err == -ENOSPC &&
		       ext4_should_retry_alloc(inode->i_sb, &retries));
		goto out_ret;
	}

	folio_lock(folio);
	size = i_size_read(inode);
	 
	if (folio->mapping != mapping || folio_pos(folio) > size) {
		folio_unlock(folio);
		ret = VM_FAULT_NOPAGE;
		goto out;
	}

	len = folio_size(folio);
	if (folio_pos(folio) + len > size)
		len = size - folio_pos(folio);
	 
	if (folio_buffers(folio)) {
		if (!ext4_walk_page_buffers(NULL, inode, folio_buffers(folio),
					    0, len, NULL,
					    ext4_bh_unmapped)) {
			 
			folio_wait_stable(folio);
			ret = VM_FAULT_LOCKED;
			goto out;
		}
	}
	folio_unlock(folio);
	 
	if (ext4_should_dioread_nolock(inode))
		get_block = ext4_get_block_unwritten;
	else
		get_block = ext4_get_block;
retry_alloc:
	handle = ext4_journal_start(inode, EXT4_HT_WRITE_PAGE,
				    ext4_writepage_trans_blocks(inode));
	if (IS_ERR(handle)) {
		ret = VM_FAULT_SIGBUS;
		goto out;
	}
	 
	if (!ext4_should_journal_data(inode)) {
		err = block_page_mkwrite(vma, vmf, get_block);
	} else {
		folio_lock(folio);
		size = i_size_read(inode);
		 
		if (folio->mapping != mapping || folio_pos(folio) > size) {
			ret = VM_FAULT_NOPAGE;
			goto out_error;
		}

		len = folio_size(folio);
		if (folio_pos(folio) + len > size)
			len = size - folio_pos(folio);

		err = __block_write_begin(&folio->page, 0, len, ext4_get_block);
		if (!err) {
			ret = VM_FAULT_SIGBUS;
			if (ext4_journal_folio_buffers(handle, folio, len))
				goto out_error;
		} else {
			folio_unlock(folio);
		}
	}
	ext4_journal_stop(handle);
	if (err == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry_alloc;
out_ret:
	ret = vmf_fs_error(err);
out:
	filemap_invalidate_unlock_shared(mapping);
	sb_end_pagefault(inode->i_sb);
	return ret;
out_error:
	folio_unlock(folio);
	ext4_journal_stop(handle);
	goto out;
}
