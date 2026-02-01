
#include <linux/ceph/ceph_debug.h>

#include <linux/fs.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/iversion.h>
#include "super.h"
#include "mds_client.h"
#include <linux/ceph/decode.h>

 
#define CEPH_SNAPID_MAP_TIMEOUT	(5 * 60 * HZ)

 
 


 
void ceph_get_snap_realm(struct ceph_mds_client *mdsc,
			 struct ceph_snap_realm *realm)
{
	lockdep_assert_held(&mdsc->snap_rwsem);

	 
	if (atomic_inc_not_zero(&realm->nref))
		return;

	spin_lock(&mdsc->snap_empty_lock);
	if (atomic_inc_return(&realm->nref) == 1)
		list_del_init(&realm->empty_item);
	spin_unlock(&mdsc->snap_empty_lock);
}

static void __insert_snap_realm(struct rb_root *root,
				struct ceph_snap_realm *new)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ceph_snap_realm *r = NULL;

	while (*p) {
		parent = *p;
		r = rb_entry(parent, struct ceph_snap_realm, node);
		if (new->ino < r->ino)
			p = &(*p)->rb_left;
		else if (new->ino > r->ino)
			p = &(*p)->rb_right;
		else
			BUG();
	}

	rb_link_node(&new->node, parent, p);
	rb_insert_color(&new->node, root);
}

 
static struct ceph_snap_realm *ceph_create_snap_realm(
	struct ceph_mds_client *mdsc,
	u64 ino)
{
	struct ceph_snap_realm *realm;

	lockdep_assert_held_write(&mdsc->snap_rwsem);

	realm = kzalloc(sizeof(*realm), GFP_NOFS);
	if (!realm)
		return ERR_PTR(-ENOMEM);

	 
	if (ino == CEPH_INO_GLOBAL_SNAPREALM)
		atomic_set(&realm->nref, 2);
	else
		atomic_set(&realm->nref, 1);
	realm->ino = ino;
	INIT_LIST_HEAD(&realm->children);
	INIT_LIST_HEAD(&realm->child_item);
	INIT_LIST_HEAD(&realm->empty_item);
	INIT_LIST_HEAD(&realm->dirty_item);
	INIT_LIST_HEAD(&realm->rebuild_item);
	INIT_LIST_HEAD(&realm->inodes_with_caps);
	spin_lock_init(&realm->inodes_with_caps_lock);
	__insert_snap_realm(&mdsc->snap_realms, realm);
	mdsc->num_snap_realms++;

	dout("%s %llx %p\n", __func__, realm->ino, realm);
	return realm;
}

 
static struct ceph_snap_realm *__lookup_snap_realm(struct ceph_mds_client *mdsc,
						   u64 ino)
{
	struct rb_node *n = mdsc->snap_realms.rb_node;
	struct ceph_snap_realm *r;

	lockdep_assert_held(&mdsc->snap_rwsem);

	while (n) {
		r = rb_entry(n, struct ceph_snap_realm, node);
		if (ino < r->ino)
			n = n->rb_left;
		else if (ino > r->ino)
			n = n->rb_right;
		else {
			dout("%s %llx %p\n", __func__, r->ino, r);
			return r;
		}
	}
	return NULL;
}

struct ceph_snap_realm *ceph_lookup_snap_realm(struct ceph_mds_client *mdsc,
					       u64 ino)
{
	struct ceph_snap_realm *r;
	r = __lookup_snap_realm(mdsc, ino);
	if (r)
		ceph_get_snap_realm(mdsc, r);
	return r;
}

static void __put_snap_realm(struct ceph_mds_client *mdsc,
			     struct ceph_snap_realm *realm);

 
static void __destroy_snap_realm(struct ceph_mds_client *mdsc,
				 struct ceph_snap_realm *realm)
{
	lockdep_assert_held_write(&mdsc->snap_rwsem);

	dout("%s %p %llx\n", __func__, realm, realm->ino);

	rb_erase(&realm->node, &mdsc->snap_realms);
	mdsc->num_snap_realms--;

	if (realm->parent) {
		list_del_init(&realm->child_item);
		__put_snap_realm(mdsc, realm->parent);
	}

	kfree(realm->prior_parent_snaps);
	kfree(realm->snaps);
	ceph_put_snap_context(realm->cached_context);
	kfree(realm);
}

 
static void __put_snap_realm(struct ceph_mds_client *mdsc,
			     struct ceph_snap_realm *realm)
{
	lockdep_assert_held_write(&mdsc->snap_rwsem);

	 
	if (atomic_dec_and_test(&realm->nref))
		__destroy_snap_realm(mdsc, realm);
}

 
void ceph_put_snap_realm(struct ceph_mds_client *mdsc,
			 struct ceph_snap_realm *realm)
{
	if (!atomic_dec_and_lock(&realm->nref, &mdsc->snap_empty_lock))
		return;

