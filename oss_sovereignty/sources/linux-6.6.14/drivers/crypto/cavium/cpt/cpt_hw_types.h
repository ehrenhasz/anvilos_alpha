


#ifndef __CPT_HW_TYPES_H
#define __CPT_HW_TYPES_H

#include "cpt_common.h"


enum cpt_comp_e {
	CPT_COMP_E_NOTDONE = 0x00,
	CPT_COMP_E_GOOD = 0x01,
	CPT_COMP_E_FAULT = 0x02,
	CPT_COMP_E_SWERR = 0x03,
	CPT_COMP_E_LAST_ENTRY = 0xFF
};


union cpt_inst_s {
	u64 u[8];
	struct cpt_inst_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_17_63:47;
		u64 doneint:1;
		u64 reserved_0_1:16;
#else 
		u64 reserved_0_15:16;
		u64 doneint:1;
		u64 reserved_17_63:47;
#endif 
		u64 res_addr;
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_172_19:20;
		u64 grp:10;
		u64 tt:2;
		u64 tag:32;
#else 
		u64 tag:32;
		u64 tt:2;
		u64 grp:10;
		u64 reserved_172_191:20;
#endif 
		u64 wq_ptr;
		u64 ei0;
		u64 ei1;
		u64 ei2;
		u64 ei3;
	} s;
};


union cpt_res_s {
	u64 u[2];
	struct cpt_res_s_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_17_63:47;
		u64 doneint:1;
		u64 reserved_8_15:8;
		u64 compcode:8;
#else 
		u64 compcode:8;
		u64 reserved_8_15:8;
		u64 doneint:1;
		u64 reserved_17_63:47;
#endif 
		u64 reserved_64_127;
	} s;
};


union cptx_pf_bist_status {
	u64 u;
	struct cptx_pf_bist_status_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_30_63:34;
		u64 bstatus:30;
#else 
		u64 bstatus:30;
		u64 reserved_30_63:34;
#endif 
	} s;
};


union cptx_pf_constants {
	u64 u;
	struct cptx_pf_constants_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_40_63:24;
		u64 epcis:8;
		u64 grps:8;
		u64 ae:8;
		u64 se:8;
		u64 vq:8;
#else 
		u64 vq:8;
		u64 se:8;
		u64 ae:8;
		u64 grps:8;
		u64 epcis:8;
		u64 reserved_40_63:24;
#endif 
	} s;
};


union cptx_pf_exe_bist_status {
	u64 u;
	struct cptx_pf_exe_bist_status_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_48_63:16;
		u64 bstatus:48;
#else 
		u64 bstatus:48;
		u64 reserved_48_63:16;
#endif 
	} s;
};


union cptx_pf_qx_ctl {
	u64 u;
	struct cptx_pf_qx_ctl_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_60_63:4;
		u64 aura:12;
		u64 reserved_45_47:3;
		u64 size:13;
		u64 reserved_11_31:21;
		u64 cont_err:1;
		u64 inst_free:1;
		u64 inst_be:1;
		u64 iqb_ldwb:1;
		u64 reserved_4_6:3;
		u64 grp:3;
		u64 pri:1;
#else 
		u64 pri:1;
		u64 grp:3;
		u64 reserved_4_6:3;
		u64 iqb_ldwb:1;
		u64 inst_be:1;
		u64 inst_free:1;
		u64 cont_err:1;
		u64 reserved_11_31:21;
		u64 size:13;
		u64 reserved_45_47:3;
		u64 aura:12;
		u64 reserved_60_63:4;
#endif 
	} s;
};


union cptx_vqx_saddr {
	u64 u;
	struct cptx_vqx_saddr_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_49_63:15;
		u64 ptr:43;
		u64 reserved_0_5:6;
#else 
		u64 reserved_0_5:6;
		u64 ptr:43;
		u64 reserved_49_63:15;
#endif 
	} s;
};


union cptx_vqx_misc_ena_w1s {
	u64 u;
	struct cptx_vqx_misc_ena_w1s_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_5_63:59;
		u64 swerr:1;
		u64 nwrp:1;
		u64 irde:1;
		u64 dovf:1;
		u64 mbox:1;
#else 
		u64 mbox:1;
		u64 dovf:1;
		u64 irde:1;
		u64 nwrp:1;
		u64 swerr:1;
		u64 reserved_5_63:59;
#endif 
	} s;
};


union cptx_vqx_doorbell {
	u64 u;
	struct cptx_vqx_doorbell_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_20_63:44;
		u64 dbell_cnt:20;
#else 
		u64 dbell_cnt:20;
		u64 reserved_20_63:44;
#endif 
	} s;
};


union cptx_vqx_inprog {
	u64 u;
	struct cptx_vqx_inprog_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_8_63:56;
		u64 inflight:8;
#else 
		u64 inflight:8;
		u64 reserved_8_63:56;
#endif 
	} s;
};


union cptx_vqx_misc_int {
	u64 u;
	struct cptx_vqx_misc_int_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_5_63:59;
		u64 swerr:1;
		u64 nwrp:1;
		u64 irde:1;
		u64 dovf:1;
		u64 mbox:1;
#else 
		u64 mbox:1;
		u64 dovf:1;
		u64 irde:1;
		u64 nwrp:1;
		u64 swerr:1;
		u64 reserved_5_63:59;
#endif 
	} s;
};


union cptx_vqx_done_ack {
	u64 u;
	struct cptx_vqx_done_ack_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_20_63:44;
		u64 done_ack:20;
#else 
		u64 done_ack:20;
		u64 reserved_20_63:44;
#endif 
	} s;
};


union cptx_vqx_done {
	u64 u;
	struct cptx_vqx_done_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_20_63:44;
		u64 done:20;
#else 
		u64 done:20;
		u64 reserved_20_63:44;
#endif 
	} s;
};


union cptx_vqx_done_wait {
	u64 u;
	struct cptx_vqx_done_wait_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_48_63:16;
		u64 time_wait:16;
		u64 reserved_20_31:12;
		u64 num_wait:20;
#else 
		u64 num_wait:20;
		u64 reserved_20_31:12;
		u64 time_wait:16;
		u64 reserved_48_63:16;
#endif 
	} s;
};


union cptx_vqx_done_ena_w1s {
	u64 u;
	struct cptx_vqx_done_ena_w1s_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_1_63:63;
		u64 done:1;
#else 
		u64 done:1;
		u64 reserved_1_63:63;
#endif 
	} s;
};


union cptx_vqx_ctl {
	u64 u;
	struct cptx_vqx_ctl_s {
#if defined(__BIG_ENDIAN_BITFIELD) 
		u64 reserved_1_63:63;
		u64 ena:1;
#else 
		u64 ena:1;
		u64 reserved_1_63:63;
#endif 
	} s;
};
#endif 
