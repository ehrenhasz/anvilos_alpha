 
 

#ifndef H_JFS_XATTR
#define H_JFS_XATTR

#include <linux/xattr.h>

 
struct jfs_ea {
	u8 flag;	 
	u8 namelen;	 
	__le16 valuelen;	 
	char name[];	 
};			 

struct jfs_ea_list {
	__le32 size;		 
	struct jfs_ea ea[];	 
};

 
#define MAXEASIZE	65535
#define MAXEALISTSIZE	MAXEASIZE

 
#define EA_SIZE(ea) \
	(sizeof (struct jfs_ea) + (ea)->namelen + 1 + \
	 le16_to_cpu((ea)->valuelen))
#define	NEXT_EA(ea) ((struct jfs_ea *) (((char *) (ea)) + (EA_SIZE (ea))))
#define	FIRST_EA(ealist) ((ealist)->ea)
#define	EALIST_SIZE(ealist) le32_to_cpu((ealist)->size)
#define	END_EALIST(ealist) \
	((struct jfs_ea *) (((char *) (ealist)) + EALIST_SIZE(ealist)))

extern int __jfs_setxattr(tid_t, struct inode *, const char *, const void *,
			  size_t, int);
extern ssize_t __jfs_getxattr(struct inode *, const char *, void *, size_t);
extern ssize_t jfs_listxattr(struct dentry *, char *, size_t);

extern const struct xattr_handler *jfs_xattr_handlers[];

#ifdef CONFIG_JFS_SECURITY
extern int jfs_init_security(tid_t, struct inode *, struct inode *,
			     const struct qstr *);
#else
static inline int jfs_init_security(tid_t tid, struct inode *inode,
				    struct inode *dir, const struct qstr *qstr)
{
	return 0;
}
#endif

#endif	 
