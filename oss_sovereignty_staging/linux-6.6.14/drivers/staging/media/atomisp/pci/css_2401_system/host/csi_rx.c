
 

#include "system_global.h"
#include "csi_rx_global.h"

const u32 N_SHORT_PACKET_LUT_ENTRIES[N_CSI_RX_BACKEND_ID] = {
	4,	 
	4,	 
	4	 
};

const u32 N_LONG_PACKET_LUT_ENTRIES[N_CSI_RX_BACKEND_ID] = {
	8,	 
	4,	 
	4	 
};

const u32 N_CSI_RX_FE_CTRL_DLANES[N_CSI_RX_FRONTEND_ID] = {
	N_CSI_RX_DLANE_ID,	 
	N_CSI_RX_DLANE_ID,	 
	N_CSI_RX_DLANE_ID	 
};

 
const u32 N_CSI_RX_BE_SID_WIDTH[N_CSI_RX_BACKEND_ID] = {
	3,
	2,
	2
};
