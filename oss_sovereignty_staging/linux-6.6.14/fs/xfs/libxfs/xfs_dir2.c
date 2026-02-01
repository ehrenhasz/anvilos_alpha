
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trace.h"

const struct xfs_name xfs_name_dotdot = {
	.name	= (const unsigned char *)"..",
	.len	= 2,
	.type	= XFS_DIR3_FT_DIR,
};

 
unsigned char
xfs_mode_to_ftype(
	int		mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
		return XFS_DIR3_FT_REG_FILE;
	case S_IFDIR:
		return XFS_DIR3_FT_DIR;
	case S_IFCHR:
		return XFS_DIR3_FT_CHRDEV;
	case S_IFBLK:
		return XFS_DIR3_FT_BLKDEV;
	case S_IFIFO:
		return XFS_DIR3_FT_FIFO;
	case S_IFSOCK:
		return XFS_DIR3_FT_SOCK;
	case S_IFLNK:
		return XFS_DIR3_FT_SYMLINK;
	default:
		return XFS_DIR3_FT_UNKNOWN;
	}
}

 
xfs_dahash_t
xfs_ascii_ci_hashname(
	const struct xfs_name	*name)
{
	xfs_dahash_t		hash;
	int			i;

	for (i = 0, hash = 0; i < name->len; i++)
		hash = xfs_ascii_ci_xfrm(name->name[i]) ^ rol32(hash, 7);

	return hash;
}

enum xfs_dacmp
xfs_ascii_ci_compname(
	struct xfs_da_args	*args,
	const unsigned char	*name,
	int			len)
{
	enum xfs_dacmp		result;
	int			i;

	if (args->namelen != len)
		return XFS_CMP_DIFFERENT;

	result = XFS_CMP_EXACT;
	for (i = 0; i < len; i++) {
		if (args->name[i] == name[i])
			continue;
		if (xfs_ascii_ci_xfrm(args->name[i]) !=
		    xfs_ascii_ci_xfrm(name[i]))
			return XFS_CMP_DIFFERENT;
		result = XFS_CMP_CASE;
	}

	return result;
}

int
xfs_da_mount(
	struct xfs_mount	*mp)
{
	struct xfs_da_geometry	*dageo;


	ASSERT(mp->m_sb.sb_versionnum & XFS_SB_VERSION_DIRV2BIT);
	ASSERT(xfs_dir2_dirblock_bytes(&mp->m_sb) <= XFS_MAX_BLOCKSIZE);

	mp->m_dir_geo = kmem_zalloc(sizeof(struct xfs_da_geometry),
				    KM_MAYFAIL);
	mp->m_attr_geo = kmem_zalloc(sizeof(struct xfs_da_geometry),
				     KM_MAYFAIL);
	if (!mp->m_dir_geo || !mp->m_attr_geo) {
		kmem_free(mp->m_dir_geo);
		kmem_free(mp->m_attr_geo);
		return -ENOMEM;
	}

	 
	dageo = mp->m_dir_geo;
	dageo->blklog = mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog;
	dageo->fsblog = mp->m_sb.sb_blocklog;
	dageo->blksize = xfs_dir2_dirblock_bytes(&mp->m_sb);
	dageo->fsbcount = 1 << mp->m_sb.sb_dirblklog;
	if (xfs_has_crc(mp)) {
		dageo->node_hdr_size = sizeof(struct xfs_da3_node_hdr);
		dageo->leaf_hdr_size = sizeof(struct xfs_dir3_leaf_hdr);
		dageo->free_hdr_size = sizeof(struct xfs_dir3_free_hdr);
		dageo->data_entry_offset =
				sizeof(struct xfs_dir3_data_hdr);
	} else {
		dageo->node_hdr_size = sizeof(struct xfs_da_node_hdr);
		dageo->leaf_hdr_size = sizeof(struct xfs_dir2_leaf_hdr);
		dageo->free_hdr_size = sizeof(struct xfs_dir2_free_hdr);
		dageo->data_entry_offset =
				sizeof(struct xfs_dir2_data_hdr);
	}
	dageo->leaf_max_ents = (dageo->blksize - dageo->leaf_hdr_size) /
			sizeof(struct xfs_dir2_leaf_entry);
	dageo->free_max_bests = (dageo->blksize - dageo->free_hdr_size) /
			sizeof(xfs_dir2_data_off_t);

