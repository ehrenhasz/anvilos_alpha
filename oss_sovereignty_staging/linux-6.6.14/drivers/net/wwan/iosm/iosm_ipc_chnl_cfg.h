 

#ifndef IOSM_IPC_CHNL_CFG_H
#define IOSM_IPC_CHNL_CFG_H

#include "iosm_ipc_mux.h"

 
#define IPC_MEM_TDS_TRC 32

 
#define IPC_MEM_MAX_DL_TRC_BUF_SIZE 8192

 
enum ipc_channel_id {
	IPC_MEM_IP_CHL_ID_0 = 0,
	IPC_MEM_CTRL_CHL_ID_1,
	IPC_MEM_CTRL_CHL_ID_2,
	IPC_MEM_CTRL_CHL_ID_3,
	IPC_MEM_CTRL_CHL_ID_4,
	IPC_MEM_CTRL_CHL_ID_5,
	IPC_MEM_CTRL_CHL_ID_6,
	IPC_MEM_CTRL_CHL_ID_7,
};

 
struct ipc_chnl_cfg {
	u32 id;
	u32 ul_pipe;
	u32 dl_pipe;
	u32 ul_nr_of_entries;
	u32 dl_nr_of_entries;
	u32 dl_buf_size;
	u32 wwan_port_type;
	u32 accumulation_backoff;
};

 
int ipc_chnl_cfg_get(struct ipc_chnl_cfg *chnl_cfg, int index);

#endif
