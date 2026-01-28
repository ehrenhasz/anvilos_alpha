#ifndef __SOUND_HDA_INTEL_H
#define __SOUND_HDA_INTEL_H
#include "hda_controller.h"
struct hda_intel {
	struct azx chip;
	struct work_struct irq_pending_work;
	struct completion probe_wait;
	struct delayed_work probe_work;
	struct list_head list;
	unsigned int irq_pending_warned:1;
	unsigned int probe_continued:1;
	unsigned int use_vga_switcheroo:1;
	unsigned int vga_switcheroo_registered:1;
	unsigned int init_failed:1;  
	unsigned int freed:1;  
	bool need_i915_power:1;  
	int probe_retry;	 
};
#endif
