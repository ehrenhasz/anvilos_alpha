
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_trace.h"
#include "xfs_bmap.h"
#include "xfs_trans.h"
#include "xfs_error.h"

 
static unsigned char xfs_dir3_filetype_table[] = {
	DT_UNKNOWN, DT_REG, DT_DIR, DT_CHR, DT_BLK,
	DT_FIFO, DT_SOCK, DT_LNK, DT_WHT,
};

unsigned char
xfs_dir3_get_dtype(
	struct xfs_mount	*mp,
	uint8_t			filetype)
{
	if (!xfs_has_ftype(mp))
		return DT_UNKNOWN;

	if (filetype >= XFS_DIR3_FT_MAX)
		return DT_UNKNOWN;

	return xfs_dir3_filetype_table[filetype];
}

STATIC int
xfs_dir2_sf_getdents(
	struct xfs_da_args	*args,
	struct dir_context	*ctx)
{
	int			i;		 
	struct xfs_inode	*dp = args->dp;	 
	struct xfs_mount	*mp = dp->i_mount;
	xfs_dir2_dataptr_t	off;		 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 
	xfs_dir2_dataptr_t	dot_offset;
	xfs_dir2_dataptr_t	dotdot_offset;
	xfs_ino_t		ino;
	struct xfs_da_geometry	*geo = args->geo;

	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(dp->i_df.if_bytes == dp->i_disk_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;

	 
	if (xfs_dir2_dataptr_to_db(geo, ctx->pos) > geo->datablk)
		return 0;

	 
	dot_offset = xfs_dir2_db_off_to_dataptr(geo, geo->datablk,
			geo->data_entry_offset);
	dotdot_offset = xfs_dir2_db_off_to_dataptr(geo, geo->datablk,
			geo->data_entry_offset +
			xfs_dir2_data_entsize(mp, sizeof(".") - 1));

	 
	if (ctx->pos <= dot_offset) {
		ctx->pos = dot_offset & 0x7fffffff;
		if (!dir_emit(ctx, ".", 1, dp->i_ino, DT_DIR))
			return 0;
	}

	 
	if (ctx->pos <= dotdot_offset) {
		ino = xfs_dir2_sf_get_parent_ino(sfp);
		ctx->pos = dotdot_offset & 0x7fffffff;
		if (!dir_emit(ctx, "..", 2, ino, DT_DIR))
			return 0;
	}

	 
	sfep = xfs_dir2_sf_firstentry(sfp);
	for (i = 0; i < sfp->count; i++) {
		uint8_t filetype;

		off = xfs_dir2_db_off_to_dataptr(geo, geo->datablk,
				xfs_dir2_sf_get_offset(sfep));

		if (ctx->pos > off) {
			sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
			continue;
		}

		ino = xfs_dir2_sf_get_ino(mp, sfp, sfep);
		filetype = xfs_dir2_sf_get_ftype(mp, sfep);
		ctx->pos = off & 0x7fffffff;
		if (XFS_IS_CORRUPT(dp->i_mount,
				   !xfs_dir2_namecheck(sfep->name,
						       sfep->namelen)))
			return -EFSCORRUPTED;
		if (!dir_emit(ctx, (char *)sfep->name, sfep->namelen, ino,
			    xfs_dir3_get_dtype(mp, filetype)))
			return 0;
		sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
	}

	ctx->pos = xfs_dir2_db_off_to_dataptr(geo, geo->datablk + 1, 0) &
								0x7fffffff;
	return 0;
}

 
STATIC int
xfs_dir2_block_getdents(
	struct xfs_da_args	*args,
	struct dir_context	*ctx,
	unsigned int		*lock_mode)
{
	struct xfs_inode	*dp = args->dp;	 
	struct xfs_buf		*bp;		 
	int			error;		 
	int			wantoff;	 
	xfs_off_t		cook;
	struct xfs_da_geometry	*geo = args->geo;
	unsigned int		offset, next_offset;
	unsigned int		end;

	 
	if (xfs_dir2_dataptr_to_db(geo, ctx->pos) > geo->datablk)
		return 0;

	error = xfs_dir3_block_read(args->trans, dp, &bp);
	if (error)
		return error;

	xfs_iunlock(dp, *lock_mode);
	*lock_mode = 0;

	 
	wantoff = xfs_dir2_dataptr_to_off(geo, ctx->pos);
	xfs_dir3_data_check(dp, bp);

	 
	end = xfs_dir3_data_end_offset(geo, bp->b_addr);
	for (offset = geo->data_entry_offset;
	     offset < end;
	     offset = next_offset) {
		struct xfs_dir2_data_unused	*dup = bp->b_addr + offset;
		struct xfs_dir2_data_entry	*dep = bp->b_addr + offset;
		uint8_t filetype;

		 
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			next_offset = offset + be16_to_cpu(dup->length);
			continue;
		}

		 
		next_offset = offset +
			xfs_dir2_data_entsize(dp->i_mount, dep->namelen);

		 
		if (offset < wantoff)
			continue;

		cook = xfs_dir2_db_off_to_dataptr(geo, geo->datablk, offset);

		ctx->pos = cook & 0x7fffffff;
		filetype = xfs_dir2_data_get_ftype(dp->i_mount, dep);
		 
		if (XFS_IS_CORRUPT(dp->i_mount,
				   !xfs_dir2_namecheck(dep->name,
						       dep->namelen))) {
			error = -EFSCORRUPTED;
			goto out_rele;
		}
		if (!dir_emit(ctx, (char *)dep->name, dep->namelen,
			    be64_to_cpu(dep->inumber),
			    xfs_dir3_get_dtype(dp->i_mount, filetype)))
			goto out_rele;
	}

	 
	ctx->pos = xfs_dir2_db_off_to_dataptr(geo, geo->datablk + 1, 0) &
								0x7fffffff;
out_rele:
	xfs_trans_brelse(args->trans, bp);
	return error;
}

 
STATIC int
xfs_dir2_leaf_readbuf(
	struct xfs_da_args	*args,
	size_t			bufsize,
	xfs_dir2_off_t		*cur_off,
	xfs_dablk_t		*ra_blk,
	struct xfs_buf		**bpp)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_buf		*bp = NULL;
	struct xfs_da_geometry	*geo = args->geo;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(dp, XFS_DATA_FORK);
	struct xfs_bmbt_irec	map;
	struct blk_plug		plug;
	xfs_dir2_off_t		new_off;
	xfs_dablk_t		next_ra;
	xfs_dablk_t		map_off;
	xfs_dablk_t		last_da;
	struct xfs_iext_cursor	icur;
	int			ra_want;
	int			error = 0;

	error = xfs_iread_extents(args->trans, dp, XFS_DATA_FORK);
	if (error)
		goto out;

	 
	last_da = xfs_dir2_byte_to_da(geo, XFS_DIR2_LEAF_OFFSET);
	map_off = xfs_dir2_db_to_da(geo, xfs_dir2_byte_to_db(geo, *cur_off));
	if (!xfs_iext_lookup_extent(dp, ifp, map_off, &icur, &map))
		goto out;
	if (map.br_startoff >= last_da)
		goto out;
	xfs_trim_extent(&map, map_off, last_da - map_off);

	 
	new_off = xfs_dir2_da_to_byte(geo, map.br_startoff);
	if (new_off > *cur_off)
		*cur_off = new_off;
	error = xfs_dir3_data_read(args->trans, dp, map.br_startoff, 0, &bp);
	if (error)
		goto out;

	 
	ra_want = howmany(bufsize + geo->blksize, (1 << geo->fsblog));
	if (*ra_blk >= last_da)
		goto out;
	else if (*ra_blk == 0)
		*ra_blk = map.br_startoff;
	next_ra = map.br_startoff + geo->fsbcount;
	if (next_ra >= last_da)
		goto out_no_ra;
	if (map.br_blockcount < geo->fsbcount &&
	    !xfs_iext_next_extent(ifp, &icur, &map))
		goto out_no_ra;
	if (map.br_startoff >= last_da)
		goto out_no_ra;
	xfs_trim_extent(&map, next_ra, last_da - next_ra);

	 
	blk_start_plug(&plug);
	while (ra_want > 0) {
		next_ra = roundup((xfs_dablk_t)map.br_startoff, geo->fsbcount);
		while (ra_want > 0 &&
		       next_ra < map.br_startoff + map.br_blockcount) {
			if (next_ra >= last_da) {
				*ra_blk = last_da;
				break;
			}
			if (next_ra > *ra_blk) {
				xfs_dir3_data_readahead(dp, next_ra,
							XFS_DABUF_MAP_HOLE_OK);
				*ra_blk = next_ra;
			}
			ra_want -= geo->fsbcount;
			next_ra += geo->fsbcount;
		}
		if (!xfs_iext_next_extent(ifp, &icur, &map)) {
			*ra_blk = last_da;
			break;
		}
	}
	blk_finish_plug(&plug);