	dageo->data_first_offset = dageo->data_entry_offset +
			xfs_dir2_data_entsize(mp, 1) +
			xfs_dir2_data_entsize(mp, 2);

	 
	dageo->datablk = xfs_dir2_byte_to_da(dageo, XFS_DIR2_DATA_OFFSET);
	dageo->leafblk = xfs_dir2_byte_to_da(dageo, XFS_DIR2_LEAF_OFFSET);
	dageo->freeblk = xfs_dir2_byte_to_da(dageo, XFS_DIR2_FREE_OFFSET);
	dageo->node_ents = (dageo->blksize - dageo->node_hdr_size) /
				(uint)sizeof(xfs_da_node_entry_t);
	dageo->max_extents = (XFS_DIR2_MAX_SPACES * XFS_DIR2_SPACE_SIZE) >>
					mp->m_sb.sb_blocklog;
	dageo->magicpct = (dageo->blksize * 37) / 100;

	 
	dageo = mp->m_attr_geo;
	dageo->blklog = mp->m_sb.sb_blocklog;
	dageo->fsblog = mp->m_sb.sb_blocklog;
	dageo->blksize = 1 << dageo->blklog;
	dageo->fsbcount = 1;
	dageo->node_hdr_size = mp->m_dir_geo->node_hdr_size;
	dageo->node_ents = (dageo->blksize - dageo->node_hdr_size) /
				(uint)sizeof(xfs_da_node_entry_t);

	if (xfs_has_large_extent_counts(mp))
		dageo->max_extents = XFS_MAX_EXTCNT_ATTR_FORK_LARGE;
	else
		dageo->max_extents = XFS_MAX_EXTCNT_ATTR_FORK_SMALL;

	dageo->magicpct = (dageo->blksize * 37) / 100;
	return 0;
}

void
xfs_da_unmount(
	struct xfs_mount	*mp)
{
	kmem_free(mp->m_dir_geo);
	kmem_free(mp->m_attr_geo);
}

 
int
xfs_dir_isempty(
	xfs_inode_t	*dp)
{
	xfs_dir2_sf_hdr_t	*sfp;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));
	if (dp->i_disk_size == 0)	 
		return 1;
	if (dp->i_disk_size > xfs_inode_data_fork_size(dp))
		return 0;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	return !sfp->count;
}

 
int
xfs_dir_ino_validate(
	xfs_mount_t	*mp,
	xfs_ino_t	ino)
{
	bool		ino_ok = xfs_verify_dir_ino(mp, ino);

	if (XFS_IS_CORRUPT(mp, !ino_ok) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_DIR_INO_VALIDATE)) {
		xfs_warn(mp, "Invalid inode number 0x%Lx",
				(unsigned long long) ino);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
int
xfs_dir_init(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	xfs_inode_t	*pdp)
{
	struct xfs_da_args *args;
	int		error;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));
	error = xfs_dir_ino_validate(tp->t_mountp, pdp->i_ino);
	if (error)
		return error;

	args = kmem_zalloc(sizeof(*args), KM_NOFS);
	if (!args)
		return -ENOMEM;

	args->geo = dp->i_mount->m_dir_geo;
	args->dp = dp;
	args->trans = tp;
	error = xfs_dir2_sf_create(args, pdp->i_ino);
	kmem_free(args);
	return error;
}

 
int
xfs_dir_createname(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	const struct xfs_name	*name,
	xfs_ino_t		inum,		 
	xfs_extlen_t		total)		 
{
	struct xfs_da_args	*args;
	int			rval;
	bool			v;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));

	if (inum) {
		rval = xfs_dir_ino_validate(tp->t_mountp, inum);
		if (rval)
			return rval;
		XFS_STATS_INC(dp->i_mount, xs_dir_create);
	}

	args = kmem_zalloc(sizeof(*args), KM_NOFS);
	if (!args)
		return -ENOMEM;

	args->geo = dp->i_mount->m_dir_geo;
	args->name = name->name;
	args->namelen = name->len;
	args->filetype = name->type;
	args->hashval = xfs_dir2_hashname(dp->i_mount, name);
	args->inumber = inum;
	args->dp = dp;
	args->total = total;
	args->whichfork = XFS_DATA_FORK;
	args->trans = tp;
	args->op_flags = XFS_DA_OP_ADDNAME | XFS_DA_OP_OKNOENT;
	if (!inum)
		args->op_flags |= XFS_DA_OP_JUSTCHECK;

	if (dp->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		rval = xfs_dir2_sf_addname(args);
		goto out_free;
	}

	rval = xfs_dir2_isblock(args, &v);
	if (rval)
		goto out_free;
	if (v) {
		rval = xfs_dir2_block_addname(args);
		goto out_free;
	}

	rval = xfs_dir2_isleaf(args, &v);
	if (rval)
		goto out_free;
	if (v)
		rval = xfs_dir2_leaf_addname(args);
	else
		rval = xfs_dir2_node_addname(args);

