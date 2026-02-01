
 
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
#include "xfs_error.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_log.h"

static xfs_failaddr_t xfs_dir2_data_freefind_verify(
		struct xfs_dir2_data_hdr *hdr, struct xfs_dir2_data_free *bf,
		struct xfs_dir2_data_unused *dup,
		struct xfs_dir2_data_free **bf_ent);

struct xfs_dir2_data_free *
xfs_dir2_data_bestfree_p(
	struct xfs_mount		*mp,
	struct xfs_dir2_data_hdr	*hdr)
{
	if (xfs_has_crc(mp))
		return ((struct xfs_dir3_data_hdr *)hdr)->best_free;
	return hdr->bestfree;
}

 
__be16 *
xfs_dir2_data_entry_tag_p(
	struct xfs_mount		*mp,
	struct xfs_dir2_data_entry	*dep)
{
	return (__be16 *)((char *)dep +
		xfs_dir2_data_entsize(mp, dep->namelen) - sizeof(__be16));
}

uint8_t
xfs_dir2_data_get_ftype(
	struct xfs_mount		*mp,
	struct xfs_dir2_data_entry	*dep)
{
	if (xfs_has_ftype(mp)) {
		uint8_t			ftype = dep->name[dep->namelen];

		if (likely(ftype < XFS_DIR3_FT_MAX))
			return ftype;
	}

	return XFS_DIR3_FT_UNKNOWN;
}

void
xfs_dir2_data_put_ftype(
	struct xfs_mount		*mp,
	struct xfs_dir2_data_entry	*dep,
	uint8_t				ftype)
{
	ASSERT(ftype < XFS_DIR3_FT_MAX);
	ASSERT(dep->namelen != 0);

	if (xfs_has_ftype(mp))
		dep->name[dep->namelen] = ftype;
}

 
static inline unsigned int
xfs_dir2_data_max_leaf_entries(
	struct xfs_da_geometry		*geo)
{
	return (geo->blksize - sizeof(struct xfs_dir2_block_tail) -
		geo->data_entry_offset) /
			sizeof(struct xfs_dir2_leaf_entry);
}

 
xfs_failaddr_t
__xfs_dir3_data_check(
	struct xfs_inode	*dp,		 
	struct xfs_buf		*bp)		 
{
	xfs_dir2_dataptr_t	addr;		 
	xfs_dir2_data_free_t	*bf;		 
	xfs_dir2_block_tail_t	*btp=NULL;	 
	int			count;		 
	xfs_dir2_data_hdr_t	*hdr;		 
	xfs_dir2_data_free_t	*dfp;		 
	int			freeseen;	 
	xfs_dahash_t		hash;		 
	int			i;		 
	int			lastfree;	 
	xfs_dir2_leaf_entry_t	*lep=NULL;	 
	struct xfs_mount	*mp = bp->b_mount;
	int			stale;		 
	struct xfs_name		name;
	unsigned int		offset;
	unsigned int		end;
	struct xfs_da_geometry	*geo = mp->m_dir_geo;

	 
	if (dp && !S_ISDIR(VFS_I(dp)->i_mode))
		return __this_address;

	hdr = bp->b_addr;
	offset = geo->data_entry_offset;

	switch (hdr->magic) {
	case cpu_to_be32(XFS_DIR3_BLOCK_MAGIC):
	case cpu_to_be32(XFS_DIR2_BLOCK_MAGIC):
		btp = xfs_dir2_block_tail_p(geo, hdr);
		lep = xfs_dir2_block_leaf_p(btp);

		if (be32_to_cpu(btp->count) >=
		    xfs_dir2_data_max_leaf_entries(geo))
			return __this_address;
		break;
	case cpu_to_be32(XFS_DIR3_DATA_MAGIC):
	case cpu_to_be32(XFS_DIR2_DATA_MAGIC):
		break;
	default:
		return __this_address;
	}
	end = xfs_dir3_data_end_offset(geo, hdr);
	if (!end)
		return __this_address;

	 
	bf = xfs_dir2_data_bestfree_p(mp, hdr);
	count = lastfree = freeseen = 0;
	if (!bf[0].length) {
		if (bf[0].offset)
			return __this_address;
		freeseen |= 1 << 0;
	}
	if (!bf[1].length) {
		if (bf[1].offset)
			return __this_address;
		freeseen |= 1 << 1;
	}
	if (!bf[2].length) {
		if (bf[2].offset)
			return __this_address;
		freeseen |= 1 << 2;
	}

