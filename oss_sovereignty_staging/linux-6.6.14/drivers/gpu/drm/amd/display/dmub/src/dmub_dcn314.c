 

#include "../dmub_srv.h"
#include "dmub_reg.h"
#include "dmub_dcn314.h"

#include "dcn/dcn_3_1_4_offset.h"
#include "dcn/dcn_3_1_4_sh_mask.h"

#define DCN_BASE__INST0_SEG0                       0x00000012
#define DCN_BASE__INST0_SEG1                       0x000000C0
#define DCN_BASE__INST0_SEG2                       0x000034C0
#define DCN_BASE__INST0_SEG3                       0x00009000
#define DCN_BASE__INST0_SEG4                       0x02403C00
#define DCN_BASE__INST0_SEG5                       0

#define BASE_INNER(seg) DCN_BASE__INST0_SEG##seg
#define CTX dmub
#define REGS dmub->regs_dcn31
#define REG_OFFSET_EXP(reg_name) (BASE(reg##reg_name##_BASE_IDX) + reg##reg_name)

 

const struct dmub_srv_dcn31_regs dmub_srv_dcn314_regs = {
#define DMUB_SR(reg) REG_OFFSET_EXP(reg),
	{
		DMUB_DCN31_REGS()
		DMCUB_INTERNAL_REGS()
	},
#undef DMUB_SR

#define DMUB_SF(reg, field) FD_MASK(reg, field),
	{ DMUB_DCN31_FIELDS() },
#undef DMUB_SF

#define DMUB_SF(reg, field) FD_SHIFT(reg, field),
	{ DMUB_DCN31_FIELDS() },
#undef DMUB_SF
};

bool dmub_dcn314_is_psrsu_supported(struct dmub_srv *dmub)
{
	return dmub->fw_version >= DMUB_FW_VERSION(8, 0, 16);
}