out_free:
	kmem_free(args);
	return rval;
}

 
int
xfs_dir_cilookup_result(
	struct xfs_da_args *args,
	const unsigned char *name,
	int		len)
{
	if (args->cmpresult == XFS_CMP_DIFFERENT)
		return -ENOENT;
	if (args->cmpresult != XFS_CMP_CASE ||
					!(args->op_flags & XFS_DA_OP_CILOOKUP))
		return -EEXIST;

	args->value = kmem_alloc(len, KM_NOFS | KM_MAYFAIL);
	if (!args->value)
		return -ENOMEM;

	memcpy(args->value, name, len);
	args->valuelen = len;
	return -EEXIST;
}

 

int
xfs_dir_lookup(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	const struct xfs_name	*name,
	xfs_ino_t		*inum,	   
	struct xfs_name		*ci_name)  
{
	struct xfs_da_args	*args;
	int			rval;
	bool			v;
	int			lock_mode;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));
	XFS_STATS_INC(dp->i_mount, xs_dir_lookup);

	 
	args = kmem_zalloc(sizeof(*args), KM_NOFS);
	args->geo = dp->i_mount->m_dir_geo;
	args->name = name->name;
	args->namelen = name->len;
	args->filetype = name->type;
	args->hashval = xfs_dir2_hashname(dp->i_mount, name);
	args->dp = dp;
	args->whichfork = XFS_DATA_FORK;
	args->trans = tp;
	args->op_flags = XFS_DA_OP_OKNOENT;
	if (ci_name)
		args->op_flags |= XFS_DA_OP_CILOOKUP;

	lock_mode = xfs_ilock_data_map_shared(dp);
	if (dp->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		rval = xfs_dir2_sf_lookup(args);
		goto out_check_rval;
	}

	rval = xfs_dir2_isblock(args, &v);
	if (rval)
		goto out_free;
	if (v) {
		rval = xfs_dir2_block_lookup(args);
		goto out_check_rval;
	}

	rval = xfs_dir2_isleaf(args, &v);
	if (rval)
		goto out_free;
	if (v)
		rval = xfs_dir2_leaf_lookup(args);
	else
		rval = xfs_dir2_node_lookup(args);

