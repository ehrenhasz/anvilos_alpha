
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_quota.h"
#include "xfs_trans.h"
#include "xfs_qm.h"
#include "xfs_error.h"

int
xfs_calc_dquots_per_chunk(
	unsigned int		nbblks)	 
{
	ASSERT(nbblks > 0);
	return BBTOB(nbblks) / sizeof(struct xfs_dqblk);
}

 

xfs_failaddr_t
xfs_dquot_verify(
	struct xfs_mount	*mp,
	struct xfs_disk_dquot	*ddq,
	xfs_dqid_t		id)	 
{
	__u8			ddq_type;

	 
	if (ddq->d_magic != cpu_to_be16(XFS_DQUOT_MAGIC))
		return __this_address;
	if (ddq->d_version != XFS_DQUOT_VERSION)
		return __this_address;

	if (ddq->d_type & ~XFS_DQTYPE_ANY)
		return __this_address;
	ddq_type = ddq->d_type & XFS_DQTYPE_REC_MASK;
	if (ddq_type != XFS_DQTYPE_USER &&
	    ddq_type != XFS_DQTYPE_PROJ &&
	    ddq_type != XFS_DQTYPE_GROUP)
		return __this_address;

	if ((ddq->d_type & XFS_DQTYPE_BIGTIME) &&
	    !xfs_has_bigtime(mp))
		return __this_address;

	if ((ddq->d_type & XFS_DQTYPE_BIGTIME) && !ddq->d_id)
		return __this_address;

	if (id != -1 && id != be32_to_cpu(ddq->d_id))
		return __this_address;

	if (!ddq->d_id)
		return NULL;

	if (ddq->d_blk_softlimit &&
	    be64_to_cpu(ddq->d_bcount) > be64_to_cpu(ddq->d_blk_softlimit) &&
	    !ddq->d_btimer)
		return __this_address;

	if (ddq->d_ino_softlimit &&
	    be64_to_cpu(ddq->d_icount) > be64_to_cpu(ddq->d_ino_softlimit) &&
	    !ddq->d_itimer)
		return __this_address;

	if (ddq->d_rtb_softlimit &&
	    be64_to_cpu(ddq->d_rtbcount) > be64_to_cpu(ddq->d_rtb_softlimit) &&
	    !ddq->d_rtbtimer)
		return __this_address;

	return NULL;
}

xfs_failaddr_t
xfs_dqblk_verify(
	struct xfs_mount	*mp,
	struct xfs_dqblk	*dqb,
	xfs_dqid_t		id)	 
{
	if (xfs_has_crc(mp) &&
	    !uuid_equal(&dqb->dd_uuid, &mp->m_sb.sb_meta_uuid))
		return __this_address;

	return xfs_dquot_verify(mp, &dqb->dd_diskdq, id);
}

 
void
xfs_dqblk_repair(
	struct xfs_mount	*mp,
	struct xfs_dqblk	*dqb,
	xfs_dqid_t		id,
	xfs_dqtype_t		type)
{
	 
	ASSERT(id != -1);
	memset(dqb, 0, sizeof(struct xfs_dqblk));

	dqb->dd_diskdq.d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
	dqb->dd_diskdq.d_version = XFS_DQUOT_VERSION;
	dqb->dd_diskdq.d_type = type;
	dqb->dd_diskdq.d_id = cpu_to_be32(id);

	if (xfs_has_crc(mp)) {
		uuid_copy(&dqb->dd_uuid, &mp->m_sb.sb_meta_uuid);
		xfs_update_cksum((char *)dqb, sizeof(struct xfs_dqblk),
				 XFS_DQUOT_CRC_OFF);
	}
}

