 
 

#ifndef _COMMON_H
#define _COMMON_H

#include <rdma/hfi/hfi1_user.h>

 

 
#define IPS_PROTO_VERSION 2

 

 
#define HFI1_CAP_USER_SHIFT      24
#define HFI1_CAP_MASK            ((1UL << HFI1_CAP_USER_SHIFT) - 1)
 
#define HFI1_CAP_LOCKED_SHIFT    63
#define HFI1_CAP_LOCKED_MASK     0x1ULL
#define HFI1_CAP_LOCKED_SMASK    (HFI1_CAP_LOCKED_MASK << HFI1_CAP_LOCKED_SHIFT)
 
#define HFI1_CAP_MISC_SHIFT      (HFI1_CAP_USER_SHIFT * 2)
#define HFI1_CAP_MISC_MASK       ((1ULL << (HFI1_CAP_LOCKED_SHIFT - \
					   HFI1_CAP_MISC_SHIFT)) - 1)

#define HFI1_CAP_KSET(cap) ({ hfi1_cap_mask |= HFI1_CAP_##cap; hfi1_cap_mask; })
#define HFI1_CAP_KCLEAR(cap)						\
	({								\
		hfi1_cap_mask &= ~HFI1_CAP_##cap;			\
		hfi1_cap_mask;						\
	})
#define HFI1_CAP_USET(cap)						\
	({								\
		hfi1_cap_mask |= (HFI1_CAP_##cap << HFI1_CAP_USER_SHIFT); \
		hfi1_cap_mask;						\
		})
#define HFI1_CAP_UCLEAR(cap)						\
	({								\
		hfi1_cap_mask &= ~(HFI1_CAP_##cap << HFI1_CAP_USER_SHIFT); \
		hfi1_cap_mask;						\
	})
#define HFI1_CAP_SET(cap)						\
	({								\
		hfi1_cap_mask |= (HFI1_CAP_##cap | (HFI1_CAP_##cap <<	\
						  HFI1_CAP_USER_SHIFT)); \
		hfi1_cap_mask;						\
	})
#define HFI1_CAP_CLEAR(cap)						\
	({								\
		hfi1_cap_mask &= ~(HFI1_CAP_##cap |			\
				  (HFI1_CAP_##cap << HFI1_CAP_USER_SHIFT)); \
		hfi1_cap_mask;						\
	})
#define HFI1_CAP_LOCK()							\
	({ hfi1_cap_mask |= HFI1_CAP_LOCKED_SMASK; hfi1_cap_mask; })
#define HFI1_CAP_LOCKED() (!!(hfi1_cap_mask & HFI1_CAP_LOCKED_SMASK))
 
#define HFI1_CAP_WRITABLE_MASK   (HFI1_CAP_SDMA_AHG |			\
				  HFI1_CAP_HDRSUPP |			\
				  HFI1_CAP_MULTI_PKT_EGR |		\
				  HFI1_CAP_NODROP_RHQ_FULL |		\
				  HFI1_CAP_NODROP_EGR_FULL |		\
				  HFI1_CAP_ALLOW_PERM_JKEY |		\
				  HFI1_CAP_STATIC_RATE_CTRL |		\
				  HFI1_CAP_PRINT_UNIMPL |		\
				  HFI1_CAP_TID_UNMAP |			\
				  HFI1_CAP_OPFN)
 
#define HFI1_CAP_RESERVED_MASK   ((HFI1_CAP_SDMA |			\
				   HFI1_CAP_USE_SDMA_HEAD |		\
				   HFI1_CAP_EXTENDED_PSN |		\
				   HFI1_CAP_PRINT_UNIMPL |		\
				   HFI1_CAP_NO_INTEGRITY |		\
				   HFI1_CAP_PKEY_CHECK |		\
				   HFI1_CAP_TID_RDMA |			\
				   HFI1_CAP_OPFN |			\
				   HFI1_CAP_AIP) <<			\
				  HFI1_CAP_USER_SHIFT)
 
#define HFI1_CAP_MUST_HAVE_KERN (HFI1_CAP_STATIC_RATE_CTRL)
 
