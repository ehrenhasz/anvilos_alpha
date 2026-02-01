 

#ifndef _GVT_INTERRUPT_H_
#define _GVT_INTERRUPT_H_

#include <linux/hrtimer.h>
#include <linux/kernel.h>

#include "i915_reg_defs.h"

enum intel_gvt_event_type {
	RCS_MI_USER_INTERRUPT = 0,
	RCS_DEBUG,
	RCS_MMIO_SYNC_FLUSH,
	RCS_CMD_STREAMER_ERR,
	RCS_PIPE_CONTROL,
	RCS_L3_PARITY_ERR,
	RCS_WATCHDOG_EXCEEDED,
	RCS_PAGE_DIRECTORY_FAULT,
	RCS_AS_CONTEXT_SWITCH,
	RCS_MONITOR_BUFF_HALF_FULL,

	VCS_MI_USER_INTERRUPT,
	VCS_MMIO_SYNC_FLUSH,
	VCS_CMD_STREAMER_ERR,
	VCS_MI_FLUSH_DW,
	VCS_WATCHDOG_EXCEEDED,
	VCS_PAGE_DIRECTORY_FAULT,
	VCS_AS_CONTEXT_SWITCH,

	VCS2_MI_USER_INTERRUPT,
	VCS2_MI_FLUSH_DW,
	VCS2_AS_CONTEXT_SWITCH,

	BCS_MI_USER_INTERRUPT,
	BCS_MMIO_SYNC_FLUSH,
	BCS_CMD_STREAMER_ERR,
	BCS_MI_FLUSH_DW,
	BCS_PAGE_DIRECTORY_FAULT,
	BCS_AS_CONTEXT_SWITCH,

	VECS_MI_USER_INTERRUPT,
	VECS_MI_FLUSH_DW,
	VECS_AS_CONTEXT_SWITCH,

	PIPE_A_FIFO_UNDERRUN,
	PIPE_B_FIFO_UNDERRUN,
	PIPE_A_CRC_ERR,
	PIPE_B_CRC_ERR,
	PIPE_A_CRC_DONE,
	PIPE_B_CRC_DONE,
	PIPE_A_ODD_FIELD,
	PIPE_B_ODD_FIELD,
	PIPE_A_EVEN_FIELD,
	PIPE_B_EVEN_FIELD,
	PIPE_A_LINE_COMPARE,
	PIPE_B_LINE_COMPARE,
	PIPE_C_LINE_COMPARE,
	PIPE_A_VBLANK,
	PIPE_B_VBLANK,
	PIPE_C_VBLANK,
	PIPE_A_VSYNC,
	PIPE_B_VSYNC,
	PIPE_C_VSYNC,
	PRIMARY_A_FLIP_DONE,
	PRIMARY_B_FLIP_DONE,
	PRIMARY_C_FLIP_DONE,
	SPRITE_A_FLIP_DONE,
	SPRITE_B_FLIP_DONE,
	SPRITE_C_FLIP_DONE,

	PCU_THERMAL,
	PCU_PCODE2DRIVER_MAILBOX,

	DPST_PHASE_IN,
	DPST_HISTOGRAM,
	GSE,
	DP_A_HOTPLUG,
	AUX_CHANNEL_A,
	PERF_COUNTER,
	POISON,
	GTT_FAULT,
	ERROR_INTERRUPT_COMBINED,

	FDI_RX_INTERRUPTS_TRANSCODER_A,
	AUDIO_CP_CHANGE_TRANSCODER_A,
	AUDIO_CP_REQUEST_TRANSCODER_A,
	FDI_RX_INTERRUPTS_TRANSCODER_B,
	AUDIO_CP_CHANGE_TRANSCODER_B,
	AUDIO_CP_REQUEST_TRANSCODER_B,
	FDI_RX_INTERRUPTS_TRANSCODER_C,
	AUDIO_CP_CHANGE_TRANSCODER_C,
	AUDIO_CP_REQUEST_TRANSCODER_C,
	ERR_AND_DBG,
	GMBUS,
	SDVO_B_HOTPLUG,
	CRT_HOTPLUG,
	DP_B_HOTPLUG,
	DP_C_HOTPLUG,
	DP_D_HOTPLUG,
	AUX_CHANNEL_B,
	AUX_CHANNEL_C,
	AUX_CHANNEL_D,
	AUDIO_POWER_STATE_CHANGE_B,
	AUDIO_POWER_STATE_CHANGE_C,
	AUDIO_POWER_STATE_CHANGE_D,

