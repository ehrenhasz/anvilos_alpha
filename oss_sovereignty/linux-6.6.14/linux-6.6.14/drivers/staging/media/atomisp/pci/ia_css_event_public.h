#ifndef __IA_CSS_EVENT_PUBLIC_H
#define __IA_CSS_EVENT_PUBLIC_H
#include <type_support.h>	 
#include <ia_css_err.h>		 
#include <ia_css_types.h>	 
#include <ia_css_timer.h>	 
#include <linux/bits.h>
enum ia_css_event_type {
	IA_CSS_EVENT_TYPE_OUTPUT_FRAME_DONE		= BIT(0),
	IA_CSS_EVENT_TYPE_SECOND_OUTPUT_FRAME_DONE	= BIT(1),
	IA_CSS_EVENT_TYPE_VF_OUTPUT_FRAME_DONE		= BIT(2),
	IA_CSS_EVENT_TYPE_SECOND_VF_OUTPUT_FRAME_DONE	= BIT(3),
	IA_CSS_EVENT_TYPE_3A_STATISTICS_DONE		= BIT(4),
	IA_CSS_EVENT_TYPE_DIS_STATISTICS_DONE		= BIT(5),
	IA_CSS_EVENT_TYPE_PIPELINE_DONE			= BIT(6),
	IA_CSS_EVENT_TYPE_FRAME_TAGGED			= BIT(7),
	IA_CSS_EVENT_TYPE_INPUT_FRAME_DONE		= BIT(8),
	IA_CSS_EVENT_TYPE_METADATA_DONE			= BIT(9),
	IA_CSS_EVENT_TYPE_LACE_STATISTICS_DONE		= BIT(10),
	IA_CSS_EVENT_TYPE_ACC_STAGE_COMPLETE		= BIT(11),
	IA_CSS_EVENT_TYPE_TIMER				= BIT(12),
	IA_CSS_EVENT_TYPE_PORT_EOF			= BIT(13),
	IA_CSS_EVENT_TYPE_FW_WARNING			= BIT(14),
	IA_CSS_EVENT_TYPE_FW_ASSERT			= BIT(15),
};
#define IA_CSS_EVENT_TYPE_NONE 0
#define IA_CSS_EVENT_TYPE_ALL \
	(IA_CSS_EVENT_TYPE_OUTPUT_FRAME_DONE		| \
	 IA_CSS_EVENT_TYPE_SECOND_OUTPUT_FRAME_DONE	| \
	 IA_CSS_EVENT_TYPE_VF_OUTPUT_FRAME_DONE		| \
	 IA_CSS_EVENT_TYPE_SECOND_VF_OUTPUT_FRAME_DONE	| \
	 IA_CSS_EVENT_TYPE_3A_STATISTICS_DONE		| \
	 IA_CSS_EVENT_TYPE_DIS_STATISTICS_DONE		| \
	 IA_CSS_EVENT_TYPE_PIPELINE_DONE		| \
	 IA_CSS_EVENT_TYPE_FRAME_TAGGED			| \
	 IA_CSS_EVENT_TYPE_INPUT_FRAME_DONE		| \
	 IA_CSS_EVENT_TYPE_METADATA_DONE		| \
	 IA_CSS_EVENT_TYPE_LACE_STATISTICS_DONE		| \
	 IA_CSS_EVENT_TYPE_ACC_STAGE_COMPLETE)
struct ia_css_event {
	struct ia_css_pipe    *pipe;
	enum ia_css_event_type type;
	u8                port;
	u8                exp_id;
	u32               fw_handle;
	enum ia_css_fw_warning fw_warning;
	u8                fw_assert_module_id;
	u16               fw_assert_line_no;
	clock_value_t	       timer_data;
	u8                timer_code;
	u8                timer_subcode;
};
int
ia_css_dequeue_psys_event(struct ia_css_event *event);
int
ia_css_dequeue_isys_event(struct ia_css_event *event);
#endif  