STATIC bool
xfs_dquot_buf_verify_crc(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp,
	bool			readahead)
{
	struct xfs_dqblk	*d = (struct xfs_dqblk *)bp->b_addr;
	int			ndquots;
	int			i;

	if (!xfs_has_crc(mp))
		return true;

	 
	if (mp->m_quotainfo)
		ndquots = mp->m_quotainfo->qi_dqperchunk;
	else
		ndquots = xfs_calc_dquots_per_chunk(bp->b_length);

	for (i = 0; i < ndquots; i++, d++) {
		if (!xfs_verify_cksum((char *)d, sizeof(struct xfs_dqblk),
				 XFS_DQUOT_CRC_OFF)) {
			if (!readahead)
				xfs_buf_verifier_error(bp, -EFSBADCRC, __func__,
					d, sizeof(*d), __this_address);
			return false;
		}
	}
	return true;
}

STATIC xfs_failaddr_t
xfs_dquot_buf_verify(
	struct xfs_mount	*mp,
	struct xfs_buf		*bp,
	bool			readahead)
{
	struct xfs_dqblk	*dqb = bp->b_addr;
	xfs_failaddr_t		fa;
	xfs_dqid_t		id = 0;
	int			ndquots;
	int			i;

	 
	if (mp->m_quotainfo)
		ndquots = mp->m_quotainfo->qi_dqperchunk;
	else
		ndquots = xfs_calc_dquots_per_chunk(bp->b_length);

	 
	for (i = 0; i < ndquots; i++) {
		struct xfs_disk_dquot	*ddq;

		ddq = &dqb[i].dd_diskdq;

		if (i == 0)
			id = be32_to_cpu(ddq->d_id);

		fa = xfs_dqblk_verify(mp, &dqb[i], id + i);
		if (fa) {
			if (!readahead)
				xfs_buf_verifier_error(bp, -EFSCORRUPTED,
					__func__, &dqb[i],
					sizeof(struct xfs_dqblk), fa);
			return fa;
		}
	}

	return NULL;
}

static xfs_failaddr_t
xfs_dquot_buf_verify_struct(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;

	return xfs_dquot_buf_verify(mp, bp, false);
}

static void
xfs_dquot_buf_read_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;

	if (!xfs_dquot_buf_verify_crc(mp, bp, false))
		return;
	xfs_dquot_buf_verify(mp, bp, false);
}

 
static void
xfs_dquot_buf_readahead_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;

	if (!xfs_dquot_buf_verify_crc(mp, bp, true) ||
	    xfs_dquot_buf_verify(mp, bp, true) != NULL) {
		xfs_buf_ioerror(bp, -EIO);
		bp->b_flags &= ~XBF_DONE;
	}
}

 
static void
xfs_dquot_buf_write_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;

	xfs_dquot_buf_verify(mp, bp, false);
}

const struct xfs_buf_ops xfs_dquot_buf_ops = {
	.name = "xfs_dquot",
	.magic16 = { cpu_to_be16(XFS_DQUOT_MAGIC),
		     cpu_to_be16(XFS_DQUOT_MAGIC) },
	.verify_read = xfs_dquot_buf_read_verify,
	.verify_write = xfs_dquot_buf_write_verify,
	.verify_struct = xfs_dquot_buf_verify_struct,
};

const struct xfs_buf_ops xfs_dquot_buf_ra_ops = {
	.name = "xfs_dquot_ra",
	.magic16 = { cpu_to_be16(XFS_DQUOT_MAGIC),
		     cpu_to_be16(XFS_DQUOT_MAGIC) },
	.verify_read = xfs_dquot_buf_readahead_verify,
	.verify_write = xfs_dquot_buf_write_verify,
};

 
time64_t
xfs_dquot_from_disk_ts(
	struct xfs_disk_dquot	*ddq,
	__be32			dtimer)
{
	uint32_t		t = be32_to_cpu(dtimer);

	if (t != 0 && (ddq->d_type & XFS_DQTYPE_BIGTIME))
		return xfs_dq_bigtime_to_unix(t);

	return t;
}

 
__be32
xfs_dquot_to_disk_ts(
	struct xfs_dquot	*dqp,
	time64_t		timer)
{
	uint32_t		t = timer;

	if (timer != 0 && (dqp->q_type & XFS_DQTYPE_BIGTIME))
		t = xfs_dq_unix_to_bigtime(timer);

	return cpu_to_be32(t);
}
