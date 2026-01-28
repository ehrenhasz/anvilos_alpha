#ifndef __DMUB_ABM_LCD_H__
#define __DMUB_ABM_LCD_H__
#include "abm.h"
struct abm_save_restore;
void dmub_abm_init(struct abm *abm, uint32_t backlight);
bool dmub_abm_set_level(struct abm *abm, uint32_t level, uint8_t panel_mask);
unsigned int dmub_abm_get_current_backlight(struct abm *abm);
unsigned int dmub_abm_get_target_backlight(struct abm *abm);
void dmub_abm_init_config(struct abm *abm,
	const char *src,
	unsigned int bytes,
	unsigned int inst);
bool dmub_abm_set_pause(struct abm *abm, bool pause, unsigned int panel_inst, unsigned int stream_inst);
bool dmub_abm_save_restore(
		struct dc_context *dc,
		unsigned int panel_inst,
		struct abm_save_restore *pData);
bool dmub_abm_set_pipe(struct abm *abm, uint32_t otg_inst, uint32_t option, uint32_t panel_inst, uint32_t pwrseq_inst);
bool dmub_abm_set_backlight_level(struct abm *abm,
		unsigned int backlight_pwm_u16_16,
		unsigned int frame_ramp,
		unsigned int panel_inst);
#endif
