
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_trace.h"

 
static void xfs_dir2_sf_addname_easy(xfs_da_args_t *args,
				     xfs_dir2_sf_entry_t *sfep,
				     xfs_dir2_data_aoff_t offset,
				     int new_isize);
static void xfs_dir2_sf_addname_hard(xfs_da_args_t *args, int objchange,
				     int new_isize);
static int xfs_dir2_sf_addname_pick(xfs_da_args_t *args, int objchange,
				    xfs_dir2_sf_entry_t **sfepp,
				    xfs_dir2_data_aoff_t *offsetp);
#ifdef DEBUG
static void xfs_dir2_sf_check(xfs_da_args_t *args);
#else
#define	xfs_dir2_sf_check(args)
#endif  

static void xfs_dir2_sf_toino4(xfs_da_args_t *args);
static void xfs_dir2_sf_toino8(xfs_da_args_t *args);

int
xfs_dir2_sf_entsize(
	struct xfs_mount	*mp,
	struct xfs_dir2_sf_hdr	*hdr,
	int			len)
{
	int			count = len;

	count += sizeof(struct xfs_dir2_sf_entry);	 
	count += hdr->i8count ? XFS_INO64_SIZE : XFS_INO32_SIZE;  

	if (xfs_has_ftype(mp))
		count += sizeof(uint8_t);
	return count;
}

struct xfs_dir2_sf_entry *
xfs_dir2_sf_nextentry(
	struct xfs_mount	*mp,
	struct xfs_dir2_sf_hdr	*hdr,
	struct xfs_dir2_sf_entry *sfep)
{
	return (void *)sfep + xfs_dir2_sf_entsize(mp, hdr, sfep->namelen);
}

 
xfs_ino_t
xfs_dir2_sf_get_ino(
	struct xfs_mount		*mp,
	struct xfs_dir2_sf_hdr		*hdr,
	struct xfs_dir2_sf_entry	*sfep)
{
	uint8_t				*from = sfep->name + sfep->namelen;

	if (xfs_has_ftype(mp))
		from++;

	if (!hdr->i8count)
		return get_unaligned_be32(from);
	return get_unaligned_be64(from) & XFS_MAXINUMBER;
}

void
xfs_dir2_sf_put_ino(
	struct xfs_mount		*mp,
	struct xfs_dir2_sf_hdr		*hdr,
	struct xfs_dir2_sf_entry	*sfep,
	xfs_ino_t			ino)
{
	uint8_t				*to = sfep->name + sfep->namelen;

	ASSERT(ino <= XFS_MAXINUMBER);

	if (xfs_has_ftype(mp))
		to++;

	if (hdr->i8count)
		put_unaligned_be64(ino, to);
	else
		put_unaligned_be32(ino, to);
}

xfs_ino_t
xfs_dir2_sf_get_parent_ino(
	struct xfs_dir2_sf_hdr	*hdr)
{
	if (!hdr->i8count)
		return get_unaligned_be32(hdr->parent);
	return get_unaligned_be64(hdr->parent) & XFS_MAXINUMBER;
}

void
xfs_dir2_sf_put_parent_ino(
	struct xfs_dir2_sf_hdr		*hdr,
	xfs_ino_t			ino)
{
	ASSERT(ino <= XFS_MAXINUMBER);

	if (hdr->i8count)
		put_unaligned_be64(ino, hdr->parent);
	else
		put_unaligned_be32(ino, hdr->parent);
}

 
uint8_t
xfs_dir2_sf_get_ftype(
	struct xfs_mount		*mp,
	struct xfs_dir2_sf_entry	*sfep)
{
	if (xfs_has_ftype(mp)) {
		uint8_t			ftype = sfep->name[sfep->namelen];

		if (ftype < XFS_DIR3_FT_MAX)
			return ftype;
	}

	return XFS_DIR3_FT_UNKNOWN;
}

