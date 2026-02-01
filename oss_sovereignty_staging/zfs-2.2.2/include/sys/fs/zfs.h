 

 

#ifndef	_SYS_FS_ZFS_H
#define	_SYS_FS_ZFS_H extern __attribute__((visibility("default")))

#include <sys/time.h>
#include <sys/zio_priority.h>

#ifdef	__cplusplus
extern "C" {
#endif

 

 
typedef enum {
	ZFS_TYPE_INVALID	= 0,
	ZFS_TYPE_FILESYSTEM	= (1 << 0),
	ZFS_TYPE_SNAPSHOT	= (1 << 1),
	ZFS_TYPE_VOLUME		= (1 << 2),
	ZFS_TYPE_POOL		= (1 << 3),
	ZFS_TYPE_BOOKMARK	= (1 << 4),
	ZFS_TYPE_VDEV		= (1 << 5),
} zfs_type_t;

 
typedef enum dmu_objset_type {
	DMU_OST_NONE,
	DMU_OST_META,
	DMU_OST_ZFS,
	DMU_OST_ZVOL,
	DMU_OST_OTHER,			 
	DMU_OST_ANY,			 
	DMU_OST_NUMTYPES
} dmu_objset_type_t;

#define	ZFS_TYPE_DATASET	\
	(ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME | ZFS_TYPE_SNAPSHOT)

 
#define	ZAP_MAXNAMELEN 256
#define	ZAP_MAXVALUELEN (1024 * 8)
#define	ZAP_OLDMAXVALUELEN 1024
#define	ZFS_MAX_DATASET_NAME_LEN 256

 
typedef enum {
	ZPROP_CONT = -2,
	ZPROP_INVAL = -1,
	ZPROP_USERPROP = ZPROP_INVAL,
	ZFS_PROP_TYPE = 0,
	ZFS_PROP_CREATION,
	ZFS_PROP_USED,
	ZFS_PROP_AVAILABLE,
	ZFS_PROP_REFERENCED,
	ZFS_PROP_COMPRESSRATIO,
	ZFS_PROP_MOUNTED,
	ZFS_PROP_ORIGIN,
	ZFS_PROP_QUOTA,
	ZFS_PROP_RESERVATION,
	ZFS_PROP_VOLSIZE,
	ZFS_PROP_VOLBLOCKSIZE,
	ZFS_PROP_RECORDSIZE,
	ZFS_PROP_MOUNTPOINT,
	ZFS_PROP_SHARENFS,
	ZFS_PROP_CHECKSUM,
	ZFS_PROP_COMPRESSION,
	ZFS_PROP_ATIME,
	ZFS_PROP_DEVICES,
	ZFS_PROP_EXEC,
	ZFS_PROP_SETUID,
	ZFS_PROP_READONLY,
	ZFS_PROP_ZONED,
	ZFS_PROP_SNAPDIR,
	ZFS_PROP_ACLMODE,
	ZFS_PROP_ACLINHERIT,
	ZFS_PROP_CREATETXG,
	ZFS_PROP_NAME,			 
	ZFS_PROP_CANMOUNT,
	ZFS_PROP_ISCSIOPTIONS,		 
	ZFS_PROP_XATTR,
	ZFS_PROP_NUMCLONES,		 
	ZFS_PROP_COPIES,
	ZFS_PROP_VERSION,
	ZFS_PROP_UTF8ONLY,
	ZFS_PROP_NORMALIZE,
	ZFS_PROP_CASE,
	ZFS_PROP_VSCAN,
	ZFS_PROP_NBMAND,
	ZFS_PROP_SHARESMB,
	ZFS_PROP_REFQUOTA,
	ZFS_PROP_REFRESERVATION,
	ZFS_PROP_GUID,
	ZFS_PROP_PRIMARYCACHE,
	ZFS_PROP_SECONDARYCACHE,
	ZFS_PROP_USEDSNAP,
	ZFS_PROP_USEDDS,
	ZFS_PROP_USEDCHILD,
	ZFS_PROP_USEDREFRESERV,
	ZFS_PROP_USERACCOUNTING,	 
	ZFS_PROP_STMF_SHAREINFO,	 
	ZFS_PROP_DEFER_DESTROY,
	ZFS_PROP_USERREFS,
	ZFS_PROP_LOGBIAS,
	ZFS_PROP_UNIQUE,		 
	ZFS_PROP_OBJSETID,
	ZFS_PROP_DEDUP,
	ZFS_PROP_MLSLABEL,
	ZFS_PROP_SYNC,
	ZFS_PROP_DNODESIZE,
	ZFS_PROP_REFRATIO,
	ZFS_PROP_WRITTEN,
	ZFS_PROP_CLONES,
	ZFS_PROP_LOGICALUSED,
	ZFS_PROP_LOGICALREFERENCED,
	ZFS_PROP_INCONSISTENT,		 
	ZFS_PROP_VOLMODE,
	ZFS_PROP_FILESYSTEM_LIMIT,
	ZFS_PROP_SNAPSHOT_LIMIT,
	ZFS_PROP_FILESYSTEM_COUNT,
	ZFS_PROP_SNAPSHOT_COUNT,
	ZFS_PROP_SNAPDEV,
	ZFS_PROP_ACLTYPE,
	ZFS_PROP_SELINUX_CONTEXT,
	ZFS_PROP_SELINUX_FSCONTEXT,
	ZFS_PROP_SELINUX_DEFCONTEXT,
	ZFS_PROP_SELINUX_ROOTCONTEXT,
	ZFS_PROP_RELATIME,
	ZFS_PROP_REDUNDANT_METADATA,
	ZFS_PROP_OVERLAY,
	ZFS_PROP_PREV_SNAP,
	ZFS_PROP_RECEIVE_RESUME_TOKEN,
	ZFS_PROP_ENCRYPTION,
	ZFS_PROP_KEYLOCATION,
	ZFS_PROP_KEYFORMAT,
	ZFS_PROP_PBKDF2_SALT,
	ZFS_PROP_PBKDF2_ITERS,
	ZFS_PROP_ENCRYPTION_ROOT,
	ZFS_PROP_KEY_GUID,
	ZFS_PROP_KEYSTATUS,
	ZFS_PROP_REMAPTXG,		 
	ZFS_PROP_SPECIAL_SMALL_BLOCKS,
	ZFS_PROP_IVSET_GUID,		 
	ZFS_PROP_REDACTED,
	ZFS_PROP_REDACT_SNAPS,
	ZFS_PROP_SNAPSHOTS_CHANGED,
	ZFS_NUM_PROPS
} zfs_prop_t;

typedef enum {
	ZFS_PROP_USERUSED,
	ZFS_PROP_USERQUOTA,
	ZFS_PROP_GROUPUSED,
	ZFS_PROP_GROUPQUOTA,
	ZFS_PROP_USEROBJUSED,
	ZFS_PROP_USEROBJQUOTA,
	ZFS_PROP_GROUPOBJUSED,
	ZFS_PROP_GROUPOBJQUOTA,
	ZFS_PROP_PROJECTUSED,
	ZFS_PROP_PROJECTQUOTA,
	ZFS_PROP_PROJECTOBJUSED,
	ZFS_PROP_PROJECTOBJQUOTA,
	ZFS_NUM_USERQUOTA_PROPS
} zfs_userquota_prop_t;

_SYS_FS_ZFS_H const char *const zfs_userquota_prop_prefixes[
    ZFS_NUM_USERQUOTA_PROPS];

 
typedef enum {
	ZPOOL_PROP_INVAL = -1,
	ZPOOL_PROP_NAME,
	ZPOOL_PROP_SIZE,
	ZPOOL_PROP_CAPACITY,
	ZPOOL_PROP_ALTROOT,
	ZPOOL_PROP_HEALTH,
	ZPOOL_PROP_GUID,
	ZPOOL_PROP_VERSION,
	ZPOOL_PROP_BOOTFS,
	ZPOOL_PROP_DELEGATION,
	ZPOOL_PROP_AUTOREPLACE,
	ZPOOL_PROP_CACHEFILE,
	ZPOOL_PROP_FAILUREMODE,
	ZPOOL_PROP_LISTSNAPS,
	ZPOOL_PROP_AUTOEXPAND,
	ZPOOL_PROP_DEDUPDITTO,
	ZPOOL_PROP_DEDUPRATIO,
	ZPOOL_PROP_FREE,
	ZPOOL_PROP_ALLOCATED,
	ZPOOL_PROP_READONLY,
	ZPOOL_PROP_ASHIFT,
	ZPOOL_PROP_COMMENT,
	ZPOOL_PROP_EXPANDSZ,
	ZPOOL_PROP_FREEING,
	ZPOOL_PROP_FRAGMENTATION,
	ZPOOL_PROP_LEAKED,
	ZPOOL_PROP_MAXBLOCKSIZE,
	ZPOOL_PROP_TNAME,
	ZPOOL_PROP_MAXDNODESIZE,
	ZPOOL_PROP_MULTIHOST,
	ZPOOL_PROP_CHECKPOINT,
	ZPOOL_PROP_LOAD_GUID,
	ZPOOL_PROP_AUTOTRIM,
	ZPOOL_PROP_COMPATIBILITY,
	ZPOOL_PROP_BCLONEUSED,
	ZPOOL_PROP_BCLONESAVED,
	ZPOOL_PROP_BCLONERATIO,
	ZPOOL_NUM_PROPS
} zpool_prop_t;

 
#define	ZPROP_MAX_COMMENT	32
#define	ZPROP_BOOLEAN_NA	2

#define	ZPROP_VALUE		"value"
#define	ZPROP_SOURCE		"source"

typedef enum {
	ZPROP_SRC_NONE = 0x1,
	ZPROP_SRC_DEFAULT = 0x2,
	ZPROP_SRC_TEMPORARY = 0x4,
	ZPROP_SRC_LOCAL = 0x8,
	ZPROP_SRC_INHERITED = 0x10,
	ZPROP_SRC_RECEIVED = 0x20
} zprop_source_t;

#define	ZPROP_SRC_ALL	0x3f

#define	ZPROP_SOURCE_VAL_RECVD	"$recvd"
#define	ZPROP_N_MORE_ERRORS	"N_MORE_ERRORS"

 
#define	ZPROP_HAS_RECVD		"$hasrecvd"

typedef enum {
	ZPROP_ERR_NOCLEAR = 0x1,  
	ZPROP_ERR_NORESTORE = 0x2  
} zprop_errflags_t;

typedef int (*zprop_func)(int, void *);

 
#define	ZPOOL_ROOTFS_PROPS	"root-props-nvl"

 
#define	ZFS_WRITTEN_PROP_PREFIX_LEN	8

 
typedef enum {
	VDEV_PROP_INVAL = -1,
	VDEV_PROP_USERPROP = VDEV_PROP_INVAL,
	VDEV_PROP_NAME,
	VDEV_PROP_CAPACITY,
	VDEV_PROP_STATE,
	VDEV_PROP_GUID,
	VDEV_PROP_ASIZE,
	VDEV_PROP_PSIZE,
	VDEV_PROP_ASHIFT,
	VDEV_PROP_SIZE,
	VDEV_PROP_FREE,
	VDEV_PROP_ALLOCATED,
	VDEV_PROP_COMMENT,
	VDEV_PROP_EXPANDSZ,
	VDEV_PROP_FRAGMENTATION,
	VDEV_PROP_BOOTSIZE,
	VDEV_PROP_PARITY,
	VDEV_PROP_PATH,
	VDEV_PROP_DEVID,
	VDEV_PROP_PHYS_PATH,
	VDEV_PROP_ENC_PATH,
	VDEV_PROP_FRU,
	VDEV_PROP_PARENT,
	VDEV_PROP_CHILDREN,
	VDEV_PROP_NUMCHILDREN,
	VDEV_PROP_READ_ERRORS,
	VDEV_PROP_WRITE_ERRORS,
	VDEV_PROP_CHECKSUM_ERRORS,
	VDEV_PROP_INITIALIZE_ERRORS,
	VDEV_PROP_OPS_NULL,
	VDEV_PROP_OPS_READ,
	VDEV_PROP_OPS_WRITE,
	VDEV_PROP_OPS_FREE,
	VDEV_PROP_OPS_CLAIM,
	VDEV_PROP_OPS_TRIM,
	VDEV_PROP_BYTES_NULL,
	VDEV_PROP_BYTES_READ,
	VDEV_PROP_BYTES_WRITE,
	VDEV_PROP_BYTES_FREE,
	VDEV_PROP_BYTES_CLAIM,
	VDEV_PROP_BYTES_TRIM,
	VDEV_PROP_REMOVING,
	VDEV_PROP_ALLOCATING,
	VDEV_PROP_FAILFAST,
	VDEV_PROP_CHECKSUM_N,
	VDEV_PROP_CHECKSUM_T,
	VDEV_PROP_IO_N,
	VDEV_PROP_IO_T,
	VDEV_NUM_PROPS
} vdev_prop_t;

 
_SYS_FS_ZFS_H const char *zfs_prop_default_string(zfs_prop_t);
_SYS_FS_ZFS_H uint64_t zfs_prop_default_numeric(zfs_prop_t);
_SYS_FS_ZFS_H boolean_t zfs_prop_readonly(zfs_prop_t);
_SYS_FS_ZFS_H boolean_t zfs_prop_visible(zfs_prop_t prop);
_SYS_FS_ZFS_H boolean_t zfs_prop_inheritable(zfs_prop_t);
_SYS_FS_ZFS_H boolean_t zfs_prop_setonce(zfs_prop_t);
_SYS_FS_ZFS_H boolean_t zfs_prop_encryption_key_param(zfs_prop_t);
_SYS_FS_ZFS_H boolean_t zfs_prop_valid_keylocation(const char *, boolean_t);
_SYS_FS_ZFS_H const char *zfs_prop_to_name(zfs_prop_t);
_SYS_FS_ZFS_H zfs_prop_t zfs_name_to_prop(const char *);
_SYS_FS_ZFS_H boolean_t zfs_prop_user(const char *);
_SYS_FS_ZFS_H boolean_t zfs_prop_userquota(const char *);
_SYS_FS_ZFS_H boolean_t zfs_prop_written(const char *);
_SYS_FS_ZFS_H int zfs_prop_index_to_string(zfs_prop_t, uint64_t, const char **);
_SYS_FS_ZFS_H int zfs_prop_string_to_index(zfs_prop_t, const char *,
    uint64_t *);
_SYS_FS_ZFS_H uint64_t zfs_prop_random_value(zfs_prop_t, uint64_t seed);
_SYS_FS_ZFS_H boolean_t zfs_prop_valid_for_type(int, zfs_type_t, boolean_t);

 
_SYS_FS_ZFS_H zpool_prop_t zpool_name_to_prop(const char *);
_SYS_FS_ZFS_H const char *zpool_prop_to_name(zpool_prop_t);
_SYS_FS_ZFS_H const char *zpool_prop_default_string(zpool_prop_t);
_SYS_FS_ZFS_H uint64_t zpool_prop_default_numeric(zpool_prop_t);
_SYS_FS_ZFS_H boolean_t zpool_prop_readonly(zpool_prop_t);
_SYS_FS_ZFS_H boolean_t zpool_prop_setonce(zpool_prop_t);
_SYS_FS_ZFS_H boolean_t zpool_prop_feature(const char *);
_SYS_FS_ZFS_H boolean_t zpool_prop_unsupported(const char *);
_SYS_FS_ZFS_H int zpool_prop_index_to_string(zpool_prop_t, uint64_t,
    const char **);
_SYS_FS_ZFS_H int zpool_prop_string_to_index(zpool_prop_t, const char *,
    uint64_t *);
_SYS_FS_ZFS_H uint64_t zpool_prop_random_value(zpool_prop_t, uint64_t seed);

 
_SYS_FS_ZFS_H vdev_prop_t vdev_name_to_prop(const char *);
_SYS_FS_ZFS_H boolean_t vdev_prop_user(const char *name);
_SYS_FS_ZFS_H const char *vdev_prop_to_name(vdev_prop_t);
_SYS_FS_ZFS_H const char *vdev_prop_default_string(vdev_prop_t);
_SYS_FS_ZFS_H uint64_t vdev_prop_default_numeric(vdev_prop_t);
_SYS_FS_ZFS_H boolean_t vdev_prop_readonly(vdev_prop_t prop);
_SYS_FS_ZFS_H int vdev_prop_index_to_string(vdev_prop_t, uint64_t,
    const char **);
_SYS_FS_ZFS_H int vdev_prop_string_to_index(vdev_prop_t, const char *,
    uint64_t *);
_SYS_FS_ZFS_H boolean_t zpool_prop_vdev(const char *name);
_SYS_FS_ZFS_H uint64_t vdev_prop_random_value(vdev_prop_t prop, uint64_t seed);

 
typedef enum {
	ZFS_DELEG_WHO_UNKNOWN = 0,
	ZFS_DELEG_USER = 'u',
	ZFS_DELEG_USER_SETS = 'U',
	ZFS_DELEG_GROUP = 'g',
	ZFS_DELEG_GROUP_SETS = 'G',
	ZFS_DELEG_EVERYONE = 'e',
	ZFS_DELEG_EVERYONE_SETS = 'E',
	ZFS_DELEG_CREATE = 'c',
	ZFS_DELEG_CREATE_SETS = 'C',
	ZFS_DELEG_NAMED_SET = 's',
	ZFS_DELEG_NAMED_SET_SETS = 'S'
} zfs_deleg_who_type_t;

typedef enum {
	ZFS_DELEG_NONE = 0,
	ZFS_DELEG_PERM_LOCAL = 1,
	ZFS_DELEG_PERM_DESCENDENT = 2,
	ZFS_DELEG_PERM_LOCALDESCENDENT = 3,
	ZFS_DELEG_PERM_CREATE = 4
} zfs_deleg_inherit_t;

#define	ZFS_DELEG_PERM_UID	"uid"
#define	ZFS_DELEG_PERM_GID	"gid"
#define	ZFS_DELEG_PERM_GROUPS	"groups"

#define	ZFS_MLSLABEL_DEFAULT	"none"

#define	ZFS_SMB_ACL_SRC		"src"
#define	ZFS_SMB_ACL_TARGET	"target"

typedef enum {
	ZFS_CANMOUNT_OFF = 0,
	ZFS_CANMOUNT_ON = 1,
	ZFS_CANMOUNT_NOAUTO = 2
} zfs_canmount_type_t;

typedef enum {
	ZFS_LOGBIAS_LATENCY = 0,
	ZFS_LOGBIAS_THROUGHPUT = 1
} zfs_logbias_op_t;

typedef enum zfs_share_op {
	ZFS_SHARE_NFS = 0,
	ZFS_UNSHARE_NFS = 1,
	ZFS_SHARE_SMB = 2,
	ZFS_UNSHARE_SMB = 3
} zfs_share_op_t;

typedef enum zfs_smb_acl_op {
	ZFS_SMB_ACL_ADD,
	ZFS_SMB_ACL_REMOVE,
	ZFS_SMB_ACL_RENAME,
	ZFS_SMB_ACL_PURGE
} zfs_smb_acl_op_t;

typedef enum zfs_cache_type {
	ZFS_CACHE_NONE = 0,
	ZFS_CACHE_METADATA = 1,
	ZFS_CACHE_ALL = 2
} zfs_cache_type_t;

typedef enum {
	ZFS_SYNC_STANDARD = 0,
	ZFS_SYNC_ALWAYS = 1,
	ZFS_SYNC_DISABLED = 2
} zfs_sync_type_t;

typedef enum {
	ZFS_XATTR_OFF = 0,
	ZFS_XATTR_DIR = 1,
	ZFS_XATTR_SA = 2
} zfs_xattr_type_t;

typedef enum {
	ZFS_DNSIZE_LEGACY = 0,
	ZFS_DNSIZE_AUTO = 1,
	ZFS_DNSIZE_1K = 1024,
	ZFS_DNSIZE_2K = 2048,
	ZFS_DNSIZE_4K = 4096,
	ZFS_DNSIZE_8K = 8192,
	ZFS_DNSIZE_16K = 16384
} zfs_dnsize_type_t;

typedef enum {
	ZFS_REDUNDANT_METADATA_ALL,
	ZFS_REDUNDANT_METADATA_MOST,
	ZFS_REDUNDANT_METADATA_SOME,
	ZFS_REDUNDANT_METADATA_NONE
} zfs_redundant_metadata_type_t;

typedef enum {
	ZFS_VOLMODE_DEFAULT = 0,
	ZFS_VOLMODE_GEOM = 1,
	ZFS_VOLMODE_DEV = 2,
	ZFS_VOLMODE_NONE = 3
} zfs_volmode_t;

typedef enum zfs_keystatus {
	ZFS_KEYSTATUS_NONE = 0,
	ZFS_KEYSTATUS_UNAVAILABLE,
	ZFS_KEYSTATUS_AVAILABLE,
} zfs_keystatus_t;

typedef enum zfs_keyformat {
	ZFS_KEYFORMAT_NONE = 0,
	ZFS_KEYFORMAT_RAW,
	ZFS_KEYFORMAT_HEX,
	ZFS_KEYFORMAT_PASSPHRASE,
	ZFS_KEYFORMAT_FORMATS
} zfs_keyformat_t;

typedef enum zfs_key_location {
	ZFS_KEYLOCATION_NONE = 0,
	ZFS_KEYLOCATION_PROMPT,
	ZFS_KEYLOCATION_URI,
	ZFS_KEYLOCATION_LOCATIONS
} zfs_keylocation_t;

#define	DEFAULT_PBKDF2_ITERATIONS 350000
#define	MIN_PBKDF2_ITERATIONS 100000

 
#define	SPA_VERSION_1			1ULL
#define	SPA_VERSION_2			2ULL
#define	SPA_VERSION_3			3ULL
#define	SPA_VERSION_4			4ULL
#define	SPA_VERSION_5			5ULL
#define	SPA_VERSION_6			6ULL
#define	SPA_VERSION_7			7ULL
#define	SPA_VERSION_8			8ULL
#define	SPA_VERSION_9			9ULL
#define	SPA_VERSION_10			10ULL
#define	SPA_VERSION_11			11ULL
#define	SPA_VERSION_12			12ULL
#define	SPA_VERSION_13			13ULL
#define	SPA_VERSION_14			14ULL
#define	SPA_VERSION_15			15ULL
#define	SPA_VERSION_16			16ULL
#define	SPA_VERSION_17			17ULL
#define	SPA_VERSION_18			18ULL
#define	SPA_VERSION_19			19ULL
#define	SPA_VERSION_20			20ULL
#define	SPA_VERSION_21			21ULL
#define	SPA_VERSION_22			22ULL
#define	SPA_VERSION_23			23ULL
#define	SPA_VERSION_24			24ULL
#define	SPA_VERSION_25			25ULL
#define	SPA_VERSION_26			26ULL
#define	SPA_VERSION_27			27ULL
#define	SPA_VERSION_28			28ULL
#define	SPA_VERSION_5000		5000ULL

 
#define	SPA_VERSION			SPA_VERSION_5000
#define	SPA_VERSION_STRING		"5000"

 
#define	SPA_VERSION_INITIAL		SPA_VERSION_1
#define	SPA_VERSION_DITTO_BLOCKS	SPA_VERSION_2
#define	SPA_VERSION_SPARES		SPA_VERSION_3
#define	SPA_VERSION_RAIDZ2		SPA_VERSION_3
#define	SPA_VERSION_BPOBJ_ACCOUNT	SPA_VERSION_3
#define	SPA_VERSION_RAIDZ_DEFLATE	SPA_VERSION_3
#define	SPA_VERSION_DNODE_BYTES		SPA_VERSION_3
#define	SPA_VERSION_ZPOOL_HISTORY	SPA_VERSION_4
#define	SPA_VERSION_GZIP_COMPRESSION	SPA_VERSION_5
#define	SPA_VERSION_BOOTFS		SPA_VERSION_6
#define	SPA_VERSION_SLOGS		SPA_VERSION_7
#define	SPA_VERSION_DELEGATED_PERMS	SPA_VERSION_8
#define	SPA_VERSION_FUID		SPA_VERSION_9
#define	SPA_VERSION_REFRESERVATION	SPA_VERSION_9
#define	SPA_VERSION_REFQUOTA		SPA_VERSION_9
#define	SPA_VERSION_UNIQUE_ACCURATE	SPA_VERSION_9
#define	SPA_VERSION_L2CACHE		SPA_VERSION_10
#define	SPA_VERSION_NEXT_CLONES		SPA_VERSION_11
#define	SPA_VERSION_ORIGIN		SPA_VERSION_11
#define	SPA_VERSION_DSL_SCRUB		SPA_VERSION_11
#define	SPA_VERSION_SNAP_PROPS		SPA_VERSION_12
#define	SPA_VERSION_USED_BREAKDOWN	SPA_VERSION_13
#define	SPA_VERSION_PASSTHROUGH_X	SPA_VERSION_14
#define	SPA_VERSION_USERSPACE		SPA_VERSION_15
#define	SPA_VERSION_STMF_PROP		SPA_VERSION_16
#define	SPA_VERSION_RAIDZ3		SPA_VERSION_17
#define	SPA_VERSION_USERREFS		SPA_VERSION_18
#define	SPA_VERSION_HOLES		SPA_VERSION_19
#define	SPA_VERSION_ZLE_COMPRESSION	SPA_VERSION_20
#define	SPA_VERSION_DEDUP		SPA_VERSION_21
#define	SPA_VERSION_RECVD_PROPS		SPA_VERSION_22
#define	SPA_VERSION_SLIM_ZIL		SPA_VERSION_23
#define	SPA_VERSION_SA			SPA_VERSION_24
#define	SPA_VERSION_SCAN		SPA_VERSION_25
#define	SPA_VERSION_DIR_CLONES		SPA_VERSION_26
#define	SPA_VERSION_DEADLISTS		SPA_VERSION_26
#define	SPA_VERSION_FAST_SNAP		SPA_VERSION_27
#define	SPA_VERSION_MULTI_REPLACE	SPA_VERSION_28
#define	SPA_VERSION_BEFORE_FEATURES	SPA_VERSION_28
#define	SPA_VERSION_FEATURES		SPA_VERSION_5000

#define	SPA_VERSION_IS_SUPPORTED(v) \
	(((v) >= SPA_VERSION_INITIAL && (v) <= SPA_VERSION_BEFORE_FEATURES) || \
	((v) >= SPA_VERSION_FEATURES && (v) <= SPA_VERSION))

 
#define	ZPL_VERSION_1			1ULL
#define	ZPL_VERSION_2			2ULL
#define	ZPL_VERSION_3			3ULL
#define	ZPL_VERSION_4			4ULL
#define	ZPL_VERSION_5			5ULL
#define	ZPL_VERSION			ZPL_VERSION_5
#define	ZPL_VERSION_STRING		"5"

#define	ZPL_VERSION_INITIAL		ZPL_VERSION_1
#define	ZPL_VERSION_DIRENT_TYPE		ZPL_VERSION_2
#define	ZPL_VERSION_FUID		ZPL_VERSION_3
#define	ZPL_VERSION_NORMALIZATION	ZPL_VERSION_3
#define	ZPL_VERSION_SYSATTR		ZPL_VERSION_3
#define	ZPL_VERSION_USERSPACE		ZPL_VERSION_4
#define	ZPL_VERSION_SA			ZPL_VERSION_5

 
#define	L2ARC_PERSISTENT_VERSION_1	1ULL
#define	L2ARC_PERSISTENT_VERSION	L2ARC_PERSISTENT_VERSION_1
#define	L2ARC_PERSISTENT_VERSION_STRING	"1"

 
#define	ZPOOL_NO_REWIND		1   
#define	ZPOOL_NEVER_REWIND	2   
#define	ZPOOL_TRY_REWIND	4   
#define	ZPOOL_DO_REWIND		8   
#define	ZPOOL_EXTREME_REWIND	16  
#define	ZPOOL_REWIND_MASK	28  
#define	ZPOOL_REWIND_POLICIES	31  

typedef struct zpool_load_policy {
	uint32_t	zlp_rewind;	 
	uint64_t	zlp_maxmeta;	 
	uint64_t	zlp_maxdata;	 
	uint64_t	zlp_txg;	 
} zpool_load_policy_t;

 
#define	ZPOOL_CONFIG_VERSION		"version"
#define	ZPOOL_CONFIG_POOL_NAME		"name"
#define	ZPOOL_CONFIG_POOL_STATE		"state"
#define	ZPOOL_CONFIG_POOL_TXG		"txg"
#define	ZPOOL_CONFIG_POOL_GUID		"pool_guid"
#define	ZPOOL_CONFIG_CREATE_TXG		"create_txg"
#define	ZPOOL_CONFIG_TOP_GUID		"top_guid"
#define	ZPOOL_CONFIG_VDEV_TREE		"vdev_tree"
#define	ZPOOL_CONFIG_TYPE		"type"
#define	ZPOOL_CONFIG_CHILDREN		"children"
#define	ZPOOL_CONFIG_ID			"id"
#define	ZPOOL_CONFIG_GUID		"guid"
#define	ZPOOL_CONFIG_INDIRECT_OBJECT	"com.delphix:indirect_object"
#define	ZPOOL_CONFIG_INDIRECT_BIRTHS	"com.delphix:indirect_births"
#define	ZPOOL_CONFIG_PREV_INDIRECT_VDEV	"com.delphix:prev_indirect_vdev"
#define	ZPOOL_CONFIG_PATH		"path"
#define	ZPOOL_CONFIG_DEVID		"devid"
#define	ZPOOL_CONFIG_SPARE_ID		"spareid"
#define	ZPOOL_CONFIG_METASLAB_ARRAY	"metaslab_array"
#define	ZPOOL_CONFIG_METASLAB_SHIFT	"metaslab_shift"
#define	ZPOOL_CONFIG_ASHIFT		"ashift"
#define	ZPOOL_CONFIG_ASIZE		"asize"
#define	ZPOOL_CONFIG_DTL		"DTL"
#define	ZPOOL_CONFIG_SCAN_STATS		"scan_stats"	 
#define	ZPOOL_CONFIG_REMOVAL_STATS	"removal_stats"	 
#define	ZPOOL_CONFIG_CHECKPOINT_STATS	"checkpoint_stats"  
#define	ZPOOL_CONFIG_VDEV_STATS		"vdev_stats"	 
#define	ZPOOL_CONFIG_INDIRECT_SIZE	"indirect_size"	 

 
#define	ZPOOL_CONFIG_VDEV_STATS_EX	"vdev_stats_ex"

 
#define	ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE	"vdev_sync_r_active_queue"
#define	ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE	"vdev_sync_w_active_queue"
#define	ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE	"vdev_async_r_active_queue"
#define	ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE	"vdev_async_w_active_queue"
#define	ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE	"vdev_async_scrub_active_queue"
#define	ZPOOL_CONFIG_VDEV_TRIM_ACTIVE_QUEUE	"vdev_async_trim_active_queue"
#define	ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE	"vdev_rebuild_active_queue"

 
#define	ZPOOL_CONFIG_VDEV_SYNC_R_PEND_QUEUE	"vdev_sync_r_pend_queue"
#define	ZPOOL_CONFIG_VDEV_SYNC_W_PEND_QUEUE	"vdev_sync_w_pend_queue"
#define	ZPOOL_CONFIG_VDEV_ASYNC_R_PEND_QUEUE	"vdev_async_r_pend_queue"
#define	ZPOOL_CONFIG_VDEV_ASYNC_W_PEND_QUEUE	"vdev_async_w_pend_queue"
#define	ZPOOL_CONFIG_VDEV_SCRUB_PEND_QUEUE	"vdev_async_scrub_pend_queue"
#define	ZPOOL_CONFIG_VDEV_TRIM_PEND_QUEUE	"vdev_async_trim_pend_queue"
#define	ZPOOL_CONFIG_VDEV_REBUILD_PEND_QUEUE	"vdev_rebuild_pend_queue"

 
#define	ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO	"vdev_tot_r_lat_histo"
#define	ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO	"vdev_tot_w_lat_histo"
#define	ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO	"vdev_disk_r_lat_histo"
#define	ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO	"vdev_disk_w_lat_histo"
#define	ZPOOL_CONFIG_VDEV_SYNC_R_LAT_HISTO	"vdev_sync_r_lat_histo"
#define	ZPOOL_CONFIG_VDEV_SYNC_W_LAT_HISTO	"vdev_sync_w_lat_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_R_LAT_HISTO	"vdev_async_r_lat_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_W_LAT_HISTO	"vdev_async_w_lat_histo"
#define	ZPOOL_CONFIG_VDEV_SCRUB_LAT_HISTO	"vdev_scrub_histo"
#define	ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO	"vdev_trim_histo"
#define	ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO	"vdev_rebuild_histo"

 
#define	ZPOOL_CONFIG_VDEV_SYNC_IND_R_HISTO	"vdev_sync_ind_r_histo"
#define	ZPOOL_CONFIG_VDEV_SYNC_IND_W_HISTO	"vdev_sync_ind_w_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_IND_R_HISTO	"vdev_async_ind_r_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_IND_W_HISTO	"vdev_async_ind_w_histo"
#define	ZPOOL_CONFIG_VDEV_IND_SCRUB_HISTO	"vdev_ind_scrub_histo"
#define	ZPOOL_CONFIG_VDEV_IND_TRIM_HISTO	"vdev_ind_trim_histo"
#define	ZPOOL_CONFIG_VDEV_IND_REBUILD_HISTO	"vdev_ind_rebuild_histo"
#define	ZPOOL_CONFIG_VDEV_SYNC_AGG_R_HISTO	"vdev_sync_agg_r_histo"
#define	ZPOOL_CONFIG_VDEV_SYNC_AGG_W_HISTO	"vdev_sync_agg_w_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_AGG_R_HISTO	"vdev_async_agg_r_histo"
#define	ZPOOL_CONFIG_VDEV_ASYNC_AGG_W_HISTO	"vdev_async_agg_w_histo"
#define	ZPOOL_CONFIG_VDEV_AGG_SCRUB_HISTO	"vdev_agg_scrub_histo"
#define	ZPOOL_CONFIG_VDEV_AGG_TRIM_HISTO	"vdev_agg_trim_histo"
#define	ZPOOL_CONFIG_VDEV_AGG_REBUILD_HISTO	"vdev_agg_rebuild_histo"

 
#define	ZPOOL_CONFIG_VDEV_SLOW_IOS		"vdev_slow_ios"

 
#define	ZPOOL_CONFIG_VDEV_ENC_SYSFS_PATH	"vdev_enc_sysfs_path"

#define	ZPOOL_CONFIG_WHOLE_DISK		"whole_disk"
#define	ZPOOL_CONFIG_ERRCOUNT		"error_count"
#define	ZPOOL_CONFIG_NOT_PRESENT	"not_present"
#define	ZPOOL_CONFIG_SPARES		"spares"
#define	ZPOOL_CONFIG_IS_SPARE		"is_spare"
#define	ZPOOL_CONFIG_NPARITY		"nparity"
#define	ZPOOL_CONFIG_HOSTID		"hostid"
#define	ZPOOL_CONFIG_HOSTNAME		"hostname"
#define	ZPOOL_CONFIG_LOADED_TIME	"initial_load_time"
#define	ZPOOL_CONFIG_UNSPARE		"unspare"
#define	ZPOOL_CONFIG_PHYS_PATH		"phys_path"
#define	ZPOOL_CONFIG_IS_LOG		"is_log"
#define	ZPOOL_CONFIG_L2CACHE		"l2cache"
#define	ZPOOL_CONFIG_HOLE_ARRAY		"hole_array"
#define	ZPOOL_CONFIG_VDEV_CHILDREN	"vdev_children"
#define	ZPOOL_CONFIG_IS_HOLE		"is_hole"
#define	ZPOOL_CONFIG_DDT_HISTOGRAM	"ddt_histogram"
#define	ZPOOL_CONFIG_DDT_OBJ_STATS	"ddt_object_stats"
#define	ZPOOL_CONFIG_DDT_STATS		"ddt_stats"
#define	ZPOOL_CONFIG_SPLIT		"splitcfg"
#define	ZPOOL_CONFIG_ORIG_GUID		"orig_guid"
#define	ZPOOL_CONFIG_SPLIT_GUID		"split_guid"
#define	ZPOOL_CONFIG_SPLIT_LIST		"guid_list"
#define	ZPOOL_CONFIG_NONALLOCATING	"non_allocating"
#define	ZPOOL_CONFIG_REMOVING		"removing"
#define	ZPOOL_CONFIG_RESILVER_TXG	"resilver_txg"
#define	ZPOOL_CONFIG_REBUILD_TXG	"rebuild_txg"
#define	ZPOOL_CONFIG_COMMENT		"comment"
#define	ZPOOL_CONFIG_SUSPENDED		"suspended"	 
#define	ZPOOL_CONFIG_SUSPENDED_REASON	"suspended_reason"	 
#define	ZPOOL_CONFIG_TIMESTAMP		"timestamp"	 
#define	ZPOOL_CONFIG_BOOTFS		"bootfs"	 
#define	ZPOOL_CONFIG_MISSING_DEVICES	"missing_vdevs"	 
#define	ZPOOL_CONFIG_LOAD_INFO		"load_info"	 
#define	ZPOOL_CONFIG_REWIND_INFO	"rewind_info"	 
#define	ZPOOL_CONFIG_UNSUP_FEAT		"unsup_feat"	 
#define	ZPOOL_CONFIG_ENABLED_FEAT	"enabled_feat"	 
#define	ZPOOL_CONFIG_CAN_RDONLY		"can_rdonly"	 
#define	ZPOOL_CONFIG_FEATURES_FOR_READ	"features_for_read"
#define	ZPOOL_CONFIG_FEATURE_STATS	"feature_stats"	 
#define	ZPOOL_CONFIG_ERRATA		"errata"	 
#define	ZPOOL_CONFIG_VDEV_ROOT_ZAP	"com.klarasystems:vdev_zap_root"
#define	ZPOOL_CONFIG_VDEV_TOP_ZAP	"com.delphix:vdev_zap_top"
#define	ZPOOL_CONFIG_VDEV_LEAF_ZAP	"com.delphix:vdev_zap_leaf"
#define	ZPOOL_CONFIG_HAS_PER_VDEV_ZAPS	"com.delphix:has_per_vdev_zaps"
#define	ZPOOL_CONFIG_RESILVER_DEFER	"com.datto:resilver_defer"
#define	ZPOOL_CONFIG_CACHEFILE		"cachefile"	 
#define	ZPOOL_CONFIG_MMP_STATE		"mmp_state"	 
#define	ZPOOL_CONFIG_MMP_TXG		"mmp_txg"	 
#define	ZPOOL_CONFIG_MMP_SEQ		"mmp_seq"	 
#define	ZPOOL_CONFIG_MMP_HOSTNAME	"mmp_hostname"	 
#define	ZPOOL_CONFIG_MMP_HOSTID		"mmp_hostid"	 
#define	ZPOOL_CONFIG_ALLOCATION_BIAS	"alloc_bias"	 
#define	ZPOOL_CONFIG_EXPANSION_TIME	"expansion_time"	 
#define	ZPOOL_CONFIG_REBUILD_STATS	"org.openzfs:rebuild_stats"
#define	ZPOOL_CONFIG_COMPATIBILITY	"compatibility"

 
#define	ZPOOL_CONFIG_OFFLINE		"offline"
#define	ZPOOL_CONFIG_FAULTED		"faulted"
#define	ZPOOL_CONFIG_DEGRADED		"degraded"
#define	ZPOOL_CONFIG_REMOVED		"removed"
#define	ZPOOL_CONFIG_FRU		"fru"
#define	ZPOOL_CONFIG_AUX_STATE		"aux_state"

 
#define	ZPOOL_LOAD_POLICY		"load-policy"
#define	ZPOOL_LOAD_REWIND_POLICY	"load-rewind-policy"
#define	ZPOOL_LOAD_REQUEST_TXG		"load-request-txg"
#define	ZPOOL_LOAD_META_THRESH		"load-meta-thresh"
#define	ZPOOL_LOAD_DATA_THRESH		"load-data-thresh"

 
#define	ZPOOL_CONFIG_LOAD_TIME		"rewind_txg_ts"
#define	ZPOOL_CONFIG_LOAD_META_ERRORS	"verify_meta_errors"
#define	ZPOOL_CONFIG_LOAD_DATA_ERRORS	"verify_data_errors"
#define	ZPOOL_CONFIG_REWIND_TIME	"seconds_of_rewind"

 
#define	ZPOOL_CONFIG_DRAID_NDATA	"draid_ndata"
#define	ZPOOL_CONFIG_DRAID_NSPARES	"draid_nspares"
#define	ZPOOL_CONFIG_DRAID_NGROUPS	"draid_ngroups"

#define	VDEV_TYPE_ROOT			"root"
#define	VDEV_TYPE_MIRROR		"mirror"
#define	VDEV_TYPE_REPLACING		"replacing"
#define	VDEV_TYPE_RAIDZ			"raidz"
#define	VDEV_TYPE_DRAID			"draid"
#define	VDEV_TYPE_DRAID_SPARE		"dspare"
#define	VDEV_TYPE_DISK			"disk"
#define	VDEV_TYPE_FILE			"file"
#define	VDEV_TYPE_MISSING		"missing"
#define	VDEV_TYPE_HOLE			"hole"
#define	VDEV_TYPE_SPARE			"spare"
#define	VDEV_TYPE_LOG			"log"
#define	VDEV_TYPE_L2CACHE		"l2cache"
#define	VDEV_TYPE_INDIRECT		"indirect"

#define	VDEV_RAIDZ_MAXPARITY		3

#define	VDEV_DRAID_MAXPARITY		3
#define	VDEV_DRAID_MIN_CHILDREN		2
#define	VDEV_DRAID_MAX_CHILDREN		UINT8_MAX

 
#define	VDEV_TOP_ZAP_INDIRECT_OBSOLETE_SM \
	"com.delphix:indirect_obsolete_sm"
#define	VDEV_TOP_ZAP_OBSOLETE_COUNTS_ARE_PRECISE \
	"com.delphix:obsolete_counts_are_precise"
#define	VDEV_TOP_ZAP_POOL_CHECKPOINT_SM \
	"com.delphix:pool_checkpoint_sm"
#define	VDEV_TOP_ZAP_MS_UNFLUSHED_PHYS_TXGS \
	"com.delphix:ms_unflushed_phys_txgs"

#define	VDEV_TOP_ZAP_VDEV_REBUILD_PHYS \
	"org.openzfs:vdev_rebuild"

#define	VDEV_TOP_ZAP_ALLOCATION_BIAS \
	"org.zfsonlinux:allocation_bias"

 
#define	VDEV_ALLOC_BIAS_LOG		"log"
#define	VDEV_ALLOC_BIAS_SPECIAL		"special"
#define	VDEV_ALLOC_BIAS_DEDUP		"dedup"

 
#define	VDEV_LEAF_ZAP_INITIALIZE_LAST_OFFSET	\
	"com.delphix:next_offset_to_initialize"
#define	VDEV_LEAF_ZAP_INITIALIZE_STATE	\
	"com.delphix:vdev_initialize_state"
#define	VDEV_LEAF_ZAP_INITIALIZE_ACTION_TIME	\
	"com.delphix:vdev_initialize_action_time"

 
#define	VDEV_LEAF_ZAP_TRIM_LAST_OFFSET	\
	"org.zfsonlinux:next_offset_to_trim"
#define	VDEV_LEAF_ZAP_TRIM_STATE	\
	"org.zfsonlinux:vdev_trim_state"
#define	VDEV_LEAF_ZAP_TRIM_ACTION_TIME	\
	"org.zfsonlinux:vdev_trim_action_time"
#define	VDEV_LEAF_ZAP_TRIM_RATE		\
	"org.zfsonlinux:vdev_trim_rate"
#define	VDEV_LEAF_ZAP_TRIM_PARTIAL	\
	"org.zfsonlinux:vdev_trim_partial"
#define	VDEV_LEAF_ZAP_TRIM_SECURE	\
	"org.zfsonlinux:vdev_trim_secure"

 
#define	SPA_MINDEVSIZE		(64ULL << 20)

 
#define	ZFS_FRAG_INVALID	UINT64_MAX

 
#define	ZPOOL_CACHE_BOOT	"/boot/zfs/zpool.cache"
#define	ZPOOL_CACHE		"/etc/zfs/zpool.cache"
 
#define	ZPOOL_SYSCONF_COMPAT_D	SYSCONFDIR "/zfs/compatibility.d"
#define	ZPOOL_DATA_COMPAT_D	PKGDATADIR "/compatibility.d"
#define	ZPOOL_COMPAT_MAXSIZE	16384

 
#define	ZPOOL_COMPAT_LEGACY	"legacy"
#define	ZPOOL_COMPAT_OFF	"off"

 
typedef enum vdev_state {
	VDEV_STATE_UNKNOWN = 0,	 
	VDEV_STATE_CLOSED,	 
	VDEV_STATE_OFFLINE,	 
	VDEV_STATE_REMOVED,	 
	VDEV_STATE_CANT_OPEN,	 
	VDEV_STATE_FAULTED,	 
	VDEV_STATE_DEGRADED,	 
	VDEV_STATE_HEALTHY	 
} vdev_state_t;

#define	VDEV_STATE_ONLINE	VDEV_STATE_HEALTHY

 
typedef enum vdev_aux {
	VDEV_AUX_NONE,		 
	VDEV_AUX_OPEN_FAILED,	 
	VDEV_AUX_CORRUPT_DATA,	 
	VDEV_AUX_NO_REPLICAS,	 
	VDEV_AUX_BAD_GUID_SUM,	 
	VDEV_AUX_TOO_SMALL,	 
	VDEV_AUX_BAD_LABEL,	 
	VDEV_AUX_VERSION_NEWER,	 
	VDEV_AUX_VERSION_OLDER,	 
	VDEV_AUX_UNSUP_FEAT,	 
	VDEV_AUX_SPARED,	 
	VDEV_AUX_ERR_EXCEEDED,	 
	VDEV_AUX_IO_FAILURE,	 
	VDEV_AUX_BAD_LOG,	 
	VDEV_AUX_EXTERNAL,	 
	VDEV_AUX_SPLIT_POOL,	 
	VDEV_AUX_BAD_ASHIFT,	 
	VDEV_AUX_EXTERNAL_PERSIST,	 
	VDEV_AUX_ACTIVE,	 
	VDEV_AUX_CHILDREN_OFFLINE,  
	VDEV_AUX_ASHIFT_TOO_BIG,  
} vdev_aux_t;

 
typedef enum pool_state {
	POOL_STATE_ACTIVE = 0,		 
	POOL_STATE_EXPORTED,		 
	POOL_STATE_DESTROYED,		 
	POOL_STATE_SPARE,		 
	POOL_STATE_L2CACHE,		 
	POOL_STATE_UNINITIALIZED,	 
	POOL_STATE_UNAVAIL,		 
	POOL_STATE_POTENTIALLY_ACTIVE	 
} pool_state_t;

 
typedef enum mmp_state {
	MMP_STATE_ACTIVE = 0,		 
	MMP_STATE_INACTIVE,		 
	MMP_STATE_NO_HOSTID		 
} mmp_state_t;

 
typedef enum pool_scan_func {
	POOL_SCAN_NONE,
	POOL_SCAN_SCRUB,
	POOL_SCAN_RESILVER,
	POOL_SCAN_ERRORSCRUB,
	POOL_SCAN_FUNCS
} pool_scan_func_t;

 
typedef enum pool_scrub_cmd {
	POOL_SCRUB_NORMAL = 0,
	POOL_SCRUB_PAUSE,
	POOL_SCRUB_FLAGS_END
} pool_scrub_cmd_t;

typedef enum {
	CS_NONE,
	CS_CHECKPOINT_EXISTS,
	CS_CHECKPOINT_DISCARDING,
	CS_NUM_STATES
} checkpoint_state_t;

typedef struct pool_checkpoint_stat {
	uint64_t pcs_state;		 
	uint64_t pcs_start_time;	 
	uint64_t pcs_space;		 
} pool_checkpoint_stat_t;

 
typedef enum zio_type {
	ZIO_TYPE_NULL = 0,
	ZIO_TYPE_READ,
	ZIO_TYPE_WRITE,
	ZIO_TYPE_FREE,
	ZIO_TYPE_CLAIM,
	ZIO_TYPE_IOCTL,
	ZIO_TYPE_TRIM,
	ZIO_TYPES
} zio_type_t;

 
typedef struct pool_scan_stat {
	 
	uint64_t	pss_func;	 
	uint64_t	pss_state;	 
	uint64_t	pss_start_time;	 
	uint64_t	pss_end_time;	 
	uint64_t	pss_to_examine;	 
	uint64_t	pss_examined;	 
	uint64_t	pss_skipped;	 
	uint64_t	pss_processed;	 
	uint64_t	pss_errors;	 

	 
	uint64_t	pss_pass_exam;  
	uint64_t	pss_pass_start;	 
	uint64_t	pss_pass_scrub_pause;  
	 
	uint64_t	pss_pass_scrub_spent_paused;
	uint64_t	pss_pass_issued;  
	uint64_t	pss_issued;	 

	 
	uint64_t	pss_error_scrub_func;	 
	uint64_t	pss_error_scrub_state;	 
	uint64_t	pss_error_scrub_start;	 
	uint64_t	pss_error_scrub_end;	 
	uint64_t	pss_error_scrub_examined;  
	 
	uint64_t	pss_error_scrub_to_be_examined;

	 
	 
	uint64_t	pss_pass_error_scrub_pause;

} pool_scan_stat_t;

typedef struct pool_removal_stat {
	uint64_t prs_state;  
	uint64_t prs_removing_vdev;
	uint64_t prs_start_time;
	uint64_t prs_end_time;
	uint64_t prs_to_copy;  
	uint64_t prs_copied;  
	 
	uint64_t prs_mapping_memory;
} pool_removal_stat_t;

typedef enum dsl_scan_state {
	DSS_NONE,
	DSS_SCANNING,
	DSS_FINISHED,
	DSS_CANCELED,
	DSS_ERRORSCRUBBING,
	DSS_NUM_STATES
} dsl_scan_state_t;

typedef struct vdev_rebuild_stat {
	uint64_t vrs_state;		 
	uint64_t vrs_start_time;	 
	uint64_t vrs_end_time;		 
	uint64_t vrs_scan_time_ms;	 
	uint64_t vrs_bytes_scanned;	 
	uint64_t vrs_bytes_issued;	 
	uint64_t vrs_bytes_rebuilt;	 
	uint64_t vrs_bytes_est;		 
	uint64_t vrs_errors;		 
	uint64_t vrs_pass_time_ms;	 
	uint64_t vrs_pass_bytes_scanned;  
	uint64_t vrs_pass_bytes_issued;	 
	uint64_t vrs_pass_bytes_skipped;  
} vdev_rebuild_stat_t;

 
typedef enum zpool_errata {
	ZPOOL_ERRATA_NONE,
	ZPOOL_ERRATA_ZOL_2094_SCRUB,
	ZPOOL_ERRATA_ZOL_2094_ASYNC_DESTROY,
	ZPOOL_ERRATA_ZOL_6845_ENCRYPTION,
	ZPOOL_ERRATA_ZOL_8308_ENCRYPTION,
} zpool_errata_t;

 
#define	VS_ZIO_TYPES	6

typedef struct vdev_stat {
	hrtime_t	vs_timestamp;		 
	uint64_t	vs_state;		 
	uint64_t	vs_aux;			 
	uint64_t	vs_alloc;		 
	uint64_t	vs_space;		 
	uint64_t	vs_dspace;		 
	uint64_t	vs_rsize;		 
	uint64_t	vs_esize;		 
	uint64_t	vs_ops[VS_ZIO_TYPES];	 
	uint64_t	vs_bytes[VS_ZIO_TYPES];	 
	uint64_t	vs_read_errors;		 
	uint64_t	vs_write_errors;	 
	uint64_t	vs_checksum_errors;	 
	uint64_t	vs_initialize_errors;	 
	uint64_t	vs_self_healed;		 
	uint64_t	vs_scan_removing;	 
	uint64_t	vs_scan_processed;	 
	uint64_t	vs_fragmentation;	 
	uint64_t	vs_initialize_bytes_done;  
	uint64_t	vs_initialize_bytes_est;  
	uint64_t	vs_initialize_state;	 
	uint64_t	vs_initialize_action_time;  
	uint64_t	vs_checkpoint_space;     
	uint64_t	vs_resilver_deferred;	 
	uint64_t	vs_slow_ios;		 
	uint64_t	vs_trim_errors;		 
	uint64_t	vs_trim_notsup;		 
	uint64_t	vs_trim_bytes_done;	 
	uint64_t	vs_trim_bytes_est;	 
	uint64_t	vs_trim_state;		 
	uint64_t	vs_trim_action_time;	 
	uint64_t	vs_rebuild_processed;	 
	uint64_t	vs_configured_ashift;    
	uint64_t	vs_logical_ashift;	 
	uint64_t	vs_physical_ashift;	 
	uint64_t	vs_noalloc;		 
	uint64_t	vs_pspace;		 
} vdev_stat_t;

#define	VDEV_STAT_VALID(field, uint64_t_field_count) \
((uint64_t_field_count * sizeof (uint64_t)) >=	 \
	(offsetof(vdev_stat_t, field) + sizeof (((vdev_stat_t *)NULL)->field)))

 
typedef struct vdev_stat_ex {
	 
	uint64_t vsx_active_queue[ZIO_PRIORITY_NUM_QUEUEABLE];

	 
	uint64_t vsx_pend_queue[ZIO_PRIORITY_NUM_QUEUEABLE];

	 

	 
#define	VDEV_L_HISTO_BUCKETS 37		 
#define	VDEV_RQ_HISTO_BUCKETS 25	 

	 
	uint64_t vsx_queue_histo[ZIO_PRIORITY_NUM_QUEUEABLE]
	    [VDEV_L_HISTO_BUCKETS];

	 
	uint64_t vsx_total_histo[ZIO_TYPES][VDEV_L_HISTO_BUCKETS];

	 
	uint64_t vsx_disk_histo[ZIO_TYPES][VDEV_L_HISTO_BUCKETS];

	 
#define	HISTO(val, buckets) (val != 0 ? MIN(highbit64(val) - 1, \
	    buckets - 1) : 0)
