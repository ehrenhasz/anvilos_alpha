 

#include "../dmub_srv.h"
#include "dmub_reg.h"
#include "dmub_dcn301.h"

#include "dcn/dcn_3_0_1_offset.h"
#include "dcn/dcn_3_0_1_sh_mask.h"
#include "vangogh_ip_offset.h"

#define BASE_INNER(seg) DCN_BASE__INST0_SEG##seg
#define CTX dmub
#define REGS dmub->regs

 

const struct dmub_srv_common_regs dmub_srv_dcn301_regs = {
#define DMUB_SR(reg) REG_OFFSET(reg),
	{
		DMUB_COMMON_REGS()
		DMCUB_INTERNAL_REGS()
	},
#undef DMUB_SR

#define DMUB_SF(reg, field) FD_MASK(reg, field),
	{ DMUB_COMMON_FIELDS() },
#undef DMUB_SF

#define DMUB_SF(reg, field) FD_SHIFT(reg, field),
	{ DMUB_COMMON_FIELDS() },
#undef DMUB_SF
};

 

