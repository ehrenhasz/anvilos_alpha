
 

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/bio.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "inode.h"
#include "journal.h"
#include "uptodate.h"
#include "buffer_head_io.h"
#include "ocfs2_trace.h"

 
enum ocfs2_state_bits {
	BH_NeedsValidate = BH_JBDPrivateStart,
};

 
BUFFER_FNS(NeedsValidate, needs_validate);

int ocfs2_write_block(struct ocfs2_super *osb, struct buffer_head *bh,
		      struct ocfs2_caching_info *ci)
{
	int ret = 0;

	trace_ocfs2_write_block((unsigned long long)bh->b_blocknr, ci);

	BUG_ON(bh->b_blocknr < OCFS2_SUPER_BLOCK_BLKNO);
	BUG_ON(buffer_jbd(bh));

	 
	if (ocfs2_is_hard_readonly(osb)) {
		ret = -EROFS;
		mlog_errno(ret);
		goto out;
	}

	ocfs2_metadata_cache_io_lock(ci);

	lock_buffer(bh);
	set_buffer_uptodate(bh);

	 
	clear_buffer_dirty(bh);

	get_bh(bh);  
	bh->b_end_io = end_buffer_write_sync;
	submit_bh(REQ_OP_WRITE, bh);

	wait_on_buffer(bh);

	if (buffer_uptodate(bh)) {
		ocfs2_set_buffer_uptodate(ci, bh);
	} else {
		 
		ret = -EIO;
		mlog_errno(ret);
	}

	ocfs2_metadata_cache_io_unlock(ci);
out:
	return ret;
}

 
int ocfs2_read_blocks_sync(struct ocfs2_super *osb, u64 block,
			   unsigned int nr, struct buffer_head *bhs[])
{
	int status = 0;
	unsigned int i;
	struct buffer_head *bh;
	int new_bh = 0;

	trace_ocfs2_read_blocks_sync((unsigned long long)block, nr);

	if (!nr)
		goto bail;

	 
	new_bh = (bhs[0] == NULL);

	for (i = 0 ; i < nr ; i++) {
		if (bhs[i] == NULL) {
			bhs[i] = sb_getblk(osb->sb, block++);
			if (bhs[i] == NULL) {
				status = -ENOMEM;
				mlog_errno(status);
				break;
			}
		}
		bh = bhs[i];

		if (buffer_jbd(bh)) {
			trace_ocfs2_read_blocks_sync_jbd(
					(unsigned long long)bh->b_blocknr);
			continue;
		}

		if (buffer_dirty(bh)) {
			 
			mlog(ML_ERROR,
			     "trying to sync read a dirty "
			     "buffer! (blocknr = %llu), skipping\n",
			     (unsigned long long)bh->b_blocknr);
			continue;
		}

		lock_buffer(bh);
		if (buffer_jbd(bh)) {
#ifdef CATCH_BH_JBD_RACES
			mlog(ML_ERROR,
			     "block %llu had the JBD bit set "
			     "while I was in lock_buffer!",
			     (unsigned long long)bh->b_blocknr);
			BUG();
#else
			unlock_buffer(bh);
			continue;
#endif
		}

		get_bh(bh);  
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(REQ_OP_READ, bh);
	}

read_failure:
	for (i = nr; i > 0; i--) {
		bh = bhs[i - 1];

		if (unlikely(status)) {
			if (new_bh && bh) {
				 
				if (!buffer_jbd(bh))
					wait_on_buffer(bh);
				put_bh(bh);
				bhs[i - 1] = NULL;
			} else if (bh && buffer_uptodate(bh)) {
				clear_buffer_uptodate(bh);
			}
			continue;
		}

		 
		if (!buffer_jbd(bh))
			wait_on_buffer(bh);

		if (!buffer_uptodate(bh)) {
			 
			status = -EIO;
			goto read_failure;
		}
	}

bail:
	return status;
}

 
int ocfs2_read_blocks(struct ocfs2_caching_info *ci, u64 block, int nr,
		      struct buffer_head *bhs[], int flags,
		      int (*validate)(struct super_block *sb,
				      struct buffer_head *bh))
{
	int status = 0;
	int i, ignore_cache = 0;
	struct buffer_head *bh;
	struct super_block *sb = ocfs2_metadata_cache_get_super(ci);
	int new_bh = 0;

	trace_ocfs2_read_blocks_begin(ci, (unsigned long long)block, nr, flags);

	BUG_ON(!ci);
	BUG_ON((flags & OCFS2_BH_READAHEAD) &&
	       (flags & OCFS2_BH_IGNORE_CACHE));

	if (bhs == NULL) {
		status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}

	if (nr < 0) {
		mlog(ML_ERROR, "asked to read %d blocks!\n", nr);
		status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}

	if (nr == 0) {
		status = 0;
		goto bail;
	}

	 
	new_bh = (bhs[0] == NULL);

