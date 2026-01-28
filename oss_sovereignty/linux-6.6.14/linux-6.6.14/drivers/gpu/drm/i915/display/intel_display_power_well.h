#ifndef __INTEL_DISPLAY_POWER_WELL_H__
#define __INTEL_DISPLAY_POWER_WELL_H__
#include <linux/types.h>
#include "intel_display_power.h"
#include "intel_dpio_phy.h"
struct drm_i915_private;
struct i915_power_well_ops;
struct intel_encoder;
#define for_each_power_well(__dev_priv, __power_well)				\
	for ((__power_well) = (__dev_priv)->display.power.domains.power_wells;	\
	     (__power_well) - (__dev_priv)->display.power.domains.power_wells <	\
		(__dev_priv)->display.power.domains.power_well_count;		\
	     (__power_well)++)
#define for_each_power_well_reverse(__dev_priv, __power_well)			\
	for ((__power_well) = (__dev_priv)->display.power.domains.power_wells +		\
			      (__dev_priv)->display.power.domains.power_well_count - 1;	\
	     (__power_well) - (__dev_priv)->display.power.domains.power_wells >= 0;	\
	     (__power_well)--)
enum i915_power_well_id {
	DISP_PW_ID_NONE = 0,		 
	VLV_DISP_PW_DISP2D,
	BXT_DISP_PW_DPIO_CMN_A,
	VLV_DISP_PW_DPIO_CMN_BC,
	GLK_DISP_PW_DPIO_CMN_C,
	CHV_DISP_PW_DPIO_CMN_D,
	HSW_DISP_PW_GLOBAL,
	SKL_DISP_PW_MISC_IO,
	SKL_DISP_PW_1,
	SKL_DISP_PW_2,
	ICL_DISP_PW_3,
	SKL_DISP_DC_OFF,
	TGL_DISP_PW_TC_COLD_OFF,
};
struct i915_power_well_instance {
	const char *name;
	const struct i915_power_domain_list {
		const enum intel_display_power_domain *list;
		u8 count;
	} *domain_list;
	enum i915_power_well_id id;
	union {
		struct {
			u8 idx;
		} vlv;
		struct {
			enum dpio_phy phy;
		} bxt;
		struct {
			u8 idx;
		} hsw;
		struct {
			u8 aux_ch;
		} xelpdp;
	};
};
struct i915_power_well_desc {
	const struct i915_power_well_ops *ops;
	const struct i915_power_well_instance_list {
		const struct i915_power_well_instance *list;
		u8 count;
	} *instances;
	u16 irq_pipe_mask:4;
	u16 always_on:1;
	u16 fixed_enable_delay:1;
	u16 has_vga:1;
	u16 has_fuses:1;
	u16 is_tc_tbt:1;
	u16 enable_timeout;
};
struct i915_power_well {
	const struct i915_power_well_desc *desc;
	struct intel_power_domain_mask domains;
	int count;
	bool hw_enabled;
	u8 instance_idx;
};
struct i915_power_well *lookup_power_well(struct drm_i915_private *i915,
					  enum i915_power_well_id id);
void intel_power_well_enable(struct drm_i915_private *i915,
			     struct i915_power_well *power_well);
void intel_power_well_disable(struct drm_i915_private *i915,
			      struct i915_power_well *power_well);
void intel_power_well_sync_hw(struct drm_i915_private *i915,
			      struct i915_power_well *power_well);
void intel_power_well_get(struct drm_i915_private *i915,
			  struct i915_power_well *power_well);
void intel_power_well_put(struct drm_i915_private *i915,
			  struct i915_power_well *power_well);
bool intel_power_well_is_enabled(struct drm_i915_private *i915,
				 struct i915_power_well *power_well);
bool intel_power_well_is_enabled_cached(struct i915_power_well *power_well);
bool intel_display_power_well_is_enabled(struct drm_i915_private *dev_priv,
					 enum i915_power_well_id power_well_id);
bool intel_power_well_is_always_on(struct i915_power_well *power_well);
const char *intel_power_well_name(struct i915_power_well *power_well);
struct intel_power_domain_mask *intel_power_well_domains(struct i915_power_well *power_well);
int intel_power_well_refcount(struct i915_power_well *power_well);
void chv_phy_powergate_lanes(struct intel_encoder *encoder,
			     bool override, unsigned int mask);
bool chv_phy_powergate_ch(struct drm_i915_private *dev_priv, enum dpio_phy phy,
			  enum dpio_channel ch, bool override);
void gen9_enable_dc5(struct drm_i915_private *dev_priv);
void skl_enable_dc6(struct drm_i915_private *dev_priv);
void gen9_sanitize_dc_state(struct drm_i915_private *dev_priv);
void gen9_set_dc_state(struct drm_i915_private *dev_priv, u32 state);
void gen9_disable_dc_states(struct drm_i915_private *dev_priv);
void bxt_enable_dc9(struct drm_i915_private *dev_priv);
void bxt_disable_dc9(struct drm_i915_private *dev_priv);
extern const struct i915_power_well_ops i9xx_always_on_power_well_ops;
extern const struct i915_power_well_ops chv_pipe_power_well_ops;
extern const struct i915_power_well_ops chv_dpio_cmn_power_well_ops;
extern const struct i915_power_well_ops i830_pipes_power_well_ops;
extern const struct i915_power_well_ops hsw_power_well_ops;
extern const struct i915_power_well_ops gen9_dc_off_power_well_ops;
extern const struct i915_power_well_ops bxt_dpio_cmn_power_well_ops;
extern const struct i915_power_well_ops vlv_display_power_well_ops;
extern const struct i915_power_well_ops vlv_dpio_cmn_power_well_ops;
extern const struct i915_power_well_ops vlv_dpio_power_well_ops;
extern const struct i915_power_well_ops icl_aux_power_well_ops;
extern const struct i915_power_well_ops icl_ddi_power_well_ops;
extern const struct i915_power_well_ops tgl_tc_cold_off_ops;
extern const struct i915_power_well_ops xelpdp_aux_power_well_ops;
#endif