void
xfs_dir2_sf_put_ftype(
	struct xfs_mount	*mp,
	struct xfs_dir2_sf_entry *sfep,
	uint8_t			ftype)
{
	ASSERT(ftype < XFS_DIR3_FT_MAX);

	if (xfs_has_ftype(mp))
		sfep->name[sfep->namelen] = ftype;
}

 
int						 
xfs_dir2_block_sfsize(
	xfs_inode_t		*dp,		 
	xfs_dir2_data_hdr_t	*hdr,		 
	xfs_dir2_sf_hdr_t	*sfhp)		 
{
	xfs_dir2_dataptr_t	addr;		 
	xfs_dir2_leaf_entry_t	*blp;		 
	xfs_dir2_block_tail_t	*btp;		 
	int			count;		 
	xfs_dir2_data_entry_t	*dep;		 
	int			i;		 
	int			i8count;	 
	int			isdot;		 
	int			isdotdot;	 
	xfs_mount_t		*mp;		 
	int			namelen;	 
	xfs_ino_t		parent = 0;	 
	int			size=0;		 
	int			has_ftype;
	struct xfs_da_geometry	*geo;

	mp = dp->i_mount;
	geo = mp->m_dir_geo;

	 
	has_ftype = xfs_has_ftype(mp) ? 1 : 0;

	count = i8count = namelen = 0;
	btp = xfs_dir2_block_tail_p(geo, hdr);
	blp = xfs_dir2_block_leaf_p(btp);

	 
	for (i = 0; i < be32_to_cpu(btp->count); i++) {
		if ((addr = be32_to_cpu(blp[i].address)) == XFS_DIR2_NULL_DATAPTR)
			continue;
		 
		dep = (xfs_dir2_data_entry_t *)((char *)hdr +
				xfs_dir2_dataptr_to_off(geo, addr));
		 
		isdot = dep->namelen == 1 && dep->name[0] == '.';
		isdotdot =
			dep->namelen == 2 &&
			dep->name[0] == '.' && dep->name[1] == '.';

		if (!isdot)
			i8count += be64_to_cpu(dep->inumber) > XFS_DIR2_MAX_SHORT_INUM;

		 
		if (!isdot && !isdotdot) {
			count++;
			namelen += dep->namelen + has_ftype;
		} else if (isdotdot)
			parent = be64_to_cpu(dep->inumber);
		 
		size = xfs_dir2_sf_hdr_size(i8count) +	 
		       count * 3 * sizeof(u8) +		 
		       namelen +			 
		       (i8count ?			 
				count * XFS_INO64_SIZE :
				count * XFS_INO32_SIZE);
		if (size > xfs_inode_data_fork_size(dp))
			return size;		 
	}
	 
	sfhp->count = count;
	sfhp->i8count = i8count;
	xfs_dir2_sf_put_parent_ino(sfhp, parent);
	return size;
}

 
int						 
xfs_dir2_block_to_sf(
	struct xfs_da_args	*args,		 
	struct xfs_buf		*bp,
	int			size,		 
	struct xfs_dir2_sf_hdr	*sfhp)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			error;		 
	int			logflags;	 
	struct xfs_dir2_sf_entry *sfep;		 
	struct xfs_dir2_sf_hdr	*sfp;		 
	unsigned int		offset = args->geo->data_entry_offset;
	unsigned int		end;

	trace_xfs_dir2_block_to_sf(args);

	 
	sfp = kmem_alloc(mp->m_sb.sb_inodesize, 0);
	memcpy(sfp, sfhp, xfs_dir2_sf_hdr_size(sfhp->i8count));

	 
	end = xfs_dir3_data_end_offset(args->geo, bp->b_addr);
	sfep = xfs_dir2_sf_firstentry(sfp);
	while (offset < end) {
		struct xfs_dir2_data_unused	*dup = bp->b_addr + offset;
		struct xfs_dir2_data_entry	*dep = bp->b_addr + offset;

		 
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			offset += be16_to_cpu(dup->length);
			continue;
		}

		 
		if (dep->namelen == 1 && dep->name[0] == '.')
			ASSERT(be64_to_cpu(dep->inumber) == dp->i_ino);
		 
		else if (dep->namelen == 2 &&
			 dep->name[0] == '.' && dep->name[1] == '.')
			ASSERT(be64_to_cpu(dep->inumber) ==
			       xfs_dir2_sf_get_parent_ino(sfp));
		 
		else {
			sfep->namelen = dep->namelen;
			xfs_dir2_sf_put_offset(sfep, offset);
			memcpy(sfep->name, dep->name, dep->namelen);
			xfs_dir2_sf_put_ino(mp, sfp, sfep,
					      be64_to_cpu(dep->inumber));
			xfs_dir2_sf_put_ftype(mp, sfep,
					xfs_dir2_data_get_ftype(mp, dep));

			sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
		}
		offset += xfs_dir2_data_entsize(mp, dep->namelen);
	}
	ASSERT((char *)sfep - (char *)sfp == size);

	 
	logflags = XFS_ILOG_CORE;
	error = xfs_dir2_shrink_inode(args, args->geo->datablk, bp);
	if (error) {
		ASSERT(error != -ENOSPC);
		goto out;
	}

	 
	ASSERT(dp->i_df.if_bytes == 0);
	xfs_init_local_fork(dp, XFS_DATA_FORK, sfp, size);
	dp->i_df.if_format = XFS_DINODE_FMT_LOCAL;
	dp->i_disk_size = size;

	logflags |= XFS_ILOG_DDATA;
	xfs_dir2_sf_check(args);
