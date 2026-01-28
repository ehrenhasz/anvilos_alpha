#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
struct orangefs_khandle {
	unsigned char u[16];
}  __aligned(8);
struct orangefs_object_kref {
	struct orangefs_khandle khandle;
	__s32 fs_id;
	__s32 __pad1;
};
static inline int ORANGEFS_khandle_cmp(const struct orangefs_khandle *kh1,
				   const struct orangefs_khandle *kh2)
{
	int i;
	for (i = 15; i >= 0; i--) {
		if (kh1->u[i] > kh2->u[i])
			return 1;
		if (kh1->u[i] < kh2->u[i])
			return -1;
	}
	return 0;
}
static inline void ORANGEFS_khandle_to(const struct orangefs_khandle *kh,
				   void *p, int size)
{
	memcpy(p, kh->u, 16);
	memset(p + 16, 0, size - 16);
}
static inline void ORANGEFS_khandle_from(struct orangefs_khandle *kh,
				     void *p, int size)
{
	memset(kh, 0, 16);
	memcpy(kh->u, p, 16);
}
#define ORANGEFS_SUPER_MAGIC 0x20030528
#define ORANGEFS_ERROR_BIT (1 << 30)
#define ORANGEFS_NON_ERRNO_ERROR_BIT (1 << 29)
#define ORANGEFS_ERROR_CLASS_BITS 0x380
#define ORANGEFS_ERROR_NUMBER_BITS 0x7f
#define ORANGEFS_ECANCEL    (1|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_EDEVINIT   (2|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_EDETAIL    (3|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_EHOSTNTFD  (4|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_EADDRNTFD  (5|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_ENORECVR   (6|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_ETRYAGAIN  (7|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_ENOTPVFS   (8|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_ESECURITY  (9|ORANGEFS_NON_ERRNO_ERROR_BIT|ORANGEFS_ERROR_BIT)
#define ORANGEFS_O_EXECUTE (1 << 0)
#define ORANGEFS_O_WRITE   (1 << 1)
#define ORANGEFS_O_READ    (1 << 2)
#define ORANGEFS_G_EXECUTE (1 << 3)
#define ORANGEFS_G_WRITE   (1 << 4)
#define ORANGEFS_G_READ    (1 << 5)
#define ORANGEFS_U_EXECUTE (1 << 6)
#define ORANGEFS_U_WRITE   (1 << 7)
#define ORANGEFS_U_READ    (1 << 8)
#define ORANGEFS_G_SGID    (1 << 10)
#define ORANGEFS_U_SUID    (1 << 11)
#define ORANGEFS_ITERATE_START    2147483646
#define ORANGEFS_ITERATE_END      2147483645
#define ORANGEFS_IMMUTABLE_FL FS_IMMUTABLE_FL
#define ORANGEFS_APPEND_FL    FS_APPEND_FL
#define ORANGEFS_NOATIME_FL   FS_NOATIME_FL
#define ORANGEFS_MIRROR_FL    0x01000000ULL
#define ORANGEFS_FS_ID_NULL       ((__s32)0)
#define ORANGEFS_ATTR_SYS_UID                   (1 << 0)
#define ORANGEFS_ATTR_SYS_GID                   (1 << 1)
#define ORANGEFS_ATTR_SYS_PERM                  (1 << 2)
#define ORANGEFS_ATTR_SYS_ATIME                 (1 << 3)
#define ORANGEFS_ATTR_SYS_CTIME                 (1 << 4)
#define ORANGEFS_ATTR_SYS_MTIME                 (1 << 5)
#define ORANGEFS_ATTR_SYS_TYPE                  (1 << 6)
#define ORANGEFS_ATTR_SYS_ATIME_SET             (1 << 7)
#define ORANGEFS_ATTR_SYS_MTIME_SET             (1 << 8)
#define ORANGEFS_ATTR_SYS_SIZE                  (1 << 20)
#define ORANGEFS_ATTR_SYS_LNK_TARGET            (1 << 24)
#define ORANGEFS_ATTR_SYS_DFILE_COUNT           (1 << 25)
#define ORANGEFS_ATTR_SYS_DIRENT_COUNT          (1 << 26)
#define ORANGEFS_ATTR_SYS_BLKSIZE               (1 << 28)
#define ORANGEFS_ATTR_SYS_MIRROR_COPIES_COUNT   (1 << 29)
#define ORANGEFS_ATTR_SYS_COMMON_ALL	\
	(ORANGEFS_ATTR_SYS_UID	|	\
	 ORANGEFS_ATTR_SYS_GID	|	\
	 ORANGEFS_ATTR_SYS_PERM	|	\
	 ORANGEFS_ATTR_SYS_ATIME	|	\
	 ORANGEFS_ATTR_SYS_CTIME	|	\
	 ORANGEFS_ATTR_SYS_MTIME	|	\
	 ORANGEFS_ATTR_SYS_TYPE)
#define ORANGEFS_ATTR_SYS_ALL_SETABLE		\
(ORANGEFS_ATTR_SYS_COMMON_ALL-ORANGEFS_ATTR_SYS_TYPE)
#define ORANGEFS_ATTR_SYS_ALL_NOHINT			\
	(ORANGEFS_ATTR_SYS_COMMON_ALL		|	\
	 ORANGEFS_ATTR_SYS_SIZE			|	\
	 ORANGEFS_ATTR_SYS_LNK_TARGET		|	\
	 ORANGEFS_ATTR_SYS_DFILE_COUNT		|	\
	 ORANGEFS_ATTR_SYS_MIRROR_COPIES_COUNT	|	\
	 ORANGEFS_ATTR_SYS_DIRENT_COUNT		|	\
	 ORANGEFS_ATTR_SYS_BLKSIZE)