	if (down_write_trylock(&mdsc->snap_rwsem)) {
		spin_unlock(&mdsc->snap_empty_lock);
		__destroy_snap_realm(mdsc, realm);
		up_write(&mdsc->snap_rwsem);
	} else {
		list_add(&realm->empty_item, &mdsc->snap_empty);
		spin_unlock(&mdsc->snap_empty_lock);
	}
}

 
static void __cleanup_empty_realms(struct ceph_mds_client *mdsc)
{
	struct ceph_snap_realm *realm;

	lockdep_assert_held_write(&mdsc->snap_rwsem);

	spin_lock(&mdsc->snap_empty_lock);
	while (!list_empty(&mdsc->snap_empty)) {
		realm = list_first_entry(&mdsc->snap_empty,
				   struct ceph_snap_realm, empty_item);
		list_del(&realm->empty_item);
		spin_unlock(&mdsc->snap_empty_lock);
		__destroy_snap_realm(mdsc, realm);
		spin_lock(&mdsc->snap_empty_lock);
	}
	spin_unlock(&mdsc->snap_empty_lock);
}

void ceph_cleanup_global_and_empty_realms(struct ceph_mds_client *mdsc)
{
	struct ceph_snap_realm *global_realm;

	down_write(&mdsc->snap_rwsem);
	global_realm = __lookup_snap_realm(mdsc, CEPH_INO_GLOBAL_SNAPREALM);
	if (global_realm)
		ceph_put_snap_realm(mdsc, global_realm);
	__cleanup_empty_realms(mdsc);
	up_write(&mdsc->snap_rwsem);
}

 
static int adjust_snap_realm_parent(struct ceph_mds_client *mdsc,
				    struct ceph_snap_realm *realm,
				    u64 parentino)
{
	struct ceph_snap_realm *parent;

	lockdep_assert_held_write(&mdsc->snap_rwsem);

	if (realm->parent_ino == parentino)
		return 0;

	parent = ceph_lookup_snap_realm(mdsc, parentino);
	if (!parent) {
		parent = ceph_create_snap_realm(mdsc, parentino);
		if (IS_ERR(parent))
			return PTR_ERR(parent);
	}
	dout("%s %llx %p: %llx %p -> %llx %p\n", __func__, realm->ino,
	     realm, realm->parent_ino, realm->parent, parentino, parent);
	if (realm->parent) {
		list_del_init(&realm->child_item);
		ceph_put_snap_realm(mdsc, realm->parent);
	}
	realm->parent_ino = parentino;
	realm->parent = parent;
	list_add(&realm->child_item, &parent->children);
	return 1;
}


static int cmpu64_rev(const void *a, const void *b)
{
	if (*(u64 *)a < *(u64 *)b)
		return 1;
	if (*(u64 *)a > *(u64 *)b)
		return -1;
	return 0;
}


 
static int build_snap_context(struct ceph_snap_realm *realm,
			      struct list_head *realm_queue,
			      struct list_head *dirty_realms)
{
	struct ceph_snap_realm *parent = realm->parent;
	struct ceph_snap_context *snapc;
	int err = 0;
	u32 num = realm->num_prior_parent_snaps + realm->num_snaps;

	 
	if (parent) {
		if (!parent->cached_context) {
			 
			list_add(&parent->rebuild_item, realm_queue);
			return 1;
		}
		num += parent->cached_context->num_snaps;
	}

	 
	if (realm->cached_context &&
	    realm->cached_context->seq == realm->seq &&
	    (!parent ||
	     realm->cached_context->seq >= parent->cached_context->seq)) {
		dout("%s %llx %p: %p seq %lld (%u snaps) (unchanged)\n",
		     __func__, realm->ino, realm, realm->cached_context,
		     realm->cached_context->seq,
		     (unsigned int)realm->cached_context->num_snaps);
		return 0;
	}

	 
	err = -ENOMEM;
	if (num > (SIZE_MAX - sizeof(*snapc)) / sizeof(u64))
		goto fail;
	snapc = ceph_create_snap_context(num, GFP_NOFS);
	if (!snapc)
		goto fail;

	 
	num = 0;
	snapc->seq = realm->seq;
	if (parent) {
		u32 i;

		 
		for (i = 0; i < parent->cached_context->num_snaps; i++)
			if (parent->cached_context->snaps[i] >=
			    realm->parent_since)
				snapc->snaps[num++] =
					parent->cached_context->snaps[i];
		if (parent->cached_context->seq > snapc->seq)
			snapc->seq = parent->cached_context->seq;
	}
	memcpy(snapc->snaps + num, realm->snaps,
	       sizeof(u64)*realm->num_snaps);
	num += realm->num_snaps;
	memcpy(snapc->snaps + num, realm->prior_parent_snaps,
	       sizeof(u64)*realm->num_prior_parent_snaps);
	num += realm->num_prior_parent_snaps;

