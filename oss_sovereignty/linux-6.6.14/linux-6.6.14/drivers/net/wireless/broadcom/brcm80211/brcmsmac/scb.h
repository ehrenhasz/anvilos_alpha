#ifndef _BRCM_SCB_H_
#define _BRCM_SCB_H_
#include <linux/if_ether.h>
#include <brcmu_utils.h>
#include <defs.h>
#include "types.h"
#define AMPDU_TX_BA_MAX_WSIZE	64	 
#define AMPDU_MAX_SCB_TID	NUMPRIO
#define SCB_WMECAP		0x0040
#define SCB_HTCAP		0x10000	 
#define SCB_IS40		0x80000	 
#define SCB_STBCCAP		0x40000000	 
#define SCB_MAGIC	0xbeefcafe
struct scb_ampdu_tid_ini {
	u8 txretry[AMPDU_TX_BA_MAX_WSIZE];
};
struct scb_ampdu {
	u8 max_pdu;		 
	u8 release;		 
	u32 max_rx_ampdu_bytes;	 
	struct scb_ampdu_tid_ini ini[AMPDU_MAX_SCB_TID];
};
struct scb {
	u32 magic;
	u32 flags;	 
	u16 seqctl[NUMPRIO];	 
	u16 seqnum[NUMPRIO]; 
	struct scb_ampdu scb_ampdu;	 
};
#endif				 
