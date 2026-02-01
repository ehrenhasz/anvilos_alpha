
 

 

#include "ubifs.h"

 
static inline void zero_ino_node_unused(struct ubifs_ino_node *ino)
{
	memset(ino->padding1, 0, 4);
	memset(ino->padding2, 0, 26);
}

 
static inline void zero_dent_node_unused(struct ubifs_dent_node *dent)
{
	dent->padding1 = 0;
}

 
static inline void zero_trun_node_unused(struct ubifs_trun_node *trun)
{
	memset(trun->padding, 0, 12);
}

static void ubifs_add_auth_dirt(struct ubifs_info *c, int lnum)
{
	if (ubifs_authenticated(c))
		ubifs_add_dirt(c, lnum, ubifs_auth_node_sz(c));
}

 
static int reserve_space(struct ubifs_info *c, int jhead, int len)
{
	int err = 0, err1, retries = 0, avail, lnum, offs, squeeze;
	struct ubifs_wbuf *wbuf = &c->jheads[jhead].wbuf;

	 
	ubifs_assert(c, !c->ro_media && !c->ro_mount);
	squeeze = (jhead == BASEHD);
again:
	mutex_lock_nested(&wbuf->io_mutex, wbuf->jhead);

	if (c->ro_error) {
		err = -EROFS;
		goto out_unlock;
	}

	avail = c->leb_size - wbuf->offs - wbuf->used;
	if (wbuf->lnum != -1 && avail >= len)
		return 0;

	 
	lnum = ubifs_find_free_space(c, len, &offs, squeeze);
	if (lnum >= 0)
		goto out;

	err = lnum;
	if (err != -ENOSPC)
		goto out_unlock;

	 
	dbg_jnl("no free space in jhead %s, run GC", dbg_jhead(jhead));
	mutex_unlock(&wbuf->io_mutex);

	lnum = ubifs_garbage_collect(c, 0);
	if (lnum < 0) {
		err = lnum;
		if (err != -ENOSPC)
			return err;

		 
		dbg_jnl("GC couldn't make a free LEB for jhead %s",
			dbg_jhead(jhead));
		if (retries++ < 2) {
			dbg_jnl("retry (%d)", retries);
			goto again;
		}

		dbg_jnl("return -ENOSPC");
		return err;
	}

	mutex_lock_nested(&wbuf->io_mutex, wbuf->jhead);
	dbg_jnl("got LEB %d for jhead %s", lnum, dbg_jhead(jhead));
	avail = c->leb_size - wbuf->offs - wbuf->used;

	if (wbuf->lnum != -1 && avail >= len) {
		 
		dbg_jnl("return LEB %d back, already have LEB %d:%d",
			lnum, wbuf->lnum, wbuf->offs + wbuf->used);
		err = ubifs_return_leb(c, lnum);
		if (err)
			goto out_unlock;
		return 0;
	}

	offs = 0;

out:
	 
	err = ubifs_wbuf_sync_nolock(wbuf);
	if (err)
		goto out_return;
	err = ubifs_add_bud_to_log(c, jhead, lnum, offs);
	if (err)
		goto out_return;
	err = ubifs_wbuf_seek_nolock(wbuf, lnum, offs);
	if (err)
		goto out_unlock;

	return 0;

out_unlock:
	mutex_unlock(&wbuf->io_mutex);
	return err;

out_return:
	 
	ubifs_assert(c, err < 0);
	err1 = ubifs_return_leb(c, lnum);
	if (err1 && err == -EAGAIN)
		 
		err = err1;
	mutex_unlock(&wbuf->io_mutex);
	return err;
}

