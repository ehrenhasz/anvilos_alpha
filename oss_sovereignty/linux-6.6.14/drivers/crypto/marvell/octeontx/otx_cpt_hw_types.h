#ifndef __OTX_CPT_HW_TYPES_H
#define __OTX_CPT_HW_TYPES_H
#include <linux/types.h>
#define OTX_CPT_PCI_PF_DEVICE_ID 0xa040
#define OTX_CPT_PCI_VF_DEVICE_ID 0xa041
#define OTX_CPT_PCI_PF_SUBSYS_ID 0xa340
#define OTX_CPT_PCI_VF_SUBSYS_ID 0xa341
#define OTX_CPT_PF_PCI_CFG_BAR	0
#define OTX_CPT_VF_PCI_CFG_BAR	0
#define OTX_CPT_BAR_E_CPTX_VFX_BAR0_OFFSET(a, b) \
	(0x000020000000ll + 0x1000000000ll * (a) + 0x100000ll * (b))
#define OTX_CPT_BAR_E_CPTX_VFX_BAR0_SIZE	0x400000
#define OTX_CPT_PF_MBOX_INT	3
#define OTX_CPT_PF_INT_VEC_E_MBOXX(x, a) ((x) + (a))
#define OTX_CPT_PF_MSIX_VECTORS 4
#define OTX_CPT_MAX_ENGINE_GROUPS 8
#define OTX_CPT_INST_SIZE 64
#define OTX_CPT_NEXT_CHUNK_PTR_SIZE 8
#define OTX_CPT_VF_MSIX_VECTORS 2
#define OTX_CPT_VF_INTR_MBOX_MASK BIT(0)
#define OTX_CPT_VF_INTR_DOVF_MASK BIT(1)
#define OTX_CPT_VF_INTR_IRDE_MASK BIT(2)
#define OTX_CPT_VF_INTR_NWRP_MASK BIT(3)
#define OTX_CPT_VF_INTR_SERR_MASK BIT(4)
#define OTX_CPT_PF_CONSTANTS		(0x0ll)
#define OTX_CPT_PF_RESET		(0x100ll)
#define OTX_CPT_PF_DIAG			(0x120ll)
#define OTX_CPT_PF_BIST_STATUS		(0x160ll)
#define OTX_CPT_PF_ECC0_CTL		(0x200ll)
#define OTX_CPT_PF_ECC0_FLIP		(0x210ll)
#define OTX_CPT_PF_ECC0_INT		(0x220ll)
#define OTX_CPT_PF_ECC0_INT_W1S		(0x230ll)
#define OTX_CPT_PF_ECC0_ENA_W1S		(0x240ll)
#define OTX_CPT_PF_ECC0_ENA_W1C		(0x250ll)
#define OTX_CPT_PF_MBOX_INTX(b)		(0x400ll | (u64)(b) << 3)
#define OTX_CPT_PF_MBOX_INT_W1SX(b)	(0x420ll | (u64)(b) << 3)
#define OTX_CPT_PF_MBOX_ENA_W1CX(b)	(0x440ll | (u64)(b) << 3)
#define OTX_CPT_PF_MBOX_ENA_W1SX(b)	(0x460ll | (u64)(b) << 3)
#define OTX_CPT_PF_EXEC_INT		(0x500ll)
#define OTX_CPT_PF_EXEC_INT_W1S		(0x520ll)
#define OTX_CPT_PF_EXEC_ENA_W1C		(0x540ll)
#define OTX_CPT_PF_EXEC_ENA_W1S		(0x560ll)
#define OTX_CPT_PF_GX_EN(b)		(0x600ll | (u64)(b) << 3)
#define OTX_CPT_PF_EXEC_INFO		(0x700ll)
#define OTX_CPT_PF_EXEC_BUSY		(0x800ll)
#define OTX_CPT_PF_EXEC_INFO0		(0x900ll)
#define OTX_CPT_PF_EXEC_INFO1		(0x910ll)
#define OTX_CPT_PF_INST_REQ_PC		(0x10000ll)
#define OTX_CPT_PF_INST_LATENCY_PC	(0x10020ll)
#define OTX_CPT_PF_RD_REQ_PC		(0x10040ll)
#define OTX_CPT_PF_RD_LATENCY_PC	(0x10060ll)
#define OTX_CPT_PF_RD_UC_PC		(0x10080ll)
#define OTX_CPT_PF_ACTIVE_CYCLES_PC	(0x10100ll)
#define OTX_CPT_PF_EXE_CTL		(0x4000000ll)
#define OTX_CPT_PF_EXE_STATUS		(0x4000008ll)
#define OTX_CPT_PF_EXE_CLK		(0x4000010ll)
#define OTX_CPT_PF_EXE_DBG_CTL		(0x4000018ll)
#define OTX_CPT_PF_EXE_DBG_DATA		(0x4000020ll)
#define OTX_CPT_PF_EXE_BIST_STATUS	(0x4000028ll)
#define OTX_CPT_PF_EXE_REQ_TIMER	(0x4000030ll)
#define OTX_CPT_PF_EXE_MEM_CTL		(0x4000038ll)
#define OTX_CPT_PF_EXE_PERF_CTL		(0x4001000ll)
#define OTX_CPT_PF_EXE_DBG_CNTX(b)	(0x4001100ll | (u64)(b) << 3)
#define OTX_CPT_PF_EXE_PERF_EVENT_CNT	(0x4001180ll)
#define OTX_CPT_PF_EXE_EPCI_INBX_CNT(b)	(0x4001200ll | (u64)(b) << 3)
#define OTX_CPT_PF_EXE_EPCI_OUTBX_CNT(b) (0x4001240ll | (u64)(b) << 3)
#define OTX_CPT_PF_ENGX_UCODE_BASE(b)	(0x4002000ll | (u64)(b) << 3)
#define OTX_CPT_PF_QX_CTL(b)		(0x8000000ll | (u64)(b) << 20)
#define OTX_CPT_PF_QX_GMCTL(b)		(0x8000020ll | (u64)(b) << 20)
#define OTX_CPT_PF_QX_CTL2(b)		(0x8000100ll | (u64)(b) << 20)
#define OTX_CPT_PF_VFX_MBOXX(b, c)	(0x8001000ll | (u64)(b) << 20 | \
					 (u64)(c) << 8)