	sort(snapc->snaps, num, sizeof(u64), cmpu64_rev, NULL);
	snapc->num_snaps = num;
	dout("%s %llx %p: %p seq %lld (%u snaps)\n", __func__, realm->ino,
	     realm, snapc, snapc->seq, (unsigned int) snapc->num_snaps);

	ceph_put_snap_context(realm->cached_context);
	realm->cached_context = snapc;
	 
	list_add_tail(&realm->dirty_item, dirty_realms);
	return 0;

fail:
	 
	if (realm->cached_context) {
		ceph_put_snap_context(realm->cached_context);
		realm->cached_context = NULL;
	}
	pr_err("%s %llx %p fail %d\n", __func__, realm->ino, realm, err);
	return err;
}

 
static void rebuild_snap_realms(struct ceph_snap_realm *realm,
				struct list_head *dirty_realms)
{
	LIST_HEAD(realm_queue);
	int last = 0;
	bool skip = false;

	list_add_tail(&realm->rebuild_item, &realm_queue);

	while (!list_empty(&realm_queue)) {
		struct ceph_snap_realm *_realm, *child;

		_realm = list_first_entry(&realm_queue,
					  struct ceph_snap_realm,
					  rebuild_item);

		 
		if (last < 0) {
			list_del_init(&_realm->rebuild_item);
			continue;
		}

		last = build_snap_context(_realm, &realm_queue, dirty_realms);
		dout("%s %llx %p, %s\n", __func__, _realm->ino, _realm,
		     last > 0 ? "is deferred" : !last ? "succeeded" : "failed");

		 
		list_for_each_entry(child, &_realm->children, child_item) {
			if (!list_empty(&child->rebuild_item)) {
				skip = true;
				break;
			}
		}

		if (!skip) {
			list_for_each_entry(child, &_realm->children, child_item)
				list_add_tail(&child->rebuild_item, &realm_queue);
		}

		 
		if (last <= 0)
			list_del_init(&_realm->rebuild_item);
	}
}


 
static int dup_array(u64 **dst, __le64 *src, u32 num)
{
	u32 i;

	kfree(*dst);
	if (num) {
		*dst = kcalloc(num, sizeof(u64), GFP_NOFS);
		if (!*dst)
			return -ENOMEM;
		for (i = 0; i < num; i++)
			(*dst)[i] = get_unaligned_le64(src + i);
	} else {
		*dst = NULL;
	}
	return 0;
}

static bool has_new_snaps(struct ceph_snap_context *o,
			  struct ceph_snap_context *n)
{
	if (n->num_snaps == 0)
		return false;
	 
	return n->snaps[0] > o->seq;
}

 
static void ceph_queue_cap_snap(struct ceph_inode_info *ci,
				struct ceph_cap_snap **pcapsnap)
{
	struct inode *inode = &ci->netfs.inode;
	struct ceph_snap_context *old_snapc, *new_snapc;
	struct ceph_cap_snap *capsnap = *pcapsnap;
	struct ceph_buffer *old_blob = NULL;
	int used, dirty;

	spin_lock(&ci->i_ceph_lock);
	used = __ceph_caps_used(ci);
	dirty = __ceph_caps_dirty(ci);

	old_snapc = ci->i_head_snapc;
	new_snapc = ci->i_snap_realm->cached_context;

	 
	if (used & CEPH_CAP_FILE_WR)
		dirty |= CEPH_CAP_FILE_WR;

	if (__ceph_have_pending_cap_snap(ci)) {
		 
		dout("%s %p %llx.%llx already pending\n",
		     __func__, inode, ceph_vinop(inode));
		goto update_snapc;
	}
	if (ci->i_wrbuffer_ref_head == 0 &&
	    !(dirty & (CEPH_CAP_ANY_EXCL|CEPH_CAP_FILE_WR))) {
		dout("%s %p %llx.%llx nothing dirty|writing\n",
		     __func__, inode, ceph_vinop(inode));
		goto update_snapc;
	}

	BUG_ON(!old_snapc);

	 
	if (has_new_snaps(old_snapc, new_snapc)) {
		if (dirty & (CEPH_CAP_ANY_EXCL|CEPH_CAP_FILE_WR))
			capsnap->need_flush = true;
	} else {
		if (!(used & CEPH_CAP_FILE_WR) &&
		    ci->i_wrbuffer_ref_head == 0) {
			dout("%s %p %llx.%llx no new_snap|dirty_page|writing\n",
			     __func__, inode, ceph_vinop(inode));
			goto update_snapc;
		}
	}