	ocfs2_metadata_cache_io_lock(ci);
	for (i = 0 ; i < nr ; i++) {
		if (bhs[i] == NULL) {
			bhs[i] = sb_getblk(sb, block++);
			if (bhs[i] == NULL) {
				ocfs2_metadata_cache_io_unlock(ci);
				status = -ENOMEM;
				mlog_errno(status);
				 
				break;
			}
		}
		bh = bhs[i];
		ignore_cache = (flags & OCFS2_BH_IGNORE_CACHE);

		 

		if (!ignore_cache && !ocfs2_buffer_uptodate(ci, bh)) {
			trace_ocfs2_read_blocks_from_disk(
			     (unsigned long long)bh->b_blocknr,
			     (unsigned long long)ocfs2_metadata_cache_owner(ci));
			 
			ignore_cache = 1;
		}

		trace_ocfs2_read_blocks_bh((unsigned long long)bh->b_blocknr,
			ignore_cache, buffer_jbd(bh), buffer_dirty(bh));

		if (buffer_jbd(bh)) {
			continue;
		}

		if (ignore_cache) {
			if (buffer_dirty(bh)) {
				 
				continue;
			}

			 
			if ((flags & OCFS2_BH_READAHEAD)
			    && ocfs2_buffer_read_ahead(ci, bh))
				continue;

			lock_buffer(bh);
			if (buffer_jbd(bh)) {
#ifdef CATCH_BH_JBD_RACES
				mlog(ML_ERROR, "block %llu had the JBD bit set "
					       "while I was in lock_buffer!",
				     (unsigned long long)bh->b_blocknr);
				BUG();
#else
				unlock_buffer(bh);
				continue;
#endif
			}

			 
			if (!(flags & OCFS2_BH_IGNORE_CACHE)
			    && !(flags & OCFS2_BH_READAHEAD)
			    && ocfs2_buffer_uptodate(ci, bh)) {
				unlock_buffer(bh);
				continue;
			}

			get_bh(bh);  
			if (validate)
				set_buffer_needs_validate(bh);
			bh->b_end_io = end_buffer_read_sync;
			submit_bh(REQ_OP_READ, bh);
			continue;
		}
	}

read_failure:
	for (i = (nr - 1); i >= 0; i--) {
		bh = bhs[i];

		if (!(flags & OCFS2_BH_READAHEAD)) {
			if (unlikely(status)) {
				 
				if (new_bh && bh) {
					 
					if (!buffer_jbd(bh))
						wait_on_buffer(bh);
					put_bh(bh);
					bhs[i] = NULL;
				} else if (bh && buffer_uptodate(bh)) {
					clear_buffer_uptodate(bh);
				}
				continue;
			}
			 
			if (!buffer_jbd(bh))
				wait_on_buffer(bh);

			if (!buffer_uptodate(bh)) {
				 
				status = -EIO;
				clear_buffer_needs_validate(bh);
				goto read_failure;
			}

			if (buffer_needs_validate(bh)) {
				 
				BUG_ON(buffer_jbd(bh));
				clear_buffer_needs_validate(bh);
				status = validate(sb, bh);
				if (status)
					goto read_failure;
			}
		}

		 
		ocfs2_set_buffer_uptodate(ci, bh);
	}
	ocfs2_metadata_cache_io_unlock(ci);

	trace_ocfs2_read_blocks_end((unsigned long long)block, nr,
				    flags, ignore_cache);

bail:

	return status;
}

 
static void ocfs2_check_super_or_backup(struct super_block *sb,
					sector_t blkno)
{
	int i;
	u64 backup_blkno;

	if (blkno == OCFS2_SUPER_BLOCK_BLKNO)
		return;

	for (i = 0; i < OCFS2_MAX_BACKUP_SUPERBLOCKS; i++) {
		backup_blkno = ocfs2_backup_super_blkno(sb, i);
		if (backup_blkno == blkno)
			return;
	}

	BUG();
}

 
int ocfs2_write_super_or_backup(struct ocfs2_super *osb,
				struct buffer_head *bh)
{
	int ret = 0;
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)bh->b_data;

	BUG_ON(buffer_jbd(bh));
	ocfs2_check_super_or_backup(osb->sb, bh->b_blocknr);

	if (ocfs2_is_hard_readonly(osb) || ocfs2_is_soft_readonly(osb)) {
		ret = -EROFS;
		mlog_errno(ret);
		goto out;
	}

	lock_buffer(bh);
	set_buffer_uptodate(bh);

	 
	clear_buffer_dirty(bh);

	get_bh(bh);  
	bh->b_end_io = end_buffer_write_sync;
	ocfs2_compute_meta_ecc(osb->sb, bh->b_data, &di->i_check);
	submit_bh(REQ_OP_WRITE, bh);

	wait_on_buffer(bh);

	if (!buffer_uptodate(bh)) {
		ret = -EIO;
		mlog_errno(ret);
	}

out:
	return ret;
}
