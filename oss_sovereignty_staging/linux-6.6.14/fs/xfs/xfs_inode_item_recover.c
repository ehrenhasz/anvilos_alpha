
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_inode_item.h"
#include "xfs_trace.h"
#include "xfs_trans_priv.h"
#include "xfs_buf_item.h"
#include "xfs_log.h"
#include "xfs_error.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"
#include "xfs_icache.h"
#include "xfs_bmap_btree.h"

STATIC void
xlog_recover_inode_ra_pass2(
	struct xlog                     *log,
	struct xlog_recover_item        *item)
{
	if (item->ri_buf[0].i_len == sizeof(struct xfs_inode_log_format)) {
		struct xfs_inode_log_format	*ilfp = item->ri_buf[0].i_addr;

		xlog_buf_readahead(log, ilfp->ilf_blkno, ilfp->ilf_len,
				   &xfs_inode_buf_ra_ops);
	} else {
		struct xfs_inode_log_format_32	*ilfp = item->ri_buf[0].i_addr;

		xlog_buf_readahead(log, ilfp->ilf_blkno, ilfp->ilf_len,
				   &xfs_inode_buf_ra_ops);
	}
}

 

STATIC int
xfs_recover_inode_owner_change(
	struct xfs_mount	*mp,
	struct xfs_dinode	*dip,
	struct xfs_inode_log_format *in_f,
	struct list_head	*buffer_list)
{
	struct xfs_inode	*ip;
	int			error;

	ASSERT(in_f->ilf_fields & (XFS_ILOG_DOWNER|XFS_ILOG_AOWNER));

	ip = xfs_inode_alloc(mp, in_f->ilf_ino);
	if (!ip)
		return -ENOMEM;

	 
	ASSERT(dip->di_version >= 3);

	error = xfs_inode_from_disk(ip, dip);
	if (error)
		goto out_free_ip;

	if (in_f->ilf_fields & XFS_ILOG_DOWNER) {
		ASSERT(in_f->ilf_fields & XFS_ILOG_DBROOT);
		error = xfs_bmbt_change_owner(NULL, ip, XFS_DATA_FORK,
					      ip->i_ino, buffer_list);
		if (error)
			goto out_free_ip;
	}

	if (in_f->ilf_fields & XFS_ILOG_AOWNER) {
		ASSERT(in_f->ilf_fields & XFS_ILOG_ABROOT);
		error = xfs_bmbt_change_owner(NULL, ip, XFS_ATTR_FORK,
					      ip->i_ino, buffer_list);
		if (error)
			goto out_free_ip;
	}

out_free_ip:
	xfs_inode_free(ip);
	return error;
}

static inline bool xfs_log_dinode_has_bigtime(const struct xfs_log_dinode *ld)
{
	return ld->di_version >= 3 &&
	       (ld->di_flags2 & XFS_DIFLAG2_BIGTIME);
}

 
static inline xfs_timestamp_t
xfs_log_dinode_to_disk_ts(
	struct xfs_log_dinode		*from,
	const xfs_log_timestamp_t	its)
{
	struct xfs_legacy_timestamp	*lts;
	struct xfs_log_legacy_timestamp	*lits;
	xfs_timestamp_t			ts;

	if (xfs_log_dinode_has_bigtime(from))
		return cpu_to_be64(its);

	lts = (struct xfs_legacy_timestamp *)&ts;
	lits = (struct xfs_log_legacy_timestamp *)&its;
	lts->t_sec = cpu_to_be32(lits->t_sec);
	lts->t_nsec = cpu_to_be32(lits->t_nsec);

	return ts;
}

static inline bool xfs_log_dinode_has_large_extent_counts(
		const struct xfs_log_dinode *ld)
{
	return ld->di_version >= 3 &&
	       (ld->di_flags2 & XFS_DIFLAG2_NREXT64);
}

static inline void
xfs_log_dinode_to_disk_iext_counters(
	struct xfs_log_dinode	*from,
	struct xfs_dinode	*to)
{
	if (xfs_log_dinode_has_large_extent_counts(from)) {
		to->di_big_nextents = cpu_to_be64(from->di_big_nextents);
		to->di_big_anextents = cpu_to_be32(from->di_big_anextents);
		to->di_nrext64_pad = cpu_to_be16(from->di_nrext64_pad);
	} else {
		to->di_nextents = cpu_to_be32(from->di_nextents);
		to->di_anextents = cpu_to_be16(from->di_anextents);
	}

}

