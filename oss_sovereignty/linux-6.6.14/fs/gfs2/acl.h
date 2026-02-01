 
 

#ifndef __ACL_DOT_H__
#define __ACL_DOT_H__

#include "incore.h"

#define GFS2_ACL_MAX_ENTRIES(sdp) ((300 << (sdp)->sd_sb.sb_bsize_shift) >> 12)

extern struct posix_acl *gfs2_get_acl(struct inode *inode, int type, bool rcu);
extern int __gfs2_set_acl(struct inode *inode, struct posix_acl *acl, int type);
extern int gfs2_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
			struct posix_acl *acl, int type);

#endif  