	dout("%s %p %llx.%llx cap_snap %p queuing under %p %s %s\n",
	     __func__, inode, ceph_vinop(inode), capsnap, old_snapc,
	     ceph_cap_string(dirty), capsnap->need_flush ? "" : "no_flush");
	ihold(inode);

	capsnap->follows = old_snapc->seq;
	capsnap->issued = __ceph_caps_issued(ci, NULL);
	capsnap->dirty = dirty;

	capsnap->mode = inode->i_mode;
	capsnap->uid = inode->i_uid;
	capsnap->gid = inode->i_gid;

	if (dirty & CEPH_CAP_XATTR_EXCL) {
		old_blob = __ceph_build_xattrs_blob(ci);
		capsnap->xattr_blob =
			ceph_buffer_get(ci->i_xattrs.blob);
		capsnap->xattr_version = ci->i_xattrs.version;
	} else {
		capsnap->xattr_blob = NULL;
		capsnap->xattr_version = 0;
	}

	capsnap->inline_data = ci->i_inline_version != CEPH_INLINE_NONE;

	 
	capsnap->dirty_pages = ci->i_wrbuffer_ref_head;
	ci->i_wrbuffer_ref_head = 0;
	capsnap->context = old_snapc;
	list_add_tail(&capsnap->ci_item, &ci->i_cap_snaps);

	if (used & CEPH_CAP_FILE_WR) {
		dout("%s %p %llx.%llx cap_snap %p snapc %p seq %llu used WR,"
		     " now pending\n", __func__, inode, ceph_vinop(inode),
		     capsnap, old_snapc, old_snapc->seq);
		capsnap->writing = 1;
	} else {
		 
		__ceph_finish_cap_snap(ci, capsnap);
	}
	*pcapsnap = NULL;
	old_snapc = NULL;

update_snapc:
	if (ci->i_wrbuffer_ref_head == 0 &&
	    ci->i_wr_ref == 0 &&
	    ci->i_dirty_caps == 0 &&
	    ci->i_flushing_caps == 0) {
		ci->i_head_snapc = NULL;
	} else {
		ci->i_head_snapc = ceph_get_snap_context(new_snapc);
		dout(" new snapc is %p\n", new_snapc);
	}
	spin_unlock(&ci->i_ceph_lock);

	ceph_buffer_put(old_blob);
	ceph_put_snap_context(old_snapc);
}

 
int __ceph_finish_cap_snap(struct ceph_inode_info *ci,
			    struct ceph_cap_snap *capsnap)
{
	struct inode *inode = &ci->netfs.inode;
	struct ceph_mds_client *mdsc = ceph_sb_to_mdsc(inode->i_sb);

	BUG_ON(capsnap->writing);
	capsnap->size = i_size_read(inode);
	capsnap->mtime = inode->i_mtime;
	capsnap->atime = inode->i_atime;
	capsnap->ctime = inode_get_ctime(inode);
	capsnap->btime = ci->i_btime;
	capsnap->change_attr = inode_peek_iversion_raw(inode);
	capsnap->time_warp_seq = ci->i_time_warp_seq;
	capsnap->truncate_size = ci->i_truncate_size;
	capsnap->truncate_seq = ci->i_truncate_seq;
	if (capsnap->dirty_pages) {
		dout("%s %p %llx.%llx cap_snap %p snapc %p %llu %s s=%llu "
		     "still has %d dirty pages\n", __func__, inode,
		     ceph_vinop(inode), capsnap, capsnap->context,
		     capsnap->context->seq, ceph_cap_string(capsnap->dirty),
		     capsnap->size, capsnap->dirty_pages);
		return 0;
	}

	 
	if (ci->i_wrbuffer_ref) {
		dout("%s %p %llx.%llx cap_snap %p snapc %p %llu %s s=%llu "
		     "used WRBUFFER, delaying\n", __func__, inode,
		     ceph_vinop(inode), capsnap, capsnap->context,
		     capsnap->context->seq, ceph_cap_string(capsnap->dirty),
		     capsnap->size);
		ceph_queue_writeback(inode);
		return 0;
	}

	ci->i_ceph_flags |= CEPH_I_FLUSH_SNAPS;
	dout("%s %p %llx.%llx cap_snap %p snapc %p %llu %s s=%llu\n",
	     __func__, inode, ceph_vinop(inode), capsnap, capsnap->context,
	     capsnap->context->seq, ceph_cap_string(capsnap->dirty),
	     capsnap->size);