	if (be16_to_cpu(bf[0].length) < be16_to_cpu(bf[1].length))
		return __this_address;
	if (be16_to_cpu(bf[1].length) < be16_to_cpu(bf[2].length))
		return __this_address;
	 
	while (offset < end) {
		struct xfs_dir2_data_unused	*dup = bp->b_addr + offset;
		struct xfs_dir2_data_entry	*dep = bp->b_addr + offset;

		 
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			xfs_failaddr_t	fa;

			if (lastfree != 0)
				return __this_address;
			if (offset + be16_to_cpu(dup->length) > end)
				return __this_address;
			if (be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)) !=
			    offset)
				return __this_address;
			fa = xfs_dir2_data_freefind_verify(hdr, bf, dup, &dfp);
			if (fa)
				return fa;
			if (dfp) {
				i = (int)(dfp - bf);
				if ((freeseen & (1 << i)) != 0)
					return __this_address;
				freeseen |= 1 << i;
			} else {
				if (be16_to_cpu(dup->length) >
				    be16_to_cpu(bf[2].length))
					return __this_address;
			}
			offset += be16_to_cpu(dup->length);
			lastfree = 1;
			continue;
		}
		 
		if (dep->namelen == 0)
			return __this_address;
		if (!xfs_verify_dir_ino(mp, be64_to_cpu(dep->inumber)))
			return __this_address;
		if (offset + xfs_dir2_data_entsize(mp, dep->namelen) > end)
			return __this_address;
		if (be16_to_cpu(*xfs_dir2_data_entry_tag_p(mp, dep)) != offset)
			return __this_address;
		if (xfs_dir2_data_get_ftype(mp, dep) >= XFS_DIR3_FT_MAX)
			return __this_address;
		count++;
		lastfree = 0;
		if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
		    hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC)) {
			addr = xfs_dir2_db_off_to_dataptr(geo, geo->datablk,
						(xfs_dir2_data_aoff_t)
						((char *)dep - (char *)hdr));
			name.name = dep->name;
			name.len = dep->namelen;
			hash = xfs_dir2_hashname(mp, &name);
			for (i = 0; i < be32_to_cpu(btp->count); i++) {
				if (be32_to_cpu(lep[i].address) == addr &&
				    be32_to_cpu(lep[i].hashval) == hash)
					break;
			}
			if (i >= be32_to_cpu(btp->count))
				return __this_address;
		}
		offset += xfs_dir2_data_entsize(mp, dep->namelen);
	}
	 
	if (freeseen != 7)
		return __this_address;
	if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	    hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC)) {
		for (i = stale = 0; i < be32_to_cpu(btp->count); i++) {
			if (lep[i].address ==
			    cpu_to_be32(XFS_DIR2_NULL_DATAPTR))
				stale++;
			if (i > 0 && be32_to_cpu(lep[i].hashval) <
				     be32_to_cpu(lep[i - 1].hashval))
				return __this_address;
		}
		if (count != be32_to_cpu(btp->count) - be32_to_cpu(btp->stale))
			return __this_address;
		if (stale != be32_to_cpu(btp->stale))
			return __this_address;
	}
	return NULL;
}

#ifdef DEBUG
void
xfs_dir3_data_check(
	struct xfs_inode	*dp,
	struct xfs_buf		*bp)
{
	xfs_failaddr_t		fa;

	fa = __xfs_dir3_data_check(dp, bp);
	if (!fa)
		return;
	xfs_corruption_error(__func__, XFS_ERRLEVEL_LOW, dp->i_mount,
			bp->b_addr, BBTOB(bp->b_length), __FILE__, __LINE__,
			fa);
	ASSERT(0);
}
#endif

