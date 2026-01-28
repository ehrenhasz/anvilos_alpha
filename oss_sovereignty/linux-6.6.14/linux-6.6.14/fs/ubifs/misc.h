#ifndef __UBIFS_MISC_H__
#define __UBIFS_MISC_H__
static inline int ubifs_zn_dirty(const struct ubifs_znode *znode)
{
	return !!test_bit(DIRTY_ZNODE, &znode->flags);
}
static inline int ubifs_zn_obsolete(const struct ubifs_znode *znode)
{
	return !!test_bit(OBSOLETE_ZNODE, &znode->flags);
}
static inline int ubifs_zn_cow(const struct ubifs_znode *znode)
{
	return !!test_bit(COW_ZNODE, &znode->flags);
}
static inline void ubifs_wake_up_bgt(struct ubifs_info *c)
{
	if (c->bgt && !c->need_bgt) {
		c->need_bgt = 1;
		wake_up_process(c->bgt);
	}
}
static inline struct ubifs_znode *
ubifs_tnc_find_child(struct ubifs_znode *znode, int start)
{
	while (start < znode->child_cnt) {
		if (znode->zbranch[start].znode)
			return znode->zbranch[start].znode;
		start += 1;
	}
	return NULL;
}
static inline struct ubifs_inode *ubifs_inode(const struct inode *inode)
{
	return container_of(inode, struct ubifs_inode, vfs_inode);
}
static inline int ubifs_compr_present(struct ubifs_info *c, int compr_type)
{
	ubifs_assert(c, compr_type >= 0 && compr_type < UBIFS_COMPR_TYPES_CNT);
	return !!ubifs_compressors[compr_type]->capi_name;
}
static inline const char *ubifs_compr_name(struct ubifs_info *c, int compr_type)
{
	ubifs_assert(c, compr_type >= 0 && compr_type < UBIFS_COMPR_TYPES_CNT);
	return ubifs_compressors[compr_type]->name;
}
static inline int ubifs_wbuf_sync(struct ubifs_wbuf *wbuf)
{
	int err;
	mutex_lock_nested(&wbuf->io_mutex, wbuf->jhead);
	err = ubifs_wbuf_sync_nolock(wbuf);
	mutex_unlock(&wbuf->io_mutex);
	return err;
}
static inline int ubifs_encode_dev(union ubifs_dev_desc *dev, dev_t rdev)
{
	dev->new = cpu_to_le32(new_encode_dev(rdev));
	return sizeof(dev->new);
}
static inline int ubifs_add_dirt(struct ubifs_info *c, int lnum, int dirty)
{
	return ubifs_update_one_lp(c, lnum, LPROPS_NC, dirty, 0, 0);
}
static inline int ubifs_return_leb(struct ubifs_info *c, int lnum)
{
	return ubifs_change_one_lp(c, lnum, LPROPS_NC, LPROPS_NC, 0,
				   LPROPS_TAKEN, 0);
}
static inline int ubifs_idx_node_sz(const struct ubifs_info *c, int child_cnt)
{
	return UBIFS_IDX_NODE_SZ + (UBIFS_BRANCH_SZ + c->key_len + c->hash_len)
				   * child_cnt;
}
static inline
struct ubifs_branch *ubifs_idx_branch(const struct ubifs_info *c,
				      const struct ubifs_idx_node *idx,
				      int bnum)
{
	return (struct ubifs_branch *)((void *)idx->branches +
			(UBIFS_BRANCH_SZ + c->key_len + c->hash_len) * bnum);
}
static inline void *ubifs_idx_key(const struct ubifs_info *c,
				  const struct ubifs_idx_node *idx)
{
	return (void *)((struct ubifs_branch *)idx->branches)->key;
}
static inline int ubifs_tnc_lookup(struct ubifs_info *c,
				   const union ubifs_key *key, void *node)
{
	return ubifs_tnc_locate(c, key, node, NULL, NULL);
}
static inline void ubifs_get_lprops(struct ubifs_info *c)
{
	mutex_lock(&c->lp_mutex);
}
static inline void ubifs_release_lprops(struct ubifs_info *c)
{
	ubifs_assert(c, mutex_is_locked(&c->lp_mutex));
	ubifs_assert(c, c->lst.empty_lebs >= 0 &&
		     c->lst.empty_lebs <= c->main_lebs);
	mutex_unlock(&c->lp_mutex);
}
static inline int ubifs_next_log_lnum(const struct ubifs_info *c, int lnum)
{
	lnum += 1;
	if (lnum > c->log_last)
		lnum = UBIFS_LOG_LNUM;
	return lnum;
}
static inline int ubifs_xattr_max_cnt(struct ubifs_info *c)
{
	int max_xattrs = (c->leb_size / 2) / UBIFS_INO_NODE_SZ;
	ubifs_assert(c, max_xattrs < c->max_orphans);
	return max_xattrs;
}
const char *ubifs_assert_action_name(struct ubifs_info *c);
#endif  
