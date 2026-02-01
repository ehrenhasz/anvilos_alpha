
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_attr.h"
#include "xfs_attr_remote.h"
#include "xfs_trace.h"
#include "xfs_error.h"

#define ATTR_RMTVALUE_MAPSIZE	1	 

 

 
int
xfs_attr3_rmt_blocks(
	struct xfs_mount *mp,
	int		attrlen)
{
	if (xfs_has_crc(mp)) {
		int buflen = XFS_ATTR3_RMT_BUF_SPACE(mp, mp->m_sb.sb_blocksize);
		return (attrlen + buflen - 1) / buflen;
	}
	return XFS_B_TO_FSB(mp, attrlen);
}

 
static xfs_failaddr_t
xfs_attr3_rmt_hdr_ok(
	void			*ptr,
	xfs_ino_t		ino,
	uint32_t		offset,
	uint32_t		size,
	xfs_daddr_t		bno)
{
	struct xfs_attr3_rmt_hdr *rmt = ptr;

	if (bno != be64_to_cpu(rmt->rm_blkno))
		return __this_address;
	if (offset != be32_to_cpu(rmt->rm_offset))
		return __this_address;
	if (size != be32_to_cpu(rmt->rm_bytes))
		return __this_address;
	if (ino != be64_to_cpu(rmt->rm_owner))
		return __this_address;

	 
	return NULL;
}

static xfs_failaddr_t
xfs_attr3_rmt_verify(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp,
	void			*ptr,
	int			fsbsize,
	xfs_daddr_t		bno)
{
	struct xfs_attr3_rmt_hdr *rmt = ptr;

	if (!xfs_verify_magic(bp, rmt->rm_magic))
		return __this_address;
	if (!uuid_equal(&rmt->rm_uuid, &mp->m_sb.sb_meta_uuid))
		return __this_address;
	if (be64_to_cpu(rmt->rm_blkno) != bno)
		return __this_address;
	if (be32_to_cpu(rmt->rm_bytes) > fsbsize - sizeof(*rmt))
		return __this_address;
	if (be32_to_cpu(rmt->rm_offset) +
				be32_to_cpu(rmt->rm_bytes) > XFS_XATTR_SIZE_MAX)
		return __this_address;
	if (rmt->rm_owner == 0)
		return __this_address;

	return NULL;
}

static int
__xfs_attr3_rmt_read_verify(
	struct xfs_buf	*bp,
	bool		check_crc,
	xfs_failaddr_t	*failaddr)
{
	struct xfs_mount *mp = bp->b_mount;
	char		*ptr;
	int		len;
	xfs_daddr_t	bno;
	int		blksize = mp->m_attr_geo->blksize;

	 
	if (!xfs_has_crc(mp))
		return 0;

	ptr = bp->b_addr;
	bno = xfs_buf_daddr(bp);
	len = BBTOB(bp->b_length);
	ASSERT(len >= blksize);

	while (len > 0) {
		if (check_crc &&
		    !xfs_verify_cksum(ptr, blksize, XFS_ATTR3_RMT_CRC_OFF)) {
			*failaddr = __this_address;
			return -EFSBADCRC;
		}
		*failaddr = xfs_attr3_rmt_verify(mp, bp, ptr, blksize, bno);
		if (*failaddr)
			return -EFSCORRUPTED;
		len -= blksize;
		ptr += blksize;
		bno += BTOBB(blksize);
	}

	if (len != 0) {
		*failaddr = __this_address;
		return -EFSCORRUPTED;
	}

	return 0;
}

static void
xfs_attr3_rmt_read_verify(
	struct xfs_buf	*bp)
{
	xfs_failaddr_t	fa;
	int		error;

	error = __xfs_attr3_rmt_read_verify(bp, true, &fa);
	if (error)
		xfs_verifier_error(bp, error, fa);
}

static xfs_failaddr_t
xfs_attr3_rmt_verify_struct(
	struct xfs_buf	*bp)
{
	xfs_failaddr_t	fa;
	int		error;

	error = __xfs_attr3_rmt_read_verify(bp, false, &fa);
	return error ? fa : NULL;
}