static xfs_failaddr_t
xfs_dir3_data_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_dir3_blk_hdr	*hdr3 = bp->b_addr;

	if (!xfs_verify_magic(bp, hdr3->magic))
		return __this_address;

	if (xfs_has_crc(mp)) {
		if (!uuid_equal(&hdr3->uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (be64_to_cpu(hdr3->blkno) != xfs_buf_daddr(bp))
			return __this_address;
		if (!xfs_log_check_lsn(mp, be64_to_cpu(hdr3->lsn)))
			return __this_address;
	}
	return __xfs_dir3_data_check(NULL, bp);
}

 
static void
xfs_dir3_data_reada_verify(
	struct xfs_buf		*bp)
{
	struct xfs_dir2_data_hdr *hdr = bp->b_addr;

	switch (hdr->magic) {
	case cpu_to_be32(XFS_DIR2_BLOCK_MAGIC):
	case cpu_to_be32(XFS_DIR3_BLOCK_MAGIC):
		bp->b_ops = &xfs_dir3_block_buf_ops;
		bp->b_ops->verify_read(bp);
		return;
	case cpu_to_be32(XFS_DIR2_DATA_MAGIC):
	case cpu_to_be32(XFS_DIR3_DATA_MAGIC):
		bp->b_ops = &xfs_dir3_data_buf_ops;
		bp->b_ops->verify_read(bp);
		return;
	default:
		xfs_verifier_error(bp, -EFSCORRUPTED, __this_address);
		break;
	}
}

static void
xfs_dir3_data_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	xfs_failaddr_t		fa;

	if (xfs_has_crc(mp) &&
	    !xfs_buf_verify_cksum(bp, XFS_DIR3_DATA_CRC_OFF))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_dir3_data_verify(bp);
		if (fa)
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}
}

static void
xfs_dir3_data_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	struct xfs_dir3_blk_hdr	*hdr3 = bp->b_addr;
	xfs_failaddr_t		fa;

	fa = xfs_dir3_data_verify(bp);
	if (fa) {
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}

	if (!xfs_has_crc(mp))
		return;

	if (bip)
		hdr3->lsn = cpu_to_be64(bip->bli_item.li_lsn);

	xfs_buf_update_cksum(bp, XFS_DIR3_DATA_CRC_OFF);
}

const struct xfs_buf_ops xfs_dir3_data_buf_ops = {
	.name = "xfs_dir3_data",
	.magic = { cpu_to_be32(XFS_DIR2_DATA_MAGIC),
		   cpu_to_be32(XFS_DIR3_DATA_MAGIC) },
	.verify_read = xfs_dir3_data_read_verify,
	.verify_write = xfs_dir3_data_write_verify,
	.verify_struct = xfs_dir3_data_verify,
};

static const struct xfs_buf_ops xfs_dir3_data_reada_buf_ops = {
	.name = "xfs_dir3_data_reada",
	.magic = { cpu_to_be32(XFS_DIR2_DATA_MAGIC),
		   cpu_to_be32(XFS_DIR3_DATA_MAGIC) },
	.verify_read = xfs_dir3_data_reada_verify,
	.verify_write = xfs_dir3_data_write_verify,
};

static xfs_failaddr_t
xfs_dir3_data_header_check(
	struct xfs_inode	*dp,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = dp->i_mount;

	if (xfs_has_crc(mp)) {
		struct xfs_dir3_data_hdr *hdr3 = bp->b_addr;

		if (be64_to_cpu(hdr3->hdr.owner) != dp->i_ino)
			return __this_address;
	}

	return NULL;
}

int
xfs_dir3_data_read(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	xfs_dablk_t		bno,
	unsigned int		flags,
	struct xfs_buf		**bpp)
{
	xfs_failaddr_t		fa;
	int			err;

