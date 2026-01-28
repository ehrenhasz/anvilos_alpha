#ifndef __GP_DEVICE_LOCAL_H_INCLUDED__
#define __GP_DEVICE_LOCAL_H_INCLUDED__
#include "gp_device_global.h"
#define _REG_GP_SDRAM_WAKEUP_ADDR					0x00
#define _REG_GP_IDLE_ADDR							0x04
#define _REG_GP_SP_STREAM_STAT_ADDR					0x10
#define _REG_GP_SP_STREAM_STAT_B_ADDR				0x14
#define _REG_GP_ISP_STREAM_STAT_ADDR				0x18
#define _REG_GP_MOD_STREAM_STAT_ADDR				0x1C
#define _REG_GP_SP_STREAM_STAT_IRQ_COND_ADDR		0x20
#define _REG_GP_SP_STREAM_STAT_B_IRQ_COND_ADDR		0x24
#define _REG_GP_ISP_STREAM_STAT_IRQ_COND_ADDR		0x28
#define _REG_GP_MOD_STREAM_STAT_IRQ_COND_ADDR		0x2C
#define _REG_GP_SP_STREAM_STAT_IRQ_ENABLE_ADDR		0x30
#define _REG_GP_SP_STREAM_STAT_B_IRQ_ENABLE_ADDR	0x34
#define _REG_GP_ISP_STREAM_STAT_IRQ_ENABLE_ADDR		0x38
#define _REG_GP_MOD_STREAM_STAT_IRQ_ENABLE_ADDR		0x3C
#define _REG_GP_SLV_REG_RST_ADDR					0x50
#define _REG_GP_SWITCH_ISYS2401_ADDR				0x54
struct gp_device_state_s {
	int syncgen_enable;
	int syncgen_free_running;
	int syncgen_pause;
	int nr_frames;
	int syngen_nr_pix;
	int syngen_nr_lines;
	int syngen_hblank_cycles;
	int syngen_vblank_cycles;
	int isel_sof;
	int isel_eof;
	int isel_sol;
	int isel_eol;
	int isel_lfsr_enable;
	int isel_lfsr_enable_b;
	int isel_lfsr_reset_value;
	int isel_tpg_enable;
	int isel_tpg_enable_b;
	int isel_hor_cnt_mask;
	int isel_ver_cnt_mask;
	int isel_xy_cnt_mask;
	int isel_hor_cnt_delta;
	int isel_ver_cnt_delta;
	int isel_tpg_mode;
	int isel_tpg_red1;
	int isel_tpg_green1;
	int isel_tpg_blue1;
	int isel_tpg_red2;
	int isel_tpg_green2;
	int isel_tpg_blue2;
	int isel_ch_id;
	int isel_fmt_type;
	int isel_data_sel;
	int isel_sband_sel;
	int isel_sync_sel;
	int syncgen_hor_cnt;
	int syncgen_ver_cnt;
	int syncgen_frame_cnt;
	int soft_reset;
};
#endif  