static void
xfs_attr3_rmt_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_mount;
	xfs_failaddr_t	fa;
	int		blksize = mp->m_attr_geo->blksize;
	char		*ptr;
	int		len;
	xfs_daddr_t	bno;

	 
	if (!xfs_has_crc(mp))
		return;

	ptr = bp->b_addr;
	bno = xfs_buf_daddr(bp);
	len = BBTOB(bp->b_length);
	ASSERT(len >= blksize);

	while (len > 0) {
		struct xfs_attr3_rmt_hdr *rmt = (struct xfs_attr3_rmt_hdr *)ptr;

		fa = xfs_attr3_rmt_verify(mp, bp, ptr, blksize, bno);
		if (fa) {
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
			return;
		}

		 
		if (rmt->rm_lsn != cpu_to_be64(NULLCOMMITLSN)) {
			xfs_verifier_error(bp, -EFSCORRUPTED, __this_address);
			return;
		}
		xfs_update_cksum(ptr, blksize, XFS_ATTR3_RMT_CRC_OFF);

		len -= blksize;
		ptr += blksize;
		bno += BTOBB(blksize);
	}

	if (len != 0)
		xfs_verifier_error(bp, -EFSCORRUPTED, __this_address);
}

const struct xfs_buf_ops xfs_attr3_rmt_buf_ops = {
	.name = "xfs_attr3_rmt",
	.magic = { 0, cpu_to_be32(XFS_ATTR3_RMT_MAGIC) },
	.verify_read = xfs_attr3_rmt_read_verify,
	.verify_write = xfs_attr3_rmt_write_verify,
	.verify_struct = xfs_attr3_rmt_verify_struct,
};

STATIC int
xfs_attr3_rmt_hdr_set(
	struct xfs_mount	*mp,
	void			*ptr,
	xfs_ino_t		ino,
	uint32_t		offset,
	uint32_t		size,
	xfs_daddr_t		bno)
{
	struct xfs_attr3_rmt_hdr *rmt = ptr;

	if (!xfs_has_crc(mp))
		return 0;

	rmt->rm_magic = cpu_to_be32(XFS_ATTR3_RMT_MAGIC);
	rmt->rm_offset = cpu_to_be32(offset);
	rmt->rm_bytes = cpu_to_be32(size);
	uuid_copy(&rmt->rm_uuid, &mp->m_sb.sb_meta_uuid);
	rmt->rm_owner = cpu_to_be64(ino);
	rmt->rm_blkno = cpu_to_be64(bno);

	 
	rmt->rm_lsn = cpu_to_be64(NULLCOMMITLSN);

	return sizeof(struct xfs_attr3_rmt_hdr);
}

 
STATIC int
xfs_attr_rmtval_copyout(
	struct xfs_mount *mp,
	struct xfs_buf	*bp,
	xfs_ino_t	ino,
	int		*offset,
	int		*valuelen,
	uint8_t		**dst)
{
	char		*src = bp->b_addr;
	xfs_daddr_t	bno = xfs_buf_daddr(bp);
	int		len = BBTOB(bp->b_length);
	int		blksize = mp->m_attr_geo->blksize;

	ASSERT(len >= blksize);

	while (len > 0 && *valuelen > 0) {
		int hdr_size = 0;
		int byte_cnt = XFS_ATTR3_RMT_BUF_SPACE(mp, blksize);

		byte_cnt = min(*valuelen, byte_cnt);

		if (xfs_has_crc(mp)) {
			if (xfs_attr3_rmt_hdr_ok(src, ino, *offset,
						  byte_cnt, bno)) {
				xfs_alert(mp,
"remote attribute header mismatch bno/off/len/owner (0x%llx/0x%x/Ox%x/0x%llx)",
					bno, *offset, byte_cnt, ino);
				return -EFSCORRUPTED;
			}
			hdr_size = sizeof(struct xfs_attr3_rmt_hdr);
		}

		memcpy(*dst, src + hdr_size, byte_cnt);

		 
		len -= blksize;
		src += blksize;
		bno += BTOBB(blksize);

		 
		*valuelen -= byte_cnt;
		*dst += byte_cnt;
		*offset += byte_cnt;
	}
	return 0;
}

