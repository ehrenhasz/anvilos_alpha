#ifndef _SH_CSS_SP_H_
#define _SH_CSS_SP_H_
#include <system_global.h>
#include <type_support.h>
#if !defined(ISP2401)
#include "input_formatter.h"
#endif
#include "ia_css_binary.h"
#include "ia_css_types.h"
#include "ia_css_pipeline.h"
void
sh_css_sp_store_init_dmem(const struct ia_css_fw_info *fw);
void
store_sp_stage_data(enum ia_css_pipe_id id, unsigned int pipe_num,
		    unsigned int stage);
void
sh_css_stage_write_binary_info(struct ia_css_binary_info *info);
void
store_sp_group_data(void);
void
sh_css_sp_start_binary_copy(unsigned int pipe_num,
			    struct ia_css_frame *out_frame,
			    unsigned int two_ppc);
unsigned int
sh_css_sp_get_binary_copy_size(void);
unsigned int
sh_css_sp_get_sw_interrupt_value(unsigned int irq);
void
sh_css_sp_init_pipeline(struct ia_css_pipeline *me,
			enum ia_css_pipe_id id,
			u8 pipe_num,
			bool xnr,
			bool two_ppc,
			bool continuous,
			bool offline,
			unsigned int required_bds_factor,
			enum sh_css_pipe_config_override copy_ovrd,
			enum ia_css_input_mode input_mode,
			const struct ia_css_metadata_config *md_config,
			const struct ia_css_metadata_info *md_info,
			const enum mipi_port_id port_id);
void
sh_css_sp_uninit_pipeline(unsigned int pipe_num);
bool sh_css_write_host2sp_command(enum host2sp_commands host2sp_command);
enum host2sp_commands
sh_css_read_host2sp_command(void);
void
sh_css_init_host2sp_frame_data(void);
void
sh_css_update_host2sp_offline_frame(
    unsigned int frame_num,
    struct ia_css_frame *frame,
    struct ia_css_metadata *metadata);
void
sh_css_update_host2sp_mipi_frame(
    unsigned int frame_num,
    struct ia_css_frame *frame);
void
sh_css_update_host2sp_mipi_metadata(
    unsigned int frame_num,
    struct ia_css_metadata *metadata);
void
sh_css_update_host2sp_num_mipi_frames(unsigned int num_frames);
void
sh_css_update_host2sp_cont_num_raw_frames(unsigned int num_frames,
	bool set_avail);
void
sh_css_event_init_irq_mask(void);
void
sh_css_sp_start_isp(void);
void
sh_css_sp_set_sp_running(bool flag);
bool
sh_css_sp_is_running(void);
#if SP_DEBUG != SP_DEBUG_NONE
void
sh_css_sp_get_debug_state(struct sh_css_sp_debug_state *state);
#endif
#if !defined(ISP2401)
void
sh_css_sp_set_if_configs(
    const input_formatter_cfg_t	*config_a,
    const input_formatter_cfg_t	*config_b,
    const uint8_t		if_config_index);
#endif
void
sh_css_sp_program_input_circuit(int fmt_type,
				int ch_id,
				enum ia_css_input_mode input_mode);
void
sh_css_sp_configure_sync_gen(int width,
			     int height,
			     int hblank_cycles,
			     int vblank_cycles);
void
sh_css_sp_configure_tpg(int x_mask,
			int y_mask,
			int x_delta,
			int y_delta,
			int xy_mask);
void
sh_css_sp_configure_prbs(int seed);
void
sh_css_sp_configure_enable_raw_pool_locking(bool lock_all);
void
sh_css_sp_enable_isys_event_queue(bool enable);
void
sh_css_sp_set_disable_continuous_viewfinder(bool flag);
void
sh_css_sp_reset_global_vars(void);
bool
sh_css_sp_init_dma_sw_reg(int dma_id);
bool
sh_css_sp_set_dma_sw_reg(int dma_id,
			 int channel_id,
			 int request_type,
			 bool enable);
extern struct sh_css_sp_group sh_css_sp_group;
extern struct sh_css_sp_stage sh_css_sp_stage;
extern struct sh_css_isp_stage sh_css_isp_stage;
#endif  
