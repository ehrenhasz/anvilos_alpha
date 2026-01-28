

#ifndef __OTX_CPTPF_UCODE_H
#define __OTX_CPTPF_UCODE_H

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/module.h>
#include "otx_cpt_hw_types.h"


#define OTX_CPT_UCODE_NAME_LENGTH	64

#define OTX_CPT_MAX_ETYPES_PER_GRP	1


#define OTX_CPT_UCODE_TAR_FILE_NAME	"cpt8x-mc.tar"


#define OTX_CPT_UCODE_ALIGNMENT		128


#define OTX_CPT_UCODE_SIGN_LEN		256


#define OTX_CPT_UCODE_VER_STR_SZ	44


#define OTX_CPT_MAX_ENGINES		64

#define OTX_CPT_ENGS_BITMASK_LEN	(OTX_CPT_MAX_ENGINES/(BITS_PER_BYTE * \
					 sizeof(unsigned long)))


enum otx_cpt_ucode_type {
	OTX_CPT_AE_UC_TYPE =	1,  
	OTX_CPT_SE_UC_TYPE1 =	20, 
	OTX_CPT_SE_UC_TYPE2 =	21, 
	OTX_CPT_SE_UC_TYPE3 =	22, 
};

struct otx_cpt_bitmap {
	unsigned long bits[OTX_CPT_ENGS_BITMASK_LEN];
	int size;
};

struct otx_cpt_engines {
	int type;
	int count;
};


struct otx_cpt_ucode_ver_num {
	u8 nn;
	u8 xx;
	u8 yy;
	u8 zz;
};

struct otx_cpt_ucode_hdr {
	struct otx_cpt_ucode_ver_num ver_num;
	u8 ver_str[OTX_CPT_UCODE_VER_STR_SZ];
	__be32 code_length;
	u32 padding[3];
};

struct otx_cpt_ucode {
	u8 ver_str[OTX_CPT_UCODE_VER_STR_SZ];
	struct otx_cpt_ucode_ver_num ver_num;
	char filename[OTX_CPT_UCODE_NAME_LENGTH];	 
	dma_addr_t dma;		
	dma_addr_t align_dma;	
	void *va;		
	void *align_va;		
	u32 size;		
	int type;		
};

struct tar_ucode_info_t {
	struct list_head list;
	struct otx_cpt_ucode ucode;
	const u8 *ucode_ptr;	
};


struct otx_cpt_engs_available {
	int max_se_cnt;
	int max_ae_cnt;
	int se_cnt;
	int ae_cnt;
};


struct otx_cpt_engs_rsvd {
	int type;	
	int count;	
	int offset;     
	unsigned long *bmap;		
	struct otx_cpt_ucode *ucode;	
};

struct otx_cpt_mirror_info {
	int is_ena;	
	int idx;	
	int ref_count;	
};

struct otx_cpt_eng_grp_info {
	struct otx_cpt_eng_grps *g; 
	struct device_attribute info_attr; 
	
	struct otx_cpt_engs_rsvd engs[OTX_CPT_MAX_ETYPES_PER_GRP];
	
	struct otx_cpt_ucode ucode[OTX_CPT_MAX_ETYPES_PER_GRP];
	
	char sysfs_info_name[OTX_CPT_UCODE_NAME_LENGTH];
	
	struct otx_cpt_mirror_info mirror;
	int idx;	 
	bool is_enabled; 
};

struct otx_cpt_eng_grps {
	struct otx_cpt_eng_grp_info grp[OTX_CPT_MAX_ENGINE_GROUPS];
	struct device_attribute ucode_load_attr;
	struct otx_cpt_engs_available avail;
	struct mutex lock;
	void *obj;
	int engs_num;			
	int eng_types_supported;	
	u8 eng_ref_cnt[OTX_CPT_MAX_ENGINES];
	bool is_ucode_load_created;	
	bool is_first_try; 
	bool is_rdonly;	
};

int otx_cpt_init_eng_grps(struct pci_dev *pdev,
			  struct otx_cpt_eng_grps *eng_grps, int pf_type);
void otx_cpt_cleanup_eng_grps(struct pci_dev *pdev,
			      struct otx_cpt_eng_grps *eng_grps);
int otx_cpt_try_create_default_eng_grps(struct pci_dev *pdev,
					struct otx_cpt_eng_grps *eng_grps,
					int pf_type);
void otx_cpt_set_eng_grps_is_rdonly(struct otx_cpt_eng_grps *eng_grps,
				    bool is_rdonly);
int otx_cpt_uc_supports_eng_type(struct otx_cpt_ucode *ucode, int eng_type);
int otx_cpt_eng_grp_has_eng_type(struct otx_cpt_eng_grp_info *eng_grp,
				 int eng_type);

#endif 
