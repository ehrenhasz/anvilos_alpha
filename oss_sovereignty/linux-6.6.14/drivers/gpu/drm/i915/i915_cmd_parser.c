 

#include <linux/highmem.h>

#include <drm/drm_cache.h>

#include "gt/intel_engine.h"
#include "gt/intel_engine_regs.h"
#include "gt/intel_gpu_commands.h"
#include "gt/intel_gt_regs.h"

#include "i915_cmd_parser.h"
#include "i915_drv.h"
#include "i915_memcpy.h"
#include "i915_reg.h"

 

 
struct drm_i915_cmd_descriptor {
	 
	u32 flags;
#define CMD_DESC_FIXED    (1<<0)
#define CMD_DESC_SKIP     (1<<1)
#define CMD_DESC_REJECT   (1<<2)
#define CMD_DESC_REGISTER (1<<3)
#define CMD_DESC_BITMASK  (1<<4)

	 
	struct {
		u32 value;
		u32 mask;
	} cmd;

	 
	union {
		u32 fixed;
		u32 mask;
	} length;

	 
	struct {
		u32 offset;
		u32 mask;
		u32 step;
	} reg;

#define MAX_CMD_DESC_BITMASKS 3
	 
	struct {
		u32 offset;
		u32 mask;
		u32 expected;
		u32 condition_offset;
		u32 condition_mask;
	} bits[MAX_CMD_DESC_BITMASKS];
};

 
struct drm_i915_cmd_table {
	const struct drm_i915_cmd_descriptor *table;
	int count;
};

#define STD_MI_OPCODE_SHIFT  (32 - 9)
#define STD_3D_OPCODE_SHIFT  (32 - 16)
#define STD_2D_OPCODE_SHIFT  (32 - 10)
#define STD_MFX_OPCODE_SHIFT (32 - 16)
#define MIN_OPCODE_SHIFT 16

#define CMD(op, opm, f, lm, fl, ...)				\
	{							\
		.flags = (fl) | ((f) ? CMD_DESC_FIXED : 0),	\
		.cmd = { (op & ~0u << (opm)), ~0u << (opm) },	\
		.length = { (lm) },				\
		__VA_ARGS__					\
	}

 
#define SMI STD_MI_OPCODE_SHIFT
#define S3D STD_3D_OPCODE_SHIFT
#define S2D STD_2D_OPCODE_SHIFT
#define SMFX STD_MFX_OPCODE_SHIFT
#define F true
#define S CMD_DESC_SKIP
#define R CMD_DESC_REJECT
#define W CMD_DESC_REGISTER
#define B CMD_DESC_BITMASK

 
static const struct drm_i915_cmd_descriptor gen7_common_cmds[] = {
	CMD(  MI_NOOP,                          SMI,    F,  1,      S  ),
	CMD(  MI_USER_INTERRUPT,                SMI,    F,  1,      R  ),
	CMD(  MI_WAIT_FOR_EVENT,                SMI,    F,  1,      R  ),
	CMD(  MI_ARB_CHECK,                     SMI,    F,  1,      S  ),
	CMD(  MI_REPORT_HEAD,                   SMI,    F,  1,      S  ),
	CMD(  MI_SUSPEND_FLUSH,                 SMI,    F,  1,      S  ),
	CMD(  MI_SEMAPHORE_MBOX,                SMI,   !F,  0xFF,   R  ),
	CMD(  MI_STORE_DWORD_INDEX,             SMI,   !F,  0xFF,   R  ),
	CMD(  MI_LOAD_REGISTER_IMM(1),          SMI,   !F,  0xFF,   W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC, .step = 2 }    ),
	CMD(  MI_STORE_REGISTER_MEM,            SMI,    F,  3,     W | B,
	      .reg = { .offset = 1, .mask = 0x007FFFFC },
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_LOAD_REGISTER_MEM,             SMI,    F,  3,     W | B,
	      .reg = { .offset = 1, .mask = 0x007FFFFC },
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	 
	CMD(  MI_BATCH_BUFFER_START,            SMI,   !F,  0xFF,   S  ),
};

static const struct drm_i915_cmd_descriptor gen7_render_cmds[] = {
	CMD(  MI_FLUSH,                         SMI,    F,  1,      S  ),
	CMD(  MI_ARB_ON_OFF,                    SMI,    F,  1,      R  ),
	CMD(  MI_PREDICATE,                     SMI,    F,  1,      S  ),
	CMD(  MI_TOPOLOGY_FILTER,               SMI,    F,  1,      S  ),
	CMD(  MI_SET_APPID,                     SMI,    F,  1,      S  ),
	CMD(  MI_DISPLAY_FLIP,                  SMI,   !F,  0xFF,   R  ),
	CMD(  MI_SET_CONTEXT,                   SMI,   !F,  0xFF,   R  ),
	CMD(  MI_URB_CLEAR,                     SMI,   !F,  0xFF,   S  ),
	CMD(  MI_STORE_DWORD_IMM,               SMI,   !F,  0x3F,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_UPDATE_GTT,                    SMI,   !F,  0xFF,   R  ),
	CMD(  MI_CLFLUSH,                       SMI,   !F,  0x3FF,  B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_REPORT_PERF_COUNT,             SMI,   !F,  0x3F,   B,
	      .bits = {{
			.offset = 1,
			.mask = MI_REPORT_PERF_COUNT_GGTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_CONDITIONAL_BATCH_BUFFER_END,  SMI,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  GFX_OP_3DSTATE_VF_STATISTICS,     S3D,    F,  1,      S  ),
	CMD(  PIPELINE_SELECT,                  S3D,    F,  1,      S  ),
	CMD(  MEDIA_VFE_STATE,			S3D,   !F,  0xFFFF, B,
	      .bits = {{
			.offset = 2,
			.mask = MEDIA_VFE_STATE_MMIO_ACCESS_MASK,
			.expected = 0,
	      }},						       ),
	CMD(  GPGPU_OBJECT,                     S3D,   !F,  0xFF,   S  ),
	CMD(  GPGPU_WALKER,                     S3D,   !F,  0xFF,   S  ),
	CMD(  GFX_OP_3DSTATE_SO_DECL_LIST,      S3D,   !F,  0x1FF,  S  ),
	CMD(  GFX_OP_PIPE_CONTROL(5),           S3D,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 1,
			.mask = (PIPE_CONTROL_MMIO_WRITE | PIPE_CONTROL_NOTIFY),
			.expected = 0,
	      },
	      {
			.offset = 1,
		        .mask = (PIPE_CONTROL_GLOBAL_GTT_IVB |
				 PIPE_CONTROL_STORE_DATA_INDEX),
			.expected = 0,
			.condition_offset = 1,
			.condition_mask = PIPE_CONTROL_POST_SYNC_OP_MASK,
	      }},						       ),
};

