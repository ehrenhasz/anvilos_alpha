 

#include "mock_uncore.h"

#define __nop_write(x) \
static void \
nop_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { }
__nop_write(8)
__nop_write(16)
__nop_write(32)

#define __nop_read(x) \
static u##x \
nop_read##x(struct intel_uncore *uncore, i915_reg_t reg, bool trace) { return 0; }
__nop_read(8)
__nop_read(16)
__nop_read(32)
__nop_read(64)

void mock_uncore_init(struct intel_uncore *uncore,
		      struct drm_i915_private *i915)
{
	intel_uncore_init_early(uncore, to_gt(i915));

	ASSIGN_RAW_WRITE_MMIO_VFUNCS(uncore, nop);
	ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, nop);
}