static int ubifs_hash_nodes(struct ubifs_info *c, void *node,
			     int len, struct shash_desc *hash)
{
	int auth_node_size = ubifs_auth_node_sz(c);
	int err;

	while (1) {
		const struct ubifs_ch *ch = node;
		int nodelen = le32_to_cpu(ch->len);

		ubifs_assert(c, len >= auth_node_size);

		if (len == auth_node_size)
			break;

		ubifs_assert(c, len > nodelen);
		ubifs_assert(c, ch->magic == cpu_to_le32(UBIFS_NODE_MAGIC));

		err = ubifs_shash_update(c, hash, (void *)node, nodelen);
		if (err)
			return err;

		node += ALIGN(nodelen, 8);
		len -= ALIGN(nodelen, 8);
	}

	return ubifs_prepare_auth_node(c, node, hash);
}

 
static int write_head(struct ubifs_info *c, int jhead, void *buf, int len,
		      int *lnum, int *offs, int sync)
{
	int err;
	struct ubifs_wbuf *wbuf = &c->jheads[jhead].wbuf;

	ubifs_assert(c, jhead != GCHD);

	*lnum = c->jheads[jhead].wbuf.lnum;
	*offs = c->jheads[jhead].wbuf.offs + c->jheads[jhead].wbuf.used;
	dbg_jnl("jhead %s, LEB %d:%d, len %d",
		dbg_jhead(jhead), *lnum, *offs, len);

	if (ubifs_authenticated(c)) {
		err = ubifs_hash_nodes(c, buf, len, c->jheads[jhead].log_hash);
		if (err)
			return err;
	}

	err = ubifs_wbuf_write_nolock(wbuf, buf, len);
	if (err)
		return err;
	if (sync)
		err = ubifs_wbuf_sync_nolock(wbuf);
	return err;
}

 
static int make_reservation(struct ubifs_info *c, int jhead, int len)
{
	int err, cmt_retries = 0, nospc_retries = 0;

again:
	down_read(&c->commit_sem);
	err = reserve_space(c, jhead, len);
	if (!err)
		 
		return 0;
	up_read(&c->commit_sem);

	if (err == -ENOSPC) {
		 
		if (nospc_retries++ < 2) {
			dbg_jnl("no space, retry");
			err = -EAGAIN;
		}

		 
	}

	if (err != -EAGAIN)
		goto out;

	 
	if (cmt_retries > 128) {
		 
		ubifs_err(c, "stuck in space allocation");
		err = -ENOSPC;
		goto out;
	} else if (cmt_retries > 32)
		ubifs_warn(c, "too many space allocation re-tries (%d)",
			   cmt_retries);

	dbg_jnl("-EAGAIN, commit and retry (retried %d times)",
		cmt_retries);
	cmt_retries += 1;

	err = ubifs_run_commit(c);
	if (err)
		return err;
	goto again;

out:
	ubifs_err(c, "cannot reserve %d bytes in jhead %d, error %d",
		  len, jhead, err);
	if (err == -ENOSPC) {
		 
		down_write(&c->commit_sem);
		dump_stack();
		ubifs_dump_budg(c, &c->bi);
		ubifs_dump_lprops(c);
		cmt_retries = dbg_check_lprops(c);
		up_write(&c->commit_sem);
	}
	return err;
}

 
static inline void release_head(struct ubifs_info *c, int jhead)
{
	mutex_unlock(&c->jheads[jhead].wbuf.io_mutex);
}

 
static void finish_reservation(struct ubifs_info *c)
{
	up_read(&c->commit_sem);
}

 
static int get_dent_type(int mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
		return UBIFS_ITYPE_REG;
	case S_IFDIR:
		return UBIFS_ITYPE_DIR;
	case S_IFLNK:
		return UBIFS_ITYPE_LNK;
	case S_IFBLK:
		return UBIFS_ITYPE_BLK;
	case S_IFCHR:
		return UBIFS_ITYPE_CHR;
	case S_IFIFO:
		return UBIFS_ITYPE_FIFO;
	case S_IFSOCK:
		return UBIFS_ITYPE_SOCK;
	default:
		BUG();
	}
	return 0;
}

 
static void pack_inode(struct ubifs_info *c, struct ubifs_ino_node *ino,
		       const struct inode *inode, int last)
{
	int data_len = 0, last_reference = !inode->i_nlink;
	struct ubifs_inode *ui = ubifs_inode(inode);

	ino->ch.node_type = UBIFS_INO_NODE;
	ino_key_init_flash(c, &ino->key, inode->i_ino);
	ino->creat_sqnum = cpu_to_le64(ui->creat_sqnum);
	ino->atime_sec  = cpu_to_le64(inode->i_atime.tv_sec);
	ino->atime_nsec = cpu_to_le32(inode->i_atime.tv_nsec);
	ino->ctime_sec  = cpu_to_le64(inode_get_ctime(inode).tv_sec);
	ino->ctime_nsec = cpu_to_le32(inode_get_ctime(inode).tv_nsec);
	ino->mtime_sec  = cpu_to_le64(inode->i_mtime.tv_sec);
	ino->mtime_nsec = cpu_to_le32(inode->i_mtime.tv_nsec);
	ino->uid   = cpu_to_le32(i_uid_read(inode));
	ino->gid   = cpu_to_le32(i_gid_read(inode));
	ino->mode  = cpu_to_le32(inode->i_mode);
	ino->flags = cpu_to_le32(ui->flags);
	ino->size  = cpu_to_le64(ui->ui_size);
	ino->nlink = cpu_to_le32(inode->i_nlink);
	ino->compr_type  = cpu_to_le16(ui->compr_type);
	ino->data_len    = cpu_to_le32(ui->data_len);
	ino->xattr_cnt   = cpu_to_le32(ui->xattr_cnt);
	ino->xattr_size  = cpu_to_le32(ui->xattr_size);
	ino->xattr_names = cpu_to_le32(ui->xattr_names);
	zero_ino_node_unused(ino);

	 
	if (!last_reference) {
		memcpy(ino->data, ui->data, ui->data_len);
		data_len = ui->data_len;
	}

	ubifs_prep_grp_node(c, ino, UBIFS_INO_NODE_SZ + data_len, last);
}

 
static void mark_inode_clean(struct ubifs_info *c, struct ubifs_inode *ui)
{
	if (ui->dirty)
		ubifs_release_dirty_inode_budget(c, ui);
	ui->dirty = 0;
}

static void set_dent_cookie(struct ubifs_info *c, struct ubifs_dent_node *dent)
{
	if (c->double_hash)
		dent->cookie = (__force __le32) get_random_u32();
	else
		dent->cookie = 0;
}

 
int ubifs_jnl_update(struct ubifs_info *c, const struct inode *dir,
		     const struct fscrypt_name *nm, const struct inode *inode,
		     int deletion, int xent)
{
	int err, dlen, ilen, len, lnum, ino_offs, dent_offs, orphan_added = 0;
	int aligned_dlen, aligned_ilen, sync = IS_DIRSYNC(dir);
	int last_reference = !!(deletion && inode->i_nlink == 0);
	struct ubifs_inode *ui = ubifs_inode(inode);
	struct ubifs_inode *host_ui = ubifs_inode(dir);
	struct ubifs_dent_node *dent;
	struct ubifs_ino_node *ino;
	union ubifs_key dent_key, ino_key;
	u8 hash_dent[UBIFS_HASH_ARR_SZ];
	u8 hash_ino[UBIFS_HASH_ARR_SZ];
	u8 hash_ino_host[UBIFS_HASH_ARR_SZ];

	ubifs_assert(c, mutex_is_locked(&host_ui->ui_mutex));

	dlen = UBIFS_DENT_NODE_SZ + fname_len(nm) + 1;
	ilen = UBIFS_INO_NODE_SZ;

	 
	if (!last_reference) {
		ilen += ui->data_len;
		sync |= IS_SYNC(inode);
	}

	aligned_dlen = ALIGN(dlen, 8);
	aligned_ilen = ALIGN(ilen, 8);

	len = aligned_dlen + aligned_ilen + UBIFS_INO_NODE_SZ;
	 
	if (ubifs_authenticated(c))
		len += ALIGN(host_ui->data_len, 8) + ubifs_auth_node_sz(c);
	else
		len += host_ui->data_len;

	dent = kzalloc(len, GFP_NOFS);
	if (!dent)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, len);
	if (err)
		goto out_free;

	if (!xent) {
		dent->ch.node_type = UBIFS_DENT_NODE;
		if (fname_name(nm) == NULL)
			dent_key_init_hash(c, &dent_key, dir->i_ino, nm->hash);
		else
			dent_key_init(c, &dent_key, dir->i_ino, nm);
	} else {
		dent->ch.node_type = UBIFS_XENT_NODE;
		xent_key_init(c, &dent_key, dir->i_ino, nm);
	}

	key_write(c, &dent_key, dent->key);
	dent->inum = deletion ? 0 : cpu_to_le64(inode->i_ino);
	dent->type = get_dent_type(inode->i_mode);
	dent->nlen = cpu_to_le16(fname_len(nm));
	memcpy(dent->name, fname_name(nm), fname_len(nm));
	dent->name[fname_len(nm)] = '\0';
	set_dent_cookie(c, dent);

	zero_dent_node_unused(dent);
	ubifs_prep_grp_node(c, dent, dlen, 0);
	err = ubifs_node_calc_hash(c, dent, hash_dent);
	if (err)
		goto out_release;

	ino = (void *)dent + aligned_dlen;
	pack_inode(c, ino, inode, 0);
	err = ubifs_node_calc_hash(c, ino, hash_ino);
	if (err)
		goto out_release;

	ino = (void *)ino + aligned_ilen;
	pack_inode(c, ino, dir, 1);
	err = ubifs_node_calc_hash(c, ino, hash_ino_host);
	if (err)
		goto out_release;

	if (last_reference) {
		err = ubifs_add_orphan(c, inode->i_ino);
		if (err) {
			release_head(c, BASEHD);
			goto out_finish;
		}
		ui->del_cmtno = c->cmt_no;
		orphan_added = 1;
	}

	err = write_head(c, BASEHD, dent, len, &lnum, &dent_offs, sync);
	if (err)
		goto out_release;
	if (!sync) {
		struct ubifs_wbuf *wbuf = &c->jheads[BASEHD].wbuf;

		ubifs_wbuf_add_ino_nolock(wbuf, inode->i_ino);
		ubifs_wbuf_add_ino_nolock(wbuf, dir->i_ino);
	}
	release_head(c, BASEHD);
	kfree(dent);
	ubifs_add_auth_dirt(c, lnum);

	if (deletion) {
		if (fname_name(nm) == NULL)
			err = ubifs_tnc_remove_dh(c, &dent_key, nm->minor_hash);
		else
			err = ubifs_tnc_remove_nm(c, &dent_key, nm);
		if (err)
			goto out_ro;
		err = ubifs_add_dirt(c, lnum, dlen);
	} else
		err = ubifs_tnc_add_nm(c, &dent_key, lnum, dent_offs, dlen,
				       hash_dent, nm);
	if (err)
		goto out_ro;

	 
	ino_key_init(c, &ino_key, inode->i_ino);
	ino_offs = dent_offs + aligned_dlen;
	err = ubifs_tnc_add(c, &ino_key, lnum, ino_offs, ilen, hash_ino);
	if (err)
		goto out_ro;

	ino_key_init(c, &ino_key, dir->i_ino);
	ino_offs += aligned_ilen;
	err = ubifs_tnc_add(c, &ino_key, lnum, ino_offs,
			    UBIFS_INO_NODE_SZ + host_ui->data_len, hash_ino_host);
	if (err)
		goto out_ro;

	finish_reservation(c);
	spin_lock(&ui->ui_lock);
	ui->synced_i_size = ui->ui_size;
	spin_unlock(&ui->ui_lock);
	if (xent) {
		spin_lock(&host_ui->ui_lock);
		host_ui->synced_i_size = host_ui->ui_size;
		spin_unlock(&host_ui->ui_lock);
	}
	mark_inode_clean(c, ui);
	mark_inode_clean(c, host_ui);
	return 0;

