 
 

#ifndef _IXGBE_MBX_H_
#define _IXGBE_MBX_H_

#include "ixgbe_type.h"

#define IXGBE_VFMAILBOX_SIZE        16  
#define IXGBE_ERR_MBX               -100

#define IXGBE_VFMAILBOX             0x002FC
#define IXGBE_VFMBMEM               0x00200

#define IXGBE_PFMAILBOX_STS   0x00000001  
#define IXGBE_PFMAILBOX_ACK   0x00000002  
#define IXGBE_PFMAILBOX_VFU   0x00000004  
#define IXGBE_PFMAILBOX_PFU   0x00000008  
#define IXGBE_PFMAILBOX_RVFU  0x00000010  

#define IXGBE_MBVFICR_VFREQ_MASK 0x0000FFFF  
#define IXGBE_MBVFICR_VFREQ_VF1  0x00000001  
#define IXGBE_MBVFICR_VFACK_MASK 0xFFFF0000  
#define IXGBE_MBVFICR_VFACK_VF1  0x00010000  


 
#define IXGBE_VT_MSGTYPE_ACK      0x80000000   
#define IXGBE_VT_MSGTYPE_NACK     0x40000000   
#define IXGBE_VT_MSGTYPE_CTS      0x20000000   
#define IXGBE_VT_MSGINFO_SHIFT    16
 
#define IXGBE_VT_MSGINFO_MASK     (0xFF << IXGBE_VT_MSGINFO_SHIFT)

 

 
enum ixgbe_pfvf_api_rev {
	ixgbe_mbox_api_10,	 
	ixgbe_mbox_api_20,	 
	ixgbe_mbox_api_11,	 
	ixgbe_mbox_api_12,	 
	ixgbe_mbox_api_13,	 
	ixgbe_mbox_api_14,	 
	 
	ixgbe_mbox_api_unknown,	 
};

 
#define IXGBE_VF_RESET            0x01  
#define IXGBE_VF_SET_MAC_ADDR     0x02  
#define IXGBE_VF_SET_MULTICAST    0x03  
#define IXGBE_VF_SET_VLAN         0x04  

 
#define IXGBE_VF_SET_LPE	0x05  
#define IXGBE_VF_SET_MACVLAN	0x06  
#define IXGBE_VF_API_NEGOTIATE	0x08  

 
#define IXGBE_VF_GET_QUEUES	0x09  

 
#define IXGBE_VF_TX_QUEUES	1	 
#define IXGBE_VF_RX_QUEUES	2	 
#define IXGBE_VF_TRANS_VLAN	3	 
#define IXGBE_VF_DEF_QUEUE	4	 

 
#define IXGBE_VF_GET_RETA	0x0a	 
#define IXGBE_VF_GET_RSS_KEY	0x0b	 

#define IXGBE_VF_UPDATE_XCAST_MODE	0x0c

 
#define IXGBE_VF_IPSEC_ADD	0x0d
#define IXGBE_VF_IPSEC_DEL	0x0e

#define IXGBE_VF_GET_LINK_STATE 0x10  

 
#define IXGBE_VF_PERMADDR_MSG_LEN 4
 
#define IXGBE_VF_MC_TYPE_WORD     3

#define IXGBE_PF_CONTROL_MSG      0x0100  

#define IXGBE_VF_MBX_INIT_TIMEOUT 2000  
#define IXGBE_VF_MBX_INIT_DELAY   500   

s32 ixgbe_read_mbx(struct ixgbe_hw *, u32 *, u16, u16);
s32 ixgbe_write_mbx(struct ixgbe_hw *, u32 *, u16, u16);
s32 ixgbe_check_for_msg(struct ixgbe_hw *, u16);
s32 ixgbe_check_for_ack(struct ixgbe_hw *, u16);
s32 ixgbe_check_for_rst(struct ixgbe_hw *, u16);
#ifdef CONFIG_PCI_IOV
void ixgbe_init_mbx_params_pf(struct ixgbe_hw *);
#endif  

extern const struct ixgbe_mbx_operations mbx_ops_generic;

#endif  
