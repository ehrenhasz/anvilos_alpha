 
 

#ifndef SHFL_HOSTINTF_H
#define SHFL_HOSTINTF_H

#include <linux/vbox_vmmdev_types.h>

 
#define SHFL_MAX_RW_COUNT           (16 * SZ_1M)

 

enum {
	SHFL_FN_QUERY_MAPPINGS = 1,	 
	SHFL_FN_QUERY_MAP_NAME = 2,	 
	SHFL_FN_CREATE = 3,		 
	SHFL_FN_CLOSE = 4,		 
	SHFL_FN_READ = 5,		 
	SHFL_FN_WRITE = 6,		 
	SHFL_FN_LOCK = 7,		 
	SHFL_FN_LIST = 8,		 
	SHFL_FN_INFORMATION = 9,	 
	 
	SHFL_FN_REMOVE = 11,		 
	SHFL_FN_MAP_FOLDER_OLD = 12,	 
	SHFL_FN_UNMAP_FOLDER = 13,	 
	SHFL_FN_RENAME = 14,		 
	SHFL_FN_FLUSH = 15,		 
	SHFL_FN_SET_UTF8 = 16,		 
	SHFL_FN_MAP_FOLDER = 17,	 
	SHFL_FN_READLINK = 18,		 
	SHFL_FN_SYMLINK = 19,		 
	SHFL_FN_SET_SYMLINKS = 20,	 
};

 
#define SHFL_ROOT_NIL		UINT_MAX

 
#define SHFL_HANDLE_NIL		ULLONG_MAX

 
#define SHFL_MAX_LEN         (256)
 
#define SHFL_MAX_MAPPINGS    (64)

 
struct shfl_string {
	 
	u16 size;

	 
	u16 length;

	 
	union {
		u8 legacy_padding[2];
		DECLARE_FLEX_ARRAY(u8, utf8);
		DECLARE_FLEX_ARRAY(u16, utf16);
	} string;
};
VMMDEV_ASSERT_SIZE(shfl_string, 6);

 
#define SHFLSTRING_HEADER_SIZE  4

 
static inline u32 shfl_string_buf_size(const struct shfl_string *string)
{
	return string ? SHFLSTRING_HEADER_SIZE + string->size : 0;
}

 
#define SHFL_UNIX_ISUID             0004000U
 
#define SHFL_UNIX_ISGID             0002000U
 
#define SHFL_UNIX_ISTXT             0001000U

 
#define SHFL_UNIX_IRUSR             0000400U
 
#define SHFL_UNIX_IWUSR             0000200U
 
#define SHFL_UNIX_IXUSR             0000100U

 
#define SHFL_UNIX_IRGRP             0000040U
 
#define SHFL_UNIX_IWGRP             0000020U
 
#define SHFL_UNIX_IXGRP             0000010U

 
#define SHFL_UNIX_IROTH             0000004U
 
#define SHFL_UNIX_IWOTH             0000002U
 
#define SHFL_UNIX_IXOTH             0000001U

 
#define SHFL_TYPE_FIFO              0010000U
 
#define SHFL_TYPE_DEV_CHAR          0020000U
 
#define SHFL_TYPE_DIRECTORY         0040000U
 
#define SHFL_TYPE_DEV_BLOCK         0060000U
 
#define SHFL_TYPE_FILE              0100000U
 
#define SHFL_TYPE_SYMLINK           0120000U
 
#define SHFL_TYPE_SOCKET            0140000U
 
#define SHFL_TYPE_WHITEOUT          0160000U
 
#define SHFL_TYPE_MASK              0170000U

 
#define SHFL_IS_DIRECTORY(m)   (((m) & SHFL_TYPE_MASK) == SHFL_TYPE_DIRECTORY)
 
#define SHFL_IS_SYMLINK(m)     (((m) & SHFL_TYPE_MASK) == SHFL_TYPE_SYMLINK)

 
enum shfl_fsobjattr_add {
	 
	SHFLFSOBJATTRADD_NOTHING = 1,
	 
	SHFLFSOBJATTRADD_UNIX,
	 
	SHFLFSOBJATTRADD_EASIZE,
	 
	SHFLFSOBJATTRADD_LAST = SHFLFSOBJATTRADD_EASIZE,

	 
	SHFLFSOBJATTRADD_32BIT_SIZE_HACK = 0x7fffffff
};

 
struct shfl_fsobjattr_unix {
	 