#define OTX_CPT_VQX_CTL(b)		(0x100ll | (u64)(b) << 20)
#define OTX_CPT_VQX_SADDR(b)		(0x200ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_WAIT(b)	(0x400ll | (u64)(b) << 20)
#define OTX_CPT_VQX_INPROG(b)		(0x410ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE(b)		(0x420ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_ACK(b)		(0x440ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_INT_W1S(b)	(0x460ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_INT_W1C(b)	(0x468ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_ENA_W1S(b)	(0x470ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DONE_ENA_W1C(b)	(0x478ll | (u64)(b) << 20)
#define OTX_CPT_VQX_MISC_INT(b)		(0x500ll | (u64)(b) << 20)
#define OTX_CPT_VQX_MISC_INT_W1S(b)	(0x508ll | (u64)(b) << 20)
#define OTX_CPT_VQX_MISC_ENA_W1S(b)	(0x510ll | (u64)(b) << 20)
#define OTX_CPT_VQX_MISC_ENA_W1C(b)	(0x518ll | (u64)(b) << 20)
#define OTX_CPT_VQX_DOORBELL(b)		(0x600ll | (u64)(b) << 20)
#define OTX_CPT_VFX_PF_MBOXX(b, c)	(0x1000ll | ((b) << 20) | ((c) << 3))
enum otx_cpt_ucode_error_code_e {
	CPT_NO_UCODE_ERROR = 0x00,
	ERR_OPCODE_UNSUPPORTED = 0x01,
	ERR_SCATTER_GATHER_WRITE_LENGTH = 0x02,
	ERR_SCATTER_GATHER_LIST = 0x03,
	ERR_SCATTER_GATHER_NOT_SUPPORTED = 0x04,
};
enum otx_cpt_comp_e {
	CPT_COMP_E_NOTDONE = 0x00,
	CPT_COMP_E_GOOD = 0x01,
	CPT_COMP_E_FAULT = 0x02,
	CPT_COMP_E_SWERR = 0x03,
	CPT_COMP_E_HWERR = 0x04,
	CPT_COMP_E_LAST_ENTRY = 0x05
};
enum otx_cpt_vf_int_vec_e {
	CPT_VF_INT_VEC_E_MISC = 0x00,
	CPT_VF_INT_VEC_E_DONE = 0x01
};
union otx_cpt_inst_s {
	u64 u[8];
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_17_63:47;
		u64 doneint:1;
		u64 reserved_0_15:16;
#else  
		u64 reserved_0_15:16;
		u64 doneint:1;
		u64 reserved_17_63:47;
#endif  
		u64 res_addr;
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_172_191:20;
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
union otx_cpt_res_s {
	u64 u[2];
	struct {
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
union otx_cptx_pf_bist_status {
	u64 u;
	struct otx_cptx_pf_bist_status_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_30_63:34;
		u64 bstatus:30;
#else  
		u64 bstatus:30;
		u64 reserved_30_63:34;
#endif  
	} s;
};
union otx_cptx_pf_constants {
	u64 u;
	struct otx_cptx_pf_constants_s {
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
union otx_cptx_pf_exe_bist_status {
	u64 u;
	struct otx_cptx_pf_exe_bist_status_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_48_63:16;
		u64 bstatus:48;
#else  
		u64 bstatus:48;
		u64 reserved_48_63:16;
#endif  
	} s;
};
union otx_cptx_pf_qx_ctl {
	u64 u;
	struct otx_cptx_pf_qx_ctl_s {
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
union otx_cptx_vqx_saddr {
	u64 u;
	struct otx_cptx_vqx_saddr_s {
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
union otx_cptx_vqx_misc_ena_w1s {
	u64 u;
	struct otx_cptx_vqx_misc_ena_w1s_s {
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
union otx_cptx_vqx_doorbell {
	u64 u;
	struct otx_cptx_vqx_doorbell_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_20_63:44;
		u64 dbell_cnt:20;
#else  
		u64 dbell_cnt:20;
		u64 reserved_20_63:44;
#endif  
	} s;
};
union otx_cptx_vqx_inprog {
	u64 u;
	struct otx_cptx_vqx_inprog_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_8_63:56;
		u64 inflight:8;
#else  
		u64 inflight:8;
		u64 reserved_8_63:56;
#endif  
	} s;
};
union otx_cptx_vqx_misc_int {
	u64 u;
	struct otx_cptx_vqx_misc_int_s {
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
union otx_cptx_vqx_done_ack {
	u64 u;
	struct otx_cptx_vqx_done_ack_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_20_63:44;
		u64 done_ack:20;
#else  
		u64 done_ack:20;
		u64 reserved_20_63:44;
#endif  
	} s;
};
union otx_cptx_vqx_done {
	u64 u;
	struct otx_cptx_vqx_done_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_20_63:44;
		u64 done:20;
#else  
		u64 done:20;
		u64 reserved_20_63:44;
#endif  
	} s;
};
union otx_cptx_vqx_done_wait {
	u64 u;
	struct otx_cptx_vqx_done_wait_s {
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
union otx_cptx_vqx_done_ena_w1s {
	u64 u;
	struct otx_cptx_vqx_done_ena_w1s_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_1_63:63;
		u64 done:1;
#else  
		u64 done:1;
		u64 reserved_1_63:63;
#endif  
	} s;
};
union otx_cptx_vqx_ctl {
	u64 u;
	struct otx_cptx_vqx_ctl_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		u64 reserved_1_63:63;
		u64 ena:1;
#else  
		u64 ena:1;
		u64 reserved_1_63:63;
#endif  
	} s;
};
union otx_cpt_error_code {
	u64 u;
	struct otx_cpt_error_code_s {
#if defined(__BIG_ENDIAN_BITFIELD)  
		uint64_t ccode:8;
		uint64_t coreid:8;
		uint64_t rptr6:48;
#else  
		uint64_t rptr6:48;
		uint64_t coreid:8;
		uint64_t ccode:8;
#endif  
	} s;
};
#endif  
