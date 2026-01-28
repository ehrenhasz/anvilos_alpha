#ifndef __OTX2_CPTPF_UCODE_H
#define __OTX2_CPTPF_UCODE_H
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/module.h>
#include "otx2_cpt_hw_types.h"
#include "otx2_cpt_common.h"
#define OTX2_CPT_MAX_ETYPES_PER_GRP 2
#define OTX2_CPT_UCODE_SIGN_LEN     256
#define OTX2_CPT_UCODE_VER_STR_SZ   44
#define OTX2_CPT_MAX_ENGINES        144
#define OTX2_CPT_ENGS_BITMASK_LEN   BITS_TO_LONGS(OTX2_CPT_MAX_ENGINES)
#define OTX2_CPT_UCODE_SZ           (64 * 1024)
enum otx2_cpt_ucode_type {
	OTX2_CPT_AE_UC_TYPE = 1,   
	OTX2_CPT_SE_UC_TYPE1 = 20, 
	OTX2_CPT_SE_UC_TYPE2 = 21, 
	OTX2_CPT_SE_UC_TYPE3 = 22, 
	OTX2_CPT_IE_UC_TYPE1 = 30,  
	OTX2_CPT_IE_UC_TYPE2 = 31,  
	OTX2_CPT_IE_UC_TYPE3 = 32,  
};
struct otx2_cpt_bitmap {
	unsigned long bits[OTX2_CPT_ENGS_BITMASK_LEN];
	int size;
};
struct otx2_cpt_engines {
	int type;
	int count;
};
struct otx2_cpt_ucode_ver_num {
	u8 nn;
	u8 xx;
	u8 yy;
	u8 zz;
};
struct otx2_cpt_ucode_hdr {
	struct otx2_cpt_ucode_ver_num ver_num;
	u8 ver_str[OTX2_CPT_UCODE_VER_STR_SZ];
	__be32 code_length;
	u32 padding[3];
};
struct otx2_cpt_ucode {
	u8 ver_str[OTX2_CPT_UCODE_VER_STR_SZ]; 
	struct otx2_cpt_ucode_ver_num ver_num; 
	char filename[OTX2_CPT_NAME_LENGTH]; 
	dma_addr_t dma;		 
	void *va;		 
	u32 size;		 
	int type;		 
};
struct otx2_cpt_uc_info_t {
	struct list_head list;
	struct otx2_cpt_ucode ucode; 
	const struct firmware *fw;
};
struct otx2_cpt_engs_available {
	int max_se_cnt;
	int max_ie_cnt;
	int max_ae_cnt;
	int se_cnt;
	int ie_cnt;
	int ae_cnt;
};
struct otx2_cpt_engs_rsvd {
	int type;	 
	int count;	 
	int offset;      
	unsigned long *bmap;		 
	struct otx2_cpt_ucode *ucode;	 
};
struct otx2_cpt_mirror_info {
	int is_ena;	 
	int idx;	 
	int ref_count;	 
};
struct otx2_cpt_eng_grp_info {
	struct otx2_cpt_eng_grps *g;  
	struct otx2_cpt_engs_rsvd engs[OTX2_CPT_MAX_ETYPES_PER_GRP];
	struct otx2_cpt_ucode ucode[OTX2_CPT_MAX_ETYPES_PER_GRP];
	struct otx2_cpt_mirror_info mirror;
	int idx;	  
	bool is_enabled;  
};
struct otx2_cpt_eng_grps {
	struct mutex lock;
	struct otx2_cpt_eng_grp_info grp[OTX2_CPT_MAX_ENGINE_GROUPS];
	struct otx2_cpt_engs_available avail;
	void *obj;			 
	int engs_num;			 
	u8 eng_ref_cnt[OTX2_CPT_MAX_ENGINES]; 
	bool is_grps_created;  
};
struct otx2_cptpf_dev;
int otx2_cpt_init_eng_grps(struct pci_dev *pdev,
			   struct otx2_cpt_eng_grps *eng_grps);
void otx2_cpt_cleanup_eng_grps(struct pci_dev *pdev,
			       struct otx2_cpt_eng_grps *eng_grps);
int otx2_cpt_create_eng_grps(struct otx2_cptpf_dev *cptpf,
			     struct otx2_cpt_eng_grps *eng_grps);
int otx2_cpt_disable_all_cores(struct otx2_cptpf_dev *cptpf);
int otx2_cpt_get_eng_grp(struct otx2_cpt_eng_grps *eng_grps, int eng_type);
int otx2_cpt_discover_eng_capabilities(struct otx2_cptpf_dev *cptpf);
int otx2_cpt_dl_custom_egrp_create(struct otx2_cptpf_dev *cptpf,
				   struct devlink_param_gset_ctx *ctx);
int otx2_cpt_dl_custom_egrp_delete(struct otx2_cptpf_dev *cptpf,
				   struct devlink_param_gset_ctx *ctx);
void otx2_cpt_print_uc_dbg_info(struct otx2_cptpf_dev *cptpf);
struct otx2_cpt_engs_rsvd *find_engines_by_type(
					struct otx2_cpt_eng_grp_info *eng_grp,
					int eng_type);
#endif  
