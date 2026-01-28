#ifndef _DMUB_PSR_H_
#define _DMUB_PSR_H_
#include "dc_types.h"
struct dc_link;
struct dmub_psr_funcs;
struct dmub_psr {
	struct dc_context *ctx;
	const struct dmub_psr_funcs *funcs;
};
struct dmub_psr_funcs {
	bool (*psr_copy_settings)(struct dmub_psr *dmub, struct dc_link *link,
	struct psr_context *psr_context, uint8_t panel_inst);
	void (*psr_enable)(struct dmub_psr *dmub, bool enable, bool wait,
	uint8_t panel_inst);
	void (*psr_get_state)(struct dmub_psr *dmub, enum dc_psr_state *dc_psr_state,
	uint8_t panel_inst);
	void (*psr_set_level)(struct dmub_psr *dmub, uint16_t psr_level,
	uint8_t panel_inst);
	void (*psr_force_static)(struct dmub_psr *dmub, uint8_t panel_inst);
	void (*psr_get_residency)(struct dmub_psr *dmub, uint32_t *residency,
	uint8_t panel_inst);
	void (*psr_set_sink_vtotal_in_psr_active)(struct dmub_psr *dmub,
	uint16_t psr_vtotal_idle, uint16_t psr_vtotal_su);
	void (*psr_set_power_opt)(struct dmub_psr *dmub, unsigned int power_opt, uint8_t panel_inst);
};
struct dmub_psr *dmub_psr_create(struct dc_context *ctx);
void dmub_psr_destroy(struct dmub_psr **dmub);
#endif  