out:
	*bpp = bp;
	return error;
out_no_ra:
	*ra_blk = last_da;
	goto out;
}

 
STATIC int
xfs_dir2_leaf_getdents(
	struct xfs_da_args	*args,
	struct dir_context	*ctx,
	size_t			bufsize,
	unsigned int		*lock_mode)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_buf		*bp = NULL;	 
	xfs_dir2_data_entry_t	*dep;		 
	xfs_dir2_data_unused_t	*dup;		 
	struct xfs_da_geometry	*geo = args->geo;
	xfs_dablk_t		rablk = 0;	 
	xfs_dir2_off_t		curoff;		 
	int			length;		 
	int			byteoff;	 
	unsigned int		offset = 0;
	int			error = 0;	 

	 
	if (ctx->pos >= XFS_DIR2_MAX_DATAPTR)
		return 0;

	 
	curoff = xfs_dir2_dataptr_to_byte(ctx->pos);

	 
	while (curoff < XFS_DIR2_LEAF_OFFSET) {
		uint8_t filetype;

		 
		if (!bp || offset >= geo->blksize) {
			if (bp) {
				xfs_trans_brelse(args->trans, bp);
				bp = NULL;
			}

			if (*lock_mode == 0)
				*lock_mode = xfs_ilock_data_map_shared(dp);
			error = xfs_dir2_leaf_readbuf(args, bufsize, &curoff,
					&rablk, &bp);
			if (error || !bp)
				break;

			xfs_iunlock(dp, *lock_mode);
			*lock_mode = 0;

			xfs_dir3_data_check(dp, bp);
			 
			offset = geo->data_entry_offset;
			byteoff = xfs_dir2_byte_to_off(geo, curoff);
			 
			if (byteoff == 0)
				curoff += geo->data_entry_offset;
			 
			else {
				while (offset < byteoff) {
					dup = bp->b_addr + offset;

					if (be16_to_cpu(dup->freetag)
						  == XFS_DIR2_DATA_FREE_TAG) {

						length = be16_to_cpu(dup->length);
						offset += length;
						continue;
					}
					dep = bp->b_addr + offset;
					length = xfs_dir2_data_entsize(mp,
							dep->namelen);
					offset += length;
				}
				 
				curoff =
					xfs_dir2_db_off_to_byte(geo,
					    xfs_dir2_byte_to_db(geo, curoff),
					    offset);
				if (offset >= geo->blksize)
					continue;
			}
		}

		 
		dup = bp->b_addr + offset;

		 
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			length = be16_to_cpu(dup->length);
			offset += length;
			curoff += length;
			continue;
		}

		dep = bp->b_addr + offset;
		length = xfs_dir2_data_entsize(mp, dep->namelen);
		filetype = xfs_dir2_data_get_ftype(mp, dep);

		ctx->pos = xfs_dir2_byte_to_dataptr(curoff) & 0x7fffffff;
		if (XFS_IS_CORRUPT(dp->i_mount,
				   !xfs_dir2_namecheck(dep->name,
						       dep->namelen))) {
			error = -EFSCORRUPTED;
			break;
		}
		if (!dir_emit(ctx, (char *)dep->name, dep->namelen,
			    be64_to_cpu(dep->inumber),
			    xfs_dir3_get_dtype(dp->i_mount, filetype)))
			break;

		 
		offset += length;
		curoff += length;
		 
		bufsize = bufsize > length ? bufsize - length : 0;
	}

	 
	if (curoff > xfs_dir2_dataptr_to_byte(XFS_DIR2_MAX_DATAPTR))
		ctx->pos = XFS_DIR2_MAX_DATAPTR & 0x7fffffff;
	else
		ctx->pos = xfs_dir2_byte_to_dataptr(curoff) & 0x7fffffff;
	if (bp)
		xfs_trans_brelse(args->trans, bp);
	return error;
}

 
int
xfs_readdir(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	struct dir_context	*ctx,
	size_t			bufsize)
{
	struct xfs_da_args	args = { NULL };
	unsigned int		lock_mode;
	bool			isblock;
	int			error;

	trace_xfs_readdir(dp);

	if (xfs_is_shutdown(dp->i_mount))
		return -EIO;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));
	ASSERT(xfs_isilocked(dp, XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL));
	XFS_STATS_INC(dp->i_mount, xs_dir_getdents);

	args.dp = dp;
	args.geo = dp->i_mount->m_dir_geo;
	args.trans = tp;

	if (dp->i_df.if_format == XFS_DINODE_FMT_LOCAL)
		return xfs_dir2_sf_getdents(&args, ctx);

	lock_mode = xfs_ilock_data_map_shared(dp);
	error = xfs_dir2_isblock(&args, &isblock);
	if (error)
		goto out_unlock;

	if (isblock) {
		error = xfs_dir2_block_getdents(&args, ctx, &lock_mode);
		goto out_unlock;
	}

	error = xfs_dir2_leaf_getdents(&args, ctx, bufsize, &lock_mode);

out_unlock:
	if (lock_mode)
		xfs_iunlock(dp, lock_mode);
	return error;
}