	spin_lock(&mdsc->snap_flush_lock);
	if (list_empty(&ci->i_snap_flush_item)) {
		ihold(inode);
		list_add_tail(&ci->i_snap_flush_item, &mdsc->snap_flush_list);
	}
	spin_unlock(&mdsc->snap_flush_lock);
	return 1;   
}

 
static void queue_realm_cap_snaps(struct ceph_snap_realm *realm)
{
	struct ceph_inode_info *ci;
	struct inode *lastinode = NULL;
	struct ceph_cap_snap *capsnap = NULL;

	dout("%s %p %llx inode\n", __func__, realm, realm->ino);

	spin_lock(&realm->inodes_with_caps_lock);
	list_for_each_entry(ci, &realm->inodes_with_caps, i_snap_realm_item) {
		struct inode *inode = igrab(&ci->netfs.inode);
		if (!inode)
			continue;
		spin_unlock(&realm->inodes_with_caps_lock);
		iput(lastinode);
		lastinode = inode;

		 
		if (!capsnap) {
			capsnap = kmem_cache_zalloc(ceph_cap_snap_cachep, GFP_NOFS);
			if (!capsnap) {
				pr_err("ENOMEM allocating ceph_cap_snap on %p\n",
				       inode);
				return;
			}
		}
		capsnap->cap_flush.is_capsnap = true;
		refcount_set(&capsnap->nref, 1);
		INIT_LIST_HEAD(&capsnap->cap_flush.i_list);
		INIT_LIST_HEAD(&capsnap->cap_flush.g_list);
		INIT_LIST_HEAD(&capsnap->ci_item);

		ceph_queue_cap_snap(ci, &capsnap);
		spin_lock(&realm->inodes_with_caps_lock);
	}
	spin_unlock(&realm->inodes_with_caps_lock);
	iput(lastinode);

	if (capsnap)
		kmem_cache_free(ceph_cap_snap_cachep, capsnap);
	dout("%s %p %llx done\n", __func__, realm, realm->ino);
}

 
int ceph_update_snap_trace(struct ceph_mds_client *mdsc,
			   void *p, void *e, bool deletion,
			   struct ceph_snap_realm **realm_ret)
{
	struct ceph_mds_snap_realm *ri;     
	__le64 *snaps;                      
	__le64 *prior_parent_snaps;         
	struct ceph_snap_realm *realm;
	struct ceph_snap_realm *first_realm = NULL;
	struct ceph_snap_realm *realm_to_rebuild = NULL;
	struct ceph_client *client = mdsc->fsc->client;
	int rebuild_snapcs;
	int err = -ENOMEM;
	int ret;
	LIST_HEAD(dirty_realms);

	lockdep_assert_held_write(&mdsc->snap_rwsem);

	dout("%s deletion=%d\n", __func__, deletion);
more:
	realm = NULL;
	rebuild_snapcs = 0;
	ceph_decode_need(&p, e, sizeof(*ri), bad);
	ri = p;
	p += sizeof(*ri);
	ceph_decode_need(&p, e, sizeof(u64)*(le32_to_cpu(ri->num_snaps) +
			    le32_to_cpu(ri->num_prior_parent_snaps)), bad);
	snaps = p;
	p += sizeof(u64) * le32_to_cpu(ri->num_snaps);
	prior_parent_snaps = p;
	p += sizeof(u64) * le32_to_cpu(ri->num_prior_parent_snaps);

	realm = ceph_lookup_snap_realm(mdsc, le64_to_cpu(ri->ino));
	if (!realm) {
		realm = ceph_create_snap_realm(mdsc, le64_to_cpu(ri->ino));
		if (IS_ERR(realm)) {
			err = PTR_ERR(realm);
			goto fail;
		}
	}

	 
	err = adjust_snap_realm_parent(mdsc, realm, le64_to_cpu(ri->parent));
	if (err < 0)
		goto fail;
	rebuild_snapcs += err;

	if (le64_to_cpu(ri->seq) > realm->seq) {
		dout("%s updating %llx %p %lld -> %lld\n", __func__,
		     realm->ino, realm, realm->seq, le64_to_cpu(ri->seq));
		 
		realm->seq = le64_to_cpu(ri->seq);
		realm->created = le64_to_cpu(ri->created);
		realm->parent_since = le64_to_cpu(ri->parent_since);

		realm->num_snaps = le32_to_cpu(ri->num_snaps);
		err = dup_array(&realm->snaps, snaps, realm->num_snaps);
		if (err < 0)
			goto fail;

		realm->num_prior_parent_snaps =
			le32_to_cpu(ri->num_prior_parent_snaps);
		err = dup_array(&realm->prior_parent_snaps, prior_parent_snaps,
				realm->num_prior_parent_snaps);
		if (err < 0)
			goto fail;

		if (realm->seq > mdsc->last_snap_seq)
			mdsc->last_snap_seq = realm->seq;

		rebuild_snapcs = 1;
	} else if (!realm->cached_context) {
		dout("%s %llx %p seq %lld new\n", __func__,
		     realm->ino, realm, realm->seq);
		rebuild_snapcs = 1;
	} else {
		dout("%s %llx %p seq %lld unchanged\n", __func__,
		     realm->ino, realm, realm->seq);
	}

