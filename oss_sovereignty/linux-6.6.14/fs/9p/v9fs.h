 
 
#ifndef FS_9P_V9FS_H
#define FS_9P_V9FS_H

#include <linux/backing-dev.h>
#include <linux/netfs.h>

 
#define	V9FS_ACCESS_ANY (V9FS_ACCESS_SINGLE | \
			 V9FS_ACCESS_USER |   \
			 V9FS_ACCESS_CLIENT)
#define V9FS_ACCESS_MASK V9FS_ACCESS_ANY
#define V9FS_ACL_MASK V9FS_POSIX_ACL

enum p9_session_flags {
	V9FS_PROTO_2000U    = 0x01,
	V9FS_PROTO_2000L    = 0x02,
	V9FS_ACCESS_SINGLE  = 0x04,
	V9FS_ACCESS_USER    = 0x08,
	V9FS_ACCESS_CLIENT  = 0x10,
	V9FS_POSIX_ACL      = 0x20,
	V9FS_NO_XATTR       = 0x40,
	V9FS_IGNORE_QV      = 0x80,  
	V9FS_DIRECT_IO      = 0x100,
	V9FS_SYNC           = 0x200
};

 

enum p9_cache_shortcuts {
	CACHE_SC_NONE       = 0b00000000,
	CACHE_SC_READAHEAD  = 0b00000001,
	CACHE_SC_MMAP       = 0b00000101,
	CACHE_SC_LOOSE      = 0b00001111,
	CACHE_SC_FSCACHE    = 0b10001111,
};

 

enum p9_cache_bits {
	CACHE_NONE          = 0b00000000,
	CACHE_FILE          = 0b00000001,
	CACHE_META          = 0b00000010,
	CACHE_WRITEBACK     = 0b00000100,
	CACHE_LOOSE         = 0b00001000,
	CACHE_FSCACHE       = 0b10000000,
};

 

struct v9fs_session_info {
	 
	unsigned int flags;
	unsigned char nodev;
	unsigned short debug;
	unsigned int afid;
	unsigned int cache;
#ifdef CONFIG_9P_FSCACHE
	char *cachetag;
	struct fscache_volume *fscache;
#endif

	char *uname;		 
	char *aname;		 
	unsigned int maxdata;	 
	kuid_t dfltuid;		 
	kgid_t dfltgid;		 
	kuid_t uid;		 
	struct p9_client *clnt;	 
	struct list_head slist;  
	struct rw_semaphore rename_sem;
	long session_lock_timeout;  
};

 
#define V9FS_INO_INVALID_ATTR 0x01

struct v9fs_inode {
	struct netfs_inode netfs;  
	struct p9_qid qid;
	unsigned int cache_validity;
	struct mutex v_mutex;
};

static inline struct v9fs_inode *V9FS_I(const struct inode *inode)
{
	return container_of(inode, struct v9fs_inode, netfs.inode);
}

static inline struct fscache_cookie *v9fs_inode_cookie(struct v9fs_inode *v9inode)
{
#ifdef CONFIG_9P_FSCACHE
	return netfs_i_cookie(&v9inode->netfs);
#else
	return NULL;
#endif
}

static inline struct fscache_volume *v9fs_session_cache(struct v9fs_session_info *v9ses)
{
#ifdef CONFIG_9P_FSCACHE
	return v9ses->fscache;
#else
	return NULL;
#endif
}


extern int v9fs_show_options(struct seq_file *m, struct dentry *root);

struct p9_fid *v9fs_session_init(struct v9fs_session_info *v9ses,
				 const char *dev_name, char *data);
extern void v9fs_session_close(struct v9fs_session_info *v9ses);
extern void v9fs_session_cancel(struct v9fs_session_info *v9ses);
extern void v9fs_session_begin_cancel(struct v9fs_session_info *v9ses);
extern struct dentry *v9fs_vfs_lookup(struct inode *dir, struct dentry *dentry,
				      unsigned int flags);
extern int v9fs_vfs_unlink(struct inode *i, struct dentry *d);
extern int v9fs_vfs_rmdir(struct inode *i, struct dentry *d);
extern int v9fs_vfs_rename(struct mnt_idmap *idmap,
			   struct inode *old_dir, struct dentry *old_dentry,
			   struct inode *new_dir, struct dentry *new_dentry,
			   unsigned int flags);
extern struct inode *v9fs_inode_from_fid(struct v9fs_session_info *v9ses,
					 struct p9_fid *fid,
					 struct super_block *sb, int new);
extern const struct inode_operations v9fs_dir_inode_operations_dotl;
extern const struct inode_operations v9fs_file_inode_operations_dotl;
extern const struct inode_operations v9fs_symlink_inode_operations_dotl;
extern const struct netfs_request_ops v9fs_req_ops;
extern struct inode *v9fs_inode_from_fid_dotl(struct v9fs_session_info *v9ses,
					      struct p9_fid *fid,
					      struct super_block *sb, int new);

 
#define V9FS_PORT	564
#define V9FS_DEFUSER	"nobody"
#define V9FS_DEFANAME	""
#define V9FS_DEFUID	KUIDT_INIT(-2)
#define V9FS_DEFGID	KGIDT_INIT(-2)

static inline struct v9fs_session_info *v9fs_inode2v9ses(struct inode *inode)
{
	return inode->i_sb->s_fs_info;
}

static inline struct v9fs_session_info *v9fs_dentry2v9ses(struct dentry *dentry)
{
	return dentry->d_sb->s_fs_info;
}

static inline int v9fs_proto_dotu(struct v9fs_session_info *v9ses)
{
	return v9ses->flags & V9FS_PROTO_2000U;
}

static inline int v9fs_proto_dotl(struct v9fs_session_info *v9ses)
{
	return v9ses->flags & V9FS_PROTO_2000L;
}

 
static inline struct inode *
v9fs_get_inode_from_fid(struct v9fs_session_info *v9ses, struct p9_fid *fid,
			struct super_block *sb)
{
	if (v9fs_proto_dotl(v9ses))
		return v9fs_inode_from_fid_dotl(v9ses, fid, sb, 0);
	else
		return v9fs_inode_from_fid(v9ses, fid, sb, 0);
}

 
static inline struct inode *
v9fs_get_new_inode_from_fid(struct v9fs_session_info *v9ses, struct p9_fid *fid,
			    struct super_block *sb)
{
	if (v9fs_proto_dotl(v9ses))
		return v9fs_inode_from_fid_dotl(v9ses, fid, sb, 1);
	else
		return v9fs_inode_from_fid(v9ses, fid, sb, 1);
}

#endif
