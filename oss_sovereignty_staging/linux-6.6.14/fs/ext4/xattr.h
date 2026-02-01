
 

#include <linux/xattr.h>

 
#define EXT4_XATTR_MAGIC		0xEA020000

 
#define EXT4_XATTR_REFCOUNT_MAX		1024

 
#define EXT4_XATTR_INDEX_USER			1
#define EXT4_XATTR_INDEX_POSIX_ACL_ACCESS	2
#define EXT4_XATTR_INDEX_POSIX_ACL_DEFAULT	3
#define EXT4_XATTR_INDEX_TRUSTED		4
#define	EXT4_XATTR_INDEX_LUSTRE			5
#define EXT4_XATTR_INDEX_SECURITY	        6
#define EXT4_XATTR_INDEX_SYSTEM			7
#define EXT4_XATTR_INDEX_RICHACL		8
#define EXT4_XATTR_INDEX_ENCRYPTION		9
#define EXT4_XATTR_INDEX_HURD			10  

struct ext4_xattr_header {
	__le32	h_magic;	 
	__le32	h_refcount;	 
	__le32	h_blocks;	 
	__le32	h_hash;		 
	__le32	h_checksum;	 
				 
	__u32	h_reserved[3];	 
};

struct ext4_xattr_ibody_header {
	__le32	h_magic;	 
};

struct ext4_xattr_entry {
	__u8	e_name_len;	 
	__u8	e_name_index;	 
	__le16	e_value_offs;	 
	__le32	e_value_inum;	 
	__le32	e_value_size;	 
	__le32	e_hash;		 
	char	e_name[];	 
};

#define EXT4_XATTR_PAD_BITS		2
#define EXT4_XATTR_PAD		(1<<EXT4_XATTR_PAD_BITS)
#define EXT4_XATTR_ROUND		(EXT4_XATTR_PAD-1)
#define EXT4_XATTR_LEN(name_len) \
	(((name_len) + EXT4_XATTR_ROUND + \
	sizeof(struct ext4_xattr_entry)) & ~EXT4_XATTR_ROUND)
#define EXT4_XATTR_NEXT(entry) \
	((struct ext4_xattr_entry *)( \
	 (char *)(entry) + EXT4_XATTR_LEN((entry)->e_name_len)))
#define EXT4_XATTR_SIZE(size) \
	(((size) + EXT4_XATTR_ROUND) & ~EXT4_XATTR_ROUND)

#define IHDR(inode, raw_inode) \
	((struct ext4_xattr_ibody_header *) \
		((void *)raw_inode + \
		EXT4_GOOD_OLD_INODE_SIZE + \
		EXT4_I(inode)->i_extra_isize))
#define IFIRST(hdr) ((struct ext4_xattr_entry *)((hdr)+1))

 
#define EXT4_XATTR_SIZE_MAX (1 << 24)

 
#define EXT4_XATTR_MIN_LARGE_EA_SIZE(b)					\
	((b) - EXT4_XATTR_LEN(3) - sizeof(struct ext4_xattr_header) - 4)

#define BHDR(bh) ((struct ext4_xattr_header *)((bh)->b_data))
#define ENTRY(ptr) ((struct ext4_xattr_entry *)(ptr))
#define BFIRST(bh) ENTRY(BHDR(bh)+1)
#define IS_LAST_ENTRY(entry) (*(__u32 *)(entry) == 0)

#define EXT4_ZERO_XATTR_VALUE ((void *)-1)

 
#define EXT4_INODE_HAS_XATTR_SPACE(inode)				\
	((EXT4_I(inode)->i_extra_isize != 0) &&				\
	 (EXT4_GOOD_OLD_INODE_SIZE + EXT4_I(inode)->i_extra_isize +	\
	  sizeof(struct ext4_xattr_ibody_header) + EXT4_XATTR_PAD <=	\
	  EXT4_INODE_SIZE((inode)->i_sb)))

struct ext4_xattr_info {
	const char *name;
	const void *value;
	size_t value_len;
	int name_index;
	int in_inode;
};