out:
	xfs_trans_log_inode(args->trans, dp, logflags);
	kmem_free(sfp);
	return error;
}

 
int						 
xfs_dir2_sf_addname(
	xfs_da_args_t		*args)		 
{
	xfs_inode_t		*dp;		 
	int			error;		 
	int			incr_isize;	 
	int			new_isize;	 
	int			objchange;	 
	xfs_dir2_data_aoff_t	offset = 0;	 
	int			pick;		 
	xfs_dir2_sf_hdr_t	*sfp;		 
	xfs_dir2_sf_entry_t	*sfep = NULL;	 

	trace_xfs_dir2_sf_addname(args);

	ASSERT(xfs_dir2_sf_lookup(args) == -ENOENT);
	dp = args->dp;
	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(dp->i_disk_size >= offsetof(struct xfs_dir2_sf_hdr, parent));
	ASSERT(dp->i_df.if_bytes == dp->i_disk_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_disk_size >= xfs_dir2_sf_hdr_size(sfp->i8count));
	 
	incr_isize = xfs_dir2_sf_entsize(dp->i_mount, sfp, args->namelen);
	objchange = 0;

	 
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && sfp->i8count == 0) {
		 
		incr_isize += (sfp->count + 2) * XFS_INO64_DIFF;
		objchange = 1;
	}

	new_isize = (int)dp->i_disk_size + incr_isize;
	 
	if (new_isize > xfs_inode_data_fork_size(dp) ||
	    (pick =
	     xfs_dir2_sf_addname_pick(args, objchange, &sfep, &offset)) == 0) {
		 
		if ((args->op_flags & XFS_DA_OP_JUSTCHECK) || args->total == 0)
			return -ENOSPC;
		 
		error = xfs_dir2_sf_to_block(args);
		if (error)
			return error;
		return xfs_dir2_block_addname(args);
	}
	 
	if (args->op_flags & XFS_DA_OP_JUSTCHECK)
		return 0;
	 
	if (pick == 1)
		xfs_dir2_sf_addname_easy(args, sfep, offset, new_isize);
	 
	else {
		ASSERT(pick == 2);
		if (objchange)
			xfs_dir2_sf_toino8(args);
		xfs_dir2_sf_addname_hard(args, objchange, new_isize);
	}
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

 
static void
xfs_dir2_sf_addname_easy(
	xfs_da_args_t		*args,		 
	xfs_dir2_sf_entry_t	*sfep,		 
	xfs_dir2_data_aoff_t	offset,		 
	int			new_isize)	 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			byteoff;	 
	xfs_dir2_sf_hdr_t	*sfp;		 

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	byteoff = (int)((char *)sfep - (char *)sfp);
	 
	xfs_idata_realloc(dp, xfs_dir2_sf_entsize(mp, sfp, args->namelen),
			  XFS_DATA_FORK);
	 
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfep = (xfs_dir2_sf_entry_t *)((char *)sfp + byteoff);
	 
	sfep->namelen = args->namelen;
	xfs_dir2_sf_put_offset(sfep, offset);
	memcpy(sfep->name, args->name, sfep->namelen);
	xfs_dir2_sf_put_ino(mp, sfp, sfep, args->inumber);
	xfs_dir2_sf_put_ftype(mp, sfep, args->filetype);

	 
	sfp->count++;
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM)
		sfp->i8count++;
	dp->i_disk_size = new_isize;
	xfs_dir2_sf_check(args);
}

 
 