	err = xfs_da_read_buf(tp, dp, bno, flags, bpp, XFS_DATA_FORK,
			&xfs_dir3_data_buf_ops);
	if (err || !*bpp)
		return err;

	 
	fa = xfs_dir3_data_header_check(dp, *bpp);
	if (fa) {
		__xfs_buf_mark_corrupt(*bpp, fa);
		xfs_trans_brelse(tp, *bpp);
		*bpp = NULL;
		return -EFSCORRUPTED;
	}

	xfs_trans_buf_set_type(tp, *bpp, XFS_BLFT_DIR_DATA_BUF);
	return err;
}

int
xfs_dir3_data_readahead(
	struct xfs_inode	*dp,
	xfs_dablk_t		bno,
	unsigned int		flags)
{
	return xfs_da_reada_buf(dp, bno, flags, XFS_DATA_FORK,
				&xfs_dir3_data_reada_buf_ops);
}

 
static xfs_failaddr_t
xfs_dir2_data_freefind_verify(
	struct xfs_dir2_data_hdr	*hdr,
	struct xfs_dir2_data_free	*bf,
	struct xfs_dir2_data_unused	*dup,
	struct xfs_dir2_data_free	**bf_ent)
{
	struct xfs_dir2_data_free	*dfp;
	xfs_dir2_data_aoff_t		off;
	bool				matched = false;
	bool				seenzero = false;

	*bf_ent = NULL;
	off = (xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr);

	 
	for (dfp = &bf[0]; dfp < &bf[XFS_DIR2_DATA_FD_COUNT]; dfp++) {
		if (!dfp->offset) {
			if (dfp->length)
				return __this_address;
			seenzero = true;
			continue;
		}
		if (seenzero)
			return __this_address;
		if (be16_to_cpu(dfp->offset) == off) {
			matched = true;
			if (dfp->length != dup->length)
				return __this_address;
		} else if (be16_to_cpu(dfp->offset) > off) {
			if (off + be16_to_cpu(dup->length) >
					be16_to_cpu(dfp->offset))
				return __this_address;
		} else {
			if (be16_to_cpu(dfp->offset) +
					be16_to_cpu(dfp->length) > off)
				return __this_address;
		}
		if (!matched &&
		    be16_to_cpu(dfp->length) < be16_to_cpu(dup->length))
			return __this_address;
		if (dfp > &bf[0] &&
		    be16_to_cpu(dfp[-1].length) < be16_to_cpu(dfp[0].length))
			return __this_address;
	}

	 
	*bf_ent = xfs_dir2_data_freefind(hdr, bf, dup);
	return NULL;
}

 
xfs_dir2_data_free_t *
xfs_dir2_data_freefind(
	struct xfs_dir2_data_hdr *hdr,		 
	struct xfs_dir2_data_free *bf,		 
	struct xfs_dir2_data_unused *dup)	 
{
	xfs_dir2_data_free_t	*dfp;		 
	xfs_dir2_data_aoff_t	off;		 

	off = (xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr);

	 
	if (be16_to_cpu(dup->length) <
	    be16_to_cpu(bf[XFS_DIR2_DATA_FD_COUNT - 1].length))
		return NULL;
	 
	for (dfp = &bf[0]; dfp < &bf[XFS_DIR2_DATA_FD_COUNT]; dfp++) {
		if (!dfp->offset)
			return NULL;
		if (be16_to_cpu(dfp->offset) == off)
			return dfp;
	}
	 