#define	L_HISTO(a) HISTO(a, VDEV_L_HISTO_BUCKETS)
#define	RQ_HISTO(a) HISTO(a, VDEV_RQ_HISTO_BUCKETS)

	 
	uint64_t vsx_ind_histo[ZIO_PRIORITY_NUM_QUEUEABLE]
	    [VDEV_RQ_HISTO_BUCKETS];

	 
	uint64_t vsx_agg_histo[ZIO_PRIORITY_NUM_QUEUEABLE]
	    [VDEV_RQ_HISTO_BUCKETS];

} vdev_stat_ex_t;

 
typedef enum pool_initialize_func {
	POOL_INITIALIZE_START,
	POOL_INITIALIZE_CANCEL,
	POOL_INITIALIZE_SUSPEND,
	POOL_INITIALIZE_UNINIT,
	POOL_INITIALIZE_FUNCS
} pool_initialize_func_t;

 
typedef enum pool_trim_func {
	POOL_TRIM_START,
	POOL_TRIM_CANCEL,
	POOL_TRIM_SUSPEND,
	POOL_TRIM_FUNCS
} pool_trim_func_t;

 
typedef struct ddt_object {
	uint64_t	ddo_count;	 
	uint64_t	ddo_dspace;	 
	uint64_t	ddo_mspace;	 
} ddt_object_t;

