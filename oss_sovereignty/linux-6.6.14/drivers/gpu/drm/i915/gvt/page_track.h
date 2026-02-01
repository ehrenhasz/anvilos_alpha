 

#ifndef _GVT_PAGE_TRACK_H_
#define _GVT_PAGE_TRACK_H_

#include <linux/types.h>

struct intel_vgpu;
struct intel_vgpu_page_track;

typedef int (*gvt_page_track_handler_t)(
			struct intel_vgpu_page_track *page_track,
			u64 gpa, void *data, int bytes);

 
struct intel_vgpu_page_track {
	gvt_page_track_handler_t handler;
	bool tracked;
	void *priv_data;
};

struct intel_vgpu_page_track *intel_vgpu_find_page_track(
		struct intel_vgpu *vgpu, unsigned long gfn);

int intel_vgpu_register_page_track(struct intel_vgpu *vgpu,
		unsigned long gfn, gvt_page_track_handler_t handler,
		void *priv);
void intel_vgpu_unregister_page_track(struct intel_vgpu *vgpu,
		unsigned long gfn);

int intel_vgpu_enable_page_track(struct intel_vgpu *vgpu, unsigned long gfn);
int intel_vgpu_disable_page_track(struct intel_vgpu *vgpu, unsigned long gfn);

int intel_vgpu_page_track_handler(struct intel_vgpu *vgpu, u64 gpa,
		void *data, unsigned int bytes);

#endif
