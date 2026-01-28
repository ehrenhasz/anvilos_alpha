#ifndef __DC_OPP_DCN30_H__
#define __DC_OPP_DCN30_H__
#include "dcn20/dcn20_opp.h"
#define OPP_REG_LIST_DCN30(id) \
	OPP_REG_LIST_DCN10(id), \
	OPP_DPG_REG_LIST(id), \
	SRI(FMT_422_CONTROL, FMT, id)
#endif