typedef struct ddt_stat {
	uint64_t	dds_blocks;	 
	uint64_t	dds_lsize;	 
	uint64_t	dds_psize;	 
	uint64_t	dds_dsize;	 
	uint64_t	dds_ref_blocks;	 
	uint64_t	dds_ref_lsize;	 
	uint64_t	dds_ref_psize;	 
	uint64_t	dds_ref_dsize;	 
} ddt_stat_t;

typedef struct ddt_histogram {
	ddt_stat_t	ddh_stat[64];	 
} ddt_histogram_t;

#define	ZVOL_DRIVER	"zvol"
#define	ZFS_DRIVER	"zfs"
#define	ZFS_DEV		"/dev/zfs"
#define	ZFS_DEVDIR	"/dev"

#define	ZFS_SUPER_MAGIC	0x2fc12fc1

 
#define	ZVOL_DIR		"/dev/zvol/"

#define	ZVOL_MAJOR		230
#define	ZVOL_MINOR_BITS		4
#define	ZVOL_MINOR_MASK		((1U << ZVOL_MINOR_BITS) - 1)
#define	ZVOL_MINORS		(1 << 4)
#define	ZVOL_DEV_NAME		"zd"

#define	ZVOL_PROP_NAME		"name"
#define	ZVOL_DEFAULT_BLOCKSIZE	16384

