
 

 

#include "ubifs.h"
#include <linux/list_sort.h>
#include <crypto/hash.h>
#include <crypto/algapi.h>

 
struct replay_entry {
	int lnum;
	int offs;
	int len;
	u8 hash[UBIFS_HASH_ARR_SZ];
	unsigned int deletion:1;
	unsigned long long sqnum;
	struct list_head list;
	union ubifs_key key;
	union {
		struct fscrypt_name nm;
		struct {
			loff_t old_size;
			loff_t new_size;
		};
	};
};

 
struct bud_entry {
	struct list_head list;
	struct ubifs_bud *bud;
	unsigned long long sqnum;
	int free;
	int dirty;
};

 
static int set_bud_lprops(struct ubifs_info *c, struct bud_entry *b)
{
	const struct ubifs_lprops *lp;
	int err = 0, dirty;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, b->bud->lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	dirty = lp->dirty;
	if (b->bud->start == 0 && (lp->free != c->leb_size || lp->dirty != 0)) {
		 
		dbg_mnt("bud LEB %d was GC'd (%d free, %d dirty)", b->bud->lnum,
			lp->free, lp->dirty);
		dbg_gc("bud LEB %d was GC'd (%d free, %d dirty)", b->bud->lnum,
			lp->free, lp->dirty);
		dirty -= c->leb_size - lp->free;
		 
		if (dirty != 0)
			dbg_mnt("LEB %d lp: %d free %d dirty replay: %d free %d dirty",
				b->bud->lnum, lp->free, lp->dirty, b->free,
				b->dirty);
	}
	lp = ubifs_change_lp(c, lp, b->free, dirty + b->dirty,
			     lp->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	 
	err = ubifs_wbuf_seek_nolock(&c->jheads[b->bud->jhead].wbuf,
				     b->bud->lnum, c->leb_size - b->free);

out:
	ubifs_release_lprops(c);
	return err;
}

 
static int set_buds_lprops(struct ubifs_info *c)
{
	struct bud_entry *b;
	int err;

	list_for_each_entry(b, &c->replay_buds, list) {
		err = set_bud_lprops(c, b);
		if (err)
			return err;
	}

	return 0;
}

 
static int trun_remove_range(struct ubifs_info *c, struct replay_entry *r)
{
	unsigned min_blk, max_blk;
	union ubifs_key min_key, max_key;
	ino_t ino;

	min_blk = r->new_size / UBIFS_BLOCK_SIZE;
	if (r->new_size & (UBIFS_BLOCK_SIZE - 1))
		min_blk += 1;

	max_blk = r->old_size / UBIFS_BLOCK_SIZE;
	if ((r->old_size & (UBIFS_BLOCK_SIZE - 1)) == 0)
		max_blk -= 1;

	ino = key_inum(c, &r->key);

	data_key_init(c, &min_key, ino, min_blk);
	data_key_init(c, &max_key, ino, max_blk);

	return ubifs_tnc_remove_range(c, &min_key, &max_key);
}

 
static bool inode_still_linked(struct ubifs_info *c, struct replay_entry *rino)
{
	struct replay_entry *r;

	ubifs_assert(c, rino->deletion);
	ubifs_assert(c, key_type(c, &rino->key) == UBIFS_INO_KEY);

	 
	list_for_each_entry_reverse(r, &c->replay_list, list) {
		ubifs_assert(c, r->sqnum >= rino->sqnum);
		if (key_inum(c, &r->key) == key_inum(c, &rino->key) &&
		    key_type(c, &r->key) == UBIFS_INO_KEY)
			return r->deletion == 0;

	}

	ubifs_assert(c, 0);
	return false;
}

 
static int apply_replay_entry(struct ubifs_info *c, struct replay_entry *r)
{
	int err;

	dbg_mntk(&r->key, "LEB %d:%d len %d deletion %d sqnum %llu key ",
		 r->lnum, r->offs, r->len, r->deletion, r->sqnum);

	if (is_hash_key(c, &r->key)) {
		if (r->deletion)
			err = ubifs_tnc_remove_nm(c, &r->key, &r->nm);
		else
			err = ubifs_tnc_add_nm(c, &r->key, r->lnum, r->offs,
					       r->len, r->hash, &r->nm);
	} else {
		if (r->deletion)
			switch (key_type(c, &r->key)) {
			case UBIFS_INO_KEY:
			{
				ino_t inum = key_inum(c, &r->key);

				if (inode_still_linked(c, r)) {
					err = 0;
					break;
				}

				err = ubifs_tnc_remove_ino(c, inum);
				break;
			}
			case UBIFS_TRUN_KEY:
				err = trun_remove_range(c, r);
				break;
			default:
				err = ubifs_tnc_remove(c, &r->key);
				break;
			}
		else
			err = ubifs_tnc_add(c, &r->key, r->lnum, r->offs,
					    r->len, r->hash);
		if (err)
			return err;

		if (c->need_recovery)
			err = ubifs_recover_size_accum(c, &r->key, r->deletion,
						       r->new_size);
	}

	return err;
}

 
static int replay_entries_cmp(void *priv, const struct list_head *a,
			      const struct list_head *b)
{
	struct ubifs_info *c = priv;
	struct replay_entry *ra, *rb;

	cond_resched();
	if (a == b)
		return 0;

	ra = list_entry(a, struct replay_entry, list);
	rb = list_entry(b, struct replay_entry, list);
	ubifs_assert(c, ra->sqnum != rb->sqnum);
	if (ra->sqnum > rb->sqnum)
		return 1;
	return -1;
}

 
static int apply_replay_list(struct ubifs_info *c)
{
	struct replay_entry *r;
	int err;

	list_sort(c, &c->replay_list, &replay_entries_cmp);

	list_for_each_entry(r, &c->replay_list, list) {
		cond_resched();

		err = apply_replay_entry(c, r);
		if (err)
			return err;
	}

	return 0;
}

 
static void destroy_replay_list(struct ubifs_info *c)
{
	struct replay_entry *r, *tmp;

	list_for_each_entry_safe(r, tmp, &c->replay_list, list) {
		if (is_hash_key(c, &r->key))
			kfree(fname_name(&r->nm));
		list_del(&r->list);
		kfree(r);
	}
}

 
static int insert_node(struct ubifs_info *c, int lnum, int offs, int len,
		       const u8 *hash, union ubifs_key *key,
		       unsigned long long sqnum, int deletion, int *used,
		       loff_t old_size, loff_t new_size)
{
	struct replay_entry *r;

	dbg_mntk(key, "add LEB %d:%d, key ", lnum, offs);

	if (key_inum(c, key) >= c->highest_inum)
		c->highest_inum = key_inum(c, key);

	r = kzalloc(sizeof(struct replay_entry), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	if (!deletion)
		*used += ALIGN(len, 8);
	r->lnum = lnum;
	r->offs = offs;
	r->len = len;
	ubifs_copy_hash(c, hash, r->hash);
	r->deletion = !!deletion;
	r->sqnum = sqnum;
	key_copy(c, key, &r->key);
	r->old_size = old_size;
	r->new_size = new_size;

	list_add_tail(&r->list, &c->replay_list);
	return 0;
}

 
static int insert_dent(struct ubifs_info *c, int lnum, int offs, int len,
		       const u8 *hash, union ubifs_key *key,
		       const char *name, int nlen, unsigned long long sqnum,
		       int deletion, int *used)
{
	struct replay_entry *r;
	char *nbuf;

	dbg_mntk(key, "add LEB %d:%d, key ", lnum, offs);
	if (key_inum(c, key) >= c->highest_inum)
		c->highest_inum = key_inum(c, key);

	r = kzalloc(sizeof(struct replay_entry), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	nbuf = kmalloc(nlen + 1, GFP_KERNEL);
	if (!nbuf) {
		kfree(r);
		return -ENOMEM;
	}

	if (!deletion)
		*used += ALIGN(len, 8);
	r->lnum = lnum;
	r->offs = offs;
	r->len = len;
	ubifs_copy_hash(c, hash, r->hash);
	r->deletion = !!deletion;
	r->sqnum = sqnum;
	key_copy(c, key, &r->key);
	fname_len(&r->nm) = nlen;
	memcpy(nbuf, name, nlen);
	nbuf[nlen] = '\0';
	fname_name(&r->nm) = nbuf;

	list_add_tail(&r->list, &c->replay_list);
	return 0;
}

 
int ubifs_validate_entry(struct ubifs_info *c,
			 const struct ubifs_dent_node *dent)
{
	int key_type = key_type_flash(c, dent->key);
	int nlen = le16_to_cpu(dent->nlen);

	if (le32_to_cpu(dent->ch.len) != nlen + UBIFS_DENT_NODE_SZ + 1 ||
	    dent->type >= UBIFS_ITYPES_CNT ||
	    nlen > UBIFS_MAX_NLEN || dent->name[nlen] != 0 ||
	    (key_type == UBIFS_XENT_KEY && strnlen(dent->name, nlen) != nlen) ||
	    le64_to_cpu(dent->inum) > MAX_INUM) {
		ubifs_err(c, "bad %s node", key_type == UBIFS_DENT_KEY ?
			  "directory entry" : "extended attribute entry");
		return -EINVAL;
	}

	if (key_type != UBIFS_DENT_KEY && key_type != UBIFS_XENT_KEY) {
		ubifs_err(c, "bad key type %d", key_type);
		return -EINVAL;
	}

	return 0;
}

 
static int is_last_bud(struct ubifs_info *c, struct ubifs_bud *bud)
{
	struct ubifs_jhead *jh = &c->jheads[bud->jhead];
	struct ubifs_bud *next;
	uint32_t data;
	int err;

	if (list_is_last(&bud->list, &jh->buds_list))
		return 1;

	 
	next = list_entry(bud->list.next, struct ubifs_bud, list);
	if (!list_is_last(&next->list, &jh->buds_list))
		return 0;

	err = ubifs_leb_read(c, next->lnum, (char *)&data, next->start, 4, 1);
	if (err)
		return 0;

	return data == 0xFFFFFFFF;
}

 
static int noinline_for_stack
authenticate_sleb_hash(struct ubifs_info *c,
		       struct shash_desc *log_hash, u8 *hash)
{
	SHASH_DESC_ON_STACK(hash_desc, c->hash_tfm);

	hash_desc->tfm = c->hash_tfm;

	ubifs_shash_copy_state(c, log_hash, hash_desc);
	return crypto_shash_final(hash_desc, hash);
}

 
static int authenticate_sleb(struct ubifs_info *c, struct ubifs_scan_leb *sleb,
			     struct shash_desc *log_hash, int is_last)
{
	int n_not_auth = 0;
	struct ubifs_scan_node *snod;
	int n_nodes = 0;
	int err;
	u8 hash[UBIFS_HASH_ARR_SZ];
	u8 hmac[UBIFS_HMAC_ARR_SZ];

	if (!ubifs_authenticated(c))
		return sleb->nodes_cnt;

	list_for_each_entry(snod, &sleb->nodes, list) {

		n_nodes++;

		if (snod->type == UBIFS_AUTH_NODE) {
			struct ubifs_auth_node *auth = snod->node;

			err = authenticate_sleb_hash(c, log_hash, hash);
			if (err)
				goto out;

			err = crypto_shash_tfm_digest(c->hmac_tfm, hash,
						      c->hash_len, hmac);
			if (err)
				goto out;

			err = ubifs_check_hmac(c, auth->hmac, hmac);
			if (err) {
				err = -EPERM;
				goto out;
			}
			n_not_auth = 0;
		} else {
			err = crypto_shash_update(log_hash, snod->node,
						  snod->len);
			if (err)
				goto out;
			n_not_auth++;
		}
	}

	 
	if (n_not_auth) {
		if (is_last) {
			dbg_mnt("%d unauthenticated nodes found on LEB %d, Ignoring them",
				n_not_auth, sleb->lnum);
			err = 0;
		} else {
			dbg_mnt("%d unauthenticated nodes found on non-last LEB %d",
				n_not_auth, sleb->lnum);
			err = -EPERM;
		}
	} else {
		err = 0;
	}
out:
	return err ? err : n_nodes - n_not_auth;
}

 
static int replay_bud(struct ubifs_info *c, struct bud_entry *b)
{
	int is_last = is_last_bud(c, b->bud);
	int err = 0, used = 0, lnum = b->bud->lnum, offs = b->bud->start;
	int n_nodes, n = 0;
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;

	dbg_mnt("replay bud LEB %d, head %d, offs %d, is_last %d",
		lnum, b->bud->jhead, offs, is_last);

	if (c->need_recovery && is_last)
		 
		sleb = ubifs_recover_leb(c, lnum, offs, c->sbuf, b->bud->jhead);
	else
		sleb = ubifs_scan(c, lnum, offs, c->sbuf, 0);
	if (IS_ERR(sleb))
		return PTR_ERR(sleb);

	n_nodes = authenticate_sleb(c, sleb, b->bud->log_hash, is_last);
	if (n_nodes < 0) {
		err = n_nodes;
		goto out;
	}

	ubifs_shash_copy_state(c, b->bud->log_hash,
			       c->jheads[b->bud->jhead].log_hash);

	 

	list_for_each_entry(snod, &sleb->nodes, list) {
		u8 hash[UBIFS_HASH_ARR_SZ];
		int deletion = 0;

		cond_resched();

		if (snod->sqnum >= SQNUM_WATERMARK) {
			ubifs_err(c, "file system's life ended");
			goto out_dump;
		}

		ubifs_node_calc_hash(c, snod->node, hash);

		if (snod->sqnum > c->max_sqnum)
			c->max_sqnum = snod->sqnum;

		switch (snod->type) {
		case UBIFS_INO_NODE:
		{
			struct ubifs_ino_node *ino = snod->node;
			loff_t new_size = le64_to_cpu(ino->size);

			if (le32_to_cpu(ino->nlink) == 0)
				deletion = 1;
			err = insert_node(c, lnum, snod->offs, snod->len, hash,
					  &snod->key, snod->sqnum, deletion,
					  &used, 0, new_size);
			break;
		}
		case UBIFS_DATA_NODE:
		{
			struct ubifs_data_node *dn = snod->node;
			loff_t new_size = le32_to_cpu(dn->size) +
					  key_block(c, &snod->key) *
					  UBIFS_BLOCK_SIZE;

			err = insert_node(c, lnum, snod->offs, snod->len, hash,
					  &snod->key, snod->sqnum, deletion,
					  &used, 0, new_size);
			break;
		}
		case UBIFS_DENT_NODE:
		case UBIFS_XENT_NODE:
		{
			struct ubifs_dent_node *dent = snod->node;

			err = ubifs_validate_entry(c, dent);
			if (err)
				goto out_dump;

			err = insert_dent(c, lnum, snod->offs, snod->len, hash,
					  &snod->key, dent->name,
					  le16_to_cpu(dent->nlen), snod->sqnum,
					  !le64_to_cpu(dent->inum), &used);
			break;
		}
		case UBIFS_TRUN_NODE:
		{
			struct ubifs_trun_node *trun = snod->node;
			loff_t old_size = le64_to_cpu(trun->old_size);
			loff_t new_size = le64_to_cpu(trun->new_size);
			union ubifs_key key;

			 
			if (old_size < 0 || old_size > c->max_inode_sz ||
			    new_size < 0 || new_size > c->max_inode_sz ||
			    old_size <= new_size) {
				ubifs_err(c, "bad truncation node");
				goto out_dump;
			}

			 
			trun_key_init(c, &key, le32_to_cpu(trun->inum));
			err = insert_node(c, lnum, snod->offs, snod->len, hash,
					  &key, snod->sqnum, 1, &used,
					  old_size, new_size);
			break;
		}
		case UBIFS_AUTH_NODE:
			break;
		default:
			ubifs_err(c, "unexpected node type %d in bud LEB %d:%d",
				  snod->type, lnum, snod->offs);
			err = -EINVAL;
			goto out_dump;
		}
		if (err)
			goto out;

		n++;
		if (n == n_nodes)
			break;
	}

	ubifs_assert(c, ubifs_search_bud(c, lnum));
	ubifs_assert(c, sleb->endpt - offs >= used);
	ubifs_assert(c, sleb->endpt % c->min_io_size == 0);

	b->dirty = sleb->endpt - offs - used;
	b->free = c->leb_size - sleb->endpt;
	dbg_mnt("bud LEB %d replied: dirty %d, free %d",
		lnum, b->dirty, b->free);

out:
	ubifs_scan_destroy(sleb);
	return err;

out_dump:
	ubifs_err(c, "bad node is at LEB %d:%d", lnum, snod->offs);
	ubifs_dump_node(c, snod->node, c->leb_size - snod->offs);
	ubifs_scan_destroy(sleb);
	return -EINVAL;
}

 
static int replay_buds(struct ubifs_info *c)
{
	struct bud_entry *b;
	int err;
	unsigned long long prev_sqnum = 0;

	list_for_each_entry(b, &c->replay_buds, list) {
		err = replay_bud(c, b);
		if (err)
			return err;

		ubifs_assert(c, b->sqnum > prev_sqnum);
		prev_sqnum = b->sqnum;
	}

	return 0;
}

 
static void destroy_bud_list(struct ubifs_info *c)
{
	struct bud_entry *b;

	while (!list_empty(&c->replay_buds)) {
		b = list_entry(c->replay_buds.next, struct bud_entry, list);
		list_del(&b->list);
		kfree(b);
	}
}

 
static int add_replay_bud(struct ubifs_info *c, int lnum, int offs, int jhead,
			  unsigned long long sqnum)
{
	struct ubifs_bud *bud;
	struct bud_entry *b;
	int err;

	dbg_mnt("add replay bud LEB %d:%d, head %d", lnum, offs, jhead);

	bud = kmalloc(sizeof(struct ubifs_bud), GFP_KERNEL);
	if (!bud)
		return -ENOMEM;

	b = kmalloc(sizeof(struct bud_entry), GFP_KERNEL);
	if (!b) {
		err = -ENOMEM;
		goto out;
	}

	bud->lnum = lnum;
	bud->start = offs;
	bud->jhead = jhead;
	bud->log_hash = ubifs_hash_get_desc(c);
	if (IS_ERR(bud->log_hash)) {
		err = PTR_ERR(bud->log_hash);
		goto out;
	}

	ubifs_shash_copy_state(c, c->log_hash, bud->log_hash);

	ubifs_add_bud(c, bud);

	b->bud = bud;
	b->sqnum = sqnum;
	list_add_tail(&b->list, &c->replay_buds);

	return 0;
out:
	kfree(bud);
	kfree(b);

	return err;
}

 
static int validate_ref(struct ubifs_info *c, const struct ubifs_ref_node *ref)
{
	struct ubifs_bud *bud;
	int lnum = le32_to_cpu(ref->lnum);
	unsigned int offs = le32_to_cpu(ref->offs);
	unsigned int jhead = le32_to_cpu(ref->jhead);

	 
	if (jhead >= c->jhead_cnt || lnum >= c->leb_cnt ||
	    lnum < c->main_first || offs > c->leb_size ||
	    offs & (c->min_io_size - 1))
		return -EINVAL;

	 
	bud = ubifs_search_bud(c, lnum);
	if (bud) {
		if (bud->jhead == jhead && bud->start <= offs)
			return 1;
		ubifs_err(c, "bud at LEB %d:%d was already referred", lnum, offs);
		return -EINVAL;
	}

	return 0;
}

 
static int replay_log_leb(struct ubifs_info *c, int lnum, int offs, void *sbuf)
{
	int err;
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	const struct ubifs_cs_node *node;

	dbg_mnt("replay log LEB %d:%d", lnum, offs);
	sleb = ubifs_scan(c, lnum, offs, sbuf, c->need_recovery);
	if (IS_ERR(sleb)) {
		if (PTR_ERR(sleb) != -EUCLEAN || !c->need_recovery)
			return PTR_ERR(sleb);
		 
		sleb = ubifs_recover_log_leb(c, lnum, offs, sbuf);
		if (IS_ERR(sleb))
			return PTR_ERR(sleb);
	}

	if (sleb->nodes_cnt == 0) {
		err = 1;
		goto out;
	}

	node = sleb->buf;
	snod = list_entry(sleb->nodes.next, struct ubifs_scan_node, list);
	if (c->cs_sqnum == 0) {
		 
		if (snod->type != UBIFS_CS_NODE) {
			ubifs_err(c, "first log node at LEB %d:%d is not CS node",
				  lnum, offs);
			goto out_dump;
		}
		if (le64_to_cpu(node->cmt_no) != c->cmt_no) {
			ubifs_err(c, "first CS node at LEB %d:%d has wrong commit number %llu expected %llu",
				  lnum, offs,
				  (unsigned long long)le64_to_cpu(node->cmt_no),
				  c->cmt_no);
			goto out_dump;
		}

		c->cs_sqnum = le64_to_cpu(node->ch.sqnum);
		dbg_mnt("commit start sqnum %llu", c->cs_sqnum);

		err = ubifs_shash_init(c, c->log_hash);
		if (err)
			goto out;

		err = ubifs_shash_update(c, c->log_hash, node, UBIFS_CS_NODE_SZ);
		if (err < 0)
			goto out;
	}

	if (snod->sqnum < c->cs_sqnum) {
		 
		err = 1;
		goto out;
	}

	 
	if (snod->offs != 0) {
		ubifs_err(c, "first node is not at zero offset");
		goto out_dump;
	}

	list_for_each_entry(snod, &sleb->nodes, list) {
		cond_resched();

		if (snod->sqnum >= SQNUM_WATERMARK) {
			ubifs_err(c, "file system's life ended");
			goto out_dump;
		}

		if (snod->sqnum < c->cs_sqnum) {
			ubifs_err(c, "bad sqnum %llu, commit sqnum %llu",
				  snod->sqnum, c->cs_sqnum);
			goto out_dump;
		}

		if (snod->sqnum > c->max_sqnum)
			c->max_sqnum = snod->sqnum;

		switch (snod->type) {
		case UBIFS_REF_NODE: {
			const struct ubifs_ref_node *ref = snod->node;

			err = validate_ref(c, ref);
			if (err == 1)
				break;  
			if (err)
				goto out_dump;

			err = ubifs_shash_update(c, c->log_hash, ref,
						 UBIFS_REF_NODE_SZ);
			if (err)
				goto out;

			err = add_replay_bud(c, le32_to_cpu(ref->lnum),
					     le32_to_cpu(ref->offs),
					     le32_to_cpu(ref->jhead),
					     snod->sqnum);
			if (err)
				goto out;

			break;
		}
		case UBIFS_CS_NODE:
			 
			if (snod->offs != 0) {
				ubifs_err(c, "unexpected node in log");
				goto out_dump;
			}
			break;
		default:
			ubifs_err(c, "unexpected node in log");
			goto out_dump;
		}
	}

	if (sleb->endpt || c->lhead_offs >= c->leb_size) {
		c->lhead_lnum = lnum;
		c->lhead_offs = sleb->endpt;
	}

	err = !sleb->endpt;
out:
	ubifs_scan_destroy(sleb);
	return err;

out_dump:
	ubifs_err(c, "log error detected while replaying the log at LEB %d:%d",
		  lnum, offs + snod->offs);
	ubifs_dump_node(c, snod->node, c->leb_size - snod->offs);
	ubifs_scan_destroy(sleb);
	return -EINVAL;
}

 
static int take_ihead(struct ubifs_info *c)
{
	const struct ubifs_lprops *lp;
	int err, free;

	ubifs_get_lprops(c);

	lp = ubifs_lpt_lookup_dirty(c, c->ihead_lnum);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	free = lp->free;

	lp = ubifs_change_lp(c, lp, LPROPS_NC, LPROPS_NC,
			     lp->flags | LPROPS_TAKEN, 0);
	if (IS_ERR(lp)) {
		err = PTR_ERR(lp);
		goto out;
	}

	err = free;
out:
	ubifs_release_lprops(c);
	return err;
}

 
int ubifs_replay_journal(struct ubifs_info *c)
{
	int err, lnum, free;

	BUILD_BUG_ON(UBIFS_TRUN_KEY > 5);

	 
	free = take_ihead(c);
	if (free < 0)
		return free;  

	if (c->ihead_offs != c->leb_size - free) {
		ubifs_err(c, "bad index head LEB %d:%d", c->ihead_lnum,
			  c->ihead_offs);
		return -EINVAL;
	}

	dbg_mnt("start replaying the journal");
	c->replaying = 1;
	lnum = c->ltail_lnum = c->lhead_lnum;

	do {
		err = replay_log_leb(c, lnum, 0, c->sbuf);
		if (err == 1) {
			if (lnum != c->lhead_lnum)
				 
				break;

			 
			ubifs_err(c, "no UBIFS nodes found at the log head LEB %d:%d, possibly corrupted",
				  lnum, 0);
			err = -EINVAL;
		}
		if (err)
			goto out;
		lnum = ubifs_next_log_lnum(c, lnum);
	} while (lnum != c->ltail_lnum);

	err = replay_buds(c);
	if (err)
		goto out;

	err = apply_replay_list(c);
	if (err)
		goto out;

	err = set_buds_lprops(c);
	if (err)
		goto out;

	 
	c->bi.uncommitted_idx = atomic_long_read(&c->dirty_zn_cnt);
	c->bi.uncommitted_idx *= c->max_idx_node_sz;

	ubifs_assert(c, c->bud_bytes <= c->max_bud_bytes || c->need_recovery);
	dbg_mnt("finished, log head LEB %d:%d, max_sqnum %llu, highest_inum %lu",
		c->lhead_lnum, c->lhead_offs, c->max_sqnum,
		(unsigned long)c->highest_inum);
out:
	destroy_replay_list(c);
	destroy_bud_list(c);
	c->replaying = 0;
	return err;
}
