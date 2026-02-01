
 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/rwsem.h>
#include <linux/fs.h>
#include <linux/nfs_fs.h>

#include "internal.h"

 
static void nfs_block_o_direct(struct nfs_inode *nfsi, struct inode *inode)
{
	if (test_bit(NFS_INO_ODIRECT, &nfsi->flags)) {
		clear_bit(NFS_INO_ODIRECT, &nfsi->flags);
		inode_dio_wait(inode);
	}
}

 
void
nfs_start_io_read(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	 
	down_read(&inode->i_rwsem);
	if (test_bit(NFS_INO_ODIRECT, &nfsi->flags) == 0)
		return;
	up_read(&inode->i_rwsem);
	 
	down_write(&inode->i_rwsem);
	nfs_block_o_direct(nfsi, inode);
	downgrade_write(&inode->i_rwsem);
}

 
void
nfs_end_io_read(struct inode *inode)
{
	up_read(&inode->i_rwsem);
}

 
void
nfs_start_io_write(struct inode *inode)
{
	down_write(&inode->i_rwsem);
	nfs_block_o_direct(NFS_I(inode), inode);
}

 
void
nfs_end_io_write(struct inode *inode)
{
	up_write(&inode->i_rwsem);
}

 
static void nfs_block_buffered(struct nfs_inode *nfsi, struct inode *inode)
{
	if (!test_bit(NFS_INO_ODIRECT, &nfsi->flags)) {
		set_bit(NFS_INO_ODIRECT, &nfsi->flags);
		nfs_sync_mapping(inode->i_mapping);
	}
}

 
void
nfs_start_io_direct(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	 
	down_read(&inode->i_rwsem);
	if (test_bit(NFS_INO_ODIRECT, &nfsi->flags) != 0)
		return;
	up_read(&inode->i_rwsem);
	 
	down_write(&inode->i_rwsem);
	nfs_block_buffered(nfsi, inode);
	downgrade_write(&inode->i_rwsem);
}

 
void
nfs_end_io_direct(struct inode *inode)
{
	up_read(&inode->i_rwsem);
}