typedef enum {
	VDEV_INITIALIZE_NONE,
	VDEV_INITIALIZE_ACTIVE,
	VDEV_INITIALIZE_CANCELED,
	VDEV_INITIALIZE_SUSPENDED,
	VDEV_INITIALIZE_COMPLETE
} vdev_initializing_state_t;

typedef enum {
	VDEV_TRIM_NONE,
	VDEV_TRIM_ACTIVE,
	VDEV_TRIM_CANCELED,
	VDEV_TRIM_SUSPENDED,
	VDEV_TRIM_COMPLETE,
} vdev_trim_state_t;

typedef enum {
	VDEV_REBUILD_NONE,
	VDEV_REBUILD_ACTIVE,
	VDEV_REBUILD_CANCELED,
	VDEV_REBUILD_COMPLETE,
} vdev_rebuild_state_t;

 
#define	SNAP_ITER_MIN_TXG	"snap_iter_min_txg"
#define	SNAP_ITER_MAX_TXG	"snap_iter_max_txg"

 
typedef enum zfs_ioc {
	 
#ifdef __FreeBSD__
	ZFS_IOC_FIRST =	0,
#else
	ZFS_IOC_FIRST =	('Z' << 8),
#endif
	ZFS_IOC = ZFS_IOC_FIRST,
	ZFS_IOC_POOL_CREATE = ZFS_IOC_FIRST,	 
	ZFS_IOC_POOL_DESTROY,			 
	ZFS_IOC_POOL_IMPORT,			 
	ZFS_IOC_POOL_EXPORT,			 
	ZFS_IOC_POOL_CONFIGS,			 
	ZFS_IOC_POOL_STATS,			 
	ZFS_IOC_POOL_TRYIMPORT,			 
	ZFS_IOC_POOL_SCAN,			 
	ZFS_IOC_POOL_FREEZE,			 
	ZFS_IOC_POOL_UPGRADE,			 
	ZFS_IOC_POOL_GET_HISTORY,		 
	ZFS_IOC_VDEV_ADD,			 
	ZFS_IOC_VDEV_REMOVE,			 
	ZFS_IOC_VDEV_SET_STATE,			 
	ZFS_IOC_VDEV_ATTACH,			 
	ZFS_IOC_VDEV_DETACH,			 
	ZFS_IOC_VDEV_SETPATH,			 
	ZFS_IOC_VDEV_SETFRU,			 
	ZFS_IOC_OBJSET_STATS,			 
	ZFS_IOC_OBJSET_ZPLPROPS,		 
	ZFS_IOC_DATASET_LIST_NEXT,		 
	ZFS_IOC_SNAPSHOT_LIST_NEXT,		 
	ZFS_IOC_SET_PROP,			 
	ZFS_IOC_CREATE,				 
	ZFS_IOC_DESTROY,			 
	ZFS_IOC_ROLLBACK,			 
	ZFS_IOC_RENAME,				 
	ZFS_IOC_RECV,				 
	ZFS_IOC_SEND,				 
	ZFS_IOC_INJECT_FAULT,			 
	ZFS_IOC_CLEAR_FAULT,			 
	ZFS_IOC_INJECT_LIST_NEXT,		 
	ZFS_IOC_ERROR_LOG,			 
	ZFS_IOC_CLEAR,				 
	ZFS_IOC_PROMOTE,			 
	ZFS_IOC_SNAPSHOT,			 
	ZFS_IOC_DSOBJ_TO_DSNAME,		 
	ZFS_IOC_OBJ_TO_PATH,			 
	ZFS_IOC_POOL_SET_PROPS,			 
	ZFS_IOC_POOL_GET_PROPS,			 
	ZFS_IOC_SET_FSACL,			 
	ZFS_IOC_GET_FSACL,			 
	ZFS_IOC_SHARE,				 
	ZFS_IOC_INHERIT_PROP,			 
	ZFS_IOC_SMB_ACL,			 
	ZFS_IOC_USERSPACE_ONE,			 
	ZFS_IOC_USERSPACE_MANY,			 
	ZFS_IOC_USERSPACE_UPGRADE,		 
	ZFS_IOC_HOLD,				 
	ZFS_IOC_RELEASE,			 
	ZFS_IOC_GET_HOLDS,			 
	ZFS_IOC_OBJSET_RECVD_PROPS,		 
	ZFS_IOC_VDEV_SPLIT,			 
	ZFS_IOC_NEXT_OBJ,			 
	ZFS_IOC_DIFF,				 
	ZFS_IOC_TMP_SNAPSHOT,			 
	ZFS_IOC_OBJ_TO_STATS,			 
	ZFS_IOC_SPACE_WRITTEN,			 
	ZFS_IOC_SPACE_SNAPS,			 
	ZFS_IOC_DESTROY_SNAPS,			 
	ZFS_IOC_POOL_REGUID,			 
	ZFS_IOC_POOL_REOPEN,			 
	ZFS_IOC_SEND_PROGRESS,			 
	ZFS_IOC_LOG_HISTORY,			 
	ZFS_IOC_SEND_NEW,			 
	ZFS_IOC_SEND_SPACE,			 
	ZFS_IOC_CLONE,				 
	ZFS_IOC_BOOKMARK,			 
	ZFS_IOC_GET_BOOKMARKS,			 
	ZFS_IOC_DESTROY_BOOKMARKS,		 
	ZFS_IOC_RECV_NEW,			 
	ZFS_IOC_POOL_SYNC,			 
	ZFS_IOC_CHANNEL_PROGRAM,		 
	ZFS_IOC_LOAD_KEY,			 
	ZFS_IOC_UNLOAD_KEY,			 
	ZFS_IOC_CHANGE_KEY,			 
	ZFS_IOC_REMAP,				 
	ZFS_IOC_POOL_CHECKPOINT,		 
	ZFS_IOC_POOL_DISCARD_CHECKPOINT,	 
	ZFS_IOC_POOL_INITIALIZE,		 
	ZFS_IOC_POOL_TRIM,			 
	ZFS_IOC_REDACT,				 
	ZFS_IOC_GET_BOOKMARK_PROPS,		 
	ZFS_IOC_WAIT,				 
	ZFS_IOC_WAIT_FS,			 
	ZFS_IOC_VDEV_GET_PROPS,			 
	ZFS_IOC_VDEV_SET_PROPS,			 
	ZFS_IOC_POOL_SCRUB,			 

	 
	ZFS_IOC_PLATFORM = ZFS_IOC_FIRST + 0x80,
	ZFS_IOC_EVENTS_NEXT,			 
	ZFS_IOC_EVENTS_CLEAR,			 
	ZFS_IOC_EVENTS_SEEK,			 
	ZFS_IOC_NEXTBOOT,			 
	ZFS_IOC_JAIL,				 
	ZFS_IOC_USERNS_ATTACH = ZFS_IOC_JAIL,	 
	ZFS_IOC_UNJAIL,				 
	ZFS_IOC_USERNS_DETACH = ZFS_IOC_UNJAIL,	 
	ZFS_IOC_SET_BOOTENV,			 
	ZFS_IOC_GET_BOOTENV,			 
	ZFS_IOC_LAST
} zfs_ioc_t;

 
#define	BLKZNAME		_IOR(0x12, 125, char[ZFS_MAX_DATASET_NAME_LEN])

