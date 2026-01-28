


#ifndef _SECURITY_LANDLOCK_FS_H
#define _SECURITY_LANDLOCK_FS_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/rcupdate.h>

#include "ruleset.h"
#include "setup.h"


struct landlock_inode_security {
	
	struct landlock_object __rcu *object;
};


struct landlock_file_security {
	
	access_mask_t allowed_access;
};


struct landlock_superblock_security {
	
	atomic_long_t inode_refs;
};

static inline struct landlock_file_security *
landlock_file(const struct file *const file)
{
	return file->f_security + landlock_blob_sizes.lbs_file;
}

static inline struct landlock_inode_security *
landlock_inode(const struct inode *const inode)
{
	return inode->i_security + landlock_blob_sizes.lbs_inode;
}

static inline struct landlock_superblock_security *
landlock_superblock(const struct super_block *const superblock)
{
	return superblock->s_security + landlock_blob_sizes.lbs_superblock;
}

__init void landlock_add_fs_hooks(void);

int landlock_append_fs_rule(struct landlock_ruleset *const ruleset,
			    const struct path *const path,
			    access_mask_t access_hierarchy);

#endif 
