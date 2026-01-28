

#ifndef __LINUX_FS_NFS_NFS3_FS_H
#define __LINUX_FS_NFS_NFS3_FS_H


#ifdef CONFIG_NFS_V3_ACL
extern struct posix_acl *nfs3_get_acl(struct inode *inode, int type, bool rcu);
extern int nfs3_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
			struct posix_acl *acl, int type);
extern int nfs3_proc_setacls(struct inode *inode, struct posix_acl *acl,
		struct posix_acl *dfacl);
extern ssize_t nfs3_listxattr(struct dentry *, char *, size_t);
#else
static inline int nfs3_proc_setacls(struct inode *inode, struct posix_acl *acl,
		struct posix_acl *dfacl)
{
	return 0;
}
#define nfs3_listxattr NULL
#endif 


struct nfs_server *nfs3_create_server(struct fs_context *);
struct nfs_server *nfs3_clone_server(struct nfs_server *, struct nfs_fh *,
				     struct nfs_fattr *, rpc_authflavor_t);


extern struct nfs_subversion nfs_v3;

#endif 
