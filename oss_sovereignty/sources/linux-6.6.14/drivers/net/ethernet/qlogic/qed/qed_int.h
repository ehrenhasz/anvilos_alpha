


#ifndef _QED_INT_H
#define _QED_INT_H

#include <linux/types.h>
#include <linux/slab.h>
#include "qed.h"


#define IGU_PF_CONF_FUNC_EN       (0x1 << 0)    
#define IGU_PF_CONF_MSI_MSIX_EN   (0x1 << 1)    
#define IGU_PF_CONF_INT_LINE_EN   (0x1 << 2)    
#define IGU_PF_CONF_ATTN_BIT_EN   (0x1 << 3)    
#define IGU_PF_CONF_SINGLE_ISR_EN (0x1 << 4)    
#define IGU_PF_CONF_SIMD_MODE     (0x1 << 5)    

#define IGU_VF_CONF_FUNC_EN        (0x1 << 0)	
#define IGU_VF_CONF_MSI_MSIX_EN    (0x1 << 1)	
#define IGU_VF_CONF_SINGLE_ISR_EN  (0x1 << 4)	
#define IGU_VF_CONF_PARENT_MASK    (0xF)	
#define IGU_VF_CONF_PARENT_SHIFT   5		


enum igu_ctrl_cmd {
	IGU_CTRL_CMD_TYPE_RD,
	IGU_CTRL_CMD_TYPE_WR,
	MAX_IGU_CTRL_CMD
};


struct igu_ctrl_reg {
	u32 ctrl_data;
#define IGU_CTRL_REG_FID_MASK           0xFFFF  
#define IGU_CTRL_REG_FID_SHIFT          0
#define IGU_CTRL_REG_PXP_ADDR_MASK      0xFFF   
#define IGU_CTRL_REG_PXP_ADDR_SHIFT     16
#define IGU_CTRL_REG_RESERVED_MASK      0x1
#define IGU_CTRL_REG_RESERVED_SHIFT     28
#define IGU_CTRL_REG_TYPE_MASK          0x1 
#define IGU_CTRL_REG_TYPE_SHIFT         31
};

enum qed_coalescing_fsm {
	QED_COAL_RX_STATE_MACHINE,
	QED_COAL_TX_STATE_MACHINE
};


void qed_int_igu_enable_int(struct qed_hwfn *p_hwfn,
			    struct qed_ptt *p_ptt,
			    enum qed_int_mode int_mode);


void qed_int_igu_disable_int(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt);


u64 qed_int_igu_read_sisr_reg(struct qed_hwfn *p_hwfn);

#define QED_SP_SB_ID 0xffff

int qed_int_sb_init(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_sb_info *sb_info,
		    void *sb_virt_addr,
		    dma_addr_t sb_phy_addr,
		    u16 sb_id);

void qed_int_sb_setup(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct qed_sb_info *sb_info);


int qed_int_sb_release(struct qed_hwfn *p_hwfn,
		       struct qed_sb_info *sb_info,
		       u16 sb_id);


void qed_int_sp_dpc(struct tasklet_struct *t);


void qed_int_get_num_sbs(struct qed_hwfn	*p_hwfn,
			 struct qed_sb_cnt_info *p_sb_cnt_info);


void qed_int_disable_post_isr_release(struct qed_dev *cdev);


void qed_int_attn_clr_enable(struct qed_dev *cdev, bool clr_enable);


int qed_int_get_sb_dbg(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		       struct qed_sb_info *p_sb, struct qed_sb_info_dbg *p_info);


int qed_db_rec_handler(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);

#define QED_CAU_DEF_RX_TIMER_RES 0
#define QED_CAU_DEF_TX_TIMER_RES 0

#define QED_SB_ATT_IDX  0x0001
#define QED_SB_EVENT_MASK       0x0003

#define SB_ALIGNED_SIZE(p_hwfn)	\
	ALIGNED_TYPE_SIZE(struct status_block, p_hwfn)

#define QED_SB_INVALID_IDX      0xffff

struct qed_igu_block {
	u8 status;
#define QED_IGU_STATUS_FREE     0x01
#define QED_IGU_STATUS_VALID    0x02
#define QED_IGU_STATUS_PF       0x04
#define QED_IGU_STATUS_DSB      0x08

	u8 vector_number;
	u8 function_id;
	u8 is_pf;

	
	u16 igu_sb_id;

	struct qed_sb_info *sb_info;
};

struct qed_igu_info {
	struct qed_igu_block entry[MAX_TOT_SB_PER_PATH];
	u16 igu_dsb_id;

	struct qed_sb_cnt_info usage;

	bool b_allow_pf_vf_change;
};


int qed_int_igu_reset_cam(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt);


u16 qed_get_igu_sb_id(struct qed_hwfn *p_hwfn, u16 sb_id);


struct qed_igu_block *qed_get_igu_free_sb(struct qed_hwfn *p_hwfn,
					  bool b_is_pf);

void qed_int_igu_init_pure_rt(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      bool b_set,
			      bool b_slowpath);

void qed_int_igu_init_rt(struct qed_hwfn *p_hwfn);


int qed_int_igu_read_cam(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt);

typedef int (*qed_int_comp_cb_t)(struct qed_hwfn *p_hwfn,
				 void *cookie);

int qed_int_register_cb(struct qed_hwfn *p_hwfn,
			qed_int_comp_cb_t comp_cb,
			void *cookie,
			u8 *sb_idx,
			__le16 **p_fw_cons);


int qed_int_unregister_cb(struct qed_hwfn *p_hwfn,
			  u8 pi);


u16 qed_int_get_sp_sb_id(struct qed_hwfn *p_hwfn);


void qed_int_igu_init_pure_rt_single(struct qed_hwfn *p_hwfn,
				     struct qed_ptt *p_ptt,
				     u16 igu_sb_id,
				     u16 opaque,
				     bool b_set);


void qed_int_cau_conf_sb(struct qed_hwfn *p_hwfn,
			 struct qed_ptt *p_ptt,
			 dma_addr_t sb_phys,
			 u16 igu_sb_id,
			 u16 vf_number,
			 u8 vf_valid);


int qed_int_alloc(struct qed_hwfn *p_hwfn,
		  struct qed_ptt *p_ptt);


void qed_int_free(struct qed_hwfn *p_hwfn);


void qed_int_setup(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt);


int qed_int_igu_enable(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
		       enum qed_int_mode int_mode);


void qed_init_cau_sb_entry(struct qed_hwfn *p_hwfn,
			   struct cau_sb_entry *p_sb_entry,
			   u8 pf_id,
			   u16 vf_number,
			   u8 vf_valid);

int qed_int_set_timer_res(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
			  u8 timer_res, u16 sb_id, bool tx);

#define QED_MAPPING_MEMORY_SIZE(dev)	(NUM_OF_SBS(dev))

int qed_pglueb_rbc_attn_handler(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt,
				bool hw_init);

#endif