static void
xfs_dir2_sf_addname_hard(
	xfs_da_args_t		*args,		 
	int			objchange,	 
	int			new_isize)	 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			add_datasize;	 
	char			*buf;		 
	int			eof;		 
	int			nbytes;		 
	xfs_dir2_data_aoff_t	new_offset;	 
	xfs_dir2_data_aoff_t	offset;		 
	int			old_isize;	 
	xfs_dir2_sf_entry_t	*oldsfep;	 
	xfs_dir2_sf_hdr_t	*oldsfp;	 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	 
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	old_isize = (int)dp->i_disk_size;
	buf = kmem_alloc(old_isize, 0);
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	memcpy(oldsfp, sfp, old_isize);
	 
	for (offset = args->geo->data_first_offset,
	      oldsfep = xfs_dir2_sf_firstentry(oldsfp),
	      add_datasize = xfs_dir2_data_entsize(mp, args->namelen),
	      eof = (char *)oldsfep == &buf[old_isize];
	     !eof;
	     offset = new_offset + xfs_dir2_data_entsize(mp, oldsfep->namelen),
	      oldsfep = xfs_dir2_sf_nextentry(mp, oldsfp, oldsfep),
	      eof = (char *)oldsfep == &buf[old_isize]) {
		new_offset = xfs_dir2_sf_get_offset(oldsfep);
		if (offset + add_datasize <= new_offset)
			break;
	}
	 
	xfs_idata_realloc(dp, -old_isize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, new_isize, XFS_DATA_FORK);
	 
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	 
	nbytes = (int)((char *)oldsfep - (char *)oldsfp);
	memcpy(sfp, oldsfp, nbytes);
	sfep = (xfs_dir2_sf_entry_t *)((char *)sfp + nbytes);
	 
	sfep->namelen = args->namelen;
	xfs_dir2_sf_put_offset(sfep, offset);
	memcpy(sfep->name, args->name, sfep->namelen);
	xfs_dir2_sf_put_ino(mp, sfp, sfep, args->inumber);
	xfs_dir2_sf_put_ftype(mp, sfep, args->filetype);
	sfp->count++;
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && !objchange)
		sfp->i8count++;
	 
	if (!eof) {
		sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
		memcpy(sfep, oldsfep, old_isize - nbytes);
	}
	kmem_free(buf);
	dp->i_disk_size = new_isize;
	xfs_dir2_sf_check(args);
}

 
 
static int					 
xfs_dir2_sf_addname_pick(
	xfs_da_args_t		*args,		 
	int			objchange,	 
	xfs_dir2_sf_entry_t	**sfepp,	 
	xfs_dir2_data_aoff_t	*offsetp)	 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			holefit;	 
	int			i;		 
	xfs_dir2_data_aoff_t	offset;		 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 
	int			size;		 
	int			used;		 

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	size = xfs_dir2_data_entsize(mp, args->namelen);
	offset = args->geo->data_first_offset;
	sfep = xfs_dir2_sf_firstentry(sfp);
	holefit = 0;
	 
	for (i = 0; i < sfp->count; i++) {
		if (!holefit)
			holefit = offset + size <= xfs_dir2_sf_get_offset(sfep);
		offset = xfs_dir2_sf_get_offset(sfep) +
			 xfs_dir2_data_entsize(mp, sfep->namelen);
		sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
	}
	 
	used = offset +
	       (sfp->count + 3) * (uint)sizeof(xfs_dir2_leaf_entry_t) +
	       (uint)sizeof(xfs_dir2_block_tail_t);
	 
	if (used + (holefit ? 0 : size) > args->geo->blksize)
		return 0;
	 
	if (objchange)
		return 2;
	 
	if (used + size > args->geo->blksize)
		return 2;
	 
	*sfepp = sfep;
	*offsetp = offset;
	return 1;
}