out_finish:
	finish_reservation(c);
out_free:
	kfree(dent);
	return err;

out_release:
	release_head(c, BASEHD);
	kfree(dent);
out_ro:
	ubifs_ro_mode(c, err);
	if (orphan_added)
		ubifs_delete_orphan(c, inode->i_ino);
	finish_reservation(c);
	return err;
}

 
int ubifs_jnl_write_data(struct ubifs_info *c, const struct inode *inode,
			 const union ubifs_key *key, const void *buf, int len)
{
	struct ubifs_data_node *data;
	int err, lnum, offs, compr_type, out_len, compr_len, auth_len;
	int dlen = COMPRESSED_DATA_NODE_BUF_SZ, allocated = 1;
	int write_len;
	struct ubifs_inode *ui = ubifs_inode(inode);
	bool encrypted = IS_ENCRYPTED(inode);
	u8 hash[UBIFS_HASH_ARR_SZ];

	dbg_jnlk(key, "ino %lu, blk %u, len %d, key ",
		(unsigned long)key_inum(c, key), key_block(c, key), len);
	ubifs_assert(c, len <= UBIFS_BLOCK_SIZE);

	if (encrypted)
		dlen += UBIFS_CIPHER_BLOCK_SIZE;

	auth_len = ubifs_auth_node_sz(c);

	data = kmalloc(dlen + auth_len, GFP_NOFS | __GFP_NOWARN);
	if (!data) {
		 
		allocated = 0;
		mutex_lock(&c->write_reserve_mutex);
		data = c->write_reserve_buf;
	}

	data->ch.node_type = UBIFS_DATA_NODE;
	key_write(c, key, &data->key);
	data->size = cpu_to_le32(len);

	if (!(ui->flags & UBIFS_COMPR_FL))
		 
		compr_type = UBIFS_COMPR_NONE;
	else
		compr_type = ui->compr_type;

	out_len = compr_len = dlen - UBIFS_DATA_NODE_SZ;
	ubifs_compress(c, buf, len, &data->data, &compr_len, &compr_type);
	ubifs_assert(c, compr_len <= UBIFS_BLOCK_SIZE);

	if (encrypted) {
		err = ubifs_encrypt(inode, data, compr_len, &out_len, key_block(c, key));
		if (err)
			goto out_free;

	} else {
		data->compr_size = 0;
		out_len = compr_len;
	}

	dlen = UBIFS_DATA_NODE_SZ + out_len;
	if (ubifs_authenticated(c))
		write_len = ALIGN(dlen, 8) + auth_len;
	else
		write_len = dlen;

	data->compr_type = cpu_to_le16(compr_type);

	 
	err = make_reservation(c, DATAHD, write_len);
	if (err)
		goto out_free;

	ubifs_prepare_node(c, data, dlen, 0);
	err = write_head(c, DATAHD, data, write_len, &lnum, &offs, 0);
	if (err)
		goto out_release;

	err = ubifs_node_calc_hash(c, data, hash);
	if (err)
		goto out_release;

	ubifs_wbuf_add_ino_nolock(&c->jheads[DATAHD].wbuf, key_inum(c, key));
	release_head(c, DATAHD);

	ubifs_add_auth_dirt(c, lnum);

	err = ubifs_tnc_add(c, key, lnum, offs, dlen, hash);
	if (err)
		goto out_ro;

	finish_reservation(c);
	if (!allocated)
		mutex_unlock(&c->write_reserve_mutex);
	else
		kfree(data);
	return 0;

out_release:
	release_head(c, DATAHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
out_free:
	if (!allocated)
		mutex_unlock(&c->write_reserve_mutex);
	else
		kfree(data);
	return err;
}

 
int ubifs_jnl_write_inode(struct ubifs_info *c, const struct inode *inode)
{
	int err, lnum, offs;
	struct ubifs_ino_node *ino, *ino_start;
	struct ubifs_inode *ui = ubifs_inode(inode);
	int sync = 0, write_len = 0, ilen = UBIFS_INO_NODE_SZ;
	int last_reference = !inode->i_nlink;
	int kill_xattrs = ui->xattr_cnt && last_reference;
	u8 hash[UBIFS_HASH_ARR_SZ];

	dbg_jnl("ino %lu, nlink %u", inode->i_ino, inode->i_nlink);

	 
	if (!last_reference) {
		ilen += ui->data_len;
		sync = IS_SYNC(inode);
	} else if (kill_xattrs) {
		write_len += UBIFS_INO_NODE_SZ * ui->xattr_cnt;
	}

	if (ubifs_authenticated(c))
		write_len += ALIGN(ilen, 8) + ubifs_auth_node_sz(c);
	else
		write_len += ilen;

	ino_start = ino = kmalloc(write_len, GFP_NOFS);
	if (!ino)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, write_len);
	if (err)
		goto out_free;

	if (kill_xattrs) {
		union ubifs_key key;
		struct fscrypt_name nm = {0};
		struct inode *xino;
		struct ubifs_dent_node *xent, *pxent = NULL;

		if (ui->xattr_cnt > ubifs_xattr_max_cnt(c)) {
			err = -EPERM;
			ubifs_err(c, "Cannot delete inode, it has too much xattrs!");
			goto out_release;
		}

		lowest_xent_key(c, &key, inode->i_ino);
		while (1) {
			xent = ubifs_tnc_next_ent(c, &key, &nm);
			if (IS_ERR(xent)) {
				err = PTR_ERR(xent);
				if (err == -ENOENT)
					break;

				kfree(pxent);
				goto out_release;
			}

			fname_name(&nm) = xent->name;
			fname_len(&nm) = le16_to_cpu(xent->nlen);

			xino = ubifs_iget(c->vfs_sb, le64_to_cpu(xent->inum));
			if (IS_ERR(xino)) {
				err = PTR_ERR(xino);
				ubifs_err(c, "dead directory entry '%s', error %d",
					  xent->name, err);
				ubifs_ro_mode(c, err);
				kfree(pxent);
				kfree(xent);
				goto out_release;
			}
			ubifs_assert(c, ubifs_inode(xino)->xattr);

			clear_nlink(xino);
			pack_inode(c, ino, xino, 0);
			ino = (void *)ino + UBIFS_INO_NODE_SZ;
			iput(xino);

			kfree(pxent);
			pxent = xent;
			key_read(c, &xent->key, &key);
		}
		kfree(pxent);
	}

	pack_inode(c, ino, inode, 1);
	err = ubifs_node_calc_hash(c, ino, hash);
	if (err)
		goto out_release;

	err = write_head(c, BASEHD, ino_start, write_len, &lnum, &offs, sync);
	if (err)
		goto out_release;
	if (!sync)
		ubifs_wbuf_add_ino_nolock(&c->jheads[BASEHD].wbuf,
					  inode->i_ino);
	release_head(c, BASEHD);

	if (last_reference) {
		err = ubifs_tnc_remove_ino(c, inode->i_ino);
		if (err)
			goto out_ro;
		ubifs_delete_orphan(c, inode->i_ino);
		err = ubifs_add_dirt(c, lnum, write_len);
	} else {
		union ubifs_key key;

		ubifs_add_auth_dirt(c, lnum);

		ino_key_init(c, &key, inode->i_ino);
		err = ubifs_tnc_add(c, &key, lnum, offs, ilen, hash);
	}
	if (err)
		goto out_ro;

	finish_reservation(c);
	spin_lock(&ui->ui_lock);
	ui->synced_i_size = ui->ui_size;
	spin_unlock(&ui->ui_lock);
	kfree(ino_start);
	return 0;

out_release:
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
out_free:
	kfree(ino_start);
	return err;
}

 
int ubifs_jnl_delete_inode(struct ubifs_info *c, const struct inode *inode)
{
	int err;
	struct ubifs_inode *ui = ubifs_inode(inode);

	ubifs_assert(c, inode->i_nlink == 0);

	if (ui->xattr_cnt || ui->del_cmtno != c->cmt_no)
		 
		return ubifs_jnl_write_inode(c, inode);

	down_read(&c->commit_sem);
	 
	if (ui->del_cmtno != c->cmt_no) {
		up_read(&c->commit_sem);
		return ubifs_jnl_write_inode(c, inode);
	}

	err = ubifs_tnc_remove_ino(c, inode->i_ino);
	if (err)
		ubifs_ro_mode(c, err);
	else
		ubifs_delete_orphan(c, inode->i_ino);
	up_read(&c->commit_sem);
	return err;
}

 
int ubifs_jnl_xrename(struct ubifs_info *c, const struct inode *fst_dir,
		      const struct inode *fst_inode,
		      const struct fscrypt_name *fst_nm,
		      const struct inode *snd_dir,
		      const struct inode *snd_inode,
		      const struct fscrypt_name *snd_nm, int sync)
{
	union ubifs_key key;
	struct ubifs_dent_node *dent1, *dent2;
	int err, dlen1, dlen2, lnum, offs, len, plen = UBIFS_INO_NODE_SZ;
	int aligned_dlen1, aligned_dlen2;
	int twoparents = (fst_dir != snd_dir);
	void *p;
	u8 hash_dent1[UBIFS_HASH_ARR_SZ];
	u8 hash_dent2[UBIFS_HASH_ARR_SZ];
	u8 hash_p1[UBIFS_HASH_ARR_SZ];
	u8 hash_p2[UBIFS_HASH_ARR_SZ];

	ubifs_assert(c, ubifs_inode(fst_dir)->data_len == 0);
	ubifs_assert(c, ubifs_inode(snd_dir)->data_len == 0);
	ubifs_assert(c, mutex_is_locked(&ubifs_inode(fst_dir)->ui_mutex));
	ubifs_assert(c, mutex_is_locked(&ubifs_inode(snd_dir)->ui_mutex));

	dlen1 = UBIFS_DENT_NODE_SZ + fname_len(snd_nm) + 1;
	dlen2 = UBIFS_DENT_NODE_SZ + fname_len(fst_nm) + 1;
	aligned_dlen1 = ALIGN(dlen1, 8);
	aligned_dlen2 = ALIGN(dlen2, 8);

	len = aligned_dlen1 + aligned_dlen2 + ALIGN(plen, 8);
	if (twoparents)
		len += plen;

	len += ubifs_auth_node_sz(c);

	dent1 = kzalloc(len, GFP_NOFS);
	if (!dent1)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, len);
	if (err)
		goto out_free;

	 
	dent1->ch.node_type = UBIFS_DENT_NODE;
	dent_key_init_flash(c, &dent1->key, snd_dir->i_ino, snd_nm);
	dent1->inum = cpu_to_le64(fst_inode->i_ino);
	dent1->type = get_dent_type(fst_inode->i_mode);
	dent1->nlen = cpu_to_le16(fname_len(snd_nm));
	memcpy(dent1->name, fname_name(snd_nm), fname_len(snd_nm));
	dent1->name[fname_len(snd_nm)] = '\0';
	set_dent_cookie(c, dent1);
	zero_dent_node_unused(dent1);
	ubifs_prep_grp_node(c, dent1, dlen1, 0);
	err = ubifs_node_calc_hash(c, dent1, hash_dent1);
	if (err)
		goto out_release;

	 
	dent2 = (void *)dent1 + aligned_dlen1;
	dent2->ch.node_type = UBIFS_DENT_NODE;
	dent_key_init_flash(c, &dent2->key, fst_dir->i_ino, fst_nm);
	dent2->inum = cpu_to_le64(snd_inode->i_ino);
	dent2->type = get_dent_type(snd_inode->i_mode);
	dent2->nlen = cpu_to_le16(fname_len(fst_nm));
	memcpy(dent2->name, fname_name(fst_nm), fname_len(fst_nm));
	dent2->name[fname_len(fst_nm)] = '\0';
	set_dent_cookie(c, dent2);
	zero_dent_node_unused(dent2);
	ubifs_prep_grp_node(c, dent2, dlen2, 0);
	err = ubifs_node_calc_hash(c, dent2, hash_dent2);
	if (err)
		goto out_release;

	p = (void *)dent2 + aligned_dlen2;
	if (!twoparents) {
		pack_inode(c, p, fst_dir, 1);
		err = ubifs_node_calc_hash(c, p, hash_p1);
		if (err)
			goto out_release;
	} else {
		pack_inode(c, p, fst_dir, 0);
		err = ubifs_node_calc_hash(c, p, hash_p1);
		if (err)
			goto out_release;
		p += ALIGN(plen, 8);
		pack_inode(c, p, snd_dir, 1);
		err = ubifs_node_calc_hash(c, p, hash_p2);
		if (err)
			goto out_release;
	}

	err = write_head(c, BASEHD, dent1, len, &lnum, &offs, sync);
	if (err)
		goto out_release;
	if (!sync) {
		struct ubifs_wbuf *wbuf = &c->jheads[BASEHD].wbuf;

		ubifs_wbuf_add_ino_nolock(wbuf, fst_dir->i_ino);
		ubifs_wbuf_add_ino_nolock(wbuf, snd_dir->i_ino);
	}
	release_head(c, BASEHD);

	ubifs_add_auth_dirt(c, lnum);

	dent_key_init(c, &key, snd_dir->i_ino, snd_nm);
	err = ubifs_tnc_add_nm(c, &key, lnum, offs, dlen1, hash_dent1, snd_nm);
	if (err)
		goto out_ro;

	offs += aligned_dlen1;
	dent_key_init(c, &key, fst_dir->i_ino, fst_nm);
	err = ubifs_tnc_add_nm(c, &key, lnum, offs, dlen2, hash_dent2, fst_nm);
	if (err)
		goto out_ro;

	offs += aligned_dlen2;

	ino_key_init(c, &key, fst_dir->i_ino);
	err = ubifs_tnc_add(c, &key, lnum, offs, plen, hash_p1);
	if (err)
		goto out_ro;

	if (twoparents) {
		offs += ALIGN(plen, 8);
		ino_key_init(c, &key, snd_dir->i_ino);
		err = ubifs_tnc_add(c, &key, lnum, offs, plen, hash_p2);
		if (err)
			goto out_ro;
	}

	finish_reservation(c);

	mark_inode_clean(c, ubifs_inode(fst_dir));
	if (twoparents)
		mark_inode_clean(c, ubifs_inode(snd_dir));
	kfree(dent1);
	return 0;