	dout("done with %llx %p, rebuild_snapcs=%d, %p %p\n", realm->ino,
	     realm, rebuild_snapcs, p, e);

	 
	if (rebuild_snapcs)
		realm_to_rebuild = realm;

	 
	if (realm_to_rebuild && p >= e)
		rebuild_snap_realms(realm_to_rebuild, &dirty_realms);

	if (!first_realm)
		first_realm = realm;
	else
		ceph_put_snap_realm(mdsc, realm);

	if (p < e)
		goto more;

	 
	while (!list_empty(&dirty_realms)) {
		realm = list_first_entry(&dirty_realms, struct ceph_snap_realm,
					 dirty_item);
		list_del_init(&realm->dirty_item);
		queue_realm_cap_snaps(realm);
	}

	if (realm_ret)
		*realm_ret = first_realm;
	else
		ceph_put_snap_realm(mdsc, first_realm);

	__cleanup_empty_realms(mdsc);
	return 0;

bad:
	err = -EIO;
fail:
	if (realm && !IS_ERR(realm))
		ceph_put_snap_realm(mdsc, realm);
	if (first_realm)
		ceph_put_snap_realm(mdsc, first_realm);
	pr_err("%s error %d\n", __func__, err);

	 
	WRITE_ONCE(mdsc->fsc->mount_state, CEPH_MOUNT_FENCE_IO);
	ret = ceph_monc_blocklist_add(&client->monc, &client->msgr.inst.addr);
	if (ret)
		pr_err("%s failed to blocklist %s: %d\n", __func__,
		       ceph_pr_addr(&client->msgr.inst.addr), ret);

	WARN(1, "%s: %s%sdo remount to continue%s",
	     __func__, ret ? "" : ceph_pr_addr(&client->msgr.inst.addr),
	     ret ? "" : " was blocklisted, ",
	     err == -EIO ? " after corrupted snaptrace is fixed" : "");

	return err;
}


 
static void flush_snaps(struct ceph_mds_client *mdsc)
{
	struct ceph_inode_info *ci;
	struct inode *inode;
	struct ceph_mds_session *session = NULL;

	dout("%s\n", __func__);
	spin_lock(&mdsc->snap_flush_lock);
	while (!list_empty(&mdsc->snap_flush_list)) {
		ci = list_first_entry(&mdsc->snap_flush_list,
				struct ceph_inode_info, i_snap_flush_item);
		inode = &ci->netfs.inode;
		ihold(inode);
		spin_unlock(&mdsc->snap_flush_lock);
		ceph_flush_snaps(ci, &session);
		iput(inode);
		spin_lock(&mdsc->snap_flush_lock);
	}
	spin_unlock(&mdsc->snap_flush_lock);

	ceph_put_mds_session(session);
	dout("%s done\n", __func__);
}

 
void ceph_change_snap_realm(struct inode *inode, struct ceph_snap_realm *realm)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	struct ceph_mds_client *mdsc = ceph_inode_to_client(inode)->mdsc;
	struct ceph_snap_realm *oldrealm = ci->i_snap_realm;

	lockdep_assert_held(&ci->i_ceph_lock);

	if (oldrealm) {
		spin_lock(&oldrealm->inodes_with_caps_lock);
		list_del_init(&ci->i_snap_realm_item);
		if (oldrealm->ino == ci->i_vino.ino)
			oldrealm->inode = NULL;
		spin_unlock(&oldrealm->inodes_with_caps_lock);
		ceph_put_snap_realm(mdsc, oldrealm);
	}

	ci->i_snap_realm = realm;

	if (realm) {
		spin_lock(&realm->inodes_with_caps_lock);
		list_add(&ci->i_snap_realm_item, &realm->inodes_with_caps);
		if (realm->ino == ci->i_vino.ino)
			realm->inode = inode;
		spin_unlock(&realm->inodes_with_caps_lock);
	}
}

 
void ceph_handle_snap(struct ceph_mds_client *mdsc,
		      struct ceph_mds_session *session,
		      struct ceph_msg *msg)
{
	struct super_block *sb = mdsc->fsc->sb;
	int mds = session->s_mds;
	u64 split;
	int op;
	int trace_len;
	struct ceph_snap_realm *realm = NULL;
	void *p = msg->front.iov_base;
	void *e = p + msg->front.iov_len;
	struct ceph_mds_snap_head *h;
	int num_split_inos, num_split_realms;
	__le64 *split_inos = NULL, *split_realms = NULL;
	int i;
	int locked_rwsem = 0;
	bool close_sessions = false;