	u32 uid;

	 
	u32 gid;

	 
	u32 hardlinks;

	 
	u32 inode_id_device;

	 
	u64 inode_id;

	 
	u32 flags;

	 
	u32 generation_id;

	 
	u32 device;
} __packed;

 
struct shfl_fsobjattr_easize {
	 
	s64 cb;
} __packed;

 
struct shfl_fsobjattr {
	 
	u32 mode;

	 
	enum shfl_fsobjattr_add additional;

	 
	union {
		struct shfl_fsobjattr_unix unix_attr;
		struct shfl_fsobjattr_easize size;
	} __packed u;
} __packed;
VMMDEV_ASSERT_SIZE(shfl_fsobjattr, 44);

struct shfl_timespec {
	s64 ns_relative_to_unix_epoch;
};

 
struct shfl_fsobjinfo {
	 
	s64 size;

	 
	s64 allocated;

	 
	struct shfl_timespec access_time;

	 
	struct shfl_timespec modification_time;

	 
	struct shfl_timespec change_time;

	 
	struct shfl_timespec birth_time;

	 
	struct shfl_fsobjattr attr;

} __packed;
VMMDEV_ASSERT_SIZE(shfl_fsobjinfo, 92);

 
enum shfl_create_result {
	SHFL_NO_RESULT,
	 
	SHFL_PATH_NOT_FOUND,
	 
	SHFL_FILE_NOT_FOUND,
	 
	SHFL_FILE_EXISTS,
	 
	SHFL_FILE_CREATED,
	 
	SHFL_FILE_REPLACED
};

 
#define SHFL_CF_NONE                  (0x00000000)

 
#define SHFL_CF_LOOKUP                (0x00000001)

 
#define SHFL_CF_OPEN_TARGET_DIRECTORY (0x00000002)

 
#define SHFL_CF_DIRECTORY             (0x00000004)

 
#define SHFL_CF_ACT_MASK_IF_EXISTS      (0x000000f0)
#define SHFL_CF_ACT_MASK_IF_NEW         (0x00000f00)

 
#define SHFL_CF_ACT_OPEN_IF_EXISTS      (0x00000000)
#define SHFL_CF_ACT_FAIL_IF_EXISTS      (0x00000010)
#define SHFL_CF_ACT_REPLACE_IF_EXISTS   (0x00000020)
#define SHFL_CF_ACT_OVERWRITE_IF_EXISTS (0x00000030)

 
#define SHFL_CF_ACT_CREATE_IF_NEW       (0x00000000)
#define SHFL_CF_ACT_FAIL_IF_NEW         (0x00000100)

 
#define SHFL_CF_ACCESS_MASK_RW          (0x00003000)

 
#define SHFL_CF_ACCESS_NONE             (0x00000000)
 
#define SHFL_CF_ACCESS_READ             (0x00001000)
 
#define SHFL_CF_ACCESS_WRITE            (0x00002000)
 
#define SHFL_CF_ACCESS_READWRITE	(0x00003000)

 
#define SHFL_CF_ACCESS_MASK_DENY        (0x0000c000)

 
#define SHFL_CF_ACCESS_DENYNONE         (0x00000000)
 
#define SHFL_CF_ACCESS_DENYREAD         (0x00004000)
 
#define SHFL_CF_ACCESS_DENYWRITE        (0x00008000)
 
#define SHFL_CF_ACCESS_DENYALL          (0x0000c000)

 
#define SHFL_CF_ACCESS_MASK_ATTR        (0x00030000)

 
#define SHFL_CF_ACCESS_ATTR_NONE        (0x00000000)
 
#define SHFL_CF_ACCESS_ATTR_READ        (0x00010000)
 
#define SHFL_CF_ACCESS_ATTR_WRITE       (0x00020000)
 
#define SHFL_CF_ACCESS_ATTR_READWRITE   (0x00030000)

 
#define SHFL_CF_ACCESS_APPEND           (0x00040000)

 
struct shfl_createparms {
	 
	u64 handle;

	 
	enum shfl_create_result result;

	 
	u32 create_flags;

	 
	struct shfl_fsobjinfo info;
} __packed;

 
struct shfl_dirinfo {
	 
	struct shfl_fsobjinfo info;
	 
	u16 short_name_len;
	 
	u16 short_name[14];
	struct shfl_string name;
};

 
struct shfl_fsproperties {
	 