#ifdef __linux__

 
#define	ZFS_IOC_GETDOSFLAGS	_IOR(0x83, 1, uint64_t)
#define	ZFS_IOC_SETDOSFLAGS	_IOW(0x83, 2, uint64_t)

 
#define	ZFS_READONLY		0x0000000100000000ull
#define	ZFS_HIDDEN		0x0000000200000000ull
#define	ZFS_SYSTEM		0x0000000400000000ull
#define	ZFS_ARCHIVE		0x0000000800000000ull
#define	ZFS_IMMUTABLE		0x0000001000000000ull
#define	ZFS_NOUNLINK		0x0000002000000000ull
#define	ZFS_APPENDONLY		0x0000004000000000ull
#define	ZFS_NODUMP		0x0000008000000000ull
#define	ZFS_OPAQUE		0x0000010000000000ull
#define	ZFS_AV_QUARANTINED	0x0000020000000000ull
#define	ZFS_AV_MODIFIED		0x0000040000000000ull
#define	ZFS_REPARSE		0x0000080000000000ull
#define	ZFS_OFFLINE		0x0000100000000000ull
#define	ZFS_SPARSE		0x0000200000000000ull

#define	ZFS_DOS_FL_USER_VISIBLE	(ZFS_IMMUTABLE | ZFS_APPENDONLY | \
	    ZFS_NOUNLINK | ZFS_ARCHIVE | ZFS_NODUMP | ZFS_SYSTEM | \
	    ZFS_HIDDEN | ZFS_READONLY | ZFS_REPARSE | ZFS_OFFLINE | \
	    ZFS_SPARSE)

