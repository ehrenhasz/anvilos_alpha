#ifndef __DC_ABM_H__
#define __DC_ABM_H__
#include "dm_services_types.h"
struct abm_save_restore;
struct abm {
	struct dc_context *ctx;
	const struct abm_funcs *funcs;
	bool dmcu_is_running;
};
struct abm_funcs {
	void (*abm_init)(struct abm *abm, uint32_t back_light);
	bool (*set_abm_level)(struct abm *abm, unsigned int abm_level);
	bool (*set_abm_immediate_disable)(struct abm *abm, unsigned int panel_inst);
	bool (*set_pipe)(struct abm *abm, unsigned int controller_id, unsigned int panel_inst);
	bool (*set_backlight_level_pwm)(struct abm *abm,
			unsigned int backlight_pwm_u16_16,
			unsigned int frame_ramp,
			unsigned int controller_id,
			unsigned int panel_inst);
	unsigned int (*get_current_backlight)(struct abm *abm);
	unsigned int (*get_target_backlight)(struct abm *abm);
	bool (*init_abm_config)(struct abm *abm,
			const char *src,
			unsigned int bytes,
			unsigned int inst);
	bool (*set_abm_pause)(struct abm *abm, bool pause, unsigned int panel_inst, unsigned int otg_inst);
	bool (*save_restore)(
			struct abm *abm,
			unsigned int panel_inst,
			struct abm_save_restore *pData);
	bool (*set_pipe_ex)(struct abm *abm,
			unsigned int otg_inst,
			unsigned int option,
			unsigned int panel_inst,
			unsigned int pwrseq_inst);
};
#endif