#define HFI1_CAP_MASK_DEFAULT    (HFI1_CAP_HDRSUPP |			\
				 HFI1_CAP_NODROP_RHQ_FULL |		\
				 HFI1_CAP_NODROP_EGR_FULL |		\
				 HFI1_CAP_SDMA |			\
				 HFI1_CAP_PRINT_UNIMPL |		\
				 HFI1_CAP_STATIC_RATE_CTRL |		\
				 HFI1_CAP_PKEY_CHECK |			\
				 HFI1_CAP_MULTI_PKT_EGR |		\
				 HFI1_CAP_EXTENDED_PSN |		\
				 HFI1_CAP_AIP |				\
				 ((HFI1_CAP_HDRSUPP |			\
				   HFI1_CAP_MULTI_PKT_EGR |		\
				   HFI1_CAP_STATIC_RATE_CTRL |		\
				   HFI1_CAP_PKEY_CHECK |		\
				   HFI1_CAP_EARLY_CREDIT_RETURN) <<	\
				  HFI1_CAP_USER_SHIFT))
 
#define HFI1_CAP_K2U (HFI1_CAP_SDMA |			\
		     HFI1_CAP_EXTENDED_PSN |		\
		     HFI1_CAP_PKEY_CHECK |		\
		     HFI1_CAP_NO_INTEGRITY)

#define HFI1_USER_SWVERSION ((HFI1_USER_SWMAJOR << HFI1_SWMAJOR_SHIFT) | \
			     HFI1_USER_SWMINOR)

 

 
#define RHF_PKT_LEN_SHIFT	0
#define RHF_PKT_LEN_MASK	0xfffull
#define RHF_PKT_LEN_SMASK (RHF_PKT_LEN_MASK << RHF_PKT_LEN_SHIFT)

#define RHF_RCV_TYPE_SHIFT	12
#define RHF_RCV_TYPE_MASK	0x7ull
#define RHF_RCV_TYPE_SMASK (RHF_RCV_TYPE_MASK << RHF_RCV_TYPE_SHIFT)

#define RHF_USE_EGR_BFR_SHIFT	15
#define RHF_USE_EGR_BFR_MASK	0x1ull
#define RHF_USE_EGR_BFR_SMASK (RHF_USE_EGR_BFR_MASK << RHF_USE_EGR_BFR_SHIFT)

#define RHF_EGR_INDEX_SHIFT	16
#define RHF_EGR_INDEX_MASK	0x7ffull
#define RHF_EGR_INDEX_SMASK (RHF_EGR_INDEX_MASK << RHF_EGR_INDEX_SHIFT)

#define RHF_DC_INFO_SHIFT	27
#define RHF_DC_INFO_MASK	0x1ull
#define RHF_DC_INFO_SMASK (RHF_DC_INFO_MASK << RHF_DC_INFO_SHIFT)

#define RHF_RCV_SEQ_SHIFT	28
#define RHF_RCV_SEQ_MASK	0xfull
#define RHF_RCV_SEQ_SMASK (RHF_RCV_SEQ_MASK << RHF_RCV_SEQ_SHIFT)

#define RHF_EGR_OFFSET_SHIFT	32
#define RHF_EGR_OFFSET_MASK	0xfffull
#define RHF_EGR_OFFSET_SMASK (RHF_EGR_OFFSET_MASK << RHF_EGR_OFFSET_SHIFT)
#define RHF_HDRQ_OFFSET_SHIFT	44
#define RHF_HDRQ_OFFSET_MASK	0x1ffull
#define RHF_HDRQ_OFFSET_SMASK (RHF_HDRQ_OFFSET_MASK << RHF_HDRQ_OFFSET_SHIFT)
#define RHF_K_HDR_LEN_ERR	(0x1ull << 53)
#define RHF_DC_UNC_ERR		(0x1ull << 54)
#define RHF_DC_ERR		(0x1ull << 55)
#define RHF_RCV_TYPE_ERR_SHIFT	56
#define RHF_RCV_TYPE_ERR_MASK	0x7ul
#define RHF_RCV_TYPE_ERR_SMASK (RHF_RCV_TYPE_ERR_MASK << RHF_RCV_TYPE_ERR_SHIFT)
#define RHF_TID_ERR		(0x1ull << 59)
#define RHF_LEN_ERR		(0x1ull << 60)
#define RHF_ECC_ERR		(0x1ull << 61)
#define RHF_RESERVED		(0x1ull << 62)
#define RHF_ICRC_ERR		(0x1ull << 63)