static const struct drm_i915_cmd_descriptor hsw_render_cmds[] = {
	CMD(  MI_SET_PREDICATE,                 SMI,    F,  1,      S  ),
	CMD(  MI_RS_CONTROL,                    SMI,    F,  1,      S  ),
	CMD(  MI_URB_ATOMIC_ALLOC,              SMI,    F,  1,      S  ),
	CMD(  MI_SET_APPID,                     SMI,    F,  1,      S  ),
	CMD(  MI_RS_CONTEXT,                    SMI,    F,  1,      S  ),
	CMD(  MI_LOAD_SCAN_LINES_INCL,          SMI,   !F,  0x3F,   R  ),
	CMD(  MI_LOAD_SCAN_LINES_EXCL,          SMI,   !F,  0x3F,   R  ),
	CMD(  MI_LOAD_REGISTER_REG,             SMI,   !F,  0xFF,   W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC, .step = 1 }    ),
	CMD(  MI_RS_STORE_DATA_IMM,             SMI,   !F,  0xFF,   S  ),
	CMD(  MI_LOAD_URB_MEM,                  SMI,   !F,  0xFF,   S  ),
	CMD(  MI_STORE_URB_MEM,                 SMI,   !F,  0xFF,   S  ),
	CMD(  GFX_OP_3DSTATE_DX9_CONSTANTF_VS,  S3D,   !F,  0x7FF,  S  ),
	CMD(  GFX_OP_3DSTATE_DX9_CONSTANTF_PS,  S3D,   !F,  0x7FF,  S  ),

	CMD(  GFX_OP_3DSTATE_BINDING_TABLE_EDIT_VS,  S3D,   !F,  0x1FF,  S  ),
	CMD(  GFX_OP_3DSTATE_BINDING_TABLE_EDIT_GS,  S3D,   !F,  0x1FF,  S  ),
	CMD(  GFX_OP_3DSTATE_BINDING_TABLE_EDIT_HS,  S3D,   !F,  0x1FF,  S  ),
	CMD(  GFX_OP_3DSTATE_BINDING_TABLE_EDIT_DS,  S3D,   !F,  0x1FF,  S  ),
	CMD(  GFX_OP_3DSTATE_BINDING_TABLE_EDIT_PS,  S3D,   !F,  0x1FF,  S  ),
};

static const struct drm_i915_cmd_descriptor gen7_video_cmds[] = {
	CMD(  MI_ARB_ON_OFF,                    SMI,    F,  1,      R  ),
	CMD(  MI_SET_APPID,                     SMI,    F,  1,      S  ),
	CMD(  MI_STORE_DWORD_IMM,               SMI,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_UPDATE_GTT,                    SMI,   !F,  0x3F,   R  ),
	CMD(  MI_FLUSH_DW,                      SMI,   !F,  0x3F,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_FLUSH_DW_NOTIFY,
			.expected = 0,
	      },
	      {
			.offset = 1,
			.mask = MI_FLUSH_DW_USE_GTT,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      },
	      {
			.offset = 0,
			.mask = MI_FLUSH_DW_STORE_INDEX,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      }},						       ),
	CMD(  MI_CONDITIONAL_BATCH_BUFFER_END,  SMI,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	 
	CMD(  MFX_WAIT,                         SMFX,   F,  1,      S  ),
};

static const struct drm_i915_cmd_descriptor gen7_vecs_cmds[] = {
	CMD(  MI_ARB_ON_OFF,                    SMI,    F,  1,      R  ),
	CMD(  MI_SET_APPID,                     SMI,    F,  1,      S  ),
	CMD(  MI_STORE_DWORD_IMM,               SMI,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_UPDATE_GTT,                    SMI,   !F,  0x3F,   R  ),
	CMD(  MI_FLUSH_DW,                      SMI,   !F,  0x3F,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_FLUSH_DW_NOTIFY,
			.expected = 0,
	      },
	      {
			.offset = 1,
			.mask = MI_FLUSH_DW_USE_GTT,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      },
	      {
			.offset = 0,
			.mask = MI_FLUSH_DW_STORE_INDEX,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      }},						       ),
	CMD(  MI_CONDITIONAL_BATCH_BUFFER_END,  SMI,   !F,  0xFF,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
};

static const struct drm_i915_cmd_descriptor gen7_blt_cmds[] = {
	CMD(  MI_DISPLAY_FLIP,                  SMI,   !F,  0xFF,   R  ),
	CMD(  MI_STORE_DWORD_IMM,               SMI,   !F,  0x3FF,  B,
	      .bits = {{
			.offset = 0,
			.mask = MI_GLOBAL_GTT,
			.expected = 0,
	      }},						       ),
	CMD(  MI_UPDATE_GTT,                    SMI,   !F,  0x3F,   R  ),
	CMD(  MI_FLUSH_DW,                      SMI,   !F,  0x3F,   B,
	      .bits = {{
			.offset = 0,
			.mask = MI_FLUSH_DW_NOTIFY,
			.expected = 0,
	      },
	      {
			.offset = 1,
			.mask = MI_FLUSH_DW_USE_GTT,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      },
	      {
			.offset = 0,
			.mask = MI_FLUSH_DW_STORE_INDEX,
			.expected = 0,
			.condition_offset = 0,
			.condition_mask = MI_FLUSH_DW_OP_MASK,
	      }},						       ),
	CMD(  COLOR_BLT,                        S2D,   !F,  0x3F,   S  ),
	CMD(  SRC_COPY_BLT,                     S2D,   !F,  0x3F,   S  ),
};

