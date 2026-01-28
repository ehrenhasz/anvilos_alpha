#ifndef __INPUT_SYSTEM_PUBLIC_H_INCLUDED__
#define __INPUT_SYSTEM_PUBLIC_H_INCLUDED__
#include <type_support.h>
#ifdef ISP2401
#include "isys_public.h"
#else
typedef struct input_system_state_s		input_system_state_t;
typedef struct receiver_state_s			receiver_state_t;
void input_system_get_state(
    const input_system_ID_t		ID,
    input_system_state_t		*state);
void receiver_get_state(
    const rx_ID_t				ID,
    receiver_state_t			*state);
bool is_mipi_format_yuv420(
    const mipi_format_t			mipi_format);
void receiver_set_compression(
    const rx_ID_t				ID,
    const unsigned int			cfg_ID,
    const mipi_compressor_t		comp,
    const mipi_predictor_t		pred);
void receiver_port_enable(
    const rx_ID_t				ID,
    const enum mipi_port_id		port_ID,
    const bool					cnd);
bool is_receiver_port_enabled(
    const rx_ID_t				ID,
    const enum mipi_port_id		port_ID);
void receiver_irq_enable(
    const rx_ID_t				ID,
    const enum mipi_port_id		port_ID,
    const rx_irq_info_t			irq_info);
rx_irq_info_t receiver_get_irq_info(
    const rx_ID_t				ID,
    const enum mipi_port_id		port_ID);
void receiver_irq_clear(
    const rx_ID_t				ID,
    const enum mipi_port_id			port_ID,
    const rx_irq_info_t			irq_info);
STORAGE_CLASS_INPUT_SYSTEM_H void input_system_reg_store(
    const input_system_ID_t			ID,
    const hrt_address			reg,
    const hrt_data				value);
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data input_system_reg_load(
    const input_system_ID_t			ID,
    const hrt_address			reg);
STORAGE_CLASS_INPUT_SYSTEM_H void receiver_reg_store(
    const rx_ID_t				ID,
    const hrt_address			reg,
    const hrt_data				value);
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data receiver_reg_load(
    const rx_ID_t				ID,
    const hrt_address			reg);
STORAGE_CLASS_INPUT_SYSTEM_H void receiver_port_reg_store(
    const rx_ID_t				ID,
    const enum mipi_port_id			port_ID,
    const hrt_address			reg,
    const hrt_data				value);
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data receiver_port_reg_load(
    const rx_ID_t				ID,
    const enum mipi_port_id		port_ID,
    const hrt_address			reg);
STORAGE_CLASS_INPUT_SYSTEM_H void input_system_sub_system_reg_store(
    const input_system_ID_t			ID,
    const sub_system_ID_t			sub_ID,
    const hrt_address			reg,
    const hrt_data				value);
STORAGE_CLASS_INPUT_SYSTEM_H hrt_data input_system_sub_system_reg_load(
    const input_system_ID_t		ID,
    const sub_system_ID_t		sub_ID,
    const hrt_address			reg);
input_system_err_t input_system_configuration_reset(void);
input_system_err_t input_system_configuration_commit(void);
input_system_err_t	input_system_csi_fifo_channel_cfg(
    u32				ch_id,
    input_system_csi_port_t	port,
    backend_channel_cfg_t	backend_ch,
    target_cfg2400_t			target
);
input_system_err_t	input_system_csi_fifo_channel_with_counting_cfg(
    u32				ch_id,
    u32				nof_frame,
    input_system_csi_port_t	port,
    backend_channel_cfg_t	backend_ch,
    u32				mem_region_size,
    u32				nof_mem_regions,
    target_cfg2400_t			target
);
input_system_err_t	input_system_csi_sram_channel_cfg(
    u32				ch_id,
    input_system_csi_port_t	port,
    backend_channel_cfg_t	backend_ch,
    u32				csi_mem_region_size,
    u32				csi_nof_mem_regions,
    target_cfg2400_t			target
);
input_system_err_t	input_system_csi_xmem_channel_cfg(
    u32				ch_id,
    input_system_csi_port_t port,
    backend_channel_cfg_t	backend_ch,
    u32				mem_region_size,
    u32				nof_mem_regions,
    u32				acq_mem_region_size,
    u32				acq_nof_mem_regions,
    target_cfg2400_t			target,
    uint32_t				nof_xmem_buffers
);
input_system_err_t	input_system_csi_xmem_capture_only_channel_cfg(
    u32				ch_id,
    u32				nof_frames,
    input_system_csi_port_t port,
    u32				csi_mem_region_size,
    u32				csi_nof_mem_regions,
    u32				acq_mem_region_size,
    u32				acq_nof_mem_regions,
    target_cfg2400_t			target
);
input_system_err_t	input_system_csi_xmem_acquire_only_channel_cfg(
    u32				ch_id,
    u32				nof_frames,
    input_system_csi_port_t port,
    backend_channel_cfg_t	backend_ch,
    u32				acq_mem_region_size,
    u32				acq_nof_mem_regions,
    target_cfg2400_t			target
);
input_system_err_t	input_system_prbs_channel_cfg(
    u32		ch_id,
    u32		nof_frames,
    u32		seed,
    u32		sync_gen_width,
    u32		sync_gen_height,
    u32		sync_gen_hblank_cycles,
    u32		sync_gen_vblank_cycles,
    target_cfg2400_t	target
);
input_system_err_t	input_system_tpg_channel_cfg(
    u32		ch_id,
    u32		nof_frames, 
    u32		x_mask,
    u32		y_mask,
    u32		x_delta,
    u32		y_delta,
    u32		xy_mask,
    u32		sync_gen_width,
    u32		sync_gen_height,
    u32		sync_gen_hblank_cycles,
    u32		sync_gen_vblank_cycles,
    target_cfg2400_t	target
);
input_system_err_t	input_system_gpfifo_channel_cfg(
    u32		ch_id,
    u32		nof_frames,
    target_cfg2400_t	target
);
#endif  
#endif  