#ifdef DEBUG
 
static void
xfs_dir2_sf_check(
	xfs_da_args_t		*args)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			i;		 
	int			i8count;	 
	xfs_ino_t		ino;		 
	int			offset;		 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	offset = args->geo->data_first_offset;
	ino = xfs_dir2_sf_get_parent_ino(sfp);
	i8count = ino > XFS_DIR2_MAX_SHORT_INUM;

	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep)) {
		ASSERT(xfs_dir2_sf_get_offset(sfep) >= offset);
		ino = xfs_dir2_sf_get_ino(mp, sfp, sfep);
		i8count += ino > XFS_DIR2_MAX_SHORT_INUM;
		offset =
			xfs_dir2_sf_get_offset(sfep) +
			xfs_dir2_data_entsize(mp, sfep->namelen);
		ASSERT(xfs_dir2_sf_get_ftype(mp, sfep) < XFS_DIR3_FT_MAX);
	}
	ASSERT(i8count == sfp->i8count);
	ASSERT((char *)sfep - (char *)sfp == dp->i_disk_size);
	ASSERT(offset +
	       (sfp->count + 2) * (uint)sizeof(xfs_dir2_leaf_entry_t) +
	       (uint)sizeof(xfs_dir2_block_tail_t) <= args->geo->blksize);
}
#endif	 

 
xfs_failaddr_t
xfs_dir2_sf_verify(
	struct xfs_inode		*ip)
{
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_ifork		*ifp = xfs_ifork_ptr(ip, XFS_DATA_FORK);
	struct xfs_dir2_sf_hdr		*sfp;
	struct xfs_dir2_sf_entry	*sfep;
	struct xfs_dir2_sf_entry	*next_sfep;
	char				*endp;
	xfs_ino_t			ino;
	int				i;
	int				i8count;
	int				offset;
	int64_t				size;
	int				error;
	uint8_t				filetype;

	ASSERT(ifp->if_format == XFS_DINODE_FMT_LOCAL);

	sfp = (struct xfs_dir2_sf_hdr *)ifp->if_u1.if_data;
	size = ifp->if_bytes;

	 
	if (size <= offsetof(struct xfs_dir2_sf_hdr, parent) ||
	    size < xfs_dir2_sf_hdr_size(sfp->i8count))
		return __this_address;

	endp = (char *)sfp + size;

	 
	ino = xfs_dir2_sf_get_parent_ino(sfp);
	i8count = ino > XFS_DIR2_MAX_SHORT_INUM;
	error = xfs_dir_ino_validate(mp, ino);
	if (error)
		return __this_address;
	offset = mp->m_dir_geo->data_first_offset;

	 
	sfep = xfs_dir2_sf_firstentry(sfp);
	for (i = 0; i < sfp->count; i++) {
		 
		if (((char *)sfep + sizeof(*sfep)) >= endp)
			return __this_address;

		 
		if (sfep->namelen == 0)
			return __this_address;

		 
		next_sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep);
		if (endp < (char *)next_sfep)
			return __this_address;

		 
		if (xfs_dir2_sf_get_offset(sfep) < offset)
			return __this_address;

		 
		ino = xfs_dir2_sf_get_ino(mp, sfp, sfep);
		i8count += ino > XFS_DIR2_MAX_SHORT_INUM;
		error = xfs_dir_ino_validate(mp, ino);
		if (error)
			return __this_address;

		 
		filetype = xfs_dir2_sf_get_ftype(mp, sfep);
		if (filetype >= XFS_DIR3_FT_MAX)
			return __this_address;

		offset = xfs_dir2_sf_get_offset(sfep) +
				xfs_dir2_data_entsize(mp, sfep->namelen);