out_release:
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
out_free:
	kfree(dent1);
	return err;
}

 
int ubifs_jnl_rename(struct ubifs_info *c, const struct inode *old_dir,
		     const struct inode *old_inode,
		     const struct fscrypt_name *old_nm,
		     const struct inode *new_dir,
		     const struct inode *new_inode,
		     const struct fscrypt_name *new_nm,
		     const struct inode *whiteout, int sync)
{
	void *p;
	union ubifs_key key;
	struct ubifs_dent_node *dent, *dent2;
	int err, dlen1, dlen2, ilen, wlen, lnum, offs, len, orphan_added = 0;
	int aligned_dlen1, aligned_dlen2, plen = UBIFS_INO_NODE_SZ;
	int last_reference = !!(new_inode && new_inode->i_nlink == 0);
	int move = (old_dir != new_dir);
	struct ubifs_inode *new_ui, *whiteout_ui;
	u8 hash_old_dir[UBIFS_HASH_ARR_SZ];
	u8 hash_new_dir[UBIFS_HASH_ARR_SZ];
	u8 hash_new_inode[UBIFS_HASH_ARR_SZ];
	u8 hash_whiteout_inode[UBIFS_HASH_ARR_SZ];
	u8 hash_dent1[UBIFS_HASH_ARR_SZ];
	u8 hash_dent2[UBIFS_HASH_ARR_SZ];

	ubifs_assert(c, ubifs_inode(old_dir)->data_len == 0);
	ubifs_assert(c, ubifs_inode(new_dir)->data_len == 0);
	ubifs_assert(c, mutex_is_locked(&ubifs_inode(old_dir)->ui_mutex));
	ubifs_assert(c, mutex_is_locked(&ubifs_inode(new_dir)->ui_mutex));

	dlen1 = UBIFS_DENT_NODE_SZ + fname_len(new_nm) + 1;
	dlen2 = UBIFS_DENT_NODE_SZ + fname_len(old_nm) + 1;
	if (new_inode) {
		new_ui = ubifs_inode(new_inode);
		ubifs_assert(c, mutex_is_locked(&new_ui->ui_mutex));
		ilen = UBIFS_INO_NODE_SZ;
		if (!last_reference)
			ilen += new_ui->data_len;
	} else
		ilen = 0;

	if (whiteout) {
		whiteout_ui = ubifs_inode(whiteout);
		ubifs_assert(c, mutex_is_locked(&whiteout_ui->ui_mutex));
		ubifs_assert(c, whiteout->i_nlink == 1);
		ubifs_assert(c, !whiteout_ui->dirty);
		wlen = UBIFS_INO_NODE_SZ;
		wlen += whiteout_ui->data_len;
	} else
		wlen = 0;

	aligned_dlen1 = ALIGN(dlen1, 8);
	aligned_dlen2 = ALIGN(dlen2, 8);
	len = aligned_dlen1 + aligned_dlen2 + ALIGN(ilen, 8) +
	      ALIGN(wlen, 8) + ALIGN(plen, 8);
	if (move)
		len += plen;

	len += ubifs_auth_node_sz(c);

	dent = kzalloc(len, GFP_NOFS);
	if (!dent)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, len);
	if (err)
		goto out_free;

	 
	dent->ch.node_type = UBIFS_DENT_NODE;
	dent_key_init_flash(c, &dent->key, new_dir->i_ino, new_nm);
	dent->inum = cpu_to_le64(old_inode->i_ino);
	dent->type = get_dent_type(old_inode->i_mode);
	dent->nlen = cpu_to_le16(fname_len(new_nm));
	memcpy(dent->name, fname_name(new_nm), fname_len(new_nm));
	dent->name[fname_len(new_nm)] = '\0';
	set_dent_cookie(c, dent);
	zero_dent_node_unused(dent);
	ubifs_prep_grp_node(c, dent, dlen1, 0);
	err = ubifs_node_calc_hash(c, dent, hash_dent1);
	if (err)
		goto out_release;

	dent2 = (void *)dent + aligned_dlen1;
	dent2->ch.node_type = UBIFS_DENT_NODE;
	dent_key_init_flash(c, &dent2->key, old_dir->i_ino, old_nm);

	if (whiteout) {
		dent2->inum = cpu_to_le64(whiteout->i_ino);
		dent2->type = get_dent_type(whiteout->i_mode);
	} else {
		 
		dent2->inum = 0;
		dent2->type = DT_UNKNOWN;
	}
	dent2->nlen = cpu_to_le16(fname_len(old_nm));
	memcpy(dent2->name, fname_name(old_nm), fname_len(old_nm));
	dent2->name[fname_len(old_nm)] = '\0';
	set_dent_cookie(c, dent2);
	zero_dent_node_unused(dent2);
	ubifs_prep_grp_node(c, dent2, dlen2, 0);
	err = ubifs_node_calc_hash(c, dent2, hash_dent2);
	if (err)
		goto out_release;

	p = (void *)dent2 + aligned_dlen2;
	if (new_inode) {
		pack_inode(c, p, new_inode, 0);
		err = ubifs_node_calc_hash(c, p, hash_new_inode);
		if (err)
			goto out_release;

		p += ALIGN(ilen, 8);
	}

	if (whiteout) {
		pack_inode(c, p, whiteout, 0);
		err = ubifs_node_calc_hash(c, p, hash_whiteout_inode);
		if (err)
			goto out_release;

		p += ALIGN(wlen, 8);
	}

	if (!move) {
		pack_inode(c, p, old_dir, 1);
		err = ubifs_node_calc_hash(c, p, hash_old_dir);
		if (err)
			goto out_release;
	} else {
		pack_inode(c, p, old_dir, 0);
		err = ubifs_node_calc_hash(c, p, hash_old_dir);
		if (err)
			goto out_release;

		p += ALIGN(plen, 8);
		pack_inode(c, p, new_dir, 1);
		err = ubifs_node_calc_hash(c, p, hash_new_dir);
		if (err)
			goto out_release;
	}

	if (last_reference) {
		err = ubifs_add_orphan(c, new_inode->i_ino);
		if (err) {
			release_head(c, BASEHD);
			goto out_finish;
		}
		new_ui->del_cmtno = c->cmt_no;
		orphan_added = 1;
	}

	err = write_head(c, BASEHD, dent, len, &lnum, &offs, sync);
	if (err)
		goto out_release;
	if (!sync) {
		struct ubifs_wbuf *wbuf = &c->jheads[BASEHD].wbuf;

		ubifs_wbuf_add_ino_nolock(wbuf, new_dir->i_ino);
		ubifs_wbuf_add_ino_nolock(wbuf, old_dir->i_ino);
		if (new_inode)
			ubifs_wbuf_add_ino_nolock(&c->jheads[BASEHD].wbuf,
						  new_inode->i_ino);
		if (whiteout)
			ubifs_wbuf_add_ino_nolock(&c->jheads[BASEHD].wbuf,
						  whiteout->i_ino);
	}
	release_head(c, BASEHD);

	ubifs_add_auth_dirt(c, lnum);

	dent_key_init(c, &key, new_dir->i_ino, new_nm);
	err = ubifs_tnc_add_nm(c, &key, lnum, offs, dlen1, hash_dent1, new_nm);
	if (err)
		goto out_ro;

	offs += aligned_dlen1;
	if (whiteout) {
		dent_key_init(c, &key, old_dir->i_ino, old_nm);
		err = ubifs_tnc_add_nm(c, &key, lnum, offs, dlen2, hash_dent2, old_nm);
		if (err)
			goto out_ro;
	} else {
		err = ubifs_add_dirt(c, lnum, dlen2);
		if (err)
			goto out_ro;

		dent_key_init(c, &key, old_dir->i_ino, old_nm);
		err = ubifs_tnc_remove_nm(c, &key, old_nm);
		if (err)
			goto out_ro;
	}

	offs += aligned_dlen2;
	if (new_inode) {
		ino_key_init(c, &key, new_inode->i_ino);
		err = ubifs_tnc_add(c, &key, lnum, offs, ilen, hash_new_inode);
		if (err)
			goto out_ro;
		offs += ALIGN(ilen, 8);
	}

	if (whiteout) {
		ino_key_init(c, &key, whiteout->i_ino);
		err = ubifs_tnc_add(c, &key, lnum, offs, wlen,
				    hash_whiteout_inode);
		if (err)
			goto out_ro;
		offs += ALIGN(wlen, 8);
	}

	ino_key_init(c, &key, old_dir->i_ino);
	err = ubifs_tnc_add(c, &key, lnum, offs, plen, hash_old_dir);
	if (err)
		goto out_ro;

	if (move) {
		offs += ALIGN(plen, 8);
		ino_key_init(c, &key, new_dir->i_ino);
		err = ubifs_tnc_add(c, &key, lnum, offs, plen, hash_new_dir);
		if (err)
			goto out_ro;
	}

	finish_reservation(c);
	if (new_inode) {
		mark_inode_clean(c, new_ui);
		spin_lock(&new_ui->ui_lock);
		new_ui->synced_i_size = new_ui->ui_size;
		spin_unlock(&new_ui->ui_lock);
	}
	 
	mark_inode_clean(c, ubifs_inode(old_dir));
	if (move)
		mark_inode_clean(c, ubifs_inode(new_dir));
	kfree(dent);
	return 0;