	return NULL;
}

 
xfs_dir2_data_free_t *				 
xfs_dir2_data_freeinsert(
	struct xfs_dir2_data_hdr *hdr,		 
	struct xfs_dir2_data_free *dfp,		 
	struct xfs_dir2_data_unused *dup,	 
	int			*loghead)	 
{
	xfs_dir2_data_free_t	new;		 

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));

	new.length = dup->length;
	new.offset = cpu_to_be16((char *)dup - (char *)hdr);

	 
	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[0].length)) {
		dfp[2] = dfp[1];
		dfp[1] = dfp[0];
		dfp[0] = new;
		*loghead = 1;
		return &dfp[0];
	}
	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[1].length)) {
		dfp[2] = dfp[1];
		dfp[1] = new;
		*loghead = 1;
		return &dfp[1];
	}
	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[2].length)) {
		dfp[2] = new;
		*loghead = 1;
		return &dfp[2];
	}
	return NULL;
}

 
STATIC void
xfs_dir2_data_freeremove(
	struct xfs_dir2_data_hdr *hdr,		 
	struct xfs_dir2_data_free *bf,		 
	struct xfs_dir2_data_free *dfp,		 
	int			*loghead)	 
{

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));

	 
	if (dfp == &bf[0]) {
		bf[0] = bf[1];
		bf[1] = bf[2];
	}
	 
	else if (dfp == &bf[1])
		bf[1] = bf[2];
	 
	else
		ASSERT(dfp == &bf[2]);
	 
	bf[2].length = 0;
	bf[2].offset = 0;
	*loghead = 1;
}

 
void
xfs_dir2_data_freescan(
	struct xfs_mount		*mp,
	struct xfs_dir2_data_hdr	*hdr,
	int				*loghead)
{
	struct xfs_da_geometry		*geo = mp->m_dir_geo;
	struct xfs_dir2_data_free	*bf = xfs_dir2_data_bestfree_p(mp, hdr);
	void				*addr = hdr;
	unsigned int			offset = geo->data_entry_offset;
	unsigned int			end;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));

	 
	memset(bf, 0, sizeof(*bf) * XFS_DIR2_DATA_FD_COUNT);
	*loghead = 1;

	end = xfs_dir3_data_end_offset(geo, addr);
	while (offset < end) {
		struct xfs_dir2_data_unused	*dup = addr + offset;
		struct xfs_dir2_data_entry	*dep = addr + offset;

		 
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ASSERT(offset ==
			       be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)));
			xfs_dir2_data_freeinsert(hdr, bf, dup, loghead);
			offset += be16_to_cpu(dup->length);
			continue;
		}

		 
		ASSERT(offset ==
		       be16_to_cpu(*xfs_dir2_data_entry_tag_p(mp, dep)));
		offset += xfs_dir2_data_entsize(mp, dep->namelen);
	}
}

 
int						 
xfs_dir3_data_init(
	struct xfs_da_args		*args,	 
	xfs_dir2_db_t			blkno,	 
	struct xfs_buf			**bpp)	 
{
	struct xfs_trans		*tp = args->trans;
	struct xfs_inode		*dp = args->dp;
	struct xfs_mount		*mp = dp->i_mount;
	struct xfs_da_geometry		*geo = args->geo;
	struct xfs_buf			*bp;
	struct xfs_dir2_data_hdr	*hdr;
	struct xfs_dir2_data_unused	*dup;
	struct xfs_dir2_data_free 	*bf;
	int				error;
	int				i;

	 
	error = xfs_da_get_buf(tp, dp, xfs_dir2_db_to_da(args->geo, blkno),
			       &bp, XFS_DATA_FORK);
	if (error)
		return error;
	bp->b_ops = &xfs_dir3_data_buf_ops;
	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_DIR_DATA_BUF);

	 
	hdr = bp->b_addr;
	if (xfs_has_crc(mp)) {
		struct xfs_dir3_blk_hdr *hdr3 = bp->b_addr;

		memset(hdr3, 0, sizeof(*hdr3));
		hdr3->magic = cpu_to_be32(XFS_DIR3_DATA_MAGIC);
		hdr3->blkno = cpu_to_be64(xfs_buf_daddr(bp));
		hdr3->owner = cpu_to_be64(dp->i_ino);
		uuid_copy(&hdr3->uuid, &mp->m_sb.sb_meta_uuid);

	} else
		hdr->magic = cpu_to_be32(XFS_DIR2_DATA_MAGIC);

	bf = xfs_dir2_data_bestfree_p(mp, hdr);
	bf[0].offset = cpu_to_be16(geo->data_entry_offset);
	bf[0].length = cpu_to_be16(geo->blksize - geo->data_entry_offset);
	for (i = 1; i < XFS_DIR2_DATA_FD_COUNT; i++) {
		bf[i].length = 0;
		bf[i].offset = 0;
	}

	 
	dup = bp->b_addr + geo->data_entry_offset;
	dup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
	dup->length = bf[0].length;
	*xfs_dir2_data_unused_tag_p(dup) = cpu_to_be16((char *)dup - (char *)hdr);

	 
	xfs_dir2_data_log_header(args, bp);
	xfs_dir2_data_log_unused(args, bp, dup);
	*bpp = bp;
	return 0;
}

 
void
xfs_dir2_data_log_entry(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp,
	xfs_dir2_data_entry_t	*dep)		 
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_dir2_data_hdr *hdr = bp->b_addr;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));

	xfs_trans_log_buf(args->trans, bp, (uint)((char *)dep - (char *)hdr),
		(uint)((char *)(xfs_dir2_data_entry_tag_p(mp, dep) + 1) -
		       (char *)hdr - 1));
}

 
void
xfs_dir2_data_log_header(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp)
{
#ifdef DEBUG
	struct xfs_dir2_data_hdr *hdr = bp->b_addr;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));
