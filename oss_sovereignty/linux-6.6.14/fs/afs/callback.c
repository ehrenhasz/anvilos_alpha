 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/circ_buf.h>
#include <linux/sched.h>
#include "internal.h"

 
void afs_invalidate_mmap_work(struct work_struct *work)
{
	struct afs_vnode *vnode = container_of(work, struct afs_vnode, cb_work);

	unmap_mapping_pages(vnode->netfs.inode.i_mapping, 0, 0, false);
}

void afs_server_init_callback_work(struct work_struct *work)
{
	struct afs_server *server = container_of(work, struct afs_server, initcb_work);
	struct afs_vnode *vnode;
	struct afs_cell *cell = server->cell;

	down_read(&cell->fs_open_mmaps_lock);

	list_for_each_entry(vnode, &cell->fs_open_mmaps, cb_mmap_link) {
		if (vnode->cb_server == server) {
			clear_bit(AFS_VNODE_CB_PROMISED, &vnode->flags);
			queue_work(system_unbound_wq, &vnode->cb_work);
		}
	}

	up_read(&cell->fs_open_mmaps_lock);
}

 
void afs_init_callback_state(struct afs_server *server)
{
	rcu_read_lock();
	do {
		server->cb_s_break++;
		atomic_inc(&server->cell->fs_s_break);
		if (!list_empty(&server->cell->fs_open_mmaps))
			queue_work(system_unbound_wq, &server->initcb_work);

	} while ((server = rcu_dereference(server->uuid_next)));
	rcu_read_unlock();
}

 
void __afs_break_callback(struct afs_vnode *vnode, enum afs_cb_break_reason reason)
{
	_enter("");

	clear_bit(AFS_VNODE_NEW_CONTENT, &vnode->flags);
	if (test_and_clear_bit(AFS_VNODE_CB_PROMISED, &vnode->flags)) {
		vnode->cb_break++;
		vnode->cb_v_break = vnode->volume->cb_v_break;
		afs_clear_permits(vnode);

		if (vnode->lock_state == AFS_VNODE_LOCK_WAITING_FOR_CB)
			afs_lock_may_be_available(vnode);

		if (reason != afs_cb_break_for_deleted &&
		    vnode->status.type == AFS_FTYPE_FILE &&
		    atomic_read(&vnode->cb_nr_mmap))
			queue_work(system_unbound_wq, &vnode->cb_work);

		trace_afs_cb_break(&vnode->fid, vnode->cb_break, reason, true);
	} else {
		trace_afs_cb_break(&vnode->fid, vnode->cb_break, reason, false);
	}
}

void afs_break_callback(struct afs_vnode *vnode, enum afs_cb_break_reason reason)
{
	write_seqlock(&vnode->cb_lock);
	__afs_break_callback(vnode, reason);
	write_sequnlock(&vnode->cb_lock);
}

 
static struct afs_volume *afs_lookup_volume_rcu(struct afs_cell *cell,
						afs_volid_t vid)
{
	struct afs_volume *volume = NULL;
	struct rb_node *p;
	int seq = 0;

	do {
		 
		read_seqbegin_or_lock(&cell->volume_lock, &seq);

		p = rcu_dereference_raw(cell->volumes.rb_node);
		while (p) {
			volume = rb_entry(p, struct afs_volume, cell_node);

			if (volume->vid < vid)
				p = rcu_dereference_raw(p->rb_left);
			else if (volume->vid > vid)
				p = rcu_dereference_raw(p->rb_right);
			else
				break;
			volume = NULL;
		}

	} while (need_seqretry(&cell->volume_lock, seq));

	done_seqretry(&cell->volume_lock, seq);
	return volume;
}

 
static void afs_break_one_callback(struct afs_volume *volume,
				   struct afs_fid *fid)
{
	struct super_block *sb;
	struct afs_vnode *vnode;
	struct inode *inode;

	if (fid->vnode == 0 && fid->unique == 0) {
		 
		write_lock(&volume->cb_v_break_lock);
		volume->cb_v_break++;
		trace_afs_cb_break(fid, volume->cb_v_break,
				   afs_cb_break_for_volume_callback, false);
		write_unlock(&volume->cb_v_break_lock);
		return;
	}

	 
	sb = rcu_dereference(volume->sb);
	if (!sb)
		return;

	inode = find_inode_rcu(sb, fid->vnode, afs_ilookup5_test_by_fid, fid);
	if (inode) {
		vnode = AFS_FS_I(inode);
		afs_break_callback(vnode, afs_cb_break_for_callback);
	} else {
		trace_afs_cb_miss(fid, afs_cb_break_for_callback);
	}
}

static void afs_break_some_callbacks(struct afs_server *server,
				     struct afs_callback_break *cbb,
				     size_t *_count)
{
	struct afs_callback_break *residue = cbb;
	struct afs_volume *volume;
	afs_volid_t vid = cbb->fid.vid;
	size_t i;

	volume = afs_lookup_volume_rcu(server->cell, vid);

	 

	for (i = *_count; i > 0; cbb++, i--) {
		if (cbb->fid.vid == vid) {
			_debug("- Fid { vl=%08llx n=%llu u=%u }",
			       cbb->fid.vid,
			       cbb->fid.vnode,
			       cbb->fid.unique);
			--*_count;
			if (volume)
				afs_break_one_callback(volume, &cbb->fid);
		} else {
			*residue++ = *cbb;
		}
	}
}

 
void afs_break_callbacks(struct afs_server *server, size_t count,
			 struct afs_callback_break *callbacks)
{
	_enter("%p,%zu,", server, count);

	ASSERT(server != NULL);

	rcu_read_lock();

	while (count > 0)
		afs_break_some_callbacks(server, callbacks, &count);

	rcu_read_unlock();
	return;
}
