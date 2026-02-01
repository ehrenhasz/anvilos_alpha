
 
#ifndef __XFS_SCRUB_COMMON_H__
#define __XFS_SCRUB_COMMON_H__

 
static inline bool
xchk_should_terminate(
	struct xfs_scrub	*sc,
	int			*error)
{
	 
	cond_resched();

	if (fatal_signal_pending(current)) {
		if (*error == 0)
			*error = -EINTR;
		return true;
	}
	return false;
}

int xchk_trans_alloc(struct xfs_scrub *sc, uint resblks);
void xchk_trans_cancel(struct xfs_scrub *sc);

bool xchk_process_error(struct xfs_scrub *sc, xfs_agnumber_t agno,
		xfs_agblock_t bno, int *error);
bool xchk_fblock_process_error(struct xfs_scrub *sc, int whichfork,
		xfs_fileoff_t offset, int *error);

bool xchk_xref_process_error(struct xfs_scrub *sc,
		xfs_agnumber_t agno, xfs_agblock_t bno, int *error);
bool xchk_fblock_xref_process_error(struct xfs_scrub *sc,
		int whichfork, xfs_fileoff_t offset, int *error);

void xchk_block_set_preen(struct xfs_scrub *sc,
		struct xfs_buf *bp);
void xchk_ino_set_preen(struct xfs_scrub *sc, xfs_ino_t ino);

void xchk_set_corrupt(struct xfs_scrub *sc);
void xchk_block_set_corrupt(struct xfs_scrub *sc,
		struct xfs_buf *bp);
void xchk_ino_set_corrupt(struct xfs_scrub *sc, xfs_ino_t ino);
void xchk_fblock_set_corrupt(struct xfs_scrub *sc, int whichfork,
		xfs_fileoff_t offset);

void xchk_block_xref_set_corrupt(struct xfs_scrub *sc,
		struct xfs_buf *bp);
void xchk_ino_xref_set_corrupt(struct xfs_scrub *sc,
		xfs_ino_t ino);
void xchk_fblock_xref_set_corrupt(struct xfs_scrub *sc,
		int whichfork, xfs_fileoff_t offset);

void xchk_ino_set_warning(struct xfs_scrub *sc, xfs_ino_t ino);
void xchk_fblock_set_warning(struct xfs_scrub *sc, int whichfork,
		xfs_fileoff_t offset);

void xchk_set_incomplete(struct xfs_scrub *sc);
int xchk_checkpoint_log(struct xfs_mount *mp);

 
bool xchk_should_check_xref(struct xfs_scrub *sc, int *error,
			   struct xfs_btree_cur **curpp);

 
int xchk_setup_agheader(struct xfs_scrub *sc);
int xchk_setup_fs(struct xfs_scrub *sc);
int xchk_setup_ag_allocbt(struct xfs_scrub *sc);
int xchk_setup_ag_iallocbt(struct xfs_scrub *sc);
int xchk_setup_ag_rmapbt(struct xfs_scrub *sc);
int xchk_setup_ag_refcountbt(struct xfs_scrub *sc);
int xchk_setup_inode(struct xfs_scrub *sc);
int xchk_setup_inode_bmap(struct xfs_scrub *sc);
int xchk_setup_inode_bmap_data(struct xfs_scrub *sc);
int xchk_setup_directory(struct xfs_scrub *sc);
int xchk_setup_xattr(struct xfs_scrub *sc);
int xchk_setup_symlink(struct xfs_scrub *sc);
int xchk_setup_parent(struct xfs_scrub *sc);
#ifdef CONFIG_XFS_RT
int xchk_setup_rtbitmap(struct xfs_scrub *sc);
int xchk_setup_rtsummary(struct xfs_scrub *sc);
#else
static inline int
xchk_setup_rtbitmap(struct xfs_scrub *sc)
{
	return -ENOENT;
}
static inline int
xchk_setup_rtsummary(struct xfs_scrub *sc)
{
	return -ENOENT;
}
#endif
#ifdef CONFIG_XFS_QUOTA
int xchk_setup_quota(struct xfs_scrub *sc);
#else
static inline int
xchk_setup_quota(struct xfs_scrub *sc)
{
	return -ENOENT;
}
#endif
int xchk_setup_fscounters(struct xfs_scrub *sc);

void xchk_ag_free(struct xfs_scrub *sc, struct xchk_ag *sa);
int xchk_ag_init(struct xfs_scrub *sc, xfs_agnumber_t agno,
		struct xchk_ag *sa);

 
static inline int
xchk_ag_init_existing(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	struct xchk_ag		*sa)
{
	int			error = xchk_ag_init(sc, agno, sa);

	return error == -ENOENT ? -EFSCORRUPTED : error;
}

int xchk_ag_read_headers(struct xfs_scrub *sc, xfs_agnumber_t agno,
		struct xchk_ag *sa);
void xchk_ag_btcur_free(struct xchk_ag *sa);
void xchk_ag_btcur_init(struct xfs_scrub *sc, struct xchk_ag *sa);
int xchk_count_rmap_ownedby_ag(struct xfs_scrub *sc, struct xfs_btree_cur *cur,
		const struct xfs_owner_info *oinfo, xfs_filblks_t *blocks);

int xchk_setup_ag_btree(struct xfs_scrub *sc, bool force_log);
int xchk_iget_for_scrubbing(struct xfs_scrub *sc);
int xchk_setup_inode_contents(struct xfs_scrub *sc, unsigned int resblks);
int xchk_install_live_inode(struct xfs_scrub *sc, struct xfs_inode *ip);

void xchk_ilock(struct xfs_scrub *sc, unsigned int ilock_flags);
bool xchk_ilock_nowait(struct xfs_scrub *sc, unsigned int ilock_flags);
void xchk_iunlock(struct xfs_scrub *sc, unsigned int ilock_flags);

void xchk_buffer_recheck(struct xfs_scrub *sc, struct xfs_buf *bp);

int xchk_iget(struct xfs_scrub *sc, xfs_ino_t inum, struct xfs_inode **ipp);
int xchk_iget_agi(struct xfs_scrub *sc, xfs_ino_t inum,
		struct xfs_buf **agi_bpp, struct xfs_inode **ipp);
void xchk_irele(struct xfs_scrub *sc, struct xfs_inode *ip);
int xchk_install_handle_inode(struct xfs_scrub *sc, struct xfs_inode *ip);

 
static inline bool xchk_skip_xref(struct xfs_scrub_metadata *sm)
{
	return sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
			       XFS_SCRUB_OFLAG_XCORRUPT);
}

#ifdef CONFIG_XFS_ONLINE_REPAIR
 
static inline bool xchk_needs_repair(const struct xfs_scrub_metadata *sm)
{
	return sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
			       XFS_SCRUB_OFLAG_XCORRUPT |
			       XFS_SCRUB_OFLAG_PREEN);
}
#else
# define xchk_needs_repair(sc)		(false)
#endif  

int xchk_metadata_inode_forks(struct xfs_scrub *sc);

 
#define xchk_xfile_descr(sc, fmt, ...) \
	kasprintf(XCHK_GFP_FLAGS, "XFS (%s): " fmt, \
			(sc)->mp->m_super->s_id, ##__VA_ARGS__)

 
static inline bool xchk_need_intent_drain(struct xfs_scrub *sc)
{
	return sc->flags & XCHK_NEED_DRAIN;
}

void xchk_fsgates_enable(struct xfs_scrub *sc, unsigned int scrub_fshooks);

int xchk_inode_is_allocated(struct xfs_scrub *sc, xfs_agino_t agino,
		bool *inuse);

#endif	 
