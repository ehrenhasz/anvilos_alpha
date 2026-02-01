
 

#include <linux/fs.h>

#include "ufs_fs.h"
#include "ufs.h"

 
 
const struct file_operations ufs_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.open           = generic_file_open,
	.fsync		= generic_file_fsync,
	.splice_read	= filemap_splice_read,
};