STATIC void
xfs_log_dinode_to_disk(
	struct xfs_log_dinode	*from,
	struct xfs_dinode	*to,
	xfs_lsn_t		lsn)
{
	to->di_magic = cpu_to_be16(from->di_magic);
	to->di_mode = cpu_to_be16(from->di_mode);
	to->di_version = from->di_version;
	to->di_format = from->di_format;
	to->di_onlink = 0;
	to->di_uid = cpu_to_be32(from->di_uid);
	to->di_gid = cpu_to_be32(from->di_gid);
	to->di_nlink = cpu_to_be32(from->di_nlink);
	to->di_projid_lo = cpu_to_be16(from->di_projid_lo);
	to->di_projid_hi = cpu_to_be16(from->di_projid_hi);

	to->di_atime = xfs_log_dinode_to_disk_ts(from, from->di_atime);
	to->di_mtime = xfs_log_dinode_to_disk_ts(from, from->di_mtime);
	to->di_ctime = xfs_log_dinode_to_disk_ts(from, from->di_ctime);

	to->di_size = cpu_to_be64(from->di_size);
	to->di_nblocks = cpu_to_be64(from->di_nblocks);
	to->di_extsize = cpu_to_be32(from->di_extsize);
	to->di_forkoff = from->di_forkoff;
	to->di_aformat = from->di_aformat;
	to->di_dmevmask = cpu_to_be32(from->di_dmevmask);
	to->di_dmstate = cpu_to_be16(from->di_dmstate);
	to->di_flags = cpu_to_be16(from->di_flags);
	to->di_gen = cpu_to_be32(from->di_gen);

	if (from->di_version == 3) {
		to->di_changecount = cpu_to_be64(from->di_changecount);
		to->di_crtime = xfs_log_dinode_to_disk_ts(from,
							  from->di_crtime);
		to->di_flags2 = cpu_to_be64(from->di_flags2);
		to->di_cowextsize = cpu_to_be32(from->di_cowextsize);
		to->di_ino = cpu_to_be64(from->di_ino);
		to->di_lsn = cpu_to_be64(lsn);
		memset(to->di_pad2, 0, sizeof(to->di_pad2));
		uuid_copy(&to->di_uuid, &from->di_uuid);
		to->di_v3_pad = 0;
	} else {
		to->di_flushiter = cpu_to_be16(from->di_flushiter);
		memset(to->di_v2_pad, 0, sizeof(to->di_v2_pad));
	}

	xfs_log_dinode_to_disk_iext_counters(from, to);
}

STATIC int
xlog_dinode_verify_extent_counts(
	struct xfs_mount	*mp,
	struct xfs_log_dinode	*ldip)
{
	xfs_extnum_t		nextents;
	xfs_aextnum_t		anextents;

