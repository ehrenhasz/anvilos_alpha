 

#include "core_types.h"
#include "dce/dce_hwseq.h"
#include "dcn301_hwseq.h"
#include "reg_helper.h"

#define DC_LOGGER_INIT(logger)

#define CTX \
	hws->ctx
#define REG(reg)\
	hws->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	hws->shifts->field_name, hws->masks->field_name


