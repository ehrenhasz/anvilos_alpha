#ifndef _INTEL_WOPCM_H_
#define _INTEL_WOPCM_H_
#include <linux/types.h>
struct intel_wopcm {
	u32 size;
	struct {
		u32 base;
		u32 size;
	} guc;
};
static inline u32 intel_wopcm_guc_base(struct intel_wopcm *wopcm)
{
	return wopcm->guc.base;
}
static inline u32 intel_wopcm_guc_size(struct intel_wopcm *wopcm)
{
	return wopcm->guc.size;
}
void intel_wopcm_init_early(struct intel_wopcm *wopcm);
void intel_wopcm_init(struct intel_wopcm *wopcm);
#endif
