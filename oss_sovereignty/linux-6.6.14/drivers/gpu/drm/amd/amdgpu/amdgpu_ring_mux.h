 

#ifndef __AMDGPU_RING_MUX__
#define __AMDGPU_RING_MUX__

#include <linux/timer.h>
#include <linux/spinlock.h>
#include "amdgpu_ring.h"

struct amdgpu_ring;

 
struct amdgpu_mux_entry {
	struct amdgpu_ring      *ring;
	u64                     start_ptr_in_hw_ring;
	u64                     end_ptr_in_hw_ring;
	u64                     sw_cptr;
	u64                     sw_rptr;
	u64                     sw_wptr;
	struct list_head        list;
};

enum amdgpu_ring_mux_offset_type {
	AMDGPU_MUX_OFFSET_TYPE_CONTROL,
	AMDGPU_MUX_OFFSET_TYPE_DE,
	AMDGPU_MUX_OFFSET_TYPE_CE,
};

enum ib_complete_status {
	 
	IB_COMPLETION_STATUS_DEFAULT = 0,
	 
	IB_COMPLETION_STATUS_PREEMPTED = 1,
	 
	IB_COMPLETION_STATUS_COMPLETED = 2,
};

struct amdgpu_ring_mux {
	struct amdgpu_ring      *real_ring;

	struct amdgpu_mux_entry *ring_entry;
	unsigned int            num_ring_entries;
	unsigned int            ring_entry_size;
	 
	spinlock_t              lock;
	bool                    s_resubmit;
	uint32_t                seqno_to_resubmit;
	u64                     wptr_resubmit;
	struct timer_list       resubmit_timer;

	bool                    pending_trailing_fence_signaled;
};

 
struct amdgpu_mux_chunk {
	struct list_head        entry;
	uint32_t                sync_seq;
	u64                     start;
	u64                     end;
	u64                     cntl_offset;
	u64                     de_offset;
	u64                     ce_offset;
};

int amdgpu_ring_mux_init(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring,
			 unsigned int entry_size);
void amdgpu_ring_mux_fini(struct amdgpu_ring_mux *mux);
int amdgpu_ring_mux_add_sw_ring(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring);
void amdgpu_ring_mux_set_wptr(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring, u64 wptr);
u64 amdgpu_ring_mux_get_wptr(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring);
u64 amdgpu_ring_mux_get_rptr(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring);
void amdgpu_ring_mux_start_ib(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring);
void amdgpu_ring_mux_end_ib(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring);
void amdgpu_ring_mux_ib_mark_offset(struct amdgpu_ring_mux *mux, struct amdgpu_ring *ring,
				    u64 offset, enum amdgpu_ring_mux_offset_type type);
bool amdgpu_mcbp_handle_trailing_fence_irq(struct amdgpu_ring_mux *mux);

u64 amdgpu_sw_ring_get_rptr_gfx(struct amdgpu_ring *ring);
u64 amdgpu_sw_ring_get_wptr_gfx(struct amdgpu_ring *ring);
void amdgpu_sw_ring_set_wptr_gfx(struct amdgpu_ring *ring);
void amdgpu_sw_ring_insert_nop(struct amdgpu_ring *ring, uint32_t count);
void amdgpu_sw_ring_ib_begin(struct amdgpu_ring *ring);
void amdgpu_sw_ring_ib_end(struct amdgpu_ring *ring);
void amdgpu_sw_ring_ib_mark_offset(struct amdgpu_ring *ring, enum amdgpu_ring_mux_offset_type type);
const char *amdgpu_sw_ring_name(int idx);
unsigned int amdgpu_sw_ring_priority(int idx);

#endif