static const struct drm_i915_cmd_descriptor hsw_blt_cmds[] = {
	CMD(  MI_LOAD_SCAN_LINES_INCL,          SMI,   !F,  0x3F,   R  ),
	CMD(  MI_LOAD_SCAN_LINES_EXCL,          SMI,   !F,  0x3F,   R  ),
};

 
static const struct drm_i915_cmd_descriptor gen9_blt_cmds[] = {
	CMD(  MI_NOOP,                          SMI,    F,  1,      S  ),
	CMD(  MI_USER_INTERRUPT,                SMI,    F,  1,      S  ),
	CMD(  MI_WAIT_FOR_EVENT,                SMI,    F,  1,      S  ),
	CMD(  MI_FLUSH,                         SMI,    F,  1,      S  ),
	CMD(  MI_ARB_CHECK,                     SMI,    F,  1,      S  ),
	CMD(  MI_REPORT_HEAD,                   SMI,    F,  1,      S  ),
	CMD(  MI_ARB_ON_OFF,                    SMI,    F,  1,      S  ),
	CMD(  MI_SUSPEND_FLUSH,                 SMI,    F,  1,      S  ),
	CMD(  MI_LOAD_SCAN_LINES_INCL,          SMI,   !F,  0x3F,   S  ),
	CMD(  MI_LOAD_SCAN_LINES_EXCL,          SMI,   !F,  0x3F,   S  ),
	CMD(  MI_STORE_DWORD_IMM,               SMI,   !F,  0x3FF,  S  ),
	CMD(  MI_LOAD_REGISTER_IMM(1),          SMI,   !F,  0xFF,   W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC, .step = 2 }    ),
	CMD(  MI_UPDATE_GTT,                    SMI,   !F,  0x3FF,  S  ),
	CMD(  MI_STORE_REGISTER_MEM_GEN8,       SMI,    F,  4,      W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC }               ),
	CMD(  MI_FLUSH_DW,                      SMI,   !F,  0x3F,   S  ),
	CMD(  MI_LOAD_REGISTER_MEM_GEN8,        SMI,    F,  4,      W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC }               ),
	CMD(  MI_LOAD_REGISTER_REG,             SMI,    !F,  0xFF,  W,
	      .reg = { .offset = 1, .mask = 0x007FFFFC, .step = 1 }    ),

	 
#define MI_BB_START_OPERAND_MASK   GENMASK(SMI-1, 0)
#define MI_BB_START_OPERAND_EXPECT (MI_BATCH_PPGTT_HSW | 1)
	CMD(  MI_BATCH_BUFFER_START_GEN8,       SMI,    !F,  0xFF,  B,
	      .bits = {{
			.offset = 0,
			.mask = MI_BB_START_OPERAND_MASK,
			.expected = MI_BB_START_OPERAND_EXPECT,
	      }},						       ),
};

static const struct drm_i915_cmd_descriptor noop_desc =
	CMD(MI_NOOP, SMI, F, 1, S);

#undef CMD
#undef SMI
#undef S3D
#undef S2D
#undef SMFX
#undef F
#undef S
#undef R
#undef W
#undef B

static const struct drm_i915_cmd_table gen7_render_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_render_cmds, ARRAY_SIZE(gen7_render_cmds) },
};

static const struct drm_i915_cmd_table hsw_render_ring_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_render_cmds, ARRAY_SIZE(gen7_render_cmds) },
	{ hsw_render_cmds, ARRAY_SIZE(hsw_render_cmds) },
};

static const struct drm_i915_cmd_table gen7_video_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_video_cmds, ARRAY_SIZE(gen7_video_cmds) },
};

static const struct drm_i915_cmd_table hsw_vebox_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_vecs_cmds, ARRAY_SIZE(gen7_vecs_cmds) },
};

static const struct drm_i915_cmd_table gen7_blt_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_blt_cmds, ARRAY_SIZE(gen7_blt_cmds) },
};

static const struct drm_i915_cmd_table hsw_blt_ring_cmd_table[] = {
	{ gen7_common_cmds, ARRAY_SIZE(gen7_common_cmds) },
	{ gen7_blt_cmds, ARRAY_SIZE(gen7_blt_cmds) },
	{ hsw_blt_cmds, ARRAY_SIZE(hsw_blt_cmds) },
};

static const struct drm_i915_cmd_table gen9_blt_cmd_table[] = {
	{ gen9_blt_cmds, ARRAY_SIZE(gen9_blt_cmds) },
};


 

 
struct drm_i915_reg_descriptor {
	i915_reg_t addr;
	u32 mask;
	u32 value;
};

 
#define REG32(_reg, ...) \
	{ .addr = (_reg), __VA_ARGS__ }

#define REG32_IDX(_reg, idx) \
	{ .addr = _reg(idx) }

 
