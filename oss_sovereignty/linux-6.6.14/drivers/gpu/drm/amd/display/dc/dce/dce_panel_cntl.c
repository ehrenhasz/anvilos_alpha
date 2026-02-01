 

#include "reg_helper.h"
#include "core_types.h"
#include "dc_dmub_srv.h"
#include "panel_cntl.h"
#include "dce_panel_cntl.h"
#include "atom.h"

#define TO_DCE_PANEL_CNTL(panel_cntl)\
	container_of(panel_cntl, struct dce_panel_cntl, base)

#define CTX \
	dce_panel_cntl->base.ctx

#define DC_LOGGER \
	dce_panel_cntl->base.ctx->logger

#define REG(reg)\
	dce_panel_cntl->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	dce_panel_cntl->shift->field_name, dce_panel_cntl->mask->field_name

static unsigned int dce_get_16_bit_backlight_from_pwm(struct panel_cntl *panel_cntl)
{
	uint64_t current_backlight;
	uint32_t bl_period, bl_int_count;
	uint32_t bl_pwm, fractional_duty_cycle_en;
	uint32_t bl_period_mask, bl_pwm_mask;
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);

	REG_READ(BL_PWM_PERIOD_CNTL);
	REG_GET(BL_PWM_PERIOD_CNTL, BL_PWM_PERIOD, &bl_period);
	REG_GET(BL_PWM_PERIOD_CNTL, BL_PWM_PERIOD_BITCNT, &bl_int_count);

	REG_READ(BL_PWM_CNTL);
	REG_GET(BL_PWM_CNTL, BL_ACTIVE_INT_FRAC_CNT, (uint32_t *)(&bl_pwm));
	REG_GET(BL_PWM_CNTL, BL_PWM_FRACTIONAL_EN, &fractional_duty_cycle_en);

	if (bl_int_count == 0)
		bl_int_count = 16;

	bl_period_mask = (1 << bl_int_count) - 1;
	bl_period &= bl_period_mask;

	bl_pwm_mask = bl_period_mask << (16 - bl_int_count);

	if (fractional_duty_cycle_en == 0)
		bl_pwm &= bl_pwm_mask;
	else
		bl_pwm &= 0xFFFF;

	current_backlight = (uint64_t)bl_pwm << (1 + bl_int_count);

	if (bl_period == 0)
		bl_period = 0xFFFF;

	current_backlight = div_u64(current_backlight, bl_period);
	current_backlight = (current_backlight + 1) >> 1;

	return (uint32_t)(current_backlight);
}

static uint32_t dce_panel_cntl_hw_init(struct panel_cntl *panel_cntl)
{
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);
	uint32_t value;
	uint32_t current_backlight;

	 
	REG_GET(BL_PWM_CNTL, BL_ACTIVE_INT_FRAC_CNT, &value);

	if (panel_cntl->stored_backlight_registers.BL_PWM_CNTL != 0) {
		REG_WRITE(BL_PWM_CNTL,
				panel_cntl->stored_backlight_registers.BL_PWM_CNTL);
		REG_WRITE(BL_PWM_CNTL2,
				panel_cntl->stored_backlight_registers.BL_PWM_CNTL2);
		REG_WRITE(BL_PWM_PERIOD_CNTL,
				panel_cntl->stored_backlight_registers.BL_PWM_PERIOD_CNTL);
		REG_UPDATE(PWRSEQ_REF_DIV,
			BL_PWM_REF_DIV,
			panel_cntl->stored_backlight_registers.LVTMA_PWRSEQ_REF_DIV_BL_PWM_REF_DIV);
	} else if ((value != 0) && (value != 1)) {
		panel_cntl->stored_backlight_registers.BL_PWM_CNTL =
				REG_READ(BL_PWM_CNTL);
		panel_cntl->stored_backlight_registers.BL_PWM_CNTL2 =
				REG_READ(BL_PWM_CNTL2);
		panel_cntl->stored_backlight_registers.BL_PWM_PERIOD_CNTL =
				REG_READ(BL_PWM_PERIOD_CNTL);

		REG_GET(PWRSEQ_REF_DIV, BL_PWM_REF_DIV,
				&panel_cntl->stored_backlight_registers.LVTMA_PWRSEQ_REF_DIV_BL_PWM_REF_DIV);
	} else {
		 
		REG_WRITE(BL_PWM_CNTL, 0x8000FA00);
		REG_WRITE(BL_PWM_PERIOD_CNTL, 0x000C0FA0);
	}

	
	
	value = REG_READ(BIOS_SCRATCH_2);
	value |= ATOM_S2_VRI_BRIGHT_ENABLE;
	REG_WRITE(BIOS_SCRATCH_2, value);

	
	REG_UPDATE(BL_PWM_CNTL, BL_PWM_EN, 1);

	
	REG_UPDATE(BL_PWM_GRP1_REG_LOCK,
			BL_PWM_GRP1_REG_LOCK, 0);

	current_backlight = dce_get_16_bit_backlight_from_pwm(panel_cntl);

	return current_backlight;
}

static bool dce_is_panel_backlight_on(struct panel_cntl *panel_cntl)
{
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);
	uint32_t blon, blon_ovrd, pwrseq_target_state;

	REG_GET_2(PWRSEQ_CNTL, LVTMA_BLON, &blon, LVTMA_BLON_OVRD, &blon_ovrd);
	REG_GET(PWRSEQ_CNTL, LVTMA_PWRSEQ_TARGET_STATE, &pwrseq_target_state);

	if (blon_ovrd)
		return blon;
	else
		return pwrseq_target_state;
}