#endif

 
typedef enum {
	ZFS_ERR_CHECKPOINT_EXISTS = 1024,
	ZFS_ERR_DISCARDING_CHECKPOINT,
	ZFS_ERR_NO_CHECKPOINT,
	ZFS_ERR_DEVRM_IN_PROGRESS,
	ZFS_ERR_VDEV_TOO_BIG,
	ZFS_ERR_IOC_CMD_UNAVAIL,
	ZFS_ERR_IOC_ARG_UNAVAIL,
	ZFS_ERR_IOC_ARG_REQUIRED,
	ZFS_ERR_IOC_ARG_BADTYPE,
	ZFS_ERR_WRONG_PARENT,
	ZFS_ERR_FROM_IVSET_GUID_MISSING,
	ZFS_ERR_FROM_IVSET_GUID_MISMATCH,
	ZFS_ERR_SPILL_BLOCK_FLAG_MISSING,
	ZFS_ERR_UNKNOWN_SEND_STREAM_FEATURE,
	ZFS_ERR_EXPORT_IN_PROGRESS,
	ZFS_ERR_BOOKMARK_SOURCE_NOT_ANCESTOR,
	ZFS_ERR_STREAM_TRUNCATED,
	ZFS_ERR_STREAM_LARGE_BLOCK_MISMATCH,
	ZFS_ERR_RESILVER_IN_PROGRESS,
	ZFS_ERR_REBUILD_IN_PROGRESS,
	ZFS_ERR_BADPROP,
	ZFS_ERR_VDEV_NOTSUP,
	ZFS_ERR_NOT_USER_NAMESPACE,
	ZFS_ERR_RESUME_EXISTS,
	ZFS_ERR_CRYPTO_NOTSUP,
} zfs_errno_t;

 
typedef enum {
	SPA_LOAD_NONE,		 
	SPA_LOAD_OPEN,		 
	SPA_LOAD_IMPORT,	 
	SPA_LOAD_TRYIMPORT,	 
	SPA_LOAD_RECOVER,	 
	SPA_LOAD_ERROR,		 
	SPA_LOAD_CREATE		 
} spa_load_state_t;