	INTEL_GVT_EVENT_RESERVED,
	INTEL_GVT_EVENT_MAX,
};

struct intel_gvt_irq;
struct intel_gvt;
struct intel_vgpu;

typedef void (*gvt_event_virt_handler_t)(struct intel_gvt_irq *irq,
	enum intel_gvt_event_type event, struct intel_vgpu *vgpu);

struct intel_gvt_irq_ops {
	void (*init_irq)(struct intel_gvt_irq *irq);
	void (*check_pending_irq)(struct intel_vgpu *vgpu);
};

 
enum intel_gvt_irq_type {
	INTEL_GVT_IRQ_INFO_GT,
	INTEL_GVT_IRQ_INFO_DPY,
	INTEL_GVT_IRQ_INFO_PCH,
	INTEL_GVT_IRQ_INFO_PM,

	INTEL_GVT_IRQ_INFO_MASTER,
	INTEL_GVT_IRQ_INFO_GT0,
	INTEL_GVT_IRQ_INFO_GT1,
	INTEL_GVT_IRQ_INFO_GT2,
	INTEL_GVT_IRQ_INFO_GT3,
	INTEL_GVT_IRQ_INFO_DE_PIPE_A,
	INTEL_GVT_IRQ_INFO_DE_PIPE_B,
	INTEL_GVT_IRQ_INFO_DE_PIPE_C,
	INTEL_GVT_IRQ_INFO_DE_PORT,
	INTEL_GVT_IRQ_INFO_DE_MISC,
	INTEL_GVT_IRQ_INFO_AUD,
	INTEL_GVT_IRQ_INFO_PCU,

	INTEL_GVT_IRQ_INFO_MAX,
};

#define INTEL_GVT_IRQ_BITWIDTH	32

 
struct intel_gvt_irq_info {
	char *name;
	i915_reg_t reg_base;
	enum intel_gvt_event_type bit_to_event[INTEL_GVT_IRQ_BITWIDTH];
	unsigned long warned;
	int group;
	DECLARE_BITMAP(downstream_irq_bitmap, INTEL_GVT_IRQ_BITWIDTH);
	bool has_upstream_irq;
};

 
struct intel_gvt_event_info {
	int bit;				 
	int policy;				 
	struct intel_gvt_irq_info *info;	 
	gvt_event_virt_handler_t v_handler;	 
};

struct intel_gvt_irq_map {
	int up_irq_group;
	int up_irq_bit;
	int down_irq_group;
	u32 down_irq_bitmask;
};

 
struct intel_gvt_irq {
	const struct intel_gvt_irq_ops *ops;
	struct intel_gvt_irq_info *info[INTEL_GVT_IRQ_INFO_MAX];
	DECLARE_BITMAP(irq_info_bitmap, INTEL_GVT_IRQ_INFO_MAX);
	struct intel_gvt_event_info events[INTEL_GVT_EVENT_MAX];
	DECLARE_BITMAP(pending_events, INTEL_GVT_EVENT_MAX);
	struct intel_gvt_irq_map *irq_map;
};

int intel_gvt_init_irq(struct intel_gvt *gvt);

void intel_vgpu_trigger_virtual_event(struct intel_vgpu *vgpu,
	enum intel_gvt_event_type event);

int intel_vgpu_reg_iir_handler(struct intel_vgpu *vgpu, unsigned int reg,
	void *p_data, unsigned int bytes);
int intel_vgpu_reg_ier_handler(struct intel_vgpu *vgpu,
	unsigned int reg, void *p_data, unsigned int bytes);
int intel_vgpu_reg_master_irq_handler(struct intel_vgpu *vgpu,
	unsigned int reg, void *p_data, unsigned int bytes);
int intel_vgpu_reg_imr_handler(struct intel_vgpu *vgpu,
	unsigned int reg, void *p_data, unsigned int bytes);

int gvt_ring_id_to_pipe_control_notify_event(int ring_id);
int gvt_ring_id_to_mi_flush_dw_event(int ring_id);
int gvt_ring_id_to_mi_user_interrupt_event(int ring_id);

#endif  
