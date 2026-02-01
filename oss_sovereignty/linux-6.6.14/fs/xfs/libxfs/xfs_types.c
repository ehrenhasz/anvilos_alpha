
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_shared.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_ag.h"


 
static inline bool
xfs_verify_agno_agbno(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		agbno)
{
	xfs_agblock_t		eoag;

	eoag = xfs_ag_block_count(mp, agno);
	if (agbno >= eoag)
		return false;
	if (agbno <= XFS_AGFL_BLOCK(mp))
		return false;
	return true;
}

 
inline bool
xfs_verify_fsbno(
	struct xfs_mount	*mp,
	xfs_fsblock_t		fsbno)
{
	xfs_agnumber_t		agno = XFS_FSB_TO_AGNO(mp, fsbno);

	if (agno >= mp->m_sb.sb_agcount)
		return false;
	return xfs_verify_agno_agbno(mp, agno, XFS_FSB_TO_AGBNO(mp, fsbno));
}

 
bool
xfs_verify_fsbext(
	struct xfs_mount	*mp,
	xfs_fsblock_t		fsbno,
	xfs_fsblock_t		len)
{
	if (fsbno + len <= fsbno)
		return false;

	if (!xfs_verify_fsbno(mp, fsbno))
		return false;

	if (!xfs_verify_fsbno(mp, fsbno + len - 1))
		return false;

	return  XFS_FSB_TO_AGNO(mp, fsbno) ==
		XFS_FSB_TO_AGNO(mp, fsbno + len - 1);
}

 
static inline bool
xfs_verify_agno_agino(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno,
	xfs_agino_t		agino)
{
	xfs_agino_t		first;
	xfs_agino_t		last;

	xfs_agino_range(mp, agno, &first, &last);
	return agino >= first && agino <= last;
}

 
inline bool
xfs_verify_ino(
	struct xfs_mount	*mp,
	xfs_ino_t		ino)
{
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, ino);
	xfs_agino_t		agino = XFS_INO_TO_AGINO(mp, ino);

	if (agno >= mp->m_sb.sb_agcount)
		return false;
	if (XFS_AGINO_TO_INO(mp, agno, agino) != ino)
		return false;
	return xfs_verify_agno_agino(mp, agno, agino);
}

 
inline bool
xfs_internal_inum(
	struct xfs_mount	*mp,
	xfs_ino_t		ino)
{
	return ino == mp->m_sb.sb_rbmino || ino == mp->m_sb.sb_rsumino ||
		(xfs_has_quota(mp) &&
		 xfs_is_quota_inode(&mp->m_sb, ino));
}

 
bool
xfs_verify_dir_ino(
	struct xfs_mount	*mp,
	xfs_ino_t		ino)
{
	if (xfs_internal_inum(mp, ino))
		return false;
	return xfs_verify_ino(mp, ino);
}

 
inline bool
xfs_verify_rtbno(
	struct xfs_mount	*mp,
	xfs_rtblock_t		rtbno)
{
	return rtbno < mp->m_sb.sb_rblocks;
}

 
bool
xfs_verify_rtext(
	struct xfs_mount	*mp,
	xfs_rtblock_t		rtbno,
	xfs_rtblock_t		len)
{
	if (rtbno + len <= rtbno)
		return false;

	if (!xfs_verify_rtbno(mp, rtbno))
		return false;

	return xfs_verify_rtbno(mp, rtbno + len - 1);
}

 
inline void
xfs_icount_range(
	struct xfs_mount	*mp,
	unsigned long long	*min,
	unsigned long long	*max)
{
	unsigned long long	nr_inos = 0;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;

	 
	*min = XFS_INODES_PER_CHUNK;

	for_each_perag(mp, agno, pag)
		nr_inos += pag->agino_max - pag->agino_min + 1;
	*max = nr_inos;
}

 
bool
xfs_verify_icount(
	struct xfs_mount	*mp,
	unsigned long long	icount)
{
	unsigned long long	min, max;

	xfs_icount_range(mp, &min, &max);
	return icount >= min && icount <= max;
}

 
bool
xfs_verify_dablk(
	struct xfs_mount	*mp,
	xfs_fileoff_t		dabno)
{
	xfs_dablk_t		max_dablk = -1U;

	return dabno <= max_dablk;
}

 
bool
xfs_verify_fileoff(
	struct xfs_mount	*mp,
	xfs_fileoff_t		off)
{
	return off <= XFS_MAX_FILEOFF;
}

 
bool
xfs_verify_fileext(
	struct xfs_mount	*mp,
	xfs_fileoff_t		off,
	xfs_fileoff_t		len)
{
	if (off + len <= off)
		return false;

	if (!xfs_verify_fileoff(mp, off))
		return false;

	return xfs_verify_fileoff(mp, off + len - 1);
}
