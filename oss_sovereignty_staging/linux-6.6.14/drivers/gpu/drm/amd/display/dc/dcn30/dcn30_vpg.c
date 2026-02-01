 


#include "dc_bios_types.h"
#include "dcn30_vpg.h"
#include "reg_helper.h"

#define DC_LOGGER \
		vpg3->base.ctx->logger

#define REG(reg)\
	(vpg3->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	vpg3->vpg_shift->field_name, vpg3->vpg_mask->field_name


#define CTX \
	vpg3->base.ctx


void vpg3_update_generic_info_packet(
	struct vpg *vpg,
	uint32_t packet_index,
	const struct dc_info_packet *info_packet,
	bool immediate_update)
{
	struct dcn30_vpg *vpg3 = DCN30_VPG_FROM_VPG(vpg);
	uint32_t i;

	 
	uint32_t max_retries = 50;

	if (packet_index > 14)
		ASSERT(0);

	 
	 

	 
	 
	REG_WAIT(VPG_GENERIC_STATUS, VPG_GENERIC_CONFLICT_OCCURED,
			0, 10, max_retries);

	 
	REG_UPDATE(VPG_GENERIC_STATUS, VPG_GENERIC_CONFLICT_CLR, 1);

	 
	REG_UPDATE(VPG_GENERIC_PACKET_ACCESS_CTRL,
			VPG_GENERIC_DATA_INDEX, packet_index*9);

	 
	REG_SET_4(VPG_GENERIC_PACKET_DATA, 0,
			VPG_GENERIC_DATA_BYTE0, info_packet->hb0,
			VPG_GENERIC_DATA_BYTE1, info_packet->hb1,
			VPG_GENERIC_DATA_BYTE2, info_packet->hb2,
			VPG_GENERIC_DATA_BYTE3, info_packet->hb3);

	 
	{
		const uint32_t *content =
			(const uint32_t *) &info_packet->sb[0];

		for (i = 0; i < 8; i++) {
			REG_WRITE(VPG_GENERIC_PACKET_DATA, *content++);
		}
	}

	 
	if (immediate_update) {
		switch (packet_index) {
		case 0:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC0_IMMEDIATE_UPDATE, 1);
			break;
		case 1:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC1_IMMEDIATE_UPDATE, 1);
			break;
		case 2:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC2_IMMEDIATE_UPDATE, 1);
			break;
		case 3:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC3_IMMEDIATE_UPDATE, 1);
			break;
		case 4:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC4_IMMEDIATE_UPDATE, 1);
			break;
		case 5:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC5_IMMEDIATE_UPDATE, 1);
			break;
		case 6:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC6_IMMEDIATE_UPDATE, 1);
			break;
		case 7:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC7_IMMEDIATE_UPDATE, 1);
			break;
		case 8:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC8_IMMEDIATE_UPDATE, 1);
			break;
		case 9:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC9_IMMEDIATE_UPDATE, 1);
			break;
		case 10:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC10_IMMEDIATE_UPDATE, 1);
			break;
		case 11:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC11_IMMEDIATE_UPDATE, 1);
			break;
		case 12:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC12_IMMEDIATE_UPDATE, 1);
			break;
		case 13:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC13_IMMEDIATE_UPDATE, 1);
			break;
		case 14:
			REG_UPDATE(VPG_GSP_IMMEDIATE_UPDATE_CTRL,
					VPG_GENERIC14_IMMEDIATE_UPDATE, 1);
			break;
		default:
			break;
		}
	} else {
		switch (packet_index) {
		case 0:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC0_FRAME_UPDATE, 1);
			break;
		case 1:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC1_FRAME_UPDATE, 1);
			break;
		case 2:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC2_FRAME_UPDATE, 1);
			break;
		case 3:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC3_FRAME_UPDATE, 1);
			break;
		case 4:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC4_FRAME_UPDATE, 1);
			break;
		case 5:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC5_FRAME_UPDATE, 1);
			break;
		case 6:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC6_FRAME_UPDATE, 1);
			break;
		case 7:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC7_FRAME_UPDATE, 1);
			break;
		case 8:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC8_FRAME_UPDATE, 1);
			break;
		case 9:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC9_FRAME_UPDATE, 1);
			break;
		case 10:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC10_FRAME_UPDATE, 1);
			break;
		case 11:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC11_FRAME_UPDATE, 1);
			break;
		case 12:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC12_FRAME_UPDATE, 1);
			break;
		case 13:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC13_FRAME_UPDATE, 1);
			break;
		case 14:
			REG_UPDATE(VPG_GSP_FRAME_UPDATE_CTRL,
					VPG_GENERIC14_FRAME_UPDATE, 1);
			break;

		default:
			break;
		}

	}
}

static struct vpg_funcs dcn30_vpg_funcs = {
	.update_generic_info_packet	= vpg3_update_generic_info_packet,
};

void vpg3_construct(struct dcn30_vpg *vpg3,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn30_vpg_registers *vpg_regs,
	const struct dcn30_vpg_shift *vpg_shift,
	const struct dcn30_vpg_mask *vpg_mask)
{
	vpg3->base.ctx = ctx;

	vpg3->base.inst = inst;
	vpg3->base.funcs = &dcn30_vpg_funcs;

	vpg3->regs = vpg_regs;
	vpg3->vpg_shift = vpg_shift;
	vpg3->vpg_mask = vpg_mask;
}
