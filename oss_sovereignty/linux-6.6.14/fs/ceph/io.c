
 

#include <linux/ceph/ceph_debug.h>

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/rwsem.h>
#include <linux/fs.h>

#include "super.h"
#include "io.h"

 
static void ceph_block_o_direct(struct ceph_inode_info *ci, struct inode *inode)
{
	lockdep_assert_held_write(&inode->i_rwsem);

	if (READ_ONCE(ci->i_ceph_flags) & CEPH_I_ODIRECT) {
		spin_lock(&ci->i_ceph_lock);
		ci->i_ceph_flags &= ~CEPH_I_ODIRECT;
		spin_unlock(&ci->i_ceph_lock);
		inode_dio_wait(inode);
	}
}

 
void
ceph_start_io_read(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	 
	down_read(&inode->i_rwsem);
	if (!(READ_ONCE(ci->i_ceph_flags) & CEPH_I_ODIRECT))
		return;
	up_read(&inode->i_rwsem);
	 
	down_write(&inode->i_rwsem);
	ceph_block_o_direct(ci, inode);
	downgrade_write(&inode->i_rwsem);
}

 
void
ceph_end_io_read(struct inode *inode)
{
	up_read(&inode->i_rwsem);
}

 
void
ceph_start_io_write(struct inode *inode)
{
	down_write(&inode->i_rwsem);
	ceph_block_o_direct(ceph_inode(inode), inode);
}

 
void
ceph_end_io_write(struct inode *inode)
{
	up_write(&inode->i_rwsem);
}

 
static void ceph_block_buffered(struct ceph_inode_info *ci, struct inode *inode)
{
	lockdep_assert_held_write(&inode->i_rwsem);

	if (!(READ_ONCE(ci->i_ceph_flags) & CEPH_I_ODIRECT)) {
		spin_lock(&ci->i_ceph_lock);
		ci->i_ceph_flags |= CEPH_I_ODIRECT;
		spin_unlock(&ci->i_ceph_lock);
		 
		filemap_write_and_wait(inode->i_mapping);
	}
}

 
void
ceph_start_io_direct(struct inode *inode)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	 
	down_read(&inode->i_rwsem);
	if (READ_ONCE(ci->i_ceph_flags) & CEPH_I_ODIRECT)
		return;
	up_read(&inode->i_rwsem);
	 
	down_write(&inode->i_rwsem);
	ceph_block_buffered(ci, inode);
	downgrade_write(&inode->i_rwsem);
}

 
void
ceph_end_io_direct(struct inode *inode)
{
	up_read(&inode->i_rwsem);
}
