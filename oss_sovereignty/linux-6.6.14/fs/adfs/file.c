
 
#include "adfs.h"

const struct file_operations adfs_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.mmap		= generic_file_mmap,
	.fsync		= generic_file_fsync,
	.write_iter	= generic_file_write_iter,
	.splice_read	= filemap_splice_read,
};

const struct inode_operations adfs_file_inode_operations = {
	.setattr	= adfs_notify_change,
};
