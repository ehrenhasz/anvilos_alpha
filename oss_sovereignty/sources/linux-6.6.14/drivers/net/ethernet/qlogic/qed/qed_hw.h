


#ifndef _QED_HW_H
#define _QED_HW_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "qed.h"
#include "qed_dev_api.h"


struct qed_ptt;

enum reserved_ptts {
	RESERVED_PTT_EDIAG,
	RESERVED_PTT_USER_SPACE,
	RESERVED_PTT_MAIN,
	RESERVED_PTT_DPC,
	RESERVED_PTT_MAX
};

enum _dmae_cmd_dst_mask {
	DMAE_CMD_DST_MASK_NONE	= 0,
	DMAE_CMD_DST_MASK_PCIE	= 1,
	DMAE_CMD_DST_MASK_GRC	= 2
};

enum _dmae_cmd_src_mask {
	DMAE_CMD_SRC_MASK_PCIE	= 0,
	DMAE_CMD_SRC_MASK_GRC	= 1
};

enum _dmae_cmd_crc_mask {
	DMAE_CMD_COMP_CRC_EN_MASK_NONE	= 0,
	DMAE_CMD_COMP_CRC_EN_MASK_SET	= 1
};


#define DMAE_GO_VALUE   0x1

#define DMAE_COMPLETION_VAL     0xD1AE
#define DMAE_CMD_ENDIANITY      0x2

#define DMAE_CMD_SIZE   14
#define DMAE_CMD_SIZE_TO_FILL   (DMAE_CMD_SIZE - 5)
#define DMAE_MIN_WAIT_TIME      0x2
#define DMAE_MAX_CLIENTS        32


void qed_gtt_init(struct qed_hwfn *p_hwfn);


void qed_ptt_invalidate(struct qed_hwfn *p_hwfn);


int qed_ptt_pool_alloc(struct qed_hwfn *p_hwfn);


void qed_ptt_pool_free(struct qed_hwfn *p_hwfn);


u32 qed_ptt_get_hw_addr(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt);


u32 qed_ptt_get_bar_addr(struct qed_ptt *p_ptt);


void qed_ptt_set_win(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     u32 new_hw_addr);


struct qed_ptt *qed_get_reserved_ptt(struct qed_hwfn *p_hwfn,
				     enum reserved_ptts ptt_idx);


void qed_wr(struct qed_hwfn *p_hwfn,
	    struct qed_ptt *p_ptt,
	    u32 hw_addr,
	    u32 val);


u32 qed_rd(struct qed_hwfn *p_hwfn,
	   struct qed_ptt *p_ptt,
	   u32 hw_addr);


void qed_memcpy_from(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     void *dest,
		     u32 hw_addr,
		     size_t n);


void qed_memcpy_to(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt,
		   u32 hw_addr,
		   void *src,
		   size_t n);

void qed_fid_pretend(struct qed_hwfn *p_hwfn,
		     struct qed_ptt *p_ptt,
		     u16 fid);


void qed_port_pretend(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      u8 port_id);


void qed_port_unpretend(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt);


void qed_port_fid_pretend(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, u8 port_id, u16 fid);


u32 qed_vfid_to_concrete(struct qed_hwfn *p_hwfn, u8 vfid);


u32 qed_dmae_idx_to_go_cmd(u8 idx);


int qed_dmae_info_alloc(struct qed_hwfn *p_hwfn);


void qed_dmae_info_free(struct qed_hwfn *p_hwfn);

union qed_qm_pq_params {
	struct {
		u8 q_idx;
	} iscsi;

	struct {
		u8 tc;
	}	core;

	struct {
		u8	is_vf;
		u8	vf_id;
		u8	tc;
	}	eth;

	struct {
		u8 dcqcn;
		u8 qpid;	
	} roce;
};

int qed_init_fw_data(struct qed_dev *cdev,
		     const u8 *fw_data);

int qed_dmae_sanity(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, const char *phase);

#define QED_HW_ERR_MAX_STR_SIZE 256


void __printf(4, 5) __cold qed_hw_err_notify(struct qed_hwfn *p_hwfn,
					     struct qed_ptt *p_ptt,
					     enum qed_hw_err_type err_type,
					     const char *fmt, ...);
#endif
