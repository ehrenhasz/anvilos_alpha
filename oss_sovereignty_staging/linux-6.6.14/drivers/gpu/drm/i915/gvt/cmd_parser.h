 
#ifndef _GVT_CMD_PARSER_H_
#define _GVT_CMD_PARSER_H_

#define GVT_CMD_HASH_BITS 7

struct intel_gvt;
struct intel_shadow_wa_ctx;
struct intel_vgpu;
struct intel_vgpu_workload;

void intel_gvt_clean_cmd_parser(struct intel_gvt *gvt);

int intel_gvt_init_cmd_parser(struct intel_gvt *gvt);

int intel_gvt_scan_and_shadow_ringbuffer(struct intel_vgpu_workload *workload);

int intel_gvt_scan_and_shadow_wa_ctx(struct intel_shadow_wa_ctx *wa_ctx);

void intel_gvt_update_reg_whitelist(struct intel_vgpu *vgpu);

int intel_gvt_scan_engine_context(struct intel_vgpu_workload *workload);

#endif