#endif

	xfs_trans_log_buf(args->trans, bp, 0, args->geo->data_entry_offset - 1);
}

 
void
xfs_dir2_data_log_unused(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp,
	xfs_dir2_data_unused_t	*dup)		 
{
	xfs_dir2_data_hdr_t	*hdr = bp->b_addr;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR3_BLOCK_MAGIC));

	 
	xfs_trans_log_buf(args->trans, bp, (uint)((char *)dup - (char *)hdr),
		(uint)((char *)&dup->length + sizeof(dup->length) -
		       1 - (char *)hdr));
	 
	xfs_trans_log_buf(args->trans, bp,
		(uint)((char *)xfs_dir2_data_unused_tag_p(dup) - (char *)hdr),
		(uint)((char *)xfs_dir2_data_unused_tag_p(dup) - (char *)hdr +
		       sizeof(xfs_dir2_data_off_t) - 1));
}

 
void
xfs_dir2_data_make_free(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp,
	xfs_dir2_data_aoff_t	offset,		 
	xfs_dir2_data_aoff_t	len,		 
	int			*needlogp,	 
	int			*needscanp)	 
{
	xfs_dir2_data_hdr_t	*hdr;		 
	xfs_dir2_data_free_t	*dfp;		 
	int			needscan;	 
	xfs_dir2_data_unused_t	*newdup;	 
	xfs_dir2_data_unused_t	*postdup;	 
	xfs_dir2_data_unused_t	*prevdup;	 
	unsigned int		end;
	struct xfs_dir2_data_free *bf;

	hdr = bp->b_addr;

	 
	end = xfs_dir3_data_end_offset(args->geo, hdr);
	ASSERT(end != 0);

	 
	if (offset > args->geo->data_entry_offset) {
		__be16			*tagp;	 

		tagp = (__be16 *)((char *)hdr + offset) - 1;
		prevdup = (xfs_dir2_data_unused_t *)((char *)hdr + be16_to_cpu(*tagp));
		if (be16_to_cpu(prevdup->freetag) != XFS_DIR2_DATA_FREE_TAG)
			prevdup = NULL;
	} else
		prevdup = NULL;
	 
	if (offset + len < end) {
		postdup =
			(xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		if (be16_to_cpu(postdup->freetag) != XFS_DIR2_DATA_FREE_TAG)
			postdup = NULL;
	} else
		postdup = NULL;
	ASSERT(*needscanp == 0);
	needscan = 0;
	 
	bf = xfs_dir2_data_bestfree_p(args->dp->i_mount, hdr);
	if (prevdup && postdup) {
		xfs_dir2_data_free_t	*dfp2;	 

		 
		dfp = xfs_dir2_data_freefind(hdr, bf, prevdup);
		dfp2 = xfs_dir2_data_freefind(hdr, bf, postdup);
		 
		needscan = (bf[2].length != 0);
		 
		be16_add_cpu(&prevdup->length, len + be16_to_cpu(postdup->length));
		*xfs_dir2_data_unused_tag_p(prevdup) =
			cpu_to_be16((char *)prevdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, prevdup);
		if (!needscan) {
			 
			ASSERT(dfp && dfp2);
			if (dfp == &bf[1]) {
				dfp = &bf[0];
				ASSERT(dfp2 == dfp);
				dfp2 = &bf[1];
			}
			xfs_dir2_data_freeremove(hdr, bf, dfp2, needlogp);
			xfs_dir2_data_freeremove(hdr, bf, dfp, needlogp);
			 
			dfp = xfs_dir2_data_freeinsert(hdr, bf, prevdup,
						       needlogp);
			ASSERT(dfp == &bf[0]);
			ASSERT(dfp->length == prevdup->length);
			ASSERT(!dfp[1].length);
			ASSERT(!dfp[2].length);
		}
	}
	 
	else if (prevdup) {
		dfp = xfs_dir2_data_freefind(hdr, bf, prevdup);
		be16_add_cpu(&prevdup->length, len);
		*xfs_dir2_data_unused_tag_p(prevdup) =
			cpu_to_be16((char *)prevdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, prevdup);
		 
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, bf, dfp, needlogp);
			xfs_dir2_data_freeinsert(hdr, bf, prevdup, needlogp);
		}
		 
		else {
			needscan = be16_to_cpu(prevdup->length) >
				   be16_to_cpu(bf[2].length);
		}
	}
	 
	else if (postdup) {
		dfp = xfs_dir2_data_freefind(hdr, bf, postdup);
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(len + be16_to_cpu(postdup->length));
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup);
		 
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, bf, dfp, needlogp);
			xfs_dir2_data_freeinsert(hdr, bf, newdup, needlogp);
		}
		 
		else {
			needscan = be16_to_cpu(newdup->length) >
				   be16_to_cpu(bf[2].length);
		}
	}
	 
	else {
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(len);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup);
		xfs_dir2_data_freeinsert(hdr, bf, newdup, needlogp);
	}
	*needscanp = needscan;
}

 
static inline xfs_failaddr_t
xfs_dir2_data_check_free(
	struct xfs_dir2_data_hdr	*hdr,
	struct xfs_dir2_data_unused	*dup,
	xfs_dir2_data_aoff_t		offset,
	xfs_dir2_data_aoff_t		len)
{
	if (hdr->magic != cpu_to_be32(XFS_DIR2_DATA_MAGIC) &&
	    hdr->magic != cpu_to_be32(XFS_DIR3_DATA_MAGIC) &&
	    hdr->magic != cpu_to_be32(XFS_DIR2_BLOCK_MAGIC) &&
	    hdr->magic != cpu_to_be32(XFS_DIR3_BLOCK_MAGIC))
		return __this_address;
	if (be16_to_cpu(dup->freetag) != XFS_DIR2_DATA_FREE_TAG)
		return __this_address;
	if (offset < (char *)dup - (char *)hdr)
		return __this_address;
	if (offset + len > (char *)dup + be16_to_cpu(dup->length) - (char *)hdr)
		return __this_address;
	if ((char *)dup - (char *)hdr !=
			be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)))
		return __this_address;
	return NULL;
}

 
static inline xfs_failaddr_t
xfs_dir2_data_check_new_free(
	struct xfs_dir2_data_hdr	*hdr,
	struct xfs_dir2_data_free	*dfp,
	struct xfs_dir2_data_unused	*newdup)
{
	if (dfp == NULL)
		return __this_address;
	if (dfp->length != newdup->length)
		return __this_address;
	if (be16_to_cpu(dfp->offset) != (char *)newdup - (char *)hdr)
		return __this_address;
	return NULL;
}

 
int
xfs_dir2_data_use_free(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp,
	xfs_dir2_data_unused_t	*dup,		 
	xfs_dir2_data_aoff_t	offset,		 
	xfs_dir2_data_aoff_t	len,		 
	int			*needlogp,	 
	int			*needscanp)	 
{
	xfs_dir2_data_hdr_t	*hdr;		 
	xfs_dir2_data_free_t	*dfp;		 
	xfs_dir2_data_unused_t	*newdup;	 
	xfs_dir2_data_unused_t	*newdup2;	 
	struct xfs_dir2_data_free *bf;
	xfs_failaddr_t		fa;
	int			matchback;	 
	int			matchfront;	 
	int			needscan;	 
	int			oldlen;		 

	hdr = bp->b_addr;
	fa = xfs_dir2_data_check_free(hdr, dup, offset, len);
	if (fa)
		goto corrupt;
	 
	oldlen = be16_to_cpu(dup->length);
	bf = xfs_dir2_data_bestfree_p(args->dp->i_mount, hdr);
	dfp = xfs_dir2_data_freefind(hdr, bf, dup);
	ASSERT(dfp || oldlen <= be16_to_cpu(bf[2].length));
	 
	matchfront = (char *)dup - (char *)hdr == offset;
	matchback = (char *)dup + oldlen - (char *)hdr == offset + len;
	ASSERT(*needscanp == 0);
	needscan = 0;
	 
	if (matchfront && matchback) {
		if (dfp) {
			needscan = (bf[2].offset != 0);
			if (!needscan)
				xfs_dir2_data_freeremove(hdr, bf, dfp,
							 needlogp);
		}
	}
	 
	else if (matchfront) {
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(oldlen - len);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup);
		 
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, bf, dfp, needlogp);
			dfp = xfs_dir2_data_freeinsert(hdr, bf, newdup,
						       needlogp);
			fa = xfs_dir2_data_check_new_free(hdr, dfp, newdup);
			if (fa)
				goto corrupt;
			 
			needscan = dfp == &bf[2];
		}
	}
	 
	else if (matchback) {
		newdup = dup;
		newdup->length = cpu_to_be16(((char *)hdr + offset) - (char *)newdup);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup);
		 
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, bf, dfp, needlogp);
			dfp = xfs_dir2_data_freeinsert(hdr, bf, newdup,
						       needlogp);
			fa = xfs_dir2_data_check_new_free(hdr, dfp, newdup);
			if (fa)
				goto corrupt;
			 
			needscan = dfp == &bf[2];
		}
	}
	 
	else {
		newdup = dup;
		newdup->length = cpu_to_be16(((char *)hdr + offset) - (char *)newdup);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup);
		newdup2 = (xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		newdup2->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup2->length = cpu_to_be16(oldlen - len - be16_to_cpu(newdup->length));
		*xfs_dir2_data_unused_tag_p(newdup2) =
			cpu_to_be16((char *)newdup2 - (char *)hdr);
		xfs_dir2_data_log_unused(args, bp, newdup2);
		 
		if (dfp) {
			needscan = (bf[2].length != 0);
			if (!needscan) {
				xfs_dir2_data_freeremove(hdr, bf, dfp,
							 needlogp);
				xfs_dir2_data_freeinsert(hdr, bf, newdup,
							 needlogp);
				xfs_dir2_data_freeinsert(hdr, bf, newdup2,
							 needlogp);
			}
		}
	}
	*needscanp = needscan;
	return 0;
corrupt:
	xfs_corruption_error(__func__, XFS_ERRLEVEL_LOW, args->dp->i_mount,
			hdr, sizeof(*hdr), __FILE__, __LINE__, fa);
	return -EFSCORRUPTED;
}

 
unsigned int
xfs_dir3_data_end_offset(
	struct xfs_da_geometry		*geo,
	struct xfs_dir2_data_hdr	*hdr)
{
	void				*p;

	switch (hdr->magic) {
	case cpu_to_be32(XFS_DIR3_BLOCK_MAGIC):
	case cpu_to_be32(XFS_DIR2_BLOCK_MAGIC):
		p = xfs_dir2_block_leaf_p(xfs_dir2_block_tail_p(geo, hdr));
		return p - (void *)hdr;
	case cpu_to_be32(XFS_DIR3_DATA_MAGIC):
	case cpu_to_be32(XFS_DIR2_DATA_MAGIC):
		return geo->blksize;
	default:
		return 0;
	}
}
