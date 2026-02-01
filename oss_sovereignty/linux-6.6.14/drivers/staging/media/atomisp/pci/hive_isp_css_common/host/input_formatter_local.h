 
 

#ifndef __INPUT_FORMATTER_LOCAL_H_INCLUDED__
#define __INPUT_FORMATTER_LOCAL_H_INCLUDED__

#include "input_formatter_global.h"

#include "isp.h"		 

typedef struct input_formatter_switch_state_s	input_formatter_switch_state_t;
typedef struct input_formatter_state_s			input_formatter_state_t;
typedef struct input_formatter_bin_state_s		input_formatter_bin_state_t;

#define HIVE_IF_FSM_SYNC_STATUS                 0x100
#define HIVE_IF_FSM_SYNC_COUNTER                0x104
#define HIVE_IF_FSM_DEINTERLEAVING_IDX          0x114
#define HIVE_IF_FSM_DECIMATION_H_COUNTER        0x118
#define HIVE_IF_FSM_DECIMATION_V_COUNTER        0x11C
#define HIVE_IF_FSM_DECIMATION_BLOCK_V_COUNTER  0x120
#define HIVE_IF_FSM_PADDING_STATUS              0x124
#define HIVE_IF_FSM_PADDING_ELEMENT_COUNTER     0x128
#define HIVE_IF_FSM_VECTOR_SUPPORT_ERROR        0x12C
#define HIVE_IF_FSM_VECTOR_SUPPORT_BUFF_FULL    0x130
#define HIVE_IF_FSM_VECTOR_SUPPORT              0x134
#define HIVE_IF_FIFO_SENSOR_STATUS              0x138

 
struct input_formatter_switch_state_s {
	int	if_input_switch_lut_reg[8];
	int	if_input_switch_fsync_lut;
	int	if_input_switch_ch_id_fmt_type;
	bool if_input_switch_map[HIVE_SWITCH_N_CHANNELS][HIVE_SWITCH_N_FORMATTYPES];
};

struct input_formatter_state_s {
	 
	int	start_line;
	int	start_column;
	int	cropped_height;
	int	cropped_width;
	int	ver_decimation;
	int	hor_decimation;
	int	ver_deinterleaving;
	int	hor_deinterleaving;
	int	left_padding;
	int	eol_offset;
	int	vmem_start_address;
	int	vmem_end_address;
	int	vmem_increment;
	int	is_yuv420;
	int	vsync_active_low;
	int	hsync_active_low;
	int	allow_fifo_overflow;
	int block_fifo_when_no_req;
	int	fsm_sync_status;
	int	fsm_sync_counter;
	int	fsm_crop_status;
	int	fsm_crop_line_counter;
	int	fsm_crop_pixel_counter;
	int	fsm_deinterleaving_index;
	int	fsm_dec_h_counter;
	int	fsm_dec_v_counter;
	int	fsm_dec_block_v_counter;
	int	fsm_padding_status;
	int	fsm_padding_elem_counter;
	int	fsm_vector_support_error;
	int	fsm_vector_buffer_full;
	int	vector_support;
	int	sensor_data_lost;
};

struct input_formatter_bin_state_s {
	u32	reset;
	u32	input_endianness;
	u32	output_endianness;
	u32	bitswap;
	u32	block_synch;
	u32	packet_synch;
	u32	readpostwrite_synch;
	u32	is_2ppc;
	u32	en_status_update;
};

#endif  