struct ext4_xattr_search {
	struct ext4_xattr_entry *first;
	void *base;
	void *end;
	struct ext4_xattr_entry *here;
	int not_found;
};

struct ext4_xattr_ibody_find {
	struct ext4_xattr_search s;
	struct ext4_iloc iloc;
};

struct ext4_xattr_inode_array {
	unsigned int count;		 
	struct inode *inodes[];
};

extern const struct xattr_handler ext4_xattr_user_handler;
extern const struct xattr_handler ext4_xattr_trusted_handler;
extern const struct xattr_handler ext4_xattr_security_handler;
extern const struct xattr_handler ext4_xattr_hurd_handler;

#define EXT4_XATTR_NAME_ENCRYPTION_CONTEXT "c"

 
static inline void ext4_write_lock_xattr(struct inode *inode, int *save)
{
	down_write(&EXT4_I(inode)->xattr_sem);
	*save = ext4_test_inode_state(inode, EXT4_STATE_NO_EXPAND);
	ext4_set_inode_state(inode, EXT4_STATE_NO_EXPAND);
}

static inline int ext4_write_trylock_xattr(struct inode *inode, int *save)
{
	if (down_write_trylock(&EXT4_I(inode)->xattr_sem) == 0)
		return 0;
	*save = ext4_test_inode_state(inode, EXT4_STATE_NO_EXPAND);
	ext4_set_inode_state(inode, EXT4_STATE_NO_EXPAND);
	return 1;
}

static inline void ext4_write_unlock_xattr(struct inode *inode, int *save)
{
	if (*save == 0)
		ext4_clear_inode_state(inode, EXT4_STATE_NO_EXPAND);
	up_write(&EXT4_I(inode)->xattr_sem);
}

extern ssize_t ext4_listxattr(struct dentry *, char *, size_t);

extern int ext4_xattr_get(struct inode *, int, const char *, void *, size_t);
extern int ext4_xattr_set(struct inode *, int, const char *, const void *, size_t, int);
extern int ext4_xattr_set_handle(handle_t *, struct inode *, int, const char *, const void *, size_t, int);
extern int ext4_xattr_set_credits(struct inode *inode, size_t value_len,
				  bool is_create, int *credits);
extern int __ext4_xattr_set_credits(struct super_block *sb, struct inode *inode,
				struct buffer_head *block_bh, size_t value_len,
				bool is_create);

extern int ext4_xattr_delete_inode(handle_t *handle, struct inode *inode,
				   struct ext4_xattr_inode_array **array,
				   int extra_credits);
extern void ext4_xattr_inode_array_free(struct ext4_xattr_inode_array *array);

extern int ext4_expand_extra_isize_ea(struct inode *inode, int new_extra_isize,
			    struct ext4_inode *raw_inode, handle_t *handle);
extern void ext4_evict_ea_inode(struct inode *inode);

extern const struct xattr_handler *ext4_xattr_handlers[];

extern int ext4_xattr_ibody_find(struct inode *inode, struct ext4_xattr_info *i,
				 struct ext4_xattr_ibody_find *is);
extern int ext4_xattr_ibody_get(struct inode *inode, int name_index,
				const char *name,
				void *buffer, size_t buffer_size);
extern int ext4_xattr_ibody_set(handle_t *handle, struct inode *inode,
				struct ext4_xattr_info *i,
				struct ext4_xattr_ibody_find *is);

extern struct mb_cache *ext4_xattr_create_cache(void);
extern void ext4_xattr_destroy_cache(struct mb_cache *);

#ifdef CONFIG_EXT4_FS_SECURITY
extern int ext4_init_security(handle_t *handle, struct inode *inode,
			      struct inode *dir, const struct qstr *qstr);
#else
static inline int ext4_init_security(handle_t *handle, struct inode *inode,
				     struct inode *dir, const struct qstr *qstr)
{
	return 0;
}
#endif

#ifdef CONFIG_LOCKDEP
extern void ext4_xattr_inode_set_class(struct inode *ea_inode);
#else
static inline void ext4_xattr_inode_set_class(struct inode *ea_inode) { }
#endif

extern int ext4_get_inode_usage(struct inode *inode, qsize_t *usage);
