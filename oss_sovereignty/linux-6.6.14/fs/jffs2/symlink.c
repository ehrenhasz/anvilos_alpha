 

#include "nodelist.h"

const struct inode_operations jffs2_symlink_inode_operations =
{
	.get_link =	simple_get_link,
	.setattr =	jffs2_setattr,
	.listxattr =	jffs2_listxattr,
};
