 

#ifndef __DC_LINK_ENCODER__DCN321_H__
#define __DC_LINK_ENCODER__DCN321_H__

#include "dcn32/dcn32_dio_link_encoder.h"

void dcn321_link_encoder_construct(
	struct dcn20_link_encoder *enc20,
	const struct encoder_init_data *init_data,
	const struct encoder_feature_support *enc_features,
	const struct dcn10_link_enc_registers *link_regs,
	const struct dcn10_link_enc_aux_registers *aux_regs,
	const struct dcn10_link_enc_hpd_registers *hpd_regs,
	const struct dcn10_link_enc_shift *link_shift,
	const struct dcn10_link_enc_mask *link_mask);


#endif  
