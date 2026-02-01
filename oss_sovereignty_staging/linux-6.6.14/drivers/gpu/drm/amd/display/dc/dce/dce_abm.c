 

#include <linux/slab.h>

#include "dce_abm.h"
#include "dm_services.h"
#include "reg_helper.h"
#include "fixed31_32.h"
#include "dc.h"

#include "atom.h"


#define TO_DCE_ABM(abm)\
	container_of(abm, struct dce_abm, base)

#define REG(reg) \
	(abm_dce->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	abm_dce->abm_shift->field_name, abm_dce->abm_mask->field_name

#define DC_LOGGER \
	abm->ctx->logger
#define CTX \
	abm_dce->base.ctx

#define MCP_ABM_LEVEL_SET 0x65
#define MCP_ABM_PIPE_SET 0x66
#define MCP_BL_SET 0x67

#define MCP_DISABLE_ABM_IMMEDIATELY 255

static bool dce_abm_set_pipe(struct abm *abm, uint32_t controller_id, uint32_t panel_inst)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);
	uint32_t rampingBoundary = 0xFFFF;

	if (abm->dmcu_is_running == false)
		return true;

	REG_WAIT(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 0,
			1, 80000);

	 
	REG_WRITE(MASTER_COMM_DATA_REG1, rampingBoundary);

	 
	REG_UPDATE_2(MASTER_COMM_CMD_REG,
			MASTER_COMM_CMD_REG_BYTE0, MCP_ABM_PIPE_SET,
			MASTER_COMM_CMD_REG_BYTE1, controller_id);

	 
	REG_UPDATE(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 1);

	REG_WAIT(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 0,
			1, 80000);

	return true;
}

static void dmcu_set_backlight_level(
	struct dce_abm *abm_dce,
	uint32_t backlight_pwm_u16_16,
	uint32_t frame_ramp,
	uint32_t controller_id,
	uint32_t panel_id)
{
	unsigned int backlight_8_bit = 0;
	uint32_t s2;

	if (backlight_pwm_u16_16 & 0x10000)
		
		backlight_8_bit = 0xFF;
	else
		
		backlight_8_bit = (backlight_pwm_u16_16 >> 8) & 0xFF;

	dce_abm_set_pipe(&abm_dce->base, controller_id, panel_id);

	 
	REG_WAIT(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT,
			0, 1, 80000);

	 
	REG_UPDATE(BL1_PWM_USER_LEVEL, BL1_PWM_USER_LEVEL, backlight_pwm_u16_16);

	 
	if (controller_id == 0)
		frame_ramp = 0;
	REG_WRITE(MASTER_COMM_DATA_REG1, frame_ramp);

	 
	REG_UPDATE(MASTER_COMM_CMD_REG, MASTER_COMM_CMD_REG_BYTE0, MCP_BL_SET);

	 
	REG_UPDATE(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 1);

	 
	s2 = REG_READ(BIOS_SCRATCH_2);

	s2 &= ~ATOM_S2_CURRENT_BL_LEVEL_MASK;
	backlight_8_bit &= (ATOM_S2_CURRENT_BL_LEVEL_MASK >>
				ATOM_S2_CURRENT_BL_LEVEL_SHIFT);
	s2 |= (backlight_8_bit << ATOM_S2_CURRENT_BL_LEVEL_SHIFT);

	REG_WRITE(BIOS_SCRATCH_2, s2);

	 
	REG_WAIT(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT,
			0, 1, 80000);
}

static void dce_abm_init(struct abm *abm, uint32_t backlight)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);

	REG_WRITE(DC_ABM1_HG_SAMPLE_RATE, 0x103);
	REG_WRITE(DC_ABM1_HG_SAMPLE_RATE, 0x101);
	REG_WRITE(DC_ABM1_LS_SAMPLE_RATE, 0x103);
	REG_WRITE(DC_ABM1_LS_SAMPLE_RATE, 0x101);
	REG_WRITE(BL1_PWM_BL_UPDATE_SAMPLE_RATE, 0x101);

	REG_SET_3(DC_ABM1_HG_MISC_CTRL, 0,
			ABM1_HG_NUM_OF_BINS_SEL, 0,
			ABM1_HG_VMAX_SEL, 1,
			ABM1_HG_BIN_BITWIDTH_SIZE_SEL, 0);

	REG_SET_3(DC_ABM1_IPCSC_COEFF_SEL, 0,
			ABM1_IPCSC_COEFF_SEL_R, 2,
			ABM1_IPCSC_COEFF_SEL_G, 4,
			ABM1_IPCSC_COEFF_SEL_B, 2);

	REG_UPDATE(BL1_PWM_CURRENT_ABM_LEVEL,
			BL1_PWM_CURRENT_ABM_LEVEL, backlight);

	REG_UPDATE(BL1_PWM_TARGET_ABM_LEVEL,
			BL1_PWM_TARGET_ABM_LEVEL, backlight);

	REG_UPDATE(BL1_PWM_USER_LEVEL,
			BL1_PWM_USER_LEVEL, backlight);

	REG_UPDATE_2(DC_ABM1_LS_MIN_MAX_PIXEL_VALUE_THRES,
			ABM1_LS_MIN_PIXEL_VALUE_THRES, 0,
			ABM1_LS_MAX_PIXEL_VALUE_THRES, 1000);

	REG_SET_3(DC_ABM1_HGLS_REG_READ_PROGRESS, 0,
			ABM1_HG_REG_READ_MISSED_FRAME_CLEAR, 1,
			ABM1_LS_REG_READ_MISSED_FRAME_CLEAR, 1,
			ABM1_BL_REG_READ_MISSED_FRAME_CLEAR, 1);
}