STATIC void
xfs_attr_rmtval_copyin(
	struct xfs_mount *mp,
	struct xfs_buf	*bp,
	xfs_ino_t	ino,
	int		*offset,
	int		*valuelen,
	uint8_t		**src)
{
	char		*dst = bp->b_addr;
	xfs_daddr_t	bno = xfs_buf_daddr(bp);
	int		len = BBTOB(bp->b_length);
	int		blksize = mp->m_attr_geo->blksize;

	ASSERT(len >= blksize);

	while (len > 0 && *valuelen > 0) {
		int hdr_size;
		int byte_cnt = XFS_ATTR3_RMT_BUF_SPACE(mp, blksize);

		byte_cnt = min(*valuelen, byte_cnt);
		hdr_size = xfs_attr3_rmt_hdr_set(mp, dst, ino, *offset,
						 byte_cnt, bno);

		memcpy(dst + hdr_size, *src, byte_cnt);

		 
		if (byte_cnt + hdr_size < blksize) {
			ASSERT(*valuelen - byte_cnt == 0);
			ASSERT(len == blksize);
			memset(dst + hdr_size + byte_cnt, 0,
					blksize - hdr_size - byte_cnt);
		}

		 
		len -= blksize;
		dst += blksize;
		bno += BTOBB(blksize);

		 
		*valuelen -= byte_cnt;
		*src += byte_cnt;
		*offset += byte_cnt;
	}
}

 
int
xfs_attr_rmtval_get(
	struct xfs_da_args	*args)
{
	struct xfs_bmbt_irec	map[ATTR_RMTVALUE_MAPSIZE];
	struct xfs_mount	*mp = args->dp->i_mount;
	struct xfs_buf		*bp;
	xfs_dablk_t		lblkno = args->rmtblkno;
	uint8_t			*dst = args->value;
	int			valuelen;
	int			nmap;
	int			error;
	int			blkcnt = args->rmtblkcnt;
	int			i;
	int			offset = 0;

	trace_xfs_attr_rmtval_get(args);

	ASSERT(args->valuelen != 0);
	ASSERT(args->rmtvaluelen == args->valuelen);

	valuelen = args->rmtvaluelen;
	while (valuelen > 0) {
		nmap = ATTR_RMTVALUE_MAPSIZE;
		error = xfs_bmapi_read(args->dp, (xfs_fileoff_t)lblkno,
				       blkcnt, map, &nmap,
				       XFS_BMAPI_ATTRFORK);
		if (error)
			return error;
		ASSERT(nmap >= 1);

		for (i = 0; (i < nmap) && (valuelen > 0); i++) {
			xfs_daddr_t	dblkno;
			int		dblkcnt;

			ASSERT((map[i].br_startblock != DELAYSTARTBLOCK) &&
			       (map[i].br_startblock != HOLESTARTBLOCK));
			dblkno = XFS_FSB_TO_DADDR(mp, map[i].br_startblock);
			dblkcnt = XFS_FSB_TO_BB(mp, map[i].br_blockcount);
			error = xfs_buf_read(mp->m_ddev_targp, dblkno, dblkcnt,
					0, &bp, &xfs_attr3_rmt_buf_ops);
			if (error)
				return error;

			error = xfs_attr_rmtval_copyout(mp, bp, args->dp->i_ino,
							&offset, &valuelen,
							&dst);
			xfs_buf_relse(bp);
			if (error)
				return error;

			 
			lblkno += map[i].br_blockcount;
			blkcnt -= map[i].br_blockcount;
		}
	}
	ASSERT(valuelen == 0);
	return 0;
}

 
int
xfs_attr_rmt_find_hole(
	struct xfs_da_args	*args)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			error;
	int			blkcnt;
	xfs_fileoff_t		lfileoff = 0;

	 
	blkcnt = xfs_attr3_rmt_blocks(mp, args->rmtvaluelen);
	error = xfs_bmap_first_unused(args->trans, args->dp, blkcnt, &lfileoff,
						   XFS_ATTR_FORK);
	if (error)
		return error;

	args->rmtblkno = (xfs_dablk_t)lfileoff;
	args->rmtblkcnt = blkcnt;

	return 0;
}

