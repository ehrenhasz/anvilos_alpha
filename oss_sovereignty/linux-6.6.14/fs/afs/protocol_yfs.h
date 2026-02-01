 
 

#define YFS_FS_SERVICE	2500
#define YFS_CM_SERVICE	2501

#define YFSCBMAX	1024

enum YFS_CM_Operations {
	YFSCBProbe		= 206,	 
	YFSCBGetLock		= 207,	 
	YFSCBXStatsVersion	= 209,	 
	YFSCBGetXStats		= 210,	 
	YFSCBInitCallBackState3	= 213,	 
	YFSCBProbeUuid		= 214,	 
	YFSCBGetServerPrefs	= 215,
	YFSCBGetCellServDV	= 216,
	YFSCBGetLocalCell	= 217,
	YFSCBGetCacheConfig	= 218,
	YFSCBGetCellByNum	= 65537,
	YFSCBTellMeAboutYourself = 65538,  
	YFSCBCallBack		= 64204,
};

enum YFS_FS_Operations {
	YFSFETCHACL		= 64131,  
	YFSFETCHSTATUS		= 64132,  
	YFSSTOREACL		= 64134,  
	YFSSTORESTATUS		= 64135,  
	YFSREMOVEFILE		= 64136,  
	YFSCREATEFILE		= 64137,  
	YFSRENAME		= 64138,  
	YFSSYMLINK		= 64139,  
	YFSLINK			= 64140,  
	YFSMAKEDIR		= 64141,  
	YFSREMOVEDIR		= 64142,  
	YFSGETVOLUMESTATUS	= 64149,  
	YFSSETVOLUMESTATUS	= 64150,  
	YFSSETLOCK		= 64156,  
	YFSEXTENDLOCK		= 64157,  
	YFSRELEASELOCK		= 64158,  
	YFSLOOKUP		= 64161,  
	YFSFLUSHCPS		= 64165,
	YFSFETCHOPAQUEACL	= 64168,  
	YFSWHOAMI		= 64170,
	YFSREMOVEACL		= 64171,
	YFSREMOVEFILE2		= 64173,
	YFSSTOREOPAQUEACL2	= 64174,
	YFSINLINEBULKSTATUS	= 64536,  
	YFSFETCHDATA64		= 64537,  
	YFSSTOREDATA64		= 64538,  
	YFSUPDATESYMLINK	= 64540,
};

struct yfs_xdr_u64 {
	__be32			msw;
	__be32			lsw;
} __packed;

static inline u64 xdr_to_u64(const struct yfs_xdr_u64 x)
{
	return ((u64)ntohl(x.msw) << 32) | ntohl(x.lsw);
}

static inline struct yfs_xdr_u64 u64_to_xdr(const u64 x)
{
	return (struct yfs_xdr_u64){ .msw = htonl(x >> 32), .lsw = htonl(x) };
}

struct yfs_xdr_vnode {
	struct yfs_xdr_u64	lo;
	__be32			hi;
	__be32			unique;
} __packed;

struct yfs_xdr_YFSFid {
	struct yfs_xdr_u64	volume;
	struct yfs_xdr_vnode	vnode;
} __packed;


struct yfs_xdr_YFSFetchStatus {
	__be32			type;
	__be32			nlink;
	struct yfs_xdr_u64	size;
	struct yfs_xdr_u64	data_version;
	struct yfs_xdr_u64	author;
	struct yfs_xdr_u64	owner;
	struct yfs_xdr_u64	group;
	__be32			mode;
	__be32			caller_access;
	__be32			anon_access;
	struct yfs_xdr_vnode	parent;
	__be32			data_access_protocol;
	struct yfs_xdr_u64	mtime_client;
	struct yfs_xdr_u64	mtime_server;
	__be32			lock_count;
	__be32			abort_code;
} __packed;

struct yfs_xdr_YFSCallBack {
	__be32			version;
	struct yfs_xdr_u64	expiration_time;
	__be32			type;
} __packed;

struct yfs_xdr_YFSStoreStatus {
	__be32			mask;
	__be32			mode;
	struct yfs_xdr_u64	mtime_client;
	struct yfs_xdr_u64	owner;
	struct yfs_xdr_u64	group;
} __packed;

struct yfs_xdr_RPCFlags {
	__be32			rpc_flags;
} __packed;

struct yfs_xdr_YFSVolSync {
	struct yfs_xdr_u64	vol_creation_date;
	struct yfs_xdr_u64	vol_update_date;
	struct yfs_xdr_u64	max_quota;
	struct yfs_xdr_u64	blocks_in_use;
	struct yfs_xdr_u64	blocks_avail;
} __packed;

enum yfs_volume_type {
	yfs_volume_type_ro = 0,
	yfs_volume_type_rw = 1,
};

#define yfs_FVSOnline		0x1
#define yfs_FVSInservice	0x2
#define yfs_FVSBlessed		0x4
#define yfs_FVSNeedsSalvage	0x8

struct yfs_xdr_YFSFetchVolumeStatus {
	struct yfs_xdr_u64	vid;
	struct yfs_xdr_u64	parent_id;
	__be32			flags;
	__be32			type;
	struct yfs_xdr_u64	max_quota;
	struct yfs_xdr_u64	blocks_in_use;
	struct yfs_xdr_u64	part_blocks_avail;
	struct yfs_xdr_u64	part_max_blocks;
	struct yfs_xdr_u64	vol_copy_date;
	struct yfs_xdr_u64	vol_backup_date;
} __packed;

struct yfs_xdr_YFSStoreVolumeStatus {
	__be32			mask;
	struct yfs_xdr_u64	min_quota;
	struct yfs_xdr_u64	max_quota;
	struct yfs_xdr_u64	file_quota;
} __packed;

enum yfs_lock_type {
	yfs_LockNone		= -1,
	yfs_LockRead		= 0,
	yfs_LockWrite		= 1,
	yfs_LockExtend		= 2,
	yfs_LockRelease		= 3,
	yfs_LockMandatoryRead	= 0x100,
	yfs_LockMandatoryWrite	= 0x101,
	yfs_LockMandatoryExtend	= 0x102,
};

 
#define YFS_VICED_CAPABILITY_ERRORTRANS		0x0001  
#define YFS_VICED_CAPABILITY_64BITFILES		0x0002  
#define YFS_VICED_CAPABILITY_WRITELOCKACL	0x0004  
#define YFS_VICED_CAPABILITY_SANEACLS		0x0008  