out_release:
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	if (orphan_added)
		ubifs_delete_orphan(c, new_inode->i_ino);
out_finish:
	finish_reservation(c);
out_free:
	kfree(dent);
	return err;
}

 
static int truncate_data_node(const struct ubifs_info *c, const struct inode *inode,
			      unsigned int block, struct ubifs_data_node *dn,
			      int *new_len, int dn_size)
{
	void *buf;
	int err, dlen, compr_type, out_len, data_size;

	out_len = le32_to_cpu(dn->size);
	buf = kmalloc_array(out_len, WORST_COMPR_FACTOR, GFP_NOFS);
	if (!buf)
		return -ENOMEM;

	dlen = le32_to_cpu(dn->ch.len) - UBIFS_DATA_NODE_SZ;
	data_size = dn_size - UBIFS_DATA_NODE_SZ;
	compr_type = le16_to_cpu(dn->compr_type);

	if (IS_ENCRYPTED(inode)) {
		err = ubifs_decrypt(inode, dn, &dlen, block);
		if (err)
			goto out;
	}

	if (compr_type == UBIFS_COMPR_NONE) {
		out_len = *new_len;
	} else {
		err = ubifs_decompress(c, &dn->data, dlen, buf, &out_len, compr_type);
		if (err)
			goto out;

		ubifs_compress(c, buf, *new_len, &dn->data, &out_len, &compr_type);
	}

	if (IS_ENCRYPTED(inode)) {
		err = ubifs_encrypt(inode, dn, out_len, &data_size, block);
		if (err)
			goto out;

		out_len = data_size;
	} else {
		dn->compr_size = 0;
	}

	ubifs_assert(c, out_len <= UBIFS_BLOCK_SIZE);
	dn->compr_type = cpu_to_le16(compr_type);
	dn->size = cpu_to_le32(*new_len);
	*new_len = UBIFS_DATA_NODE_SZ + out_len;
	err = 0;
out:
	kfree(buf);
	return err;
}

 
int ubifs_jnl_truncate(struct ubifs_info *c, const struct inode *inode,
		       loff_t old_size, loff_t new_size)
{
	union ubifs_key key, to_key;
	struct ubifs_ino_node *ino;
	struct ubifs_trun_node *trun;
	struct ubifs_data_node *dn;
	int err, dlen, len, lnum, offs, bit, sz, sync = IS_SYNC(inode);
	int dn_size;
	struct ubifs_inode *ui = ubifs_inode(inode);
	ino_t inum = inode->i_ino;
	unsigned int blk;
	u8 hash_ino[UBIFS_HASH_ARR_SZ];
	u8 hash_dn[UBIFS_HASH_ARR_SZ];

	dbg_jnl("ino %lu, size %lld -> %lld",
		(unsigned long)inum, old_size, new_size);
	ubifs_assert(c, !ui->data_len);
	ubifs_assert(c, S_ISREG(inode->i_mode));
	ubifs_assert(c, mutex_is_locked(&ui->ui_mutex));

	dn_size = COMPRESSED_DATA_NODE_BUF_SZ;

	if (IS_ENCRYPTED(inode))
		dn_size += UBIFS_CIPHER_BLOCK_SIZE;

	sz =  UBIFS_TRUN_NODE_SZ + UBIFS_INO_NODE_SZ +
		dn_size + ubifs_auth_node_sz(c);

	ino = kmalloc(sz, GFP_NOFS);
	if (!ino)
		return -ENOMEM;

	trun = (void *)ino + UBIFS_INO_NODE_SZ;
	trun->ch.node_type = UBIFS_TRUN_NODE;
	trun->inum = cpu_to_le32(inum);
	trun->old_size = cpu_to_le64(old_size);
	trun->new_size = cpu_to_le64(new_size);
	zero_trun_node_unused(trun);

	dlen = new_size & (UBIFS_BLOCK_SIZE - 1);
	if (dlen) {
		 
		dn = (void *)trun + UBIFS_TRUN_NODE_SZ;
		blk = new_size >> UBIFS_BLOCK_SHIFT;
		data_key_init(c, &key, inum, blk);
		dbg_jnlk(&key, "last block key ");
		err = ubifs_tnc_lookup(c, &key, dn);
		if (err == -ENOENT)
			dlen = 0;  
		else if (err)
			goto out_free;
		else {
			int dn_len = le32_to_cpu(dn->size);

			if (dn_len <= 0 || dn_len > UBIFS_BLOCK_SIZE) {
				ubifs_err(c, "bad data node (block %u, inode %lu)",
					  blk, inode->i_ino);
				ubifs_dump_node(c, dn, dn_size);
				goto out_free;
			}

			if (dn_len <= dlen)
				dlen = 0;  
			else {
				err = truncate_data_node(c, inode, blk, dn,
						&dlen, dn_size);
				if (err)
					goto out_free;
			}
		}
	}

	 
	len = UBIFS_TRUN_NODE_SZ + UBIFS_INO_NODE_SZ;

	if (ubifs_authenticated(c))
		len += ALIGN(dlen, 8) + ubifs_auth_node_sz(c);
	else
		len += dlen;

	err = make_reservation(c, BASEHD, len);
	if (err)
		goto out_free;

	pack_inode(c, ino, inode, 0);
	err = ubifs_node_calc_hash(c, ino, hash_ino);
	if (err)
		goto out_release;

	ubifs_prep_grp_node(c, trun, UBIFS_TRUN_NODE_SZ, dlen ? 0 : 1);
	if (dlen) {
		ubifs_prep_grp_node(c, dn, dlen, 1);
		err = ubifs_node_calc_hash(c, dn, hash_dn);
		if (err)
			goto out_release;
	}

	err = write_head(c, BASEHD, ino, len, &lnum, &offs, sync);
	if (err)
		goto out_release;
	if (!sync)
		ubifs_wbuf_add_ino_nolock(&c->jheads[BASEHD].wbuf, inum);
	release_head(c, BASEHD);

	ubifs_add_auth_dirt(c, lnum);

	if (dlen) {
		sz = offs + UBIFS_INO_NODE_SZ + UBIFS_TRUN_NODE_SZ;
		err = ubifs_tnc_add(c, &key, lnum, sz, dlen, hash_dn);
		if (err)
			goto out_ro;
	}

	ino_key_init(c, &key, inum);
	err = ubifs_tnc_add(c, &key, lnum, offs, UBIFS_INO_NODE_SZ, hash_ino);
	if (err)
		goto out_ro;

	err = ubifs_add_dirt(c, lnum, UBIFS_TRUN_NODE_SZ);
	if (err)
		goto out_ro;

	bit = new_size & (UBIFS_BLOCK_SIZE - 1);
	blk = (new_size >> UBIFS_BLOCK_SHIFT) + (bit ? 1 : 0);
	data_key_init(c, &key, inum, blk);

	bit = old_size & (UBIFS_BLOCK_SIZE - 1);
	blk = (old_size >> UBIFS_BLOCK_SHIFT) - (bit ? 0 : 1);
	data_key_init(c, &to_key, inum, blk);

	err = ubifs_tnc_remove_range(c, &key, &to_key);
	if (err)
		goto out_ro;

	finish_reservation(c);
	spin_lock(&ui->ui_lock);
	ui->synced_i_size = ui->ui_size;
	spin_unlock(&ui->ui_lock);
	mark_inode_clean(c, ui);
	kfree(ino);
	return 0;

out_release:
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
out_free:
	kfree(ino);
	return err;
}


 
int ubifs_jnl_delete_xattr(struct ubifs_info *c, const struct inode *host,
			   const struct inode *inode,
			   const struct fscrypt_name *nm)
{
	int err, xlen, hlen, len, lnum, xent_offs, aligned_xlen, write_len;
	struct ubifs_dent_node *xent;
	struct ubifs_ino_node *ino;
	union ubifs_key xent_key, key1, key2;
	int sync = IS_DIRSYNC(host);
	struct ubifs_inode *host_ui = ubifs_inode(host);
	u8 hash[UBIFS_HASH_ARR_SZ];

	ubifs_assert(c, inode->i_nlink == 0);
	ubifs_assert(c, mutex_is_locked(&host_ui->ui_mutex));

	 
	xlen = UBIFS_DENT_NODE_SZ + fname_len(nm) + 1;
	aligned_xlen = ALIGN(xlen, 8);
	hlen = host_ui->data_len + UBIFS_INO_NODE_SZ;
	len = aligned_xlen + UBIFS_INO_NODE_SZ + ALIGN(hlen, 8);

	write_len = len + ubifs_auth_node_sz(c);

	xent = kzalloc(write_len, GFP_NOFS);
	if (!xent)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, write_len);
	if (err) {
		kfree(xent);
		return err;
	}

	xent->ch.node_type = UBIFS_XENT_NODE;
	xent_key_init(c, &xent_key, host->i_ino, nm);
	key_write(c, &xent_key, xent->key);
	xent->inum = 0;
	xent->type = get_dent_type(inode->i_mode);
	xent->nlen = cpu_to_le16(fname_len(nm));
	memcpy(xent->name, fname_name(nm), fname_len(nm));
	xent->name[fname_len(nm)] = '\0';
	zero_dent_node_unused(xent);
	ubifs_prep_grp_node(c, xent, xlen, 0);

	ino = (void *)xent + aligned_xlen;
	pack_inode(c, ino, inode, 0);
	ino = (void *)ino + UBIFS_INO_NODE_SZ;
	pack_inode(c, ino, host, 1);
	err = ubifs_node_calc_hash(c, ino, hash);
	if (err)
		goto out_release;

	err = write_head(c, BASEHD, xent, write_len, &lnum, &xent_offs, sync);
	if (!sync && !err)
		ubifs_wbuf_add_ino_nolock(&c->jheads[BASEHD].wbuf, host->i_ino);
	release_head(c, BASEHD);

	ubifs_add_auth_dirt(c, lnum);
	kfree(xent);
	if (err)
		goto out_ro;

	 
	err = ubifs_tnc_remove_nm(c, &xent_key, nm);
	if (err)
		goto out_ro;
	err = ubifs_add_dirt(c, lnum, xlen);
	if (err)
		goto out_ro;

	 
	lowest_ino_key(c, &key1, inode->i_ino);
	highest_ino_key(c, &key2, inode->i_ino);
	err = ubifs_tnc_remove_range(c, &key1, &key2);
	if (err)
		goto out_ro;
	err = ubifs_add_dirt(c, lnum, UBIFS_INO_NODE_SZ);
	if (err)
		goto out_ro;

	 
	ino_key_init(c, &key1, host->i_ino);
	err = ubifs_tnc_add(c, &key1, lnum, xent_offs + len - hlen, hlen, hash);
	if (err)
		goto out_ro;

	finish_reservation(c);
	spin_lock(&host_ui->ui_lock);
	host_ui->synced_i_size = host_ui->ui_size;
	spin_unlock(&host_ui->ui_lock);
	mark_inode_clean(c, host_ui);
	return 0;