static unsigned int dce_abm_get_current_backlight(struct abm *abm)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);
	unsigned int backlight = REG_READ(BL1_PWM_CURRENT_ABM_LEVEL);

	 
	return backlight;
}

static unsigned int dce_abm_get_target_backlight(struct abm *abm)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);
	unsigned int backlight = REG_READ(BL1_PWM_TARGET_ABM_LEVEL);

	 
	return backlight;
}

static bool dce_abm_set_level(struct abm *abm, uint32_t level)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);

	if (abm->dmcu_is_running == false)
		return true;

	REG_WAIT(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 0,
			1, 80000);

	 
	REG_UPDATE_2(MASTER_COMM_CMD_REG,
			MASTER_COMM_CMD_REG_BYTE0, MCP_ABM_LEVEL_SET,
			MASTER_COMM_CMD_REG_BYTE2, level);

	 
	REG_UPDATE(MASTER_COMM_CNTL_REG, MASTER_COMM_INTERRUPT, 1);

	return true;
}

static bool dce_abm_immediate_disable(struct abm *abm, uint32_t panel_inst)
{
	if (abm->dmcu_is_running == false)
		return true;

	dce_abm_set_pipe(abm, MCP_DISABLE_ABM_IMMEDIATELY, panel_inst);

	return true;
}

static bool dce_abm_set_backlight_level_pwm(
		struct abm *abm,
		unsigned int backlight_pwm_u16_16,
		unsigned int frame_ramp,
		unsigned int controller_id,
		unsigned int panel_inst)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(abm);

	DC_LOG_BACKLIGHT("New Backlight level: %d (0x%X)\n",
			backlight_pwm_u16_16, backlight_pwm_u16_16);

	dmcu_set_backlight_level(abm_dce,
			backlight_pwm_u16_16,
			frame_ramp,
			controller_id,
			panel_inst);

	return true;
}

static const struct abm_funcs dce_funcs = {
	.abm_init = dce_abm_init,
	.set_abm_level = dce_abm_set_level,
	.set_pipe = dce_abm_set_pipe,
	.set_backlight_level_pwm = dce_abm_set_backlight_level_pwm,
	.get_current_backlight = dce_abm_get_current_backlight,
	.get_target_backlight = dce_abm_get_target_backlight,
	.init_abm_config = NULL,
	.set_abm_immediate_disable = dce_abm_immediate_disable,
};

static void dce_abm_construct(
	struct dce_abm *abm_dce,
	struct dc_context *ctx,
	const struct dce_abm_registers *regs,
	const struct dce_abm_shift *abm_shift,
	const struct dce_abm_mask *abm_mask)
{
	struct abm *base = &abm_dce->base;

	base->ctx = ctx;
	base->funcs = &dce_funcs;
	base->dmcu_is_running = false;

	abm_dce->regs = regs;
	abm_dce->abm_shift = abm_shift;
	abm_dce->abm_mask = abm_mask;
}

struct abm *dce_abm_create(
	struct dc_context *ctx,
	const struct dce_abm_registers *regs,
	const struct dce_abm_shift *abm_shift,
	const struct dce_abm_mask *abm_mask)
{
	struct dce_abm *abm_dce = kzalloc(sizeof(*abm_dce), GFP_ATOMIC);

	if (abm_dce == NULL) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	dce_abm_construct(abm_dce, ctx, regs, abm_shift, abm_mask);

	abm_dce->base.funcs = &dce_funcs;

	return &abm_dce->base;
}

void dce_abm_destroy(struct abm **abm)
{
	struct dce_abm *abm_dce = TO_DCE_ABM(*abm);

	kfree(abm_dce);
	*abm = NULL;
}
