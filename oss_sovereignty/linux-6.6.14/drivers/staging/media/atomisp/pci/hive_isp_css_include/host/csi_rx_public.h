 
 

#ifndef __CSI_RX_PUBLIC_H_INCLUDED__
#define __CSI_RX_PUBLIC_H_INCLUDED__

#ifdef ISP2401
 
 
void csi_rx_fe_ctrl_get_state(
    const csi_rx_frontend_ID_t ID,
    csi_rx_fe_ctrl_state_t *state);
 
void csi_rx_fe_ctrl_dump_state(
    const csi_rx_frontend_ID_t ID,
    csi_rx_fe_ctrl_state_t *state);
 
void csi_rx_fe_ctrl_get_dlane_state(
    const csi_rx_frontend_ID_t ID,
    const u32 lane,
    csi_rx_fe_ctrl_lane_t *dlane_state);
 
void csi_rx_be_ctrl_get_state(
    const csi_rx_backend_ID_t ID,
    csi_rx_be_ctrl_state_t *state);
 
void csi_rx_be_ctrl_dump_state(
    const csi_rx_backend_ID_t ID,
    csi_rx_be_ctrl_state_t *state);
 

 
 
hrt_data csi_rx_fe_ctrl_reg_load(
    const csi_rx_frontend_ID_t ID,
    const hrt_address reg);
 
void csi_rx_fe_ctrl_reg_store(
    const csi_rx_frontend_ID_t ID,
    const hrt_address reg,
    const hrt_data value);
 
hrt_data csi_rx_be_ctrl_reg_load(
    const csi_rx_backend_ID_t ID,
    const hrt_address reg);
 
void csi_rx_be_ctrl_reg_store(
    const csi_rx_backend_ID_t ID,
    const hrt_address reg,
    const hrt_data value);
 
#endif  
#endif  
