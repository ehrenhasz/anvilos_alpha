 
 

#ifndef _LINUX_NTFS_USNJRNL_H
#define _LINUX_NTFS_USNJRNL_H

#ifdef NTFS_RW

#include "types.h"
#include "endian.h"
#include "layout.h"
#include "volume.h"

 

 
#define UsnJrnlMajorVer		2
#define UsnJrnlMinorVer		0

 
typedef struct {
 
 sle64 maximum_size;	 
 sle64 allocation_delta;	 
 sle64 journal_id;	 
 leUSN lowest_valid_usn;	 
 
} __attribute__ ((__packed__)) USN_HEADER;

 
enum {
	USN_REASON_DATA_OVERWRITE	= cpu_to_le32(0x00000001),
	USN_REASON_DATA_EXTEND		= cpu_to_le32(0x00000002),
	USN_REASON_DATA_TRUNCATION	= cpu_to_le32(0x00000004),
	USN_REASON_NAMED_DATA_OVERWRITE	= cpu_to_le32(0x00000010),
	USN_REASON_NAMED_DATA_EXTEND	= cpu_to_le32(0x00000020),
	USN_REASON_NAMED_DATA_TRUNCATION= cpu_to_le32(0x00000040),
	USN_REASON_FILE_CREATE		= cpu_to_le32(0x00000100),
	USN_REASON_FILE_DELETE		= cpu_to_le32(0x00000200),
	USN_REASON_EA_CHANGE		= cpu_to_le32(0x00000400),
	USN_REASON_SECURITY_CHANGE	= cpu_to_le32(0x00000800),
	USN_REASON_RENAME_OLD_NAME	= cpu_to_le32(0x00001000),
	USN_REASON_RENAME_NEW_NAME	= cpu_to_le32(0x00002000),
	USN_REASON_INDEXABLE_CHANGE	= cpu_to_le32(0x00004000),
	USN_REASON_BASIC_INFO_CHANGE	= cpu_to_le32(0x00008000),
	USN_REASON_HARD_LINK_CHANGE	= cpu_to_le32(0x00010000),
	USN_REASON_COMPRESSION_CHANGE	= cpu_to_le32(0x00020000),
	USN_REASON_ENCRYPTION_CHANGE	= cpu_to_le32(0x00040000),
	USN_REASON_OBJECT_ID_CHANGE	= cpu_to_le32(0x00080000),
	USN_REASON_REPARSE_POINT_CHANGE	= cpu_to_le32(0x00100000),
	USN_REASON_STREAM_CHANGE	= cpu_to_le32(0x00200000),
	USN_REASON_CLOSE		= cpu_to_le32(0x80000000),
};

typedef le32 USN_REASON_FLAGS;

 
enum {
	USN_SOURCE_DATA_MANAGEMENT	  = cpu_to_le32(0x00000001),
	USN_SOURCE_AUXILIARY_DATA	  = cpu_to_le32(0x00000002),
	USN_SOURCE_REPLICATION_MANAGEMENT = cpu_to_le32(0x00000004),
};

typedef le32 USN_SOURCE_INFO_FLAGS;

 
typedef struct {
 
 le32 length;		 
 le16 major_ver;		 
 le16 minor_ver;		 
 leMFT_REF mft_reference; 
 leMFT_REF parent_directory; 
 leUSN usn;		 
 sle64 time;		 
 USN_REASON_FLAGS reason; 
 USN_SOURCE_INFO_FLAGS source_info; 
 le32 security_id;	 
 FILE_ATTR_FLAGS file_attributes;	 
 le16 file_name_size;	 
 le16 file_name_offset;	 
 ntfschar file_name[0];	 
 
} __attribute__ ((__packed__)) USN_RECORD;

extern bool ntfs_stamp_usnjrnl(ntfs_volume *vol);

#endif  

#endif  
