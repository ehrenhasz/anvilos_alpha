 
 

#ifndef __CSI_RX_RMGR_H_INCLUDED__
#define __CSI_RX_RMGR_H_INCLUDED__

typedef struct isys_csi_rx_rsrc_s isys_csi_rx_rsrc_t;
struct isys_csi_rx_rsrc_s {
	u32	active_table;
	u32        num_active;
	u16	num_long_packets;
	u16	num_short_packets;
};

#endif  
