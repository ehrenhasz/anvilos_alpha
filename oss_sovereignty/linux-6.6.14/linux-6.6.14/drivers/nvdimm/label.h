#ifndef __LABEL_H__
#define __LABEL_H__
#include <linux/ndctl.h>
#include <linux/sizes.h>
#include <linux/uuid.h>
#include <linux/io.h>
enum {
	NSINDEX_SIG_LEN = 16,
	NSINDEX_ALIGN = 256,
	NSINDEX_SEQ_MASK = 0x3,
	NSLABEL_UUID_LEN = 16,
	NSLABEL_NAME_LEN = 64,
	NSLABEL_FLAG_ROLABEL = 0x1,   
	NSLABEL_FLAG_LOCAL = 0x2,     
	NSLABEL_FLAG_BTT = 0x4,       
	NSLABEL_FLAG_UPDATING = 0x8,  
	BTT_ALIGN = 4096,             
	BTTINFO_SIG_LEN = 16,
	BTTINFO_UUID_LEN = 16,
	BTTINFO_FLAG_ERROR = 0x1,     
	BTTINFO_MAJOR_VERSION = 1,
	ND_LABEL_MIN_SIZE = 256 * 4,  
	ND_LABEL_ID_SIZE = 50,
	ND_NSINDEX_INIT = 0x1,
};
struct nd_namespace_index {
	u8 sig[NSINDEX_SIG_LEN];
	u8 flags[3];
	u8 labelsize;
	__le32 seq;
	__le64 myoff;
	__le64 mysize;
	__le64 otheroff;
	__le64 labeloff;
	__le32 nslot;
	__le16 major;
	__le16 minor;
	__le64 checksum;
	u8 free[];
};
struct cxl_region_label {
	u8 type[NSLABEL_UUID_LEN];
	u8 uuid[NSLABEL_UUID_LEN];
	__le32 flags;
	__le16 nlabel;
	__le16 position;
	__le64 dpa;
	__le64 rawsize;
	__le64 hpa;
	__le32 slot;
	__le32 ig;
	__le32 align;
	u8 reserved[0xac];
	__le64 checksum;
};
struct nvdimm_efi_label {
	u8 uuid[NSLABEL_UUID_LEN];
	u8 name[NSLABEL_NAME_LEN];
	__le32 flags;
	__le16 nlabel;
	__le16 position;
	__le64 isetcookie;
	__le64 lbasize;
	__le64 dpa;
	__le64 rawsize;
	__le32 slot;
	u8 align;
	u8 reserved[3];
	guid_t type_guid;
	guid_t abstraction_guid;
	u8 reserved2[88];
	__le64 checksum;
};
struct nvdimm_cxl_label {
	u8 type[NSLABEL_UUID_LEN];
	u8 uuid[NSLABEL_UUID_LEN];
	u8 name[NSLABEL_NAME_LEN];
	__le32 flags;
	__le16 nrange;
	__le16 position;
	__le64 dpa;
	__le64 rawsize;
	__le32 slot;
	__le32 align;
	u8 region_uuid[16];
	u8 abstraction_uuid[16];
	__le16 lbasize;
	u8 reserved[0x56];
	__le64 checksum;
};
struct nd_namespace_label {
	union {
		struct nvdimm_cxl_label cxl;
		struct nvdimm_efi_label efi;
	};
};
#define NVDIMM_BTT_GUID "8aed63a2-29a2-4c66-8b12-f05d15d3922a"
#define NVDIMM_BTT2_GUID "18633bfc-1735-4217-8ac9-17239282d3f8"
#define NVDIMM_PFN_GUID "266400ba-fb9f-4677-bcb0-968f11d0d225"
#define NVDIMM_DAX_GUID "97a86d9c-3cdd-4eda-986f-5068b4f80088"
#define CXL_REGION_UUID "529d7c61-da07-47c4-a93f-ecdf2c06f444"
#define CXL_NAMESPACE_UUID "68bb2c0a-5a77-4937-9f85-3caf41a0f93c"
struct nd_label_id {
	char id[ND_LABEL_ID_SIZE];
};
static inline int nd_label_next_nsindex(int index)
{
	if (index < 0)
		return -1;
	return (index + 1) % 2;
}
struct nvdimm_drvdata;
int nd_label_data_init(struct nvdimm_drvdata *ndd);
size_t sizeof_namespace_index(struct nvdimm_drvdata *ndd);
int nd_label_active_count(struct nvdimm_drvdata *ndd);
struct nd_namespace_label *nd_label_active(struct nvdimm_drvdata *ndd, int n);
u32 nd_label_alloc_slot(struct nvdimm_drvdata *ndd);
bool nd_label_free_slot(struct nvdimm_drvdata *ndd, u32 slot);
u32 nd_label_nfree(struct nvdimm_drvdata *ndd);
struct nd_region;
struct nd_namespace_pmem;
int nd_pmem_namespace_label_update(struct nd_region *nd_region,
		struct nd_namespace_pmem *nspm, resource_size_t size);
#endif  
