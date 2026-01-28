


#ifndef __NVDIMM_PFN_H
#define __NVDIMM_PFN_H

#include <linux/types.h>
#include <linux/mmzone.h>

#define PFN_SIG_LEN 16
#define PFN_SIG "NVDIMM_PFN_INFO\0"
#define DAX_SIG "NVDIMM_DAX_INFO\0"

struct nd_pfn_sb {
	u8 signature[PFN_SIG_LEN];
	u8 uuid[16];
	u8 parent_uuid[16];
	__le32 flags;
	__le16 version_major;
	__le16 version_minor;
	__le64 dataoff; 
	__le64 npfns;
	__le32 mode;
	
	
	__le32 start_pad;
	__le32 end_trunc;
	
	__le32 align;
	
	
	__le32 page_size;
	__le16 page_struct_size;
	u8 padding[3994];
	__le64 checksum;
};

#endif 