out_check_rval:
	if (rval == -EEXIST)
		rval = 0;
	if (!rval) {
		*inum = args->inumber;
		if (ci_name) {
			ci_name->name = args->value;
			ci_name->len = args->valuelen;
		}
	}
out_free:
	xfs_iunlock(dp, lock_mode);
	kmem_free(args);
	return rval;
}

 
int
xfs_dir_removename(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	struct xfs_name		*name,
	xfs_ino_t		ino,
	xfs_extlen_t		total)		 
{
	struct xfs_da_args	*args;
	int			rval;
	bool			v;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));
	XFS_STATS_INC(dp->i_mount, xs_dir_remove);

	args = kmem_zalloc(sizeof(*args), KM_NOFS);
	if (!args)
		return -ENOMEM;

	args->geo = dp->i_mount->m_dir_geo;
	args->name = name->name;
	args->namelen = name->len;
	args->filetype = name->type;
	args->hashval = xfs_dir2_hashname(dp->i_mount, name);
	args->inumber = ino;
	args->dp = dp;
	args->total = total;
	args->whichfork = XFS_DATA_FORK;
	args->trans = tp;

	if (dp->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		rval = xfs_dir2_sf_removename(args);
		goto out_free;
	}

	rval = xfs_dir2_isblock(args, &v);
	if (rval)
		goto out_free;
	if (v) {
		rval = xfs_dir2_block_removename(args);
		goto out_free;
	}

	rval = xfs_dir2_isleaf(args, &v);
	if (rval)
		goto out_free;
	if (v)
		rval = xfs_dir2_leaf_removename(args);
	else
		rval = xfs_dir2_node_removename(args);
out_free:
	kmem_free(args);
	return rval;
}

 
int
xfs_dir_replace(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	const struct xfs_name	*name,		 
	xfs_ino_t		inum,		 
	xfs_extlen_t		total)		 
{
	struct xfs_da_args	*args;
	int			rval;
	bool			v;

	ASSERT(S_ISDIR(VFS_I(dp)->i_mode));

	rval = xfs_dir_ino_validate(tp->t_mountp, inum);
	if (rval)
		return rval;

	args = kmem_zalloc(sizeof(*args), KM_NOFS);
	if (!args)
		return -ENOMEM;

	args->geo = dp->i_mount->m_dir_geo;
	args->name = name->name;
	args->namelen = name->len;
	args->filetype = name->type;
	args->hashval = xfs_dir2_hashname(dp->i_mount, name);
	args->inumber = inum;
	args->dp = dp;
	args->total = total;
	args->whichfork = XFS_DATA_FORK;
	args->trans = tp;

	if (dp->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		rval = xfs_dir2_sf_replace(args);
		goto out_free;
	}

	rval = xfs_dir2_isblock(args, &v);
	if (rval)
		goto out_free;
	if (v) {
		rval = xfs_dir2_block_replace(args);
		goto out_free;
	}

	rval = xfs_dir2_isleaf(args, &v);
	if (rval)
		goto out_free;
	if (v)
		rval = xfs_dir2_leaf_replace(args);
	else
		rval = xfs_dir2_node_replace(args);
out_free:
	kmem_free(args);
	return rval;
}

 
int
xfs_dir_canenter(
	xfs_trans_t	*tp,
	xfs_inode_t	*dp,
	struct xfs_name	*name)		 
{
	return xfs_dir_createname(tp, dp, name, 0, 0);
}

 

 
int
xfs_dir2_grow_inode(
	struct xfs_da_args	*args,
	int			space,	 
	xfs_dir2_db_t		*dbp)	 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	xfs_fileoff_t		bno;	 
	int			count;	 
	int			error;

	trace_xfs_dir2_grow_inode(args, space);

	 
	bno = XFS_B_TO_FSBT(mp, space * XFS_DIR2_SPACE_SIZE);
	count = args->geo->fsbcount;

	error = xfs_da_grow_inode_int(args, &bno, count);
	if (error)
		return error;

	*dbp = xfs_dir2_da_to_db(args->geo, (xfs_dablk_t)bno);

	 
	if (space == XFS_DIR2_DATA_SPACE) {
		xfs_fsize_t	size;		 

		size = XFS_FSB_TO_B(mp, bno + count);
		if (size > dp->i_disk_size) {
			dp->i_disk_size = size;
			xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE);
		}
	}
	return 0;
}

 
int
xfs_dir2_isblock(
	struct xfs_da_args	*args,
	bool			*isblock)
{
	struct xfs_mount	*mp = args->dp->i_mount;
	xfs_fileoff_t		eof;
	int			error;

	error = xfs_bmap_last_offset(args->dp, &eof, XFS_DATA_FORK);
	if (error)
		return error;

	*isblock = false;
	if (XFS_FSB_TO_B(mp, eof) != args->geo->blksize)
		return 0;

	*isblock = true;
	if (XFS_IS_CORRUPT(mp, args->dp->i_disk_size != args->geo->blksize))
		return -EFSCORRUPTED;
	return 0;
}

 
int
xfs_dir2_isleaf(
	struct xfs_da_args	*args,
	bool			*isleaf)
{
	xfs_fileoff_t		eof;
	int			error;

	error = xfs_bmap_last_offset(args->dp, &eof, XFS_DATA_FORK);
	if (error)
		return error;

	*isleaf = false;
	if (eof != args->geo->leafblk + args->geo->fsbcount)
		return 0;

	*isleaf = true;
	return 0;
}

 
int
xfs_dir2_shrink_inode(
	struct xfs_da_args	*args,
	xfs_dir2_db_t		db,
	struct xfs_buf		*bp)
{
	xfs_fileoff_t		bno;		 
	xfs_dablk_t		da;		 
	int			done;		 
	struct xfs_inode	*dp;
	int			error;
	struct xfs_mount	*mp;
	struct xfs_trans	*tp;