#define REG64(_reg) \
	{ .addr = _reg }, \
	{ .addr = _reg ## _UDW }

#define REG64_IDX(_reg, idx) \
	{ .addr = _reg(idx) }, \
	{ .addr = _reg ## _UDW(idx) }

#define REG64_BASE_IDX(_reg, base, idx) \
	{ .addr = _reg(base, idx) }, \
	{ .addr = _reg ## _UDW(base, idx) }

static const struct drm_i915_reg_descriptor gen7_render_regs[] = {
	REG64(GPGPU_THREADS_DISPATCHED),
	REG64(HS_INVOCATION_COUNT),
	REG64(DS_INVOCATION_COUNT),
	REG64(IA_VERTICES_COUNT),
	REG64(IA_PRIMITIVES_COUNT),
	REG64(VS_INVOCATION_COUNT),
	REG64(GS_INVOCATION_COUNT),
	REG64(GS_PRIMITIVES_COUNT),
	REG64(CL_INVOCATION_COUNT),
	REG64(CL_PRIMITIVES_COUNT),
	REG64(PS_INVOCATION_COUNT),
	REG64(PS_DEPTH_COUNT),
	REG64_IDX(RING_TIMESTAMP, RENDER_RING_BASE),
	REG64_IDX(MI_PREDICATE_SRC0, RENDER_RING_BASE),
	REG64_IDX(MI_PREDICATE_SRC1, RENDER_RING_BASE),
	REG32(GEN7_3DPRIM_END_OFFSET),
	REG32(GEN7_3DPRIM_START_VERTEX),
	REG32(GEN7_3DPRIM_VERTEX_COUNT),
	REG32(GEN7_3DPRIM_INSTANCE_COUNT),
	REG32(GEN7_3DPRIM_START_INSTANCE),
	REG32(GEN7_3DPRIM_BASE_VERTEX),
	REG32(GEN7_GPGPU_DISPATCHDIMX),
	REG32(GEN7_GPGPU_DISPATCHDIMY),
	REG32(GEN7_GPGPU_DISPATCHDIMZ),
	REG64_IDX(RING_TIMESTAMP, BSD_RING_BASE),
	REG64_IDX(GEN7_SO_NUM_PRIMS_WRITTEN, 0),
	REG64_IDX(GEN7_SO_NUM_PRIMS_WRITTEN, 1),
	REG64_IDX(GEN7_SO_NUM_PRIMS_WRITTEN, 2),
	REG64_IDX(GEN7_SO_NUM_PRIMS_WRITTEN, 3),
	REG64_IDX(GEN7_SO_PRIM_STORAGE_NEEDED, 0),
	REG64_IDX(GEN7_SO_PRIM_STORAGE_NEEDED, 1),
	REG64_IDX(GEN7_SO_PRIM_STORAGE_NEEDED, 2),
	REG64_IDX(GEN7_SO_PRIM_STORAGE_NEEDED, 3),
	REG32(GEN7_SO_WRITE_OFFSET(0)),
	REG32(GEN7_SO_WRITE_OFFSET(1)),
	REG32(GEN7_SO_WRITE_OFFSET(2)),
	REG32(GEN7_SO_WRITE_OFFSET(3)),
	REG32(GEN7_L3SQCREG1),
	REG32(GEN7_L3CNTLREG2),
	REG32(GEN7_L3CNTLREG3),
	REG64_IDX(RING_TIMESTAMP, BLT_RING_BASE),
};

static const struct drm_i915_reg_descriptor hsw_render_regs[] = {
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 0),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 1),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 2),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 3),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 4),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 5),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 6),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 7),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 8),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 9),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 10),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 11),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 12),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 13),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 14),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, RENDER_RING_BASE, 15),
	REG32(HSW_SCRATCH1,
	      .mask = ~HSW_SCRATCH1_L3_DATA_ATOMICS_DISABLE,
	      .value = 0),
	REG32(HSW_ROW_CHICKEN3,
	      .mask = ~(HSW_ROW_CHICKEN3_L3_GLOBAL_ATOMICS_DISABLE << 16 |
                        HSW_ROW_CHICKEN3_L3_GLOBAL_ATOMICS_DISABLE),
	      .value = 0),
};

static const struct drm_i915_reg_descriptor gen7_blt_regs[] = {
	REG64_IDX(RING_TIMESTAMP, RENDER_RING_BASE),
	REG64_IDX(RING_TIMESTAMP, BSD_RING_BASE),
	REG32(BCS_SWCTRL),
	REG64_IDX(RING_TIMESTAMP, BLT_RING_BASE),
};

static const struct drm_i915_reg_descriptor gen9_blt_regs[] = {
	REG64_IDX(RING_TIMESTAMP, RENDER_RING_BASE),
	REG64_IDX(RING_TIMESTAMP, BSD_RING_BASE),
	REG32(BCS_SWCTRL),
	REG64_IDX(RING_TIMESTAMP, BLT_RING_BASE),
	REG32_IDX(RING_CTX_TIMESTAMP, BLT_RING_BASE),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 0),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 1),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 2),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 3),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 4),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 5),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 6),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 7),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 8),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 9),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 10),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 11),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 12),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 13),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 14),
	REG64_BASE_IDX(GEN8_RING_CS_GPR, BLT_RING_BASE, 15),
};

#undef REG64
#undef REG32

struct drm_i915_reg_table {
	const struct drm_i915_reg_descriptor *regs;
	int num_regs;
};

static const struct drm_i915_reg_table ivb_render_reg_tables[] = {
	{ gen7_render_regs, ARRAY_SIZE(gen7_render_regs) },
};

static const struct drm_i915_reg_table ivb_blt_reg_tables[] = {
	{ gen7_blt_regs, ARRAY_SIZE(gen7_blt_regs) },
};

static const struct drm_i915_reg_table hsw_render_reg_tables[] = {
	{ gen7_render_regs, ARRAY_SIZE(gen7_render_regs) },
	{ hsw_render_regs, ARRAY_SIZE(hsw_render_regs) },
};

static const struct drm_i915_reg_table hsw_blt_reg_tables[] = {
	{ gen7_blt_regs, ARRAY_SIZE(gen7_blt_regs) },
};

static const struct drm_i915_reg_table gen9_blt_reg_tables[] = {
	{ gen9_blt_regs, ARRAY_SIZE(gen9_blt_regs) },
};

static u32 gen7_render_get_cmd_length_mask(u32 cmd_header)
{
	u32 client = cmd_header >> INSTR_CLIENT_SHIFT;
	u32 subclient =
		(cmd_header & INSTR_SUBCLIENT_MASK) >> INSTR_SUBCLIENT_SHIFT;

	if (client == INSTR_MI_CLIENT)
		return 0x3F;
	else if (client == INSTR_RC_CLIENT) {
		if (subclient == INSTR_MEDIA_SUBCLIENT)
			return 0xFFFF;
		else
			return 0xFF;
	}

	DRM_DEBUG("CMD: Abnormal rcs cmd length! 0x%08X\n", cmd_header);
	return 0;
}