	if (!ceph_inc_mds_stopping_blocker(mdsc, session))
		return;

	 
	if (msg->front.iov_len < sizeof(*h))
		goto bad;
	h = p;
	op = le32_to_cpu(h->op);
	split = le64_to_cpu(h->split);    
	num_split_inos = le32_to_cpu(h->num_split_inos);
	num_split_realms = le32_to_cpu(h->num_split_realms);
	trace_len = le32_to_cpu(h->trace_len);
	p += sizeof(*h);

	dout("%s from mds%d op %s split %llx tracelen %d\n", __func__,
	     mds, ceph_snap_op_name(op), split, trace_len);

	down_write(&mdsc->snap_rwsem);
	locked_rwsem = 1;

	if (op == CEPH_SNAP_OP_SPLIT) {
		struct ceph_mds_snap_realm *ri;

		 
		split_inos = p;
		p += sizeof(u64) * num_split_inos;
		split_realms = p;
		p += sizeof(u64) * num_split_realms;
		ceph_decode_need(&p, e, sizeof(*ri), bad);
		 
		ri = p;

		realm = ceph_lookup_snap_realm(mdsc, split);
		if (!realm) {
			realm = ceph_create_snap_realm(mdsc, split);
			if (IS_ERR(realm))
				goto out;
		}

		dout("splitting snap_realm %llx %p\n", realm->ino, realm);
		for (i = 0; i < num_split_inos; i++) {
			struct ceph_vino vino = {
				.ino = le64_to_cpu(split_inos[i]),
				.snap = CEPH_NOSNAP,
			};
			struct inode *inode = ceph_find_inode(sb, vino);
			struct ceph_inode_info *ci;

			if (!inode)
				continue;
			ci = ceph_inode(inode);

			spin_lock(&ci->i_ceph_lock);
			if (!ci->i_snap_realm)
				goto skip_inode;
			 
			if (ci->i_snap_realm->created >
			    le64_to_cpu(ri->created)) {
				dout(" leaving %p %llx.%llx in newer realm %llx %p\n",
				     inode, ceph_vinop(inode), ci->i_snap_realm->ino,
				     ci->i_snap_realm);
				goto skip_inode;
			}
			dout(" will move %p %llx.%llx to split realm %llx %p\n",
			     inode, ceph_vinop(inode), realm->ino, realm);

			ceph_get_snap_realm(mdsc, realm);
			ceph_change_snap_realm(inode, realm);
			spin_unlock(&ci->i_ceph_lock);
			iput(inode);
			continue;

skip_inode:
			spin_unlock(&ci->i_ceph_lock);
			iput(inode);
		}

		 
		for (i = 0; i < num_split_realms; i++) {
			struct ceph_snap_realm *child =
				__lookup_snap_realm(mdsc,
					   le64_to_cpu(split_realms[i]));
			if (!child)
				continue;
			adjust_snap_realm_parent(mdsc, child, realm->ino);
		}
	} else {
		 
		p += sizeof(u64) * num_split_inos;
		p += sizeof(u64) * num_split_realms;
	}

	 
	if (ceph_update_snap_trace(mdsc, p, e,
				   op == CEPH_SNAP_OP_DESTROY,
				   NULL)) {
		close_sessions = true;
		goto bad;
	}

	if (op == CEPH_SNAP_OP_SPLIT)
		 
		ceph_put_snap_realm(mdsc, realm);

	__cleanup_empty_realms(mdsc);

	up_write(&mdsc->snap_rwsem);

	flush_snaps(mdsc);
	ceph_dec_mds_stopping_blocker(mdsc);
	return;

bad:
	pr_err("%s corrupt snap message from mds%d\n", __func__, mds);
	ceph_msg_dump(msg);
out:
	if (locked_rwsem)
		up_write(&mdsc->snap_rwsem);

	ceph_dec_mds_stopping_blocker(mdsc);

	if (close_sessions)
		ceph_mdsc_close_sessions(mdsc);
	return;
}

