


#ifndef __XATTR_H__
#define __XATTR_H__




enum {
	XATTR_DOSINFO_ATTRIB		= 0x00000001,
	XATTR_DOSINFO_EA_SIZE		= 0x00000002,
	XATTR_DOSINFO_SIZE		= 0x00000004,
	XATTR_DOSINFO_ALLOC_SIZE	= 0x00000008,
	XATTR_DOSINFO_CREATE_TIME	= 0x00000010,
	XATTR_DOSINFO_CHANGE_TIME	= 0x00000020,
	XATTR_DOSINFO_ITIME		= 0x00000040
};


struct xattr_dos_attrib {
	__u16	version;	
	__u32	flags;		
	__u32	attr;		
	__u32	ea_size;	
	__u64	size;
	__u64	alloc_size;
	__u64	create_time;	
	__u64	change_time;	
	__u64	itime;		
};


enum {
	SMB_ACL_TAG_INVALID = 0,
	SMB_ACL_USER,
	SMB_ACL_USER_OBJ,
	SMB_ACL_GROUP,
	SMB_ACL_GROUP_OBJ,
	SMB_ACL_OTHER,
	SMB_ACL_MASK
};

#define SMB_ACL_READ			4
#define SMB_ACL_WRITE			2
#define SMB_ACL_EXECUTE			1

struct xattr_acl_entry {
	int type;
	uid_t uid;
	gid_t gid;
	mode_t perm;
};


struct xattr_smb_acl {
	int count;
	int next;
	struct xattr_acl_entry entries[];
};


#define XATTR_SD_HASH_TYPE_SHA256	0x1
#define XATTR_SD_HASH_SIZE		64


struct xattr_ntacl {
	__u16	version; 
	void	*sd_buf;
	__u32	sd_size;
	__u16	hash_type; 
	__u8	desc[10]; 
	__u16	desc_len;
	__u64	current_time;
	__u8	hash[XATTR_SD_HASH_SIZE]; 
	__u8	posix_acl_hash[XATTR_SD_HASH_SIZE]; 
};


#define DOS_ATTRIBUTE_PREFIX		"DOSATTRIB"
#define DOS_ATTRIBUTE_PREFIX_LEN	(sizeof(DOS_ATTRIBUTE_PREFIX) - 1)
#define XATTR_NAME_DOS_ATTRIBUTE	(XATTR_USER_PREFIX DOS_ATTRIBUTE_PREFIX)
#define XATTR_NAME_DOS_ATTRIBUTE_LEN	\
		(sizeof(XATTR_USER_PREFIX DOS_ATTRIBUTE_PREFIX) - 1)


#define STREAM_PREFIX			"DosStream."
#define STREAM_PREFIX_LEN		(sizeof(STREAM_PREFIX) - 1)
#define XATTR_NAME_STREAM		(XATTR_USER_PREFIX STREAM_PREFIX)
#define XATTR_NAME_STREAM_LEN		(sizeof(XATTR_NAME_STREAM) - 1)


#define SD_PREFIX			"NTACL"
#define SD_PREFIX_LEN	(sizeof(SD_PREFIX) - 1)
#define XATTR_NAME_SD	(XATTR_SECURITY_PREFIX SD_PREFIX)
#define XATTR_NAME_SD_LEN	\
		(sizeof(XATTR_SECURITY_PREFIX SD_PREFIX) - 1)

#endif 
