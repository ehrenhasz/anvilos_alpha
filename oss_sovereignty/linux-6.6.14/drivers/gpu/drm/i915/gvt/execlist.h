 

#ifndef _GVT_EXECLIST_H_
#define _GVT_EXECLIST_H_

#include <linux/types.h>

struct execlist_ctx_descriptor_format {
	union {
		u32 ldw;
		struct {
			u32 valid                  : 1;
			u32 force_pd_restore       : 1;
			u32 force_restore          : 1;
			u32 addressing_mode        : 2;
			u32 llc_coherency          : 1;
			u32 fault_handling         : 2;
			u32 privilege_access       : 1;
			u32 reserved               : 3;
			u32 lrca                   : 20;
		};
	};
	union {
		u32 udw;
		u32 context_id;
	};
};

struct execlist_status_format {
	union {
		u32 ldw;
		struct {
			u32 current_execlist_pointer       :1;
			u32 execlist_write_pointer         :1;
			u32 execlist_queue_full            :1;
			u32 execlist_1_valid               :1;
			u32 execlist_0_valid               :1;
			u32 last_ctx_switch_reason         :9;
			u32 current_active_elm_status      :2;
			u32 arbitration_enable             :1;
			u32 execlist_1_active              :1;
			u32 execlist_0_active              :1;
			u32 reserved                       :13;
		};
	};
	union {
		u32 udw;
		u32 context_id;
	};
};

struct execlist_context_status_pointer_format {
	union {
		u32 dw;
		struct {
			u32 write_ptr              :3;
			u32 reserved               :5;
			u32 read_ptr               :3;
			u32 reserved2              :5;
			u32 mask                   :16;
		};
	};
};

struct execlist_context_status_format {
	union {
		u32 ldw;
		struct {
			u32 idle_to_active         :1;
			u32 preempted              :1;
			u32 element_switch         :1;
			u32 active_to_idle         :1;
			u32 context_complete       :1;
			u32 wait_on_sync_flip      :1;
			u32 wait_on_vblank         :1;
			u32 wait_on_semaphore      :1;
			u32 wait_on_scanline       :1;
			u32 reserved               :2;
			u32 semaphore_wait_mode    :1;
			u32 display_plane          :3;
			u32 lite_restore           :1;
			u32 reserved_2             :16;
		};
	};
	union {
		u32 udw;
		u32 context_id;
	};
};

struct execlist_mmio_pair {
	u32 addr;
	u32 val;
};

 
struct execlist_ring_context {
	u32 nop1;
	u32 lri_cmd_1;
	struct execlist_mmio_pair ctx_ctrl;
	struct execlist_mmio_pair ring_header;
	struct execlist_mmio_pair ring_tail;
	struct execlist_mmio_pair rb_start;
	struct execlist_mmio_pair rb_ctrl;
	struct execlist_mmio_pair bb_cur_head_UDW;
	struct execlist_mmio_pair bb_cur_head_LDW;
	struct execlist_mmio_pair bb_state;
	struct execlist_mmio_pair second_bb_addr_UDW;
	struct execlist_mmio_pair second_bb_addr_LDW;
	struct execlist_mmio_pair second_bb_state;
	struct execlist_mmio_pair bb_per_ctx_ptr;
	struct execlist_mmio_pair rcs_indirect_ctx;
	struct execlist_mmio_pair rcs_indirect_ctx_offset;
	u32 nop2;
	u32 nop3;
	u32 nop4;
	u32 lri_cmd_2;
	struct execlist_mmio_pair ctx_timestamp;
	 
	struct execlist_mmio_pair pdps[8];
};

struct intel_vgpu_elsp_dwords {
	u32 data[4];
	u32 index;
};

struct intel_vgpu_execlist_slot {
	struct execlist_ctx_descriptor_format ctx[2];
	u32 index;
};

struct intel_vgpu_execlist {
	struct intel_vgpu_execlist_slot slot[2];
	struct intel_vgpu_execlist_slot *running_slot;
	struct intel_vgpu_execlist_slot *pending_slot;
	struct execlist_ctx_descriptor_format *running_context;
	struct intel_vgpu *vgpu;
	struct intel_vgpu_elsp_dwords elsp_dwords;
	const struct intel_engine_cs *engine;
};

void intel_vgpu_clean_execlist(struct intel_vgpu *vgpu);

int intel_vgpu_init_execlist(struct intel_vgpu *vgpu);

int intel_vgpu_submit_execlist(struct intel_vgpu *vgpu,
			       const struct intel_engine_cs *engine);

#endif  