static u32 gen7_bsd_get_cmd_length_mask(u32 cmd_header)
{
	u32 client = cmd_header >> INSTR_CLIENT_SHIFT;
	u32 subclient =
		(cmd_header & INSTR_SUBCLIENT_MASK) >> INSTR_SUBCLIENT_SHIFT;
	u32 op = (cmd_header & INSTR_26_TO_24_MASK) >> INSTR_26_TO_24_SHIFT;

	if (client == INSTR_MI_CLIENT)
		return 0x3F;
	else if (client == INSTR_RC_CLIENT) {
		if (subclient == INSTR_MEDIA_SUBCLIENT) {
			if (op == 6)
				return 0xFFFF;
			else
				return 0xFFF;
		} else
			return 0xFF;
	}

	DRM_DEBUG("CMD: Abnormal bsd cmd length! 0x%08X\n", cmd_header);
	return 0;
}

static u32 gen7_blt_get_cmd_length_mask(u32 cmd_header)
{
	u32 client = cmd_header >> INSTR_CLIENT_SHIFT;

	if (client == INSTR_MI_CLIENT)
		return 0x3F;
	else if (client == INSTR_BC_CLIENT)
		return 0xFF;

	DRM_DEBUG("CMD: Abnormal blt cmd length! 0x%08X\n", cmd_header);
	return 0;
}

static u32 gen9_blt_get_cmd_length_mask(u32 cmd_header)
{
	u32 client = cmd_header >> INSTR_CLIENT_SHIFT;

	if (client == INSTR_MI_CLIENT || client == INSTR_BC_CLIENT)
		return 0xFF;

	DRM_DEBUG("CMD: Abnormal blt cmd length! 0x%08X\n", cmd_header);
	return 0;
}

static bool validate_cmds_sorted(const struct intel_engine_cs *engine,
				 const struct drm_i915_cmd_table *cmd_tables,
				 int cmd_table_count)
{
	int i;
	bool ret = true;

	if (!cmd_tables || cmd_table_count == 0)
		return true;

	for (i = 0; i < cmd_table_count; i++) {
		const struct drm_i915_cmd_table *table = &cmd_tables[i];
		u32 previous = 0;
		int j;

		for (j = 0; j < table->count; j++) {
			const struct drm_i915_cmd_descriptor *desc =
				&table->table[j];
			u32 curr = desc->cmd.value & desc->cmd.mask;

			if (curr < previous) {
				drm_err(&engine->i915->drm,
					"CMD: %s [%d] command table not sorted: "
					"table=%d entry=%d cmd=0x%08X prev=0x%08X\n",
					engine->name, engine->id,
					i, j, curr, previous);
				ret = false;
			}

			previous = curr;
		}
	}

	return ret;
}

static bool check_sorted(const struct intel_engine_cs *engine,
			 const struct drm_i915_reg_descriptor *reg_table,
			 int reg_count)
{
	int i;
	u32 previous = 0;
	bool ret = true;

	for (i = 0; i < reg_count; i++) {
		u32 curr = i915_mmio_reg_offset(reg_table[i].addr);

		if (curr < previous) {
			drm_err(&engine->i915->drm,
				"CMD: %s [%d] register table not sorted: "
				"entry=%d reg=0x%08X prev=0x%08X\n",
				engine->name, engine->id,
				i, curr, previous);
			ret = false;
		}

		previous = curr;
	}

	return ret;
}

static bool validate_regs_sorted(struct intel_engine_cs *engine)
{
	int i;
	const struct drm_i915_reg_table *table;

	for (i = 0; i < engine->reg_table_count; i++) {
		table = &engine->reg_tables[i];
		if (!check_sorted(engine, table->regs, table->num_regs))
			return false;
	}

	return true;
}

struct cmd_node {
	const struct drm_i915_cmd_descriptor *desc;
	struct hlist_node node;
};

 
static inline u32 cmd_header_key(u32 x)
{
	switch (x >> INSTR_CLIENT_SHIFT) {
	default:
	case INSTR_MI_CLIENT:
		return x >> STD_MI_OPCODE_SHIFT;
	case INSTR_RC_CLIENT:
		return x >> STD_3D_OPCODE_SHIFT;
	case INSTR_BC_CLIENT:
		return x >> STD_2D_OPCODE_SHIFT;
	}
}

static int init_hash_table(struct intel_engine_cs *engine,
			   const struct drm_i915_cmd_table *cmd_tables,
			   int cmd_table_count)
{
	int i, j;

	hash_init(engine->cmd_hash);

	for (i = 0; i < cmd_table_count; i++) {
		const struct drm_i915_cmd_table *table = &cmd_tables[i];

		for (j = 0; j < table->count; j++) {
			const struct drm_i915_cmd_descriptor *desc =
				&table->table[j];
			struct cmd_node *desc_node =
				kmalloc(sizeof(*desc_node), GFP_KERNEL);

			if (!desc_node)
				return -ENOMEM;

			desc_node->desc = desc;
			hash_add(engine->cmd_hash, &desc_node->node,
				 cmd_header_key(desc->cmd.value));
		}
	}

	return 0;
}

