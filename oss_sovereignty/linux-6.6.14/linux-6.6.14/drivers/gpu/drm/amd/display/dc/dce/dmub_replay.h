#ifndef _DMUB_REPLAY_H_
#define _DMUB_REPLAY_H_
#include "dc_types.h"
#include "dmub_cmd.h"
struct dc_link;
struct dmub_replay_funcs;
struct dmub_replay {
	struct dc_context *ctx;
	const struct dmub_replay_funcs *funcs;
};
struct dmub_replay_funcs {
	void (*replay_get_state)(struct dmub_replay *dmub, enum replay_state *state,
		uint8_t panel_inst);
	void (*replay_enable)(struct dmub_replay *dmub, bool enable, bool wait,
		uint8_t panel_inst);
	bool (*replay_copy_settings)(struct dmub_replay *dmub, struct dc_link *link,
		struct replay_context *replay_context, uint8_t panel_inst);
	void (*replay_set_power_opt)(struct dmub_replay *dmub, unsigned int power_opt,
		uint8_t panel_inst);
	void (*replay_set_coasting_vtotal)(struct dmub_replay *dmub, uint16_t coasting_vtotal,
		uint8_t panel_inst);
	void (*replay_residency)(struct dmub_replay *dmub,
		uint8_t panel_inst, uint32_t *residency, const bool is_start, const bool is_alpm);
};
struct dmub_replay *dmub_replay_create(struct dc_context *ctx);
void dmub_replay_destroy(struct dmub_replay **dmub);
#endif  