	u32 max_component_len;

	 
	bool remote;

	 
	bool case_sensitive;

	 
	bool read_only;

	 
	bool supports_unicode;

	 
	bool compressed;

	 
	bool file_compression;
};
VMMDEV_ASSERT_SIZE(shfl_fsproperties, 12);

struct shfl_volinfo {
	s64 total_allocation_bytes;
	s64 available_allocation_bytes;
	u32 bytes_per_allocation_unit;
	u32 bytes_per_sector;
	u32 serial;
	struct shfl_fsproperties properties;
};


 
struct shfl_map_folder {
	 
	struct vmmdev_hgcm_function_parameter path;

	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter delimiter;

	 
	struct vmmdev_hgcm_function_parameter case_sensitive;

};

 
#define SHFL_CPARMS_MAP_FOLDER (4)


 
struct shfl_unmap_folder {
	 
	struct vmmdev_hgcm_function_parameter root;

};

 
#define SHFL_CPARMS_UNMAP_FOLDER (1)


 
struct shfl_create {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter path;

	 
	struct vmmdev_hgcm_function_parameter parms;

};

 
#define SHFL_CPARMS_CREATE (3)


 
struct shfl_close {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter handle;

};

 
#define SHFL_CPARMS_CLOSE (2)


 
struct shfl_read {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter handle;

	 
	struct vmmdev_hgcm_function_parameter offset;

	 
	struct vmmdev_hgcm_function_parameter cb;

	 
	struct vmmdev_hgcm_function_parameter buffer;

};

 
#define SHFL_CPARMS_READ (5)


 
struct shfl_write {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter handle;

	 
	struct vmmdev_hgcm_function_parameter offset;

	 
	struct vmmdev_hgcm_function_parameter cb;

	 
	struct vmmdev_hgcm_function_parameter buffer;

};

 
#define SHFL_CPARMS_WRITE (5)


 

#define SHFL_LIST_NONE			0
#define SHFL_LIST_RETURN_ONE		1

 
struct shfl_list {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter handle;

	 
	struct vmmdev_hgcm_function_parameter flags;

	 
	struct vmmdev_hgcm_function_parameter cb;

	 
	struct vmmdev_hgcm_function_parameter path;

	 
	struct vmmdev_hgcm_function_parameter buffer;

	 
	struct vmmdev_hgcm_function_parameter resume_point;

	 
	struct vmmdev_hgcm_function_parameter file_count;
};

 
#define SHFL_CPARMS_LIST (8)


 
struct shfl_readLink {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter path;

	 
	struct vmmdev_hgcm_function_parameter buffer;

};

 
#define SHFL_CPARMS_READLINK (3)


 

 
#define SHFL_INFO_MODE_MASK    (0x1)
 
#define SHFL_INFO_GET          (0x0)
 
#define SHFL_INFO_SET          (0x1)

 
#define SHFL_INFO_NAME         (0x2)
 
#define SHFL_INFO_SIZE         (0x4)
 
#define SHFL_INFO_FILE         (0x8)
 
#define SHFL_INFO_VOLUME       (0x10)

 
struct shfl_information {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter handle;

	 
	struct vmmdev_hgcm_function_parameter flags;

	 
	struct vmmdev_hgcm_function_parameter cb;

	 
	struct vmmdev_hgcm_function_parameter info;

};

 
#define SHFL_CPARMS_INFORMATION (5)


 

#define SHFL_REMOVE_FILE        (0x1)
#define SHFL_REMOVE_DIR         (0x2)
#define SHFL_REMOVE_SYMLINK     (0x4)

 
struct shfl_remove {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter path;

	 
	struct vmmdev_hgcm_function_parameter flags;

};

#define SHFL_CPARMS_REMOVE  (3)


 

#define SHFL_RENAME_FILE                (0x1)
#define SHFL_RENAME_DIR                 (0x2)
#define SHFL_RENAME_REPLACE_IF_EXISTS   (0x4)

 
struct shfl_rename {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter src;

	 
	struct vmmdev_hgcm_function_parameter dest;

	 
	struct vmmdev_hgcm_function_parameter flags;

};

#define SHFL_CPARMS_RENAME  (4)


 
struct shfl_symlink {
	 
	struct vmmdev_hgcm_function_parameter root;

	 
	struct vmmdev_hgcm_function_parameter new_path;

	 
	struct vmmdev_hgcm_function_parameter old_path;

	 
	struct vmmdev_hgcm_function_parameter info;

};

#define SHFL_CPARMS_SYMLINK  (4)

#endif