static void fini_hash_table(struct intel_engine_cs *engine)
{
	struct hlist_node *tmp;
	struct cmd_node *desc_node;
	int i;

	hash_for_each_safe(engine->cmd_hash, i, tmp, desc_node, node) {
		hash_del(&desc_node->node);
		kfree(desc_node);
	}
}

 
int intel_engine_init_cmd_parser(struct intel_engine_cs *engine)
{
	const struct drm_i915_cmd_table *cmd_tables;
	int cmd_table_count;
	int ret;

	if (GRAPHICS_VER(engine->i915) != 7 && !(GRAPHICS_VER(engine->i915) == 9 &&
						 engine->class == COPY_ENGINE_CLASS))
		return 0;

	switch (engine->class) {
	case RENDER_CLASS:
		if (IS_HASWELL(engine->i915)) {
			cmd_tables = hsw_render_ring_cmd_table;
			cmd_table_count =
				ARRAY_SIZE(hsw_render_ring_cmd_table);
		} else {
			cmd_tables = gen7_render_cmd_table;
			cmd_table_count = ARRAY_SIZE(gen7_render_cmd_table);
		}

		if (IS_HASWELL(engine->i915)) {
			engine->reg_tables = hsw_render_reg_tables;
			engine->reg_table_count = ARRAY_SIZE(hsw_render_reg_tables);
		} else {
			engine->reg_tables = ivb_render_reg_tables;
			engine->reg_table_count = ARRAY_SIZE(ivb_render_reg_tables);
		}
		engine->get_cmd_length_mask = gen7_render_get_cmd_length_mask;
		break;
	case VIDEO_DECODE_CLASS:
		cmd_tables = gen7_video_cmd_table;
		cmd_table_count = ARRAY_SIZE(gen7_video_cmd_table);
		engine->get_cmd_length_mask = gen7_bsd_get_cmd_length_mask;
		break;
	case COPY_ENGINE_CLASS:
		engine->get_cmd_length_mask = gen7_blt_get_cmd_length_mask;
		if (GRAPHICS_VER(engine->i915) == 9) {
			cmd_tables = gen9_blt_cmd_table;
			cmd_table_count = ARRAY_SIZE(gen9_blt_cmd_table);
			engine->get_cmd_length_mask =
				gen9_blt_get_cmd_length_mask;

			 
			engine->flags |= I915_ENGINE_REQUIRES_CMD_PARSER;
		} else if (IS_HASWELL(engine->i915)) {
			cmd_tables = hsw_blt_ring_cmd_table;
			cmd_table_count = ARRAY_SIZE(hsw_blt_ring_cmd_table);
		} else {
			cmd_tables = gen7_blt_cmd_table;
			cmd_table_count = ARRAY_SIZE(gen7_blt_cmd_table);
		}

		if (GRAPHICS_VER(engine->i915) == 9) {
			engine->reg_tables = gen9_blt_reg_tables;
			engine->reg_table_count =
				ARRAY_SIZE(gen9_blt_reg_tables);
		} else if (IS_HASWELL(engine->i915)) {
			engine->reg_tables = hsw_blt_reg_tables;
			engine->reg_table_count = ARRAY_SIZE(hsw_blt_reg_tables);
		} else {
			engine->reg_tables = ivb_blt_reg_tables;
			engine->reg_table_count = ARRAY_SIZE(ivb_blt_reg_tables);
		}
		break;
	case VIDEO_ENHANCEMENT_CLASS:
		cmd_tables = hsw_vebox_cmd_table;
		cmd_table_count = ARRAY_SIZE(hsw_vebox_cmd_table);
		 
		engine->get_cmd_length_mask = gen7_bsd_get_cmd_length_mask;
		break;
	default:
		MISSING_CASE(engine->class);
		goto out;
	}

	if (!validate_cmds_sorted(engine, cmd_tables, cmd_table_count)) {
		drm_err(&engine->i915->drm,
			"%s: command descriptions are not sorted\n",
			engine->name);
		goto out;
	}
	if (!validate_regs_sorted(engine)) {
		drm_err(&engine->i915->drm,
			"%s: registers are not sorted\n", engine->name);
		goto out;
	}

	ret = init_hash_table(engine, cmd_tables, cmd_table_count);
	if (ret) {
		drm_err(&engine->i915->drm,
			"%s: initialised failed!\n", engine->name);
		fini_hash_table(engine);
		goto out;
	}

	engine->flags |= I915_ENGINE_USING_CMD_PARSER;

out:
	if (intel_engine_requires_cmd_parser(engine) &&
	    !intel_engine_using_cmd_parser(engine))
		return -EINVAL;

	return 0;
}

 
void intel_engine_cleanup_cmd_parser(struct intel_engine_cs *engine)
{
	if (!intel_engine_using_cmd_parser(engine))
		return;

	fini_hash_table(engine);
}

static const struct drm_i915_cmd_descriptor*
find_cmd_in_table(struct intel_engine_cs *engine,
		  u32 cmd_header)
{
	struct cmd_node *desc_node;

	hash_for_each_possible(engine->cmd_hash, desc_node, node,
			       cmd_header_key(cmd_header)) {
		const struct drm_i915_cmd_descriptor *desc = desc_node->desc;
		if (((cmd_header ^ desc->cmd.value) & desc->cmd.mask) == 0)
			return desc;
	}

	return NULL;
}

 
static const struct drm_i915_cmd_descriptor*
find_cmd(struct intel_engine_cs *engine,
	 u32 cmd_header,
	 const struct drm_i915_cmd_descriptor *desc,
	 struct drm_i915_cmd_descriptor *default_desc)
{
	u32 mask;

	if (((cmd_header ^ desc->cmd.value) & desc->cmd.mask) == 0)
		return desc;

	desc = find_cmd_in_table(engine, cmd_header);
	if (desc)
		return desc;

	mask = engine->get_cmd_length_mask(cmd_header);
	if (!mask)
		return NULL;

	default_desc->cmd.value = cmd_header;
	default_desc->cmd.mask = ~0u << MIN_OPCODE_SHIFT;
	default_desc->length.mask = mask;
	default_desc->flags = CMD_DESC_SKIP;
	return default_desc;
}

static const struct drm_i915_reg_descriptor *
__find_reg(const struct drm_i915_reg_descriptor *table, int count, u32 addr)
{
	int start = 0, end = count;
	while (start < end) {
		int mid = start + (end - start) / 2;
		int ret = addr - i915_mmio_reg_offset(table[mid].addr);
		if (ret < 0)
			end = mid;
		else if (ret > 0)
			start = mid + 1;
		else
			return &table[mid];
	}
	return NULL;
}