		sfep = next_sfep;
	}
	if (i8count != sfp->i8count)
		return __this_address;
	if ((void *)sfep != (void *)endp)
		return __this_address;

	 
	if (offset + (sfp->count + 2) * (uint)sizeof(xfs_dir2_leaf_entry_t) +
	    (uint)sizeof(xfs_dir2_block_tail_t) > mp->m_dir_geo->blksize)
		return __this_address;

	return NULL;
}

 
int					 
xfs_dir2_sf_create(
	xfs_da_args_t	*args,		 
	xfs_ino_t	pino)		 
{
	xfs_inode_t	*dp;		 
	int		i8count;	 
	xfs_dir2_sf_hdr_t *sfp;		 
	int		size;		 

	trace_xfs_dir2_sf_create(args);

	dp = args->dp;

	ASSERT(dp != NULL);
	ASSERT(dp->i_disk_size == 0);
	 
	if (dp->i_df.if_format == XFS_DINODE_FMT_EXTENTS) {
		dp->i_df.if_format = XFS_DINODE_FMT_LOCAL;
		xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE);
	}
	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(dp->i_df.if_bytes == 0);
	i8count = pino > XFS_DIR2_MAX_SHORT_INUM;
	size = xfs_dir2_sf_hdr_size(i8count);
	 
	xfs_idata_realloc(dp, size, XFS_DATA_FORK);
	 
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	sfp->i8count = i8count;
	 
	xfs_dir2_sf_put_parent_ino(sfp, pino);
	sfp->count = 0;
	dp->i_disk_size = size;
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

 
int						 
xfs_dir2_sf_lookup(
	xfs_da_args_t		*args)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			i;		 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 
	enum xfs_dacmp		cmp;		 
	xfs_dir2_sf_entry_t	*ci_sfep;	 

	trace_xfs_dir2_sf_lookup(args);

	xfs_dir2_sf_check(args);

	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(dp->i_disk_size >= offsetof(struct xfs_dir2_sf_hdr, parent));
	ASSERT(dp->i_df.if_bytes == dp->i_disk_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_disk_size >= xfs_dir2_sf_hdr_size(sfp->i8count));
	 
	if (args->namelen == 1 && args->name[0] == '.') {
		args->inumber = dp->i_ino;
		args->cmpresult = XFS_CMP_EXACT;
		args->filetype = XFS_DIR3_FT_DIR;
		return -EEXIST;
	}
	 
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
		args->inumber = xfs_dir2_sf_get_parent_ino(sfp);
		args->cmpresult = XFS_CMP_EXACT;
		args->filetype = XFS_DIR3_FT_DIR;
		return -EEXIST;
	}
	 
	ci_sfep = NULL;
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp); i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep)) {
		 
		cmp = xfs_dir2_compname(args, sfep->name, sfep->namelen);
		if (cmp != XFS_CMP_DIFFERENT && cmp != args->cmpresult) {
			args->cmpresult = cmp;
			args->inumber = xfs_dir2_sf_get_ino(mp, sfp, sfep);
			args->filetype = xfs_dir2_sf_get_ftype(mp, sfep);
			if (cmp == XFS_CMP_EXACT)
				return -EEXIST;
			ci_sfep = sfep;
		}
	}
	ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
	 
	if (!ci_sfep)
		return -ENOENT;
	 
	return xfs_dir_cilookup_result(args, ci_sfep->name, ci_sfep->namelen);
}

 
int						 
xfs_dir2_sf_removename(
	xfs_da_args_t		*args)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			byteoff;	 
	int			entsize;	 
	int			i;		 
	int			newsize;	 
	int			oldsize;	 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	trace_xfs_dir2_sf_removename(args);

	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	oldsize = (int)dp->i_disk_size;
	ASSERT(oldsize >= offsetof(struct xfs_dir2_sf_hdr, parent));
	ASSERT(dp->i_df.if_bytes == oldsize);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsize >= xfs_dir2_sf_hdr_size(sfp->i8count));
	 
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp); i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep)) {
		if (xfs_da_compname(args, sfep->name, sfep->namelen) ==
								XFS_CMP_EXACT) {
			ASSERT(xfs_dir2_sf_get_ino(mp, sfp, sfep) ==
			       args->inumber);
			break;
		}
	}
	 
	if (i == sfp->count)
		return -ENOENT;
	 
	byteoff = (int)((char *)sfep - (char *)sfp);
	entsize = xfs_dir2_sf_entsize(mp, sfp, args->namelen);
	newsize = oldsize - entsize;
	 
	if (byteoff + entsize < oldsize)
		memmove((char *)sfp + byteoff, (char *)sfp + byteoff + entsize,
			oldsize - (byteoff + entsize));
	 
	sfp->count--;
	dp->i_disk_size = newsize;
	 
	xfs_idata_realloc(dp, newsize - oldsize, XFS_DATA_FORK);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	 
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM) {
		if (sfp->i8count == 1)
			xfs_dir2_sf_toino4(args);
		else
			sfp->i8count--;
	}
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
	return 0;
}

 
static bool
xfs_dir2_sf_replace_needblock(
	struct xfs_inode	*dp,
	xfs_ino_t		inum)
{
	int			newsize;
	struct xfs_dir2_sf_hdr	*sfp;

	if (dp->i_df.if_format != XFS_DINODE_FMT_LOCAL)
		return false;

	sfp = (struct xfs_dir2_sf_hdr *)dp->i_df.if_u1.if_data;
	newsize = dp->i_df.if_bytes + (sfp->count + 1) * XFS_INO64_DIFF;

	return inum > XFS_DIR2_MAX_SHORT_INUM &&
	       sfp->i8count == 0 && newsize > xfs_inode_data_fork_size(dp);
}

 
int						 
xfs_dir2_sf_replace(
	xfs_da_args_t		*args)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	int			i;		 
	xfs_ino_t		ino=0;		 
	int			i8elevated;	 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	trace_xfs_dir2_sf_replace(args);

	ASSERT(dp->i_df.if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(dp->i_disk_size >= offsetof(struct xfs_dir2_sf_hdr, parent));
	ASSERT(dp->i_df.if_bytes == dp->i_disk_size);
	ASSERT(dp->i_df.if_u1.if_data != NULL);
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(dp->i_disk_size >= xfs_dir2_sf_hdr_size(sfp->i8count));

	 
	if (args->inumber > XFS_DIR2_MAX_SHORT_INUM && sfp->i8count == 0) {
		int	error;			 

		 
		if (xfs_dir2_sf_replace_needblock(dp, args->inumber)) {
			error = xfs_dir2_sf_to_block(args);
			if (error)
				return error;
			return xfs_dir2_block_replace(args);
		}
		 
		xfs_dir2_sf_toino8(args);
		i8elevated = 1;
		sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	} else
		i8elevated = 0;

	ASSERT(args->namelen != 1 || args->name[0] != '.');
	 
	if (args->namelen == 2 &&
	    args->name[0] == '.' && args->name[1] == '.') {
		ino = xfs_dir2_sf_get_parent_ino(sfp);
		ASSERT(args->inumber != ino);
		xfs_dir2_sf_put_parent_ino(sfp, args->inumber);
	}
	 
	else {
		for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp); i < sfp->count;
		     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep)) {
			if (xfs_da_compname(args, sfep->name, sfep->namelen) ==
								XFS_CMP_EXACT) {
				ino = xfs_dir2_sf_get_ino(mp, sfp, sfep);
				ASSERT(args->inumber != ino);
				xfs_dir2_sf_put_ino(mp, sfp, sfep,
						args->inumber);
				xfs_dir2_sf_put_ftype(mp, sfep, args->filetype);
				break;
			}
		}
		 
		if (i == sfp->count) {
			ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
			if (i8elevated)
				xfs_dir2_sf_toino4(args);
			return -ENOENT;
		}
	}
	 
	if (ino > XFS_DIR2_MAX_SHORT_INUM &&
	    args->inumber <= XFS_DIR2_MAX_SHORT_INUM) {
		 
		if (sfp->i8count == 1)
			xfs_dir2_sf_toino4(args);
		else
			sfp->i8count--;
	}
	 
	if (ino <= XFS_DIR2_MAX_SHORT_INUM &&
	    args->inumber > XFS_DIR2_MAX_SHORT_INUM) {
		 
		ASSERT(sfp->i8count != 0);
		if (!i8elevated)
			sfp->i8count++;
	}
	xfs_dir2_sf_check(args);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_DDATA);
	return 0;
}

 
static void
xfs_dir2_sf_toino4(
	xfs_da_args_t		*args)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	char			*buf;		 
	int			i;		 
	int			newsize;	 
	xfs_dir2_sf_entry_t	*oldsfep;	 
	xfs_dir2_sf_hdr_t	*oldsfp;	 
	int			oldsize;	 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	trace_xfs_dir2_sf_toino4(args);

	 
	oldsize = dp->i_df.if_bytes;
	buf = kmem_alloc(oldsize, 0);
	oldsfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsfp->i8count == 1);
	memcpy(buf, oldsfp, oldsize);
	 
	newsize = oldsize - (oldsfp->count + 1) * XFS_INO64_DIFF;
	xfs_idata_realloc(dp, -oldsize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, newsize, XFS_DATA_FORK);
	 
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	 
	sfp->count = oldsfp->count;
	sfp->i8count = 0;
	xfs_dir2_sf_put_parent_ino(sfp, xfs_dir2_sf_get_parent_ino(oldsfp));
	 
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp),
		    oldsfep = xfs_dir2_sf_firstentry(oldsfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep),
		  oldsfep = xfs_dir2_sf_nextentry(mp, oldsfp, oldsfep)) {
		sfep->namelen = oldsfep->namelen;
		memcpy(sfep->offset, oldsfep->offset, sizeof(sfep->offset));
		memcpy(sfep->name, oldsfep->name, sfep->namelen);
		xfs_dir2_sf_put_ino(mp, sfp, sfep,
				xfs_dir2_sf_get_ino(mp, oldsfp, oldsfep));
		xfs_dir2_sf_put_ftype(mp, sfep,
				xfs_dir2_sf_get_ftype(mp, oldsfep));
	}
	 
	kmem_free(buf);
	dp->i_disk_size = newsize;
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
}

 
static void
xfs_dir2_sf_toino8(
	xfs_da_args_t		*args)		 
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	char			*buf;		 
	int			i;		 
	int			newsize;	 
	xfs_dir2_sf_entry_t	*oldsfep;	 
	xfs_dir2_sf_hdr_t	*oldsfp;	 
	int			oldsize;	 
	xfs_dir2_sf_entry_t	*sfep;		 
	xfs_dir2_sf_hdr_t	*sfp;		 

	trace_xfs_dir2_sf_toino8(args);

	 
	oldsize = dp->i_df.if_bytes;
	buf = kmem_alloc(oldsize, 0);
	oldsfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	ASSERT(oldsfp->i8count == 0);
	memcpy(buf, oldsfp, oldsize);
	 
	newsize = oldsize + (oldsfp->count + 1) * XFS_INO64_DIFF;
	xfs_idata_realloc(dp, -oldsize, XFS_DATA_FORK);
	xfs_idata_realloc(dp, newsize, XFS_DATA_FORK);
	 
	oldsfp = (xfs_dir2_sf_hdr_t *)buf;
	sfp = (xfs_dir2_sf_hdr_t *)dp->i_df.if_u1.if_data;
	 
	sfp->count = oldsfp->count;
	sfp->i8count = 1;
	xfs_dir2_sf_put_parent_ino(sfp, xfs_dir2_sf_get_parent_ino(oldsfp));
	 
	for (i = 0, sfep = xfs_dir2_sf_firstentry(sfp),
		    oldsfep = xfs_dir2_sf_firstentry(oldsfp);
	     i < sfp->count;
	     i++, sfep = xfs_dir2_sf_nextentry(mp, sfp, sfep),
		  oldsfep = xfs_dir2_sf_nextentry(mp, oldsfp, oldsfep)) {
		sfep->namelen = oldsfep->namelen;
		memcpy(sfep->offset, oldsfep->offset, sizeof(sfep->offset));
		memcpy(sfep->name, oldsfep->name, sfep->namelen);
		xfs_dir2_sf_put_ino(mp, sfp, sfep,
				xfs_dir2_sf_get_ino(mp, oldsfp, oldsfep));
		xfs_dir2_sf_put_ftype(mp, sfep,
				xfs_dir2_sf_get_ftype(mp, oldsfep));
	}
	 
	kmem_free(buf);
	dp->i_disk_size = newsize;
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_DDATA);
}
