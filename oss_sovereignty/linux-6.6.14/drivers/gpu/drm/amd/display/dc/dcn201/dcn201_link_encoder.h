 

#ifndef __DC_LINK_ENCODER__DCN201_H__
#define __DC_LINK_ENCODER__DCN201_H__

#include "dcn20/dcn20_link_encoder.h"

#define DPCS_DCN201_MASK_SH_LIST(mask_sh)\
	DPCS_MASK_SH_LIST(mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL2, RDPCS_PHY_DPALT_DISABLE_ACK, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL2, RDPCS_PHY_DPALT_DISABLE, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL2, RDPCS_PHY_DPALT_DP4, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL5, RDPCS_PHY_DP_TX0_PSTATE, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL5, RDPCS_PHY_DP_TX1_PSTATE, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL5, RDPCS_PHY_DP_TX0_MPLL_EN, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL5, RDPCS_PHY_DP_TX1_MPLL_EN, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL6, RDPCS_PHY_DP_TX2_WIDTH, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL6, RDPCS_PHY_DP_TX2_RATE, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL6, RDPCS_PHY_DP_TX3_WIDTH, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL6, RDPCS_PHY_DP_TX3_RATE, mask_sh),\
	LE_SF(RDPCSTX0_RDPCSTX_PHY_CNTL11, RDPCS_PHY_DP_REF_CLK_EN, mask_sh)

#define DPCS_DCN201_REG_LIST(id) \
	DPCS_DCN2_CMN_REG_LIST(id)

void dcn201_link_encoder_construct(
	struct dcn20_link_encoder *enc20,
	const struct encoder_init_data *init_data,
	const struct encoder_feature_support *enc_features,
	const struct dcn10_link_enc_registers *link_regs,
	const struct dcn10_link_enc_aux_registers *aux_regs,
	const struct dcn10_link_enc_hpd_registers *hpd_regs,
	const struct dcn10_link_enc_shift *link_shift,
	const struct dcn10_link_enc_mask *link_mask);

#endif  