typedef enum {
	ZPOOL_WAIT_CKPT_DISCARD,
	ZPOOL_WAIT_FREE,
	ZPOOL_WAIT_INITIALIZE,
	ZPOOL_WAIT_REPLACE,
	ZPOOL_WAIT_REMOVE,
	ZPOOL_WAIT_RESILVER,
	ZPOOL_WAIT_SCRUB,
	ZPOOL_WAIT_TRIM,
	ZPOOL_WAIT_NUM_ACTIVITIES
} zpool_wait_activity_t;

typedef enum {
	ZFS_WAIT_DELETEQ,
	ZFS_WAIT_NUM_ACTIVITIES
} zfs_wait_activity_t;

 
#define	ZPOOL_ERR_LIST		"error list"
#define	ZPOOL_ERR_DATASET	"dataset"
#define	ZPOOL_ERR_OBJECT	"object"

#define	HIS_MAX_RECORD_LEN	(MAXPATHLEN + MAXPATHLEN + 1)

 
#define	ZPOOL_HIST_RECORD	"history record"
#define	ZPOOL_HIST_TIME		"history time"
#define	ZPOOL_HIST_CMD		"history command"
#define	ZPOOL_HIST_WHO		"history who"
#define	ZPOOL_HIST_ZONE		"history zone"
#define	ZPOOL_HIST_HOST		"history hostname"
#define	ZPOOL_HIST_TXG		"history txg"
#define	ZPOOL_HIST_INT_EVENT	"history internal event"
#define	ZPOOL_HIST_INT_STR	"history internal str"
#define	ZPOOL_HIST_INT_NAME	"internal_name"
#define	ZPOOL_HIST_IOCTL	"ioctl"
#define	ZPOOL_HIST_INPUT_NVL	"in_nvl"
#define	ZPOOL_HIST_OUTPUT_NVL	"out_nvl"
#define	ZPOOL_HIST_OUTPUT_SIZE	"out_size"
#define	ZPOOL_HIST_DSNAME	"dsname"
#define	ZPOOL_HIST_DSID		"dsid"
#define	ZPOOL_HIST_ERRNO	"errno"
#define	ZPOOL_HIST_ELAPSED_NS	"elapsed_ns"

 
#define	ZPOOL_HIDDEN_ARGS	"hidden_args"

 
#define	ZPOOL_INITIALIZE_COMMAND	"initialize_command"
#define	ZPOOL_INITIALIZE_VDEVS		"initialize_vdevs"

 
#define	ZPOOL_TRIM_COMMAND		"trim_command"
#define	ZPOOL_TRIM_VDEVS		"trim_vdevs"
#define	ZPOOL_TRIM_RATE			"trim_rate"
#define	ZPOOL_TRIM_SECURE		"trim_secure"

 
#define	ZPOOL_WAIT_ACTIVITY		"wait_activity"
#define	ZPOOL_WAIT_TAG			"wait_tag"
#define	ZPOOL_WAIT_WAITED		"wait_waited"

 
#define	ZPOOL_VDEV_PROPS_GET_VDEV	"vdevprops_get_vdev"
#define	ZPOOL_VDEV_PROPS_GET_PROPS	"vdevprops_get_props"

 
#define	ZPOOL_VDEV_PROPS_SET_VDEV	"vdevprops_set_vdev"
#define	ZPOOL_VDEV_PROPS_SET_PROPS	"vdevprops_set_props"

 
#define	ZFS_WAIT_ACTIVITY		"wait_activity"
#define	ZFS_WAIT_WAITED			"wait_waited"

 
#define	ZFS_ONLINE_CHECKREMOVE	0x1
#define	ZFS_ONLINE_UNSPARE	0x2
#define	ZFS_ONLINE_FORCEFAULT	0x4
#define	ZFS_ONLINE_EXPAND	0x8
#define	ZFS_ONLINE_SPARE	0x10
#define	ZFS_OFFLINE_TEMPORARY	0x1

 
#define	ZFS_IMPORT_NORMAL	0x0
#define	ZFS_IMPORT_VERBATIM	0x1
#define	ZFS_IMPORT_ANY_HOST	0x2
#define	ZFS_IMPORT_MISSING_LOG	0x4
#define	ZFS_IMPORT_ONLY		0x8
#define	ZFS_IMPORT_TEMP_NAME	0x10
#define	ZFS_IMPORT_SKIP_MMP	0x20
#define	ZFS_IMPORT_LOAD_KEYS	0x40
#define	ZFS_IMPORT_CHECKPOINT	0x80

 
#define	ZCP_ARG_PROGRAM		"program"
#define	ZCP_ARG_ARGLIST		"arg"
#define	ZCP_ARG_SYNC		"sync"
#define	ZCP_ARG_INSTRLIMIT	"instrlimit"
#define	ZCP_ARG_MEMLIMIT	"memlimit"