static const struct drm_i915_reg_descriptor *
find_reg(const struct intel_engine_cs *engine, u32 addr)
{
	const struct drm_i915_reg_table *table = engine->reg_tables;
	const struct drm_i915_reg_descriptor *reg = NULL;
	int count = engine->reg_table_count;

	for (; !reg && (count > 0); ++table, --count)
		reg = __find_reg(table->regs, table->num_regs, addr);

	return reg;
}

 
static u32 *copy_batch(struct drm_i915_gem_object *dst_obj,
		       struct drm_i915_gem_object *src_obj,
		       unsigned long offset, unsigned long length,
		       bool *needs_clflush_after)
{
	unsigned int src_needs_clflush;
	unsigned int dst_needs_clflush;
	void *dst, *src;
	int ret;

	ret = i915_gem_object_prepare_write(dst_obj, &dst_needs_clflush);
	if (ret)
		return ERR_PTR(ret);

	dst = i915_gem_object_pin_map(dst_obj, I915_MAP_WB);
	i915_gem_object_finish_access(dst_obj);
	if (IS_ERR(dst))
		return dst;

	ret = i915_gem_object_prepare_read(src_obj, &src_needs_clflush);
	if (ret) {
		i915_gem_object_unpin_map(dst_obj);
		return ERR_PTR(ret);
	}

	src = ERR_PTR(-ENODEV);
	if (src_needs_clflush && i915_has_memcpy_from_wc()) {
		src = i915_gem_object_pin_map(src_obj, I915_MAP_WC);
		if (!IS_ERR(src)) {
			i915_unaligned_memcpy_from_wc(dst,
						      src + offset,
						      length);
			i915_gem_object_unpin_map(src_obj);
		}
	}
	if (IS_ERR(src)) {
		unsigned long x, n, remain;
		void *ptr;

		 
		remain = length;
		if (dst_needs_clflush & CLFLUSH_BEFORE)
			remain = round_up(remain,
					  boot_cpu_data.x86_clflush_size);

		ptr = dst;
		x = offset_in_page(offset);
		for (n = offset >> PAGE_SHIFT; remain; n++) {
			int len = min(remain, PAGE_SIZE - x);

			src = kmap_atomic(i915_gem_object_get_page(src_obj, n));
			if (src_needs_clflush)
				drm_clflush_virt_range(src + x, len);
			memcpy(ptr, src + x, len);
			kunmap_atomic(src);

			ptr += len;
			remain -= len;
			x = 0;
		}
	}

	i915_gem_object_finish_access(src_obj);

	memset32(dst + length, 0, (dst_obj->base.size - length) / sizeof(u32));

	 
	*needs_clflush_after = dst_needs_clflush & CLFLUSH_AFTER;

	return dst;
}

static inline bool cmd_desc_is(const struct drm_i915_cmd_descriptor * const desc,
			       const u32 cmd)
{
	return desc->cmd.value == (cmd & desc->cmd.mask);
}

static bool check_cmd(const struct intel_engine_cs *engine,
		      const struct drm_i915_cmd_descriptor *desc,
		      const u32 *cmd, u32 length)
{
	if (desc->flags & CMD_DESC_SKIP)
		return true;

	if (desc->flags & CMD_DESC_REJECT) {
		DRM_DEBUG("CMD: Rejected command: 0x%08X\n", *cmd);
		return false;
	}

	if (desc->flags & CMD_DESC_REGISTER) {
		 
		const u32 step = desc->reg.step ? desc->reg.step : length;
		u32 offset;

		for (offset = desc->reg.offset; offset < length;
		     offset += step) {
			const u32 reg_addr = cmd[offset] & desc->reg.mask;
			const struct drm_i915_reg_descriptor *reg =
				find_reg(engine, reg_addr);

			if (!reg) {
				DRM_DEBUG("CMD: Rejected register 0x%08X in command: 0x%08X (%s)\n",
					  reg_addr, *cmd, engine->name);
				return false;
			}

			 
			if (reg->mask) {
				if (cmd_desc_is(desc, MI_LOAD_REGISTER_MEM)) {
					DRM_DEBUG("CMD: Rejected LRM to masked register 0x%08X\n",
						  reg_addr);
					return false;
				}

				if (cmd_desc_is(desc, MI_LOAD_REGISTER_REG)) {
					DRM_DEBUG("CMD: Rejected LRR to masked register 0x%08X\n",
						  reg_addr);
					return false;
				}

				if (cmd_desc_is(desc, MI_LOAD_REGISTER_IMM(1)) &&
				    (offset + 2 > length ||
				     (cmd[offset + 1] & reg->mask) != reg->value)) {
					DRM_DEBUG("CMD: Rejected LRI to masked register 0x%08X\n",
						  reg_addr);
					return false;
				}
			}
		}
	}

	if (desc->flags & CMD_DESC_BITMASK) {
		int i;

		for (i = 0; i < MAX_CMD_DESC_BITMASKS; i++) {
			u32 dword;

			if (desc->bits[i].mask == 0)
				break;

			if (desc->bits[i].condition_mask != 0) {
				u32 offset =
					desc->bits[i].condition_offset;
				u32 condition = cmd[offset] &
					desc->bits[i].condition_mask;

				if (condition == 0)
					continue;
			}

			if (desc->bits[i].offset >= length) {
				DRM_DEBUG("CMD: Rejected command 0x%08X, too short to check bitmask (%s)\n",
					  *cmd, engine->name);
				return false;
			}

			dword = cmd[desc->bits[i].offset] &
				desc->bits[i].mask;

			if (dword != desc->bits[i].expected) {
				DRM_DEBUG("CMD: Rejected command 0x%08X for bitmask 0x%08X (exp=0x%08X act=0x%08X) (%s)\n",
					  *cmd,
					  desc->bits[i].mask,
					  desc->bits[i].expected,
					  dword, engine->name);
				return false;
			}
		}
	}

	return true;
}

