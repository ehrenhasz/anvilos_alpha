
 

#include "ext2.h"
#include "xattr.h"

const struct inode_operations ext2_symlink_inode_operations = {
	.get_link	= page_get_link,
	.getattr	= ext2_getattr,
	.setattr	= ext2_setattr,
	.listxattr	= ext2_listxattr,
};
 
const struct inode_operations ext2_fast_symlink_inode_operations = {
	.get_link	= simple_get_link,
	.getattr	= ext2_getattr,
	.setattr	= ext2_setattr,
	.listxattr	= ext2_listxattr,
};