#define ORANGEFS_XATTR_REPLACE 0x2
#define ORANGEFS_XATTR_CREATE  0x1
#define ORANGEFS_MAX_SERVER_ADDR_LEN 256
#define ORANGEFS_NAME_MAX                256
#define ORANGEFS_MAX_XATTR_NAMELEN   ORANGEFS_NAME_MAX	 
#define ORANGEFS_MAX_XATTR_VALUELEN  8192	 
#define ORANGEFS_MAX_XATTR_LISTLEN   16	 
enum ORANGEFS_io_type {
	ORANGEFS_IO_READ = 1,
	ORANGEFS_IO_WRITE = 2
};
enum orangefs_ds_type {
	ORANGEFS_TYPE_NONE = 0,
	ORANGEFS_TYPE_METAFILE = (1 << 0),
	ORANGEFS_TYPE_DATAFILE = (1 << 1),
	ORANGEFS_TYPE_DIRECTORY = (1 << 2),
	ORANGEFS_TYPE_SYMLINK = (1 << 3),
	ORANGEFS_TYPE_DIRDATA = (1 << 4),
	ORANGEFS_TYPE_INTERNAL = (1 << 5)	 
};
struct ORANGEFS_keyval_pair {
	char key[ORANGEFS_MAX_XATTR_NAMELEN];
	__s32 key_sz;	 
	__s32 val_sz;
	char val[ORANGEFS_MAX_XATTR_VALUELEN];
};
struct ORANGEFS_sys_attr_s {
	__u32 owner;
	__u32 group;
	__u32 perms;
	__u64 atime;
	__u64 mtime;
	__u64 ctime;
	__s64 size;
	char *link_target;
	__s32 dfile_count;
	__s32 distr_dir_servers_initial;
	__s32 distr_dir_servers_max;
	__s32 distr_dir_split_size;
	__u32 mirror_copies_count;
	char *dist_name;
	char *dist_params;
	__s64 dirent_count;
	enum orangefs_ds_type objtype;
	__u64 flags;
	__u32 mask;
	__s64 blksize;
};
#define ORANGEFS_LOOKUP_LINK_NO_FOLLOW 0
struct dev_mask_info_s {
	enum {
		KERNEL_MASK,
		CLIENT_MASK,
	} mask_type;
	__u64 mask_value;
};
struct dev_mask2_info_s {
	__u64 mask1_value;
	__u64 mask2_value;
};
__s32 ORANGEFS_util_translate_mode(int mode);
#include "orangefs-debug.h"
#define llu(x) (unsigned long long)(x)
#define lld(x) (long long)(x)
#define ORANGEFS_DEV_MAGIC 'k'
#define ORANGEFS_READDIR_DEFAULT_DESC_COUNT  5
#define DEV_GET_MAGIC           0x1
#define DEV_GET_MAX_UPSIZE      0x2
#define DEV_GET_MAX_DOWNSIZE    0x3
#define DEV_MAP                 0x4
#define DEV_REMOUNT_ALL         0x5
#define DEV_DEBUG               0x6
#define DEV_UPSTREAM            0x7
#define DEV_CLIENT_MASK         0x8
#define DEV_CLIENT_STRING       0x9
#define DEV_MAX_NR              0xa
enum {
	ORANGEFS_DEV_GET_MAGIC = _IOW(ORANGEFS_DEV_MAGIC, DEV_GET_MAGIC, __s32),
	ORANGEFS_DEV_GET_MAX_UPSIZE =
	    _IOW(ORANGEFS_DEV_MAGIC, DEV_GET_MAX_UPSIZE, __s32),
	ORANGEFS_DEV_GET_MAX_DOWNSIZE =
	    _IOW(ORANGEFS_DEV_MAGIC, DEV_GET_MAX_DOWNSIZE, __s32),
	ORANGEFS_DEV_MAP = _IO(ORANGEFS_DEV_MAGIC, DEV_MAP),
	ORANGEFS_DEV_REMOUNT_ALL = _IO(ORANGEFS_DEV_MAGIC, DEV_REMOUNT_ALL),
	ORANGEFS_DEV_DEBUG = _IOR(ORANGEFS_DEV_MAGIC, DEV_DEBUG, __s32),
	ORANGEFS_DEV_UPSTREAM = _IOW(ORANGEFS_DEV_MAGIC, DEV_UPSTREAM, int),
	ORANGEFS_DEV_CLIENT_MASK = _IOW(ORANGEFS_DEV_MAGIC,
				    DEV_CLIENT_MASK,
				    struct dev_mask2_info_s),
	ORANGEFS_DEV_CLIENT_STRING = _IOW(ORANGEFS_DEV_MAGIC,
				      DEV_CLIENT_STRING,
				      char *),
	ORANGEFS_DEV_MAXNR = DEV_MAX_NR,
};
#define ORANGEFS_KERNEL_PROTO_VERSION 0
#define ORANGEFS_MINIMUM_USERSPACE_VERSION 20903
struct ORANGEFS_dev_map_desc {
	void __user *ptr;
	__s32 total_size;
	__s32 size;
	__s32 count;
};
extern __u64 orangefs_gossip_debug_mask;
#define gossip_debug(mask, fmt, ...)					\
do {									\
	if (orangefs_gossip_debug_mask & (mask))			\
		printk(KERN_DEBUG fmt, ##__VA_ARGS__);			\
} while (0)
#define gossip_err pr_err
