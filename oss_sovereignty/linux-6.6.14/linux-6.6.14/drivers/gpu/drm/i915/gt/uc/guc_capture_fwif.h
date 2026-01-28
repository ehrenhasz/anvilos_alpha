#ifndef _INTEL_GUC_CAPTURE_FWIF_H
#define _INTEL_GUC_CAPTURE_FWIF_H
#include <linux/types.h>
#include "intel_guc_fwif.h"
struct intel_guc;
struct file;
struct __guc_capture_bufstate {
	u32 size;
	void *data;
	u32 rd;
	u32 wr;
};
struct __guc_capture_parsed_output {
	struct list_head link;
	bool is_partial;
	u32 eng_class;
	u32 eng_inst;
	u32 guc_id;
	u32 lrca;
	struct gcap_reg_list_info {
		u32 vfid;
		u32 num_regs;
		struct guc_mmio_reg *regs;
	} reginfo[GUC_CAPTURE_LIST_TYPE_MAX];
#define GCAP_PARSED_REGLIST_INDEX_GLOBAL   BIT(GUC_CAPTURE_LIST_TYPE_GLOBAL)
#define GCAP_PARSED_REGLIST_INDEX_ENGCLASS BIT(GUC_CAPTURE_LIST_TYPE_ENGINE_CLASS)
#define GCAP_PARSED_REGLIST_INDEX_ENGINST  BIT(GUC_CAPTURE_LIST_TYPE_ENGINE_INSTANCE)
};
struct guc_debug_capture_list_header {
	u32 info;
#define GUC_CAPTURELISTHDR_NUMDESCR GENMASK(15, 0)
} __packed;
struct guc_debug_capture_list {
	struct guc_debug_capture_list_header header;
	struct guc_mmio_reg regs[];
} __packed;
struct __guc_mmio_reg_descr {
	i915_reg_t reg;
	u32 flags;
	u32 mask;
	const char *regname;
};
struct __guc_mmio_reg_descr_group {
	const struct __guc_mmio_reg_descr *list;
	u32 num_regs;
	u32 owner;  
	u32 type;  
	u32 engine;  
	struct __guc_mmio_reg_descr *extlist;  
};
struct guc_state_capture_header_t {
	u32 owner;
#define CAP_HDR_CAPTURE_VFID GENMASK(7, 0)
	u32 info;
#define CAP_HDR_CAPTURE_TYPE GENMASK(3, 0)  
#define CAP_HDR_ENGINE_CLASS GENMASK(7, 4)  
#define CAP_HDR_ENGINE_INSTANCE GENMASK(11, 8)
	u32 lrca;  
	u32 guc_id;  
	u32 num_mmios;
#define CAP_HDR_NUM_MMIOS GENMASK(9, 0)
} __packed;
struct guc_state_capture_t {
	struct guc_state_capture_header_t header;
	struct guc_mmio_reg mmio_entries[];
} __packed;
enum guc_capture_group_types {
	GUC_STATE_CAPTURE_GROUP_TYPE_FULL,
	GUC_STATE_CAPTURE_GROUP_TYPE_PARTIAL,
	GUC_STATE_CAPTURE_GROUP_TYPE_MAX,
};
struct guc_state_capture_group_header_t {
	u32 owner;
#define CAP_GRP_HDR_CAPTURE_VFID GENMASK(7, 0)
	u32 info;
#define CAP_GRP_HDR_NUM_CAPTURES GENMASK(7, 0)
#define CAP_GRP_HDR_CAPTURE_TYPE GENMASK(15, 8)  
} __packed;
struct guc_state_capture_group_t {
	struct guc_state_capture_group_header_t grp_header;
	struct guc_state_capture_t capture_entries[];
} __packed;
struct __guc_capture_ads_cache {
	bool is_valid;
	void *ptr;
	size_t size;
	int status;
};
struct intel_guc_state_capture {
	const struct __guc_mmio_reg_descr_group *reglists;
	struct __guc_mmio_reg_descr_group *extlists;
	struct __guc_capture_ads_cache ads_cache[GUC_CAPTURE_LIST_INDEX_MAX]
						[GUC_CAPTURE_LIST_TYPE_MAX]
						[GUC_MAX_ENGINE_CLASSES];
	void *ads_null_cache;
	struct list_head cachelist;
#define PREALLOC_NODES_MAX_COUNT (3 * GUC_MAX_ENGINE_CLASSES * GUC_MAX_INSTANCES_PER_CLASS)
#define PREALLOC_NODES_DEFAULT_NUMREGS 64
	int max_mmio_per_node;
	struct list_head outlist;
};
#endif  