struct ceph_snapid_map* ceph_get_snapid_map(struct ceph_mds_client *mdsc,
					    u64 snap)
{
	struct ceph_snapid_map *sm, *exist;
	struct rb_node **p, *parent;
	int ret;

	exist = NULL;
	spin_lock(&mdsc->snapid_map_lock);
	p = &mdsc->snapid_map_tree.rb_node;
	while (*p) {
		exist = rb_entry(*p, struct ceph_snapid_map, node);
		if (snap > exist->snap) {
			p = &(*p)->rb_left;
		} else if (snap < exist->snap) {
			p = &(*p)->rb_right;
		} else {
			if (atomic_inc_return(&exist->ref) == 1)
				list_del_init(&exist->lru);
			break;
		}
		exist = NULL;
	}
	spin_unlock(&mdsc->snapid_map_lock);
	if (exist) {
		dout("%s found snapid map %llx -> %x\n", __func__,
		     exist->snap, exist->dev);
		return exist;
	}

	sm = kmalloc(sizeof(*sm), GFP_NOFS);
	if (!sm)
		return NULL;

	ret = get_anon_bdev(&sm->dev);
	if (ret < 0) {
		kfree(sm);
		return NULL;
	}

	INIT_LIST_HEAD(&sm->lru);
	atomic_set(&sm->ref, 1);
	sm->snap = snap;

	exist = NULL;
	parent = NULL;
	p = &mdsc->snapid_map_tree.rb_node;
	spin_lock(&mdsc->snapid_map_lock);
	while (*p) {
		parent = *p;
		exist = rb_entry(*p, struct ceph_snapid_map, node);
		if (snap > exist->snap)
			p = &(*p)->rb_left;
		else if (snap < exist->snap)
			p = &(*p)->rb_right;
		else
			break;
		exist = NULL;
	}
	if (exist) {
		if (atomic_inc_return(&exist->ref) == 1)
			list_del_init(&exist->lru);
	} else {
		rb_link_node(&sm->node, parent, p);
		rb_insert_color(&sm->node, &mdsc->snapid_map_tree);
	}
	spin_unlock(&mdsc->snapid_map_lock);
	if (exist) {
		free_anon_bdev(sm->dev);
		kfree(sm);
		dout("%s found snapid map %llx -> %x\n", __func__,
		     exist->snap, exist->dev);
		return exist;
	}

	dout("%s create snapid map %llx -> %x\n", __func__,
	     sm->snap, sm->dev);
	return sm;
}

void ceph_put_snapid_map(struct ceph_mds_client* mdsc,
			 struct ceph_snapid_map *sm)
{
	if (!sm)
		return;
	if (atomic_dec_and_lock(&sm->ref, &mdsc->snapid_map_lock)) {
		if (!RB_EMPTY_NODE(&sm->node)) {
			sm->last_used = jiffies;
			list_add_tail(&sm->lru, &mdsc->snapid_map_lru);
			spin_unlock(&mdsc->snapid_map_lock);
		} else {
			 
			spin_unlock(&mdsc->snapid_map_lock);
			kfree(sm);
		}
	}
}

void ceph_trim_snapid_map(struct ceph_mds_client *mdsc)
{
	struct ceph_snapid_map *sm;
	unsigned long now;
	LIST_HEAD(to_free);

	spin_lock(&mdsc->snapid_map_lock);
	now = jiffies;

	while (!list_empty(&mdsc->snapid_map_lru)) {
		sm = list_first_entry(&mdsc->snapid_map_lru,
				      struct ceph_snapid_map, lru);
		if (time_after(sm->last_used + CEPH_SNAPID_MAP_TIMEOUT, now))
			break;

		rb_erase(&sm->node, &mdsc->snapid_map_tree);
		list_move(&sm->lru, &to_free);
	}
	spin_unlock(&mdsc->snapid_map_lock);

	while (!list_empty(&to_free)) {
		sm = list_first_entry(&to_free, struct ceph_snapid_map, lru);
		list_del(&sm->lru);
		dout("trim snapid map %llx -> %x\n", sm->snap, sm->dev);
		free_anon_bdev(sm->dev);
		kfree(sm);
	}
}

void ceph_cleanup_snapid_map(struct ceph_mds_client *mdsc)
{
	struct ceph_snapid_map *sm;
	struct rb_node *p;
	LIST_HEAD(to_free);

	spin_lock(&mdsc->snapid_map_lock);
	while ((p = rb_first(&mdsc->snapid_map_tree))) {
		sm = rb_entry(p, struct ceph_snapid_map, node);
		rb_erase(p, &mdsc->snapid_map_tree);
		RB_CLEAR_NODE(p);
		list_move(&sm->lru, &to_free);
	}
	spin_unlock(&mdsc->snapid_map_lock);

	while (!list_empty(&to_free)) {
		sm = list_first_entry(&to_free, struct ceph_snapid_map, lru);
		list_del(&sm->lru);
		free_anon_bdev(sm->dev);
		if (WARN_ON_ONCE(atomic_read(&sm->ref))) {
			pr_err("snapid map %llx -> %x still in use\n",
			       sm->snap, sm->dev);
		}
		kfree(sm);
	}
}
