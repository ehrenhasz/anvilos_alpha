 
#ifndef __CARD_DDCB_H__
#define __CARD_DDCB_H__

 

#include <linux/types.h>
#include <asm/byteorder.h>

#include "genwqe_driver.h"
#include "card_base.h"

 
#define ASIV_LENGTH		104  
#define ASIV_LENGTH_ATS		96   
#define ASV_LENGTH		64

struct ddcb {
	union {
		__be32 icrc_hsi_shi_32;	 
		struct {
			__be16	icrc_16;
			u8	hsi;
			u8	shi;
		};
	};
	u8  pre;		 
	u8  xdir;		 
	__be16 seqnum_16;	 

	u8  acfunc;		 
	u8  cmd;		 
	__be16 cmdopts_16;	 
	u8  sur;		 
	u8  psp;		 
	__be16 rsvd_0e_16;	 

	__be64 fwiv_64;		 

	union {
		struct {
			__be64 ats_64;   
			u8     asiv[ASIV_LENGTH_ATS];  
		} n;
		u8  __asiv[ASIV_LENGTH];	 
	};
	u8     asv[ASV_LENGTH];	 

	__be16 rsvd_c0_16;	 
	__be16 vcrc_16;		 
	__be32 rsvd_32;		 

	__be64 deque_ts_64;	 

	__be16 retc_16;		 
	__be16 attn_16;		 
	__be32 progress_32;	 

	__be64 cmplt_ts_64;	 

	 
	__be32 ibdc_32;		 
	__be32 obdc_32;		 

	__be64 rsvd_SLH_64;	 
	union {			 
		u8	priv[8];
		__be64	priv_64;
	};
	__be64 disp_ts_64;	 
} __attribute__((__packed__));

 
#define CRC16_POLYNOMIAL	0x1021

 
#define DDCB_SHI_INTR		0x04  
#define DDCB_SHI_PURGE		0x02  
#define DDCB_SHI_NEXT		0x01  

 
#define DDCB_HSI_COMPLETED	0x40  
#define DDCB_HSI_FETCHED	0x04  

 
#define DDCB_INTR_BE32		cpu_to_be32(0x00000004)
#define DDCB_PURGE_BE32		cpu_to_be32(0x00000002)
#define DDCB_NEXT_BE32		cpu_to_be32(0x00000001)
#define DDCB_COMPLETED_BE32	cpu_to_be32(0x00004000)
#define DDCB_FETCHED_BE32	cpu_to_be32(0x00000400)

 
#define DDCB_PRESET_PRE		0x80
#define ICRC_LENGTH(n)		((n) + 8 + 8 + 8)   
#define VCRC_LENGTH(n)		((n))		    

 

 
#define SG_CHAINED		(0x6)

 
#define SG_DATA			(0x2)

 
#define SG_END_LIST		(0x0)

 
struct sg_entry {
	__be64 target_addr;
	__be32 len;
	__be32 flags;
};

#endif  