	trace_xfs_dir2_shrink_inode(args, db);

	dp = args->dp;
	mp = dp->i_mount;
	tp = args->trans;
	da = xfs_dir2_db_to_da(args->geo, db);

	 
	error = xfs_bunmapi(tp, dp, da, args->geo->fsbcount, 0, 0, &done);
	if (error) {
		 
		return error;
	}
	ASSERT(done);
	 
	xfs_trans_binval(tp, bp);
	 
	if (db >= xfs_dir2_byte_to_db(args->geo, XFS_DIR2_LEAF_OFFSET))
		return 0;
	 
	if (dp->i_disk_size > xfs_dir2_db_off_to_byte(args->geo, db + 1, 0))
		return 0;
	bno = da;
	if ((error = xfs_bmap_last_before(tp, dp, &bno, XFS_DATA_FORK))) {
		 
		return error;
	}
	if (db == args->geo->datablk)
		ASSERT(bno == 0);
	else
		ASSERT(bno > 0);
	 
	dp->i_disk_size = XFS_FSB_TO_B(mp, bno);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
	return 0;
}

 
bool
xfs_dir2_namecheck(
	const void	*name,
	size_t		length)
{
	 
	if (length >= MAXNAMELEN)
		return false;

	 
	return !memchr(name, '/', length) && !memchr(name, 0, length);
}

xfs_dahash_t
xfs_dir2_hashname(
	struct xfs_mount	*mp,
	const struct xfs_name	*name)
{
	if (unlikely(xfs_has_asciici(mp)))
		return xfs_ascii_ci_hashname(name);
	return xfs_da_hashname(name->name, name->len);
}

enum xfs_dacmp
xfs_dir2_compname(
	struct xfs_da_args	*args,
	const unsigned char	*name,
	int			len)
{
	if (unlikely(xfs_has_asciici(args->dp->i_mount)))
		return xfs_ascii_ci_compname(args, name, len);
	return xfs_da_compname(args, name, len);
}