static int check_bbstart(u32 *cmd, u32 offset, u32 length,
			 u32 batch_length,
			 u64 batch_addr,
			 u64 shadow_addr,
			 const unsigned long *jump_whitelist)
{
	u64 jump_offset, jump_target;
	u32 target_cmd_offset, target_cmd_index;

	 
	if (!jump_whitelist) {
		DRM_DEBUG("CMD: Rejecting BB_START for ggtt based submission\n");
		return -EACCES;
	}

	if (length != 3) {
		DRM_DEBUG("CMD: Recursive BB_START with bad length(%u)\n",
			  length);
		return -EINVAL;
	}

	jump_target = *(u64 *)(cmd + 1);
	jump_offset = jump_target - batch_addr;

	 
	if (jump_offset >= batch_length) {
		DRM_DEBUG("CMD: BB_START to 0x%llx jumps out of BB\n",
			  jump_target);
		return -EINVAL;
	}

	 
	target_cmd_offset = lower_32_bits(jump_offset);
	target_cmd_index = target_cmd_offset / sizeof(u32);

	*(u64 *)(cmd + 1) = shadow_addr + target_cmd_offset;

	if (target_cmd_index == offset)
		return 0;

	if (IS_ERR(jump_whitelist))
		return PTR_ERR(jump_whitelist);

	if (!test_bit(target_cmd_index, jump_whitelist)) {
		DRM_DEBUG("CMD: BB_START to 0x%llx not a previously executed cmd\n",
			  jump_target);
		return -EINVAL;
	}

	return 0;
}

static unsigned long *alloc_whitelist(u32 batch_length)
{
	unsigned long *jmp;

	 

	 
	jmp = bitmap_zalloc(DIV_ROUND_UP(batch_length, sizeof(u32)),
			    GFP_KERNEL | __GFP_RETRY_MAYFAIL | __GFP_NOWARN);
	if (!jmp)
		return ERR_PTR(-ENOMEM);

	return jmp;
}

#define LENGTH_BIAS 2

 

int intel_engine_cmd_parser(struct intel_engine_cs *engine,
			    struct i915_vma *batch,
			    unsigned long batch_offset,
			    unsigned long batch_length,
			    struct i915_vma *shadow,
			    bool trampoline)
{
	u32 *cmd, *batch_end, offset = 0;
	struct drm_i915_cmd_descriptor default_desc = noop_desc;
	const struct drm_i915_cmd_descriptor *desc = &default_desc;
	bool needs_clflush_after = false;
	unsigned long *jump_whitelist;
	u64 batch_addr, shadow_addr;
	int ret = 0;

	GEM_BUG_ON(!IS_ALIGNED(batch_offset, sizeof(*cmd)));
	GEM_BUG_ON(!IS_ALIGNED(batch_length, sizeof(*cmd)));
	GEM_BUG_ON(range_overflows_t(u64, batch_offset, batch_length,
				     batch->size));
	GEM_BUG_ON(!batch_length);

	cmd = copy_batch(shadow->obj, batch->obj,
			 batch_offset, batch_length,
			 &needs_clflush_after);
	if (IS_ERR(cmd)) {
		DRM_DEBUG("CMD: Failed to copy batch\n");
		return PTR_ERR(cmd);
	}

	jump_whitelist = NULL;
	if (!trampoline)
		 
		jump_whitelist = alloc_whitelist(batch_length);

	shadow_addr = gen8_canonical_addr(i915_vma_offset(shadow));
	batch_addr = gen8_canonical_addr(i915_vma_offset(batch) + batch_offset);

	 
	batch_end = cmd + batch_length / sizeof(*batch_end);
	do {
		u32 length;

		if (*cmd == MI_BATCH_BUFFER_END)
			break;

		desc = find_cmd(engine, *cmd, desc, &default_desc);
		if (!desc) {
			DRM_DEBUG("CMD: Unrecognized command: 0x%08X\n", *cmd);
			ret = -EINVAL;
			break;
		}

		if (desc->flags & CMD_DESC_FIXED)
			length = desc->length.fixed;
		else
			length = (*cmd & desc->length.mask) + LENGTH_BIAS;

		if ((batch_end - cmd) < length) {
			DRM_DEBUG("CMD: Command length exceeds batch length: 0x%08X length=%u batchlen=%td\n",
				  *cmd,
				  length,
				  batch_end - cmd);
			ret = -EINVAL;
			break;
		}

		if (!check_cmd(engine, desc, cmd, length)) {
			ret = -EACCES;
			break;
		}

		if (cmd_desc_is(desc, MI_BATCH_BUFFER_START)) {
			ret = check_bbstart(cmd, offset, length, batch_length,
					    batch_addr, shadow_addr,
					    jump_whitelist);
			break;
		}

		if (!IS_ERR_OR_NULL(jump_whitelist))
			__set_bit(offset, jump_whitelist);

		cmd += length;
		offset += length;
		if  (cmd >= batch_end) {
			DRM_DEBUG("CMD: Got to the end of the buffer w/o a BBE cmd!\n");
			ret = -EINVAL;
			break;
		}
	} while (1);

	if (trampoline) {
		 
		*batch_end = MI_BATCH_BUFFER_END;

		if (ret) {
			 
			cmd = page_mask_bits(shadow->obj->mm.mapping);
			*cmd = MI_BATCH_BUFFER_END;

			 
			if (ret == -EACCES) {
				unsigned int flags;

				flags = MI_BATCH_NON_SECURE_I965;
				if (IS_HASWELL(engine->i915))
					flags = MI_BATCH_NON_SECURE_HSW;

				GEM_BUG_ON(!IS_GRAPHICS_VER(engine->i915, 6, 7));
				__gen6_emit_bb_start(batch_end,
						     batch_addr,
						     flags);

				ret = 0;  
			}
		}
	}

	i915_gem_object_flush_map(shadow->obj);

	if (!IS_ERR_OR_NULL(jump_whitelist))
		kfree(jump_whitelist);
	i915_gem_object_unpin_map(shadow->obj);
	return ret;
}

 
int i915_cmd_parser_get_version(struct drm_i915_private *dev_priv)
{
	struct intel_engine_cs *engine;
	bool active = false;

	 
	for_each_uabi_engine(engine, dev_priv) {
		if (intel_engine_using_cmd_parser(engine)) {
			active = true;
			break;
		}
	}
	if (!active)
		return 0;

	 
	return 10;
}