#define RHF_ERROR_SMASK 0xffe0000000000000ull		 

 
#define RHF_RCV_TYPE_EXPECTED 0
#define RHF_RCV_TYPE_EAGER    1
#define RHF_RCV_TYPE_IB       2  
#define RHF_RCV_TYPE_ERROR    3
#define RHF_RCV_TYPE_BYPASS   4
#define RHF_RCV_TYPE_INVALID5 5
#define RHF_RCV_TYPE_INVALID6 6
#define RHF_RCV_TYPE_INVALID7 7

 
#define RHF_RTE_EXPECTED_FLOW_SEQ_ERR	0x2
#define RHF_RTE_EXPECTED_FLOW_GEN_ERR	0x4

 
#define RHF_RTE_EAGER_NO_ERR		0x0

 
#define RHF_RTE_IB_NO_ERR		0x0

 
#define RHF_RTE_ERROR_NO_ERR		0x0
#define RHF_RTE_ERROR_OP_CODE_ERR	0x1
#define RHF_RTE_ERROR_KHDR_MIN_LEN_ERR	0x2
#define RHF_RTE_ERROR_KHDR_HCRC_ERR	0x3
#define RHF_RTE_ERROR_KHDR_KVER_ERR	0x4
#define RHF_RTE_ERROR_CONTEXT_ERR	0x5
#define RHF_RTE_ERROR_KHDR_TID_ERR	0x6

 
#define RHF_RTE_BYPASS_NO_ERR		0x0

 
#define RHF_MAX_SEQ 13

 
#define HFI1_LRH_GRH 0x0003       
#define HFI1_LRH_BTH 0x0002       

 
#define SC15_PACKET 0xF
#define SIZE_OF_CRC 1
#define SIZE_OF_LT 1
#define MAX_16B_PADDING 12  

#define LIM_MGMT_P_KEY       0x7FFF
#define FULL_MGMT_P_KEY      0xFFFF

#define DEFAULT_P_KEY LIM_MGMT_P_KEY

#define HFI1_PSM_IOC_BASE_SEQ 0x0

 
#define HFI1_KDETH_BTH_SEQ_SHIFT 11
#define HFI1_KDETH_BTH_SEQ_MASK (BIT(HFI1_KDETH_BTH_SEQ_SHIFT) - 1)

static inline __u64 rhf_to_cpu(const __le32 *rbuf)
{
	return __le64_to_cpu(*((__le64 *)rbuf));
}

static inline u64 rhf_err_flags(u64 rhf)
{
	return rhf & RHF_ERROR_SMASK;
}

static inline u32 rhf_rcv_type(u64 rhf)
{
	return (rhf >> RHF_RCV_TYPE_SHIFT) & RHF_RCV_TYPE_MASK;
}

static inline u32 rhf_rcv_type_err(u64 rhf)
{
	return (rhf >> RHF_RCV_TYPE_ERR_SHIFT) & RHF_RCV_TYPE_ERR_MASK;
}

 
static inline u32 rhf_pkt_len(u64 rhf)
{
	return ((rhf & RHF_PKT_LEN_SMASK) >> RHF_PKT_LEN_SHIFT) << 2;
}

static inline u32 rhf_egr_index(u64 rhf)
{
	return (rhf >> RHF_EGR_INDEX_SHIFT) & RHF_EGR_INDEX_MASK;
}

static inline u32 rhf_rcv_seq(u64 rhf)
{
	return (rhf >> RHF_RCV_SEQ_SHIFT) & RHF_RCV_SEQ_MASK;
}

 
static inline u32 rhf_hdrq_offset(u64 rhf)
{
	return (rhf >> RHF_HDRQ_OFFSET_SHIFT) & RHF_HDRQ_OFFSET_MASK;
}

static inline u64 rhf_use_egr_bfr(u64 rhf)
{
	return rhf & RHF_USE_EGR_BFR_SMASK;
}

static inline u64 rhf_dc_info(u64 rhf)
{
	return rhf & RHF_DC_INFO_SMASK;
}

static inline u32 rhf_egr_buf_offset(u64 rhf)
{
	return (rhf >> RHF_EGR_OFFSET_SHIFT) & RHF_EGR_OFFSET_MASK;
}
#endif  