static bool dce_is_panel_powered_on(struct panel_cntl *panel_cntl)
{
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);
	uint32_t pwr_seq_state, dig_on, dig_on_ovrd;

	REG_GET(PWRSEQ_STATE, LVTMA_PWRSEQ_TARGET_STATE_R, &pwr_seq_state);

	REG_GET_2(PWRSEQ_CNTL, LVTMA_DIGON, &dig_on, LVTMA_DIGON_OVRD, &dig_on_ovrd);

	return (pwr_seq_state == 1) || (dig_on == 1 && dig_on_ovrd == 1);
}

static void dce_store_backlight_level(struct panel_cntl *panel_cntl)
{
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);

	panel_cntl->stored_backlight_registers.BL_PWM_CNTL =
		REG_READ(BL_PWM_CNTL);
	panel_cntl->stored_backlight_registers.BL_PWM_CNTL2 =
		REG_READ(BL_PWM_CNTL2);
	panel_cntl->stored_backlight_registers.BL_PWM_PERIOD_CNTL =
		REG_READ(BL_PWM_PERIOD_CNTL);

	REG_GET(PWRSEQ_REF_DIV, BL_PWM_REF_DIV,
		&panel_cntl->stored_backlight_registers.LVTMA_PWRSEQ_REF_DIV_BL_PWM_REF_DIV);
}

static void dce_driver_set_backlight(struct panel_cntl *panel_cntl,
		uint32_t backlight_pwm_u16_16)
{
	uint32_t backlight_16bit;
	uint32_t masked_pwm_period;
	uint8_t bit_count;
	uint64_t active_duty_cycle;
	uint32_t pwm_period_bitcnt;
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(panel_cntl);

	 

	 
	REG_GET_2(BL_PWM_PERIOD_CNTL,
			BL_PWM_PERIOD_BITCNT, &pwm_period_bitcnt,
			BL_PWM_PERIOD, &masked_pwm_period);

	if (pwm_period_bitcnt == 0)
		bit_count = 16;
	else
		bit_count = pwm_period_bitcnt;

	 
	masked_pwm_period = masked_pwm_period & ((1 << bit_count) - 1);

	 
	active_duty_cycle = backlight_pwm_u16_16 * masked_pwm_period;

	 
	backlight_16bit = active_duty_cycle >> bit_count;
	backlight_16bit &= 0xFFFF;
	backlight_16bit += (active_duty_cycle >> (bit_count - 1)) & 0x1;

	 

	 

	REG_UPDATE_2(BL_PWM_GRP1_REG_LOCK,
			BL_PWM_GRP1_IGNORE_MASTER_LOCK_EN, 1,
			BL_PWM_GRP1_REG_LOCK, 1);

	
	REG_UPDATE(BL_PWM_CNTL, BL_ACTIVE_INT_FRAC_CNT, backlight_16bit);

	 
	REG_UPDATE(BL_PWM_GRP1_REG_LOCK,
			BL_PWM_GRP1_REG_LOCK, 0);

	 
	REG_WAIT(BL_PWM_GRP1_REG_LOCK,
			BL_PWM_GRP1_REG_UPDATE_PENDING, 0,
			1, 10000);
}

static void dce_panel_cntl_destroy(struct panel_cntl **panel_cntl)
{
	struct dce_panel_cntl *dce_panel_cntl = TO_DCE_PANEL_CNTL(*panel_cntl);

	kfree(dce_panel_cntl);
	*panel_cntl = NULL;
}

static const struct panel_cntl_funcs dce_link_panel_cntl_funcs = {
	.destroy = dce_panel_cntl_destroy,
	.hw_init = dce_panel_cntl_hw_init,
	.is_panel_backlight_on = dce_is_panel_backlight_on,
	.is_panel_powered_on = dce_is_panel_powered_on,
	.store_backlight_level = dce_store_backlight_level,
	.driver_set_backlight = dce_driver_set_backlight,
	.get_current_backlight = dce_get_16_bit_backlight_from_pwm,
};

void dce_panel_cntl_construct(
	struct dce_panel_cntl *dce_panel_cntl,
	const struct panel_cntl_init_data *init_data,
	const struct dce_panel_cntl_registers *regs,
	const struct dce_panel_cntl_shift *shift,
	const struct dce_panel_cntl_mask *mask)
{
	struct panel_cntl *base = &dce_panel_cntl->base;

	base->stored_backlight_registers.BL_PWM_CNTL = 0;
	base->stored_backlight_registers.BL_PWM_CNTL2 = 0;
	base->stored_backlight_registers.BL_PWM_PERIOD_CNTL = 0;
	base->stored_backlight_registers.LVTMA_PWRSEQ_REF_DIV_BL_PWM_REF_DIV = 0;

	dce_panel_cntl->regs = regs;
	dce_panel_cntl->shift = shift;
	dce_panel_cntl->mask = mask;

	dce_panel_cntl->base.funcs = &dce_link_panel_cntl_funcs;
	dce_panel_cntl->base.ctx = init_data->ctx;
	dce_panel_cntl->base.inst = init_data->inst;
}
