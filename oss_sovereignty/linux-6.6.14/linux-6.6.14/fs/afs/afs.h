#ifndef AFS_H
#define AFS_H
#include <linux/in.h>
#define AFS_MAXCELLNAME		256  	 
#define AFS_MAXVOLNAME		64  	 
#define AFS_MAXNSERVERS		8   	 
#define AFS_NMAXNSERVERS	13  	 
#define AFS_MAXTYPES		3	 
#define AFSNAMEMAX		256 	 
#define AFSPATHMAX		1024	 
#define AFSOPAQUEMAX		1024	 
#define AFS_VL_MAX_LIFESPAN	120
#define AFS_PROBE_MAX_LIFESPAN	30
typedef u64			afs_volid_t;
typedef u64			afs_vnodeid_t;
typedef u64			afs_dataversion_t;
typedef enum {
	AFSVL_RWVOL,			 
	AFSVL_ROVOL,			 
	AFSVL_BACKVOL,			 
} __attribute__((packed)) afs_voltype_t;
typedef enum {
	AFS_FTYPE_INVALID	= 0,
	AFS_FTYPE_FILE		= 1,
	AFS_FTYPE_DIR		= 2,
	AFS_FTYPE_SYMLINK	= 3,
} afs_file_type_t;
typedef enum {
	AFS_LOCK_READ		= 0,	 
	AFS_LOCK_WRITE		= 1,	 
} afs_lock_type_t;
#define AFS_LOCKWAIT		(5 * 60)  
struct afs_fid {
	afs_volid_t	vid;		 
	afs_vnodeid_t	vnode;		 
	u32		vnode_hi;	 
	u32		unique;		 
};
typedef enum {
	AFSCM_CB_UNTYPED	= 0,	 
	AFSCM_CB_EXCLUSIVE	= 1,	 
	AFSCM_CB_SHARED		= 2,	 
	AFSCM_CB_DROPPED	= 3,	 
} afs_callback_type_t;
struct afs_callback {
	time64_t		expires_at;	 
};
struct afs_callback_break {
	struct afs_fid		fid;		 
};
#define AFSCBMAX 50	 
struct afs_uuid {
	__be32		time_low;			 
	__be16		time_mid;			 
	__be16		time_hi_and_version;		 
	__s8		clock_seq_hi_and_reserved;	 
	__s8		clock_seq_low;			 
	__s8		node[6];			 
};
struct afs_volume_info {
	afs_volid_t		vid;		 
	afs_voltype_t		type;		 
	afs_volid_t		type_vids[5];	 
	size_t			nservers;	 
	struct {
		struct in_addr	addr;		 
	} servers[8];
};
typedef u32 afs_access_t;
#define AFS_ACE_READ		0x00000001U	 
#define AFS_ACE_WRITE		0x00000002U	 
#define AFS_ACE_INSERT		0x00000004U	 
#define AFS_ACE_LOOKUP		0x00000008U	 
#define AFS_ACE_DELETE		0x00000010U	 
#define AFS_ACE_LOCK		0x00000020U	 
#define AFS_ACE_ADMINISTER	0x00000040U	 
#define AFS_ACE_USER_A		0x01000000U	 
#define AFS_ACE_USER_B		0x02000000U	 
#define AFS_ACE_USER_C		0x04000000U	 
#define AFS_ACE_USER_D		0x08000000U	 
#define AFS_ACE_USER_E		0x10000000U	 
#define AFS_ACE_USER_F		0x20000000U	 
#define AFS_ACE_USER_G		0x40000000U	 
#define AFS_ACE_USER_H		0x80000000U	 
struct afs_file_status {
	u64			size;		 
	afs_dataversion_t	data_version;	 
	struct timespec64	mtime_client;	 
	struct timespec64	mtime_server;	 
	s64			author;		 
	s64			owner;		 
	s64			group;		 
	afs_access_t		caller_access;	 
	afs_access_t		anon_access;	 
	umode_t			mode;		 
	afs_file_type_t		type;		 
	u32			nlink;		 
	s32			lock_count;	 
	u32			abort_code;	 
};
struct afs_status_cb {
	struct afs_file_status	status;
	struct afs_callback	callback;
	bool			have_status;	 
	bool			have_cb;	 
	bool			have_error;	 
};
#define AFS_SET_MTIME		0x01		 
#define AFS_SET_OWNER		0x02		 
#define AFS_SET_GROUP		0x04		 
#define AFS_SET_MODE		0x08		 
#define AFS_SET_SEG_SIZE	0x10		 
struct afs_volsync {
	time64_t		creation;	 
};
struct afs_volume_status {
	afs_volid_t		vid;		 
	afs_volid_t		parent_id;	 
	u8			online;		 
	u8			in_service;	 
	u8			blessed;	 
	u8			needs_salvage;	 
	u32			type;		 
	u64			min_quota;	 
	u64			max_quota;	 
	u64			blocks_in_use;	 
	u64			part_blocks_avail;  
	u64			part_max_blocks;  
	s64			vol_copy_date;
	s64			vol_backup_date;
};
#define AFS_BLOCK_SIZE	1024
struct afs_uuid__xdr {
	__be32		time_low;
	__be32		time_mid;
	__be32		time_hi_and_version;
	__be32		clock_seq_hi_and_reserved;
	__be32		clock_seq_low;
	__be32		node[6];
};
#endif  