int
xfs_attr_rmtval_set_value(
	struct xfs_da_args	*args)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_bmbt_irec	map;
	xfs_dablk_t		lblkno;
	uint8_t			*src = args->value;
	int			blkcnt;
	int			valuelen;
	int			nmap;
	int			error;
	int			offset = 0;

	 
	lblkno = args->rmtblkno;
	blkcnt = args->rmtblkcnt;
	valuelen = args->rmtvaluelen;
	while (valuelen > 0) {
		struct xfs_buf	*bp;
		xfs_daddr_t	dblkno;
		int		dblkcnt;

		ASSERT(blkcnt > 0);

		nmap = 1;
		error = xfs_bmapi_read(dp, (xfs_fileoff_t)lblkno,
				       blkcnt, &map, &nmap,
				       XFS_BMAPI_ATTRFORK);
		if (error)
			return error;
		ASSERT(nmap == 1);
		ASSERT((map.br_startblock != DELAYSTARTBLOCK) &&
		       (map.br_startblock != HOLESTARTBLOCK));

		dblkno = XFS_FSB_TO_DADDR(mp, map.br_startblock),
		dblkcnt = XFS_FSB_TO_BB(mp, map.br_blockcount);

		error = xfs_buf_get(mp->m_ddev_targp, dblkno, dblkcnt, &bp);
		if (error)
			return error;
		bp->b_ops = &xfs_attr3_rmt_buf_ops;

		xfs_attr_rmtval_copyin(mp, bp, args->dp->i_ino, &offset,
				       &valuelen, &src);

		error = xfs_bwrite(bp);	 
		xfs_buf_relse(bp);
		if (error)
			return error;


		 
		lblkno += map.br_blockcount;
		blkcnt -= map.br_blockcount;
	}
	ASSERT(valuelen == 0);
	return 0;
}

 
int
xfs_attr_rmtval_stale(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*map,
	xfs_buf_flags_t		incore_flags)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_buf		*bp;
	int			error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	if (XFS_IS_CORRUPT(mp, map->br_startblock == DELAYSTARTBLOCK) ||
	    XFS_IS_CORRUPT(mp, map->br_startblock == HOLESTARTBLOCK))
		return -EFSCORRUPTED;

	error = xfs_buf_incore(mp->m_ddev_targp,
			XFS_FSB_TO_DADDR(mp, map->br_startblock),
			XFS_FSB_TO_BB(mp, map->br_blockcount),
			incore_flags, &bp);
	if (error) {
		if (error == -ENOENT)
			return 0;
		return error;
	}

	xfs_buf_stale(bp);
	xfs_buf_relse(bp);
	return 0;
}

 
int
xfs_attr_rmtval_find_space(
	struct xfs_attr_intent		*attr)
{
	struct xfs_da_args		*args = attr->xattri_da_args;
	struct xfs_bmbt_irec		*map = &attr->xattri_map;
	int				error;

	attr->xattri_lblkno = 0;
	attr->xattri_blkcnt = 0;
	args->rmtblkcnt = 0;
	args->rmtblkno = 0;
	memset(map, 0, sizeof(struct xfs_bmbt_irec));

	error = xfs_attr_rmt_find_hole(args);
	if (error)
		return error;

	attr->xattri_blkcnt = args->rmtblkcnt;
	attr->xattri_lblkno = args->rmtblkno;

	return 0;
}

 
int
xfs_attr_rmtval_set_blk(
	struct xfs_attr_intent		*attr)
{
	struct xfs_da_args		*args = attr->xattri_da_args;
	struct xfs_inode		*dp = args->dp;
	struct xfs_bmbt_irec		*map = &attr->xattri_map;
	int nmap;
	int error;

	nmap = 1;
	error = xfs_bmapi_write(args->trans, dp,
			(xfs_fileoff_t)attr->xattri_lblkno,
			attr->xattri_blkcnt, XFS_BMAPI_ATTRFORK, args->total,
			map, &nmap);
	if (error)
		return error;

	ASSERT(nmap == 1);
	ASSERT((map->br_startblock != DELAYSTARTBLOCK) &&
	       (map->br_startblock != HOLESTARTBLOCK));

	 
	attr->xattri_lblkno += map->br_blockcount;
	attr->xattri_blkcnt -= map->br_blockcount;

	return 0;
}

 
int
xfs_attr_rmtval_invalidate(
	struct xfs_da_args	*args)
{
	xfs_dablk_t		lblkno;
	int			blkcnt;
	int			error;

	 
	lblkno = args->rmtblkno;
	blkcnt = args->rmtblkcnt;
	while (blkcnt > 0) {
		struct xfs_bmbt_irec	map;
		int			nmap;

		 
		nmap = 1;
		error = xfs_bmapi_read(args->dp, (xfs_fileoff_t)lblkno,
				       blkcnt, &map, &nmap, XFS_BMAPI_ATTRFORK);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(args->dp->i_mount, nmap != 1))
			return -EFSCORRUPTED;
		error = xfs_attr_rmtval_stale(args->dp, &map, XBF_TRYLOCK);
		if (error)
			return error;

		lblkno += map.br_blockcount;
		blkcnt -= map.br_blockcount;
	}
	return 0;
}

 
int
xfs_attr_rmtval_remove(
	struct xfs_attr_intent		*attr)
{
	struct xfs_da_args		*args = attr->xattri_da_args;
	int				error, done;

	 
	error = xfs_bunmapi(args->trans, args->dp, args->rmtblkno,
			    args->rmtblkcnt, XFS_BMAPI_ATTRFORK, 1, &done);
	if (error)
		return error;

	 
	if (!done) {
		trace_xfs_attr_rmtval_remove_return(attr->xattri_dela_state,
						    args->dp);
		return -EAGAIN;
	}

	args->rmtblkno = 0;
	args->rmtblkcnt = 0;
	return 0;
}