	if (xfs_log_dinode_has_large_extent_counts(ldip)) {
		if (!xfs_has_large_extent_counts(mp) ||
		    (ldip->di_nrext64_pad != 0)) {
			XFS_CORRUPTION_ERROR(
				"Bad log dinode large extent count format",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
			xfs_alert(mp,
				"Bad inode 0x%llx, large extent counts %d, padding 0x%x",
				ldip->di_ino, xfs_has_large_extent_counts(mp),
				ldip->di_nrext64_pad);
			return -EFSCORRUPTED;
		}

		nextents = ldip->di_big_nextents;
		anextents = ldip->di_big_anextents;
	} else {
		if (ldip->di_version == 3 && ldip->di_v3_pad != 0) {
			XFS_CORRUPTION_ERROR(
				"Bad log dinode di_v3_pad",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
			xfs_alert(mp,
				"Bad inode 0x%llx, di_v3_pad 0x%llx",
				ldip->di_ino, ldip->di_v3_pad);
			return -EFSCORRUPTED;
		}

		nextents = ldip->di_nextents;
		anextents = ldip->di_anextents;
	}

	if (unlikely(nextents + anextents > ldip->di_nblocks)) {
		XFS_CORRUPTION_ERROR("Bad log dinode extent counts",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
		xfs_alert(mp,
			"Bad inode 0x%llx, large extent counts %d, nextents 0x%llx, anextents 0x%x, nblocks 0x%llx",
			ldip->di_ino, xfs_has_large_extent_counts(mp), nextents,
			anextents, ldip->di_nblocks);
		return -EFSCORRUPTED;
	}

	return 0;
}

STATIC int
xlog_recover_inode_commit_pass2(
	struct xlog			*log,
	struct list_head		*buffer_list,
	struct xlog_recover_item	*item,
	xfs_lsn_t			current_lsn)
{
	struct xfs_inode_log_format	*in_f;
	struct xfs_mount		*mp = log->l_mp;
	struct xfs_buf			*bp;
	struct xfs_dinode		*dip;
	int				len;
	char				*src;
	char				*dest;
	int				error;
	int				attr_index;
	uint				fields;
	struct xfs_log_dinode		*ldip;
	uint				isize;
	int				need_free = 0;

	if (item->ri_buf[0].i_len == sizeof(struct xfs_inode_log_format)) {
		in_f = item->ri_buf[0].i_addr;
	} else {
		in_f = kmem_alloc(sizeof(struct xfs_inode_log_format), 0);
		need_free = 1;
		error = xfs_inode_item_format_convert(&item->ri_buf[0], in_f);
		if (error)
			goto error;
	}

	 
	if (xlog_is_buffer_cancelled(log, in_f->ilf_blkno, in_f->ilf_len)) {
		error = 0;
		trace_xfs_log_recover_inode_cancel(log, in_f);
		goto error;
	}
	trace_xfs_log_recover_inode_recover(log, in_f);

	error = xfs_buf_read(mp->m_ddev_targp, in_f->ilf_blkno, in_f->ilf_len,
			0, &bp, &xfs_inode_buf_ops);
	if (error)
		goto error;
	ASSERT(in_f->ilf_fields & XFS_ILOG_CORE);
	dip = xfs_buf_offset(bp, in_f->ilf_boffset);

	 
	if (XFS_IS_CORRUPT(mp, !xfs_verify_magic16(bp, dip->di_magic))) {
		xfs_alert(mp,
	"%s: Bad inode magic number, dip = "PTR_FMT", dino bp = "PTR_FMT", ino = %lld",
			__func__, dip, bp, in_f->ilf_ino);
		error = -EFSCORRUPTED;
		goto out_release;
	}
	ldip = item->ri_buf[1].i_addr;
	if (XFS_IS_CORRUPT(mp, ldip->di_magic != XFS_DINODE_MAGIC)) {
		xfs_alert(mp,
			"%s: Bad inode log record, rec ptr "PTR_FMT", ino %lld",
			__func__, item, in_f->ilf_ino);
		error = -EFSCORRUPTED;
		goto out_release;
	}

	 
	if (dip->di_version >= 3) {
		xfs_lsn_t	lsn = be64_to_cpu(dip->di_lsn);

		if (lsn && lsn != -1 && XFS_LSN_CMP(lsn, current_lsn) > 0) {
			trace_xfs_log_recover_inode_skip(log, in_f);
			error = 0;
			goto out_owner_change;
		}
	}

	 
	if (!xfs_has_v3inodes(mp)) {
		if (ldip->di_flushiter < be16_to_cpu(dip->di_flushiter)) {
			 
			if (be16_to_cpu(dip->di_flushiter) == DI_MAX_FLUSH &&
			    ldip->di_flushiter < (DI_MAX_FLUSH >> 1)) {
				 
			} else {
				trace_xfs_log_recover_inode_skip(log, in_f);
				error = 0;
				goto out_release;
			}
		}

		 
		ldip->di_flushiter = 0;
	}


	if (unlikely(S_ISREG(ldip->di_mode))) {
		if ((ldip->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (ldip->di_format != XFS_DINODE_FMT_BTREE)) {
			XFS_CORRUPTION_ERROR(
				"Bad log dinode data fork format for regular file",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
			xfs_alert(mp,
				"Bad inode 0x%llx, data fork format 0x%x",
				in_f->ilf_ino, ldip->di_format);
			error = -EFSCORRUPTED;
			goto out_release;
		}
	} else if (unlikely(S_ISDIR(ldip->di_mode))) {
		if ((ldip->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (ldip->di_format != XFS_DINODE_FMT_BTREE) &&
		    (ldip->di_format != XFS_DINODE_FMT_LOCAL)) {
			XFS_CORRUPTION_ERROR(
				"Bad log dinode data fork format for directory",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
			xfs_alert(mp,
				"Bad inode 0x%llx, data fork format 0x%x",
				in_f->ilf_ino, ldip->di_format);
			error = -EFSCORRUPTED;
			goto out_release;
		}
	}

	error = xlog_dinode_verify_extent_counts(mp, ldip);
	if (error)
		goto out_release;

	if (unlikely(ldip->di_forkoff > mp->m_sb.sb_inodesize)) {
		XFS_CORRUPTION_ERROR("Bad log dinode fork offset",
				XFS_ERRLEVEL_LOW, mp, ldip, sizeof(*ldip));
		xfs_alert(mp,
			"Bad inode 0x%llx, di_forkoff 0x%x",
			in_f->ilf_ino, ldip->di_forkoff);
		error = -EFSCORRUPTED;
		goto out_release;
	}
	isize = xfs_log_dinode_size(mp);
	if (unlikely(item->ri_buf[1].i_len > isize)) {
		XFS_CORRUPTION_ERROR("Bad log dinode size", XFS_ERRLEVEL_LOW,
				     mp, ldip, sizeof(*ldip));
		xfs_alert(mp,
			"Bad inode 0x%llx log dinode size 0x%x",
			in_f->ilf_ino, item->ri_buf[1].i_len);
		error = -EFSCORRUPTED;
		goto out_release;
	}

	 
	xfs_log_dinode_to_disk(ldip, dip, current_lsn);

	fields = in_f->ilf_fields;
	if (fields & XFS_ILOG_DEV)
		xfs_dinode_put_rdev(dip, in_f->ilf_u.ilfu_rdev);

	if (in_f->ilf_size == 2)
		goto out_owner_change;
	len = item->ri_buf[2].i_len;
	src = item->ri_buf[2].i_addr;
	ASSERT(in_f->ilf_size <= 4);
	ASSERT((in_f->ilf_size == 3) || (fields & XFS_ILOG_AFORK));
	ASSERT(!(fields & XFS_ILOG_DFORK) ||
	       (len == xlog_calc_iovec_len(in_f->ilf_dsize)));

	switch (fields & XFS_ILOG_DFORK) {
	case XFS_ILOG_DDATA:
	case XFS_ILOG_DEXT:
		memcpy(XFS_DFORK_DPTR(dip), src, len);
		break;

	case XFS_ILOG_DBROOT:
		xfs_bmbt_to_bmdr(mp, (struct xfs_btree_block *)src, len,
				 (struct xfs_bmdr_block *)XFS_DFORK_DPTR(dip),
				 XFS_DFORK_DSIZE(dip, mp));
		break;

	default:
		 
		ASSERT((fields & XFS_ILOG_DFORK) == 0);
		break;
	}

	 
	if (in_f->ilf_fields & XFS_ILOG_AFORK) {
		if (in_f->ilf_fields & XFS_ILOG_DFORK) {
			attr_index = 3;
		} else {
			attr_index = 2;
		}
		len = item->ri_buf[attr_index].i_len;
		src = item->ri_buf[attr_index].i_addr;
		ASSERT(len == xlog_calc_iovec_len(in_f->ilf_asize));

		switch (in_f->ilf_fields & XFS_ILOG_AFORK) {
		case XFS_ILOG_ADATA:
		case XFS_ILOG_AEXT:
			dest = XFS_DFORK_APTR(dip);
			ASSERT(len <= XFS_DFORK_ASIZE(dip, mp));
			memcpy(dest, src, len);
			break;

		case XFS_ILOG_ABROOT:
			dest = XFS_DFORK_APTR(dip);
			xfs_bmbt_to_bmdr(mp, (struct xfs_btree_block *)src,
					 len, (struct xfs_bmdr_block *)dest,
					 XFS_DFORK_ASIZE(dip, mp));
			break;

		default:
			xfs_warn(log->l_mp, "%s: Invalid flag", __func__);
			ASSERT(0);
			error = -EFSCORRUPTED;
			goto out_release;
		}
	}

out_owner_change:
	 
	if ((in_f->ilf_fields & (XFS_ILOG_DOWNER|XFS_ILOG_AOWNER)) &&
	    (dip->di_mode != 0))
		error = xfs_recover_inode_owner_change(mp, dip, in_f,
						       buffer_list);
	 
	xfs_dinode_calc_crc(log->l_mp, dip);

	ASSERT(bp->b_mount == mp);
	bp->b_flags |= _XBF_LOGRECOVERY;
	xfs_buf_delwri_queue(bp, buffer_list);

out_release:
	xfs_buf_relse(bp);
error:
	if (need_free)
		kmem_free(in_f);
	return error;
}

const struct xlog_recover_item_ops xlog_inode_item_ops = {
	.item_type		= XFS_LI_INODE,
	.ra_pass2		= xlog_recover_inode_ra_pass2,
	.commit_pass2		= xlog_recover_inode_commit_pass2,
};