#define	ZCP_ARG_CLIARGV		"argv"

#define	ZCP_RET_ERROR		"error"
#define	ZCP_RET_RETURN		"return"

#define	ZCP_DEFAULT_INSTRLIMIT	(10 * 1000 * 1000)
#define	ZCP_MAX_INSTRLIMIT	(10 * ZCP_DEFAULT_INSTRLIMIT)
#define	ZCP_DEFAULT_MEMLIMIT	(10 * 1024 * 1024)
#define	ZCP_MAX_MEMLIMIT	(10 * ZCP_DEFAULT_MEMLIMIT)

 
#define	ZFS_EV_POOL_NAME	"pool_name"
#define	ZFS_EV_POOL_GUID	"pool_guid"
#define	ZFS_EV_VDEV_PATH	"vdev_path"
#define	ZFS_EV_VDEV_GUID	"vdev_guid"
#define	ZFS_EV_HIST_TIME	"history_time"
#define	ZFS_EV_HIST_CMD		"history_command"
#define	ZFS_EV_HIST_WHO		"history_who"
#define	ZFS_EV_HIST_ZONE	"history_zone"
#define	ZFS_EV_HIST_HOST	"history_hostname"
#define	ZFS_EV_HIST_TXG		"history_txg"
#define	ZFS_EV_HIST_INT_EVENT	"history_internal_event"
#define	ZFS_EV_HIST_INT_STR	"history_internal_str"
#define	ZFS_EV_HIST_INT_NAME	"history_internal_name"
#define	ZFS_EV_HIST_IOCTL	"history_ioctl"
#define	ZFS_EV_HIST_DSNAME	"history_dsname"
#define	ZFS_EV_HIST_DSID	"history_dsid"
#define	ZFS_EV_RESILVER_TYPE	"resilver_type"

 
#define	SPA_MINBLOCKSHIFT	9
#define	SPA_OLD_MAXBLOCKSHIFT	17
#define	SPA_MAXBLOCKSHIFT	24
#define	SPA_MINBLOCKSIZE	(1ULL << SPA_MINBLOCKSHIFT)
#define	SPA_OLD_MAXBLOCKSIZE	(1ULL << SPA_OLD_MAXBLOCKSHIFT)
#define	SPA_MAXBLOCKSIZE	(1ULL << SPA_MAXBLOCKSHIFT)

 
enum zio_encrypt {
	ZIO_CRYPT_INHERIT = 0,
	ZIO_CRYPT_ON,
	ZIO_CRYPT_OFF,
	ZIO_CRYPT_AES_128_CCM,
	ZIO_CRYPT_AES_192_CCM,
	ZIO_CRYPT_AES_256_CCM,
	ZIO_CRYPT_AES_128_GCM,
	ZIO_CRYPT_AES_192_GCM,
	ZIO_CRYPT_AES_256_GCM,
	ZIO_CRYPT_FUNCTIONS
};

#define	ZIO_CRYPT_ON_VALUE	ZIO_CRYPT_AES_256_GCM
#define	ZIO_CRYPT_DEFAULT	ZIO_CRYPT_OFF

 
#define	ZFS_XA_NS_FREEBSD_PREFIX		"freebsd:"
#define	ZFS_XA_NS_FREEBSD_PREFIX_LEN		strlen("freebsd:")
#define	ZFS_XA_NS_LINUX_SECURITY_PREFIX		"security."
#define	ZFS_XA_NS_LINUX_SECURITY_PREFIX_LEN	strlen("security.")
#define	ZFS_XA_NS_LINUX_SYSTEM_PREFIX		"system."
#define	ZFS_XA_NS_LINUX_SYSTEM_PREFIX_LEN	strlen("system.")
#define	ZFS_XA_NS_LINUX_TRUSTED_PREFIX		"trusted."
#define	ZFS_XA_NS_LINUX_TRUSTED_PREFIX_LEN	strlen("trusted.")
#define	ZFS_XA_NS_LINUX_USER_PREFIX		"user."
#define	ZFS_XA_NS_LINUX_USER_PREFIX_LEN		strlen("user.")

#define	ZFS_XA_NS_PREFIX_MATCH(ns, name) \
	(strncmp(name, ZFS_XA_NS_##ns##_PREFIX, \
	ZFS_XA_NS_##ns##_PREFIX_LEN) == 0)

#define	ZFS_XA_NS_PREFIX_FORBIDDEN(name) \
	(ZFS_XA_NS_PREFIX_MATCH(FREEBSD, name) || \
	    ZFS_XA_NS_PREFIX_MATCH(LINUX_SECURITY, name) || \
	    ZFS_XA_NS_PREFIX_MATCH(LINUX_SYSTEM, name) || \
	    ZFS_XA_NS_PREFIX_MATCH(LINUX_TRUSTED, name) || \
	    ZFS_XA_NS_PREFIX_MATCH(LINUX_USER, name))

#ifdef	__cplusplus
}
#endif

#endif	 