out_release:
	kfree(xent);
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
	return err;
}

 
int ubifs_jnl_change_xattr(struct ubifs_info *c, const struct inode *inode,
			   const struct inode *host)
{
	int err, len1, len2, aligned_len, aligned_len1, lnum, offs;
	struct ubifs_inode *host_ui = ubifs_inode(host);
	struct ubifs_ino_node *ino;
	union ubifs_key key;
	int sync = IS_DIRSYNC(host);
	u8 hash_host[UBIFS_HASH_ARR_SZ];
	u8 hash[UBIFS_HASH_ARR_SZ];

	dbg_jnl("ino %lu, ino %lu", host->i_ino, inode->i_ino);
	ubifs_assert(c, inode->i_nlink > 0);
	ubifs_assert(c, mutex_is_locked(&host_ui->ui_mutex));

	len1 = UBIFS_INO_NODE_SZ + host_ui->data_len;
	len2 = UBIFS_INO_NODE_SZ + ubifs_inode(inode)->data_len;
	aligned_len1 = ALIGN(len1, 8);
	aligned_len = aligned_len1 + ALIGN(len2, 8);

	aligned_len += ubifs_auth_node_sz(c);

	ino = kzalloc(aligned_len, GFP_NOFS);
	if (!ino)
		return -ENOMEM;

	 
	err = make_reservation(c, BASEHD, aligned_len);
	if (err)
		goto out_free;

	pack_inode(c, ino, host, 0);
	err = ubifs_node_calc_hash(c, ino, hash_host);
	if (err)
		goto out_release;
	pack_inode(c, (void *)ino + aligned_len1, inode, 1);
	err = ubifs_node_calc_hash(c, (void *)ino + aligned_len1, hash);
	if (err)
		goto out_release;

	err = write_head(c, BASEHD, ino, aligned_len, &lnum, &offs, 0);
	if (!sync && !err) {
		struct ubifs_wbuf *wbuf = &c->jheads[BASEHD].wbuf;

		ubifs_wbuf_add_ino_nolock(wbuf, host->i_ino);
		ubifs_wbuf_add_ino_nolock(wbuf, inode->i_ino);
	}
	release_head(c, BASEHD);
	if (err)
		goto out_ro;

	ubifs_add_auth_dirt(c, lnum);

	ino_key_init(c, &key, host->i_ino);
	err = ubifs_tnc_add(c, &key, lnum, offs, len1, hash_host);
	if (err)
		goto out_ro;

	ino_key_init(c, &key, inode->i_ino);
	err = ubifs_tnc_add(c, &key, lnum, offs + aligned_len1, len2, hash);
	if (err)
		goto out_ro;

	finish_reservation(c);
	spin_lock(&host_ui->ui_lock);
	host_ui->synced_i_size = host_ui->ui_size;
	spin_unlock(&host_ui->ui_lock);
	mark_inode_clean(c, host_ui);
	kfree(ino);
	return 0;

out_release:
	release_head(c, BASEHD);
out_ro:
	ubifs_ro_mode(c, err);
	finish_reservation(c);
out_free:
	kfree(ino);
	return err;
}

