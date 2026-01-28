#ifndef _INTEL_DPLL_MGR_H_
#define _INTEL_DPLL_MGR_H_
#include <linux/types.h>
#include "intel_wakeref.h"
enum tc_port;
struct drm_i915_private;
struct intel_atomic_state;
struct intel_crtc;
struct intel_crtc_state;
struct intel_encoder;
struct intel_shared_dpll;
struct intel_shared_dpll_funcs;
enum intel_dpll_id {
	DPLL_ID_PRIVATE = -1,
	DPLL_ID_PCH_PLL_A = 0,
	DPLL_ID_PCH_PLL_B = 1,
	DPLL_ID_WRPLL1 = 0,
	DPLL_ID_WRPLL2 = 1,
	DPLL_ID_SPLL = 2,
	DPLL_ID_LCPLL_810 = 3,
	DPLL_ID_LCPLL_1350 = 4,
	DPLL_ID_LCPLL_2700 = 5,
	DPLL_ID_SKL_DPLL0 = 0,
	DPLL_ID_SKL_DPLL1 = 1,
	DPLL_ID_SKL_DPLL2 = 2,
	DPLL_ID_SKL_DPLL3 = 3,
	DPLL_ID_ICL_DPLL0 = 0,
	DPLL_ID_ICL_DPLL1 = 1,
	DPLL_ID_EHL_DPLL4 = 2,
	DPLL_ID_ICL_TBTPLL = 2,
	DPLL_ID_ICL_MGPLL1 = 3,
	DPLL_ID_ICL_MGPLL2 = 4,
	DPLL_ID_ICL_MGPLL3 = 5,
	DPLL_ID_ICL_MGPLL4 = 6,
	DPLL_ID_TGL_MGPLL5 = 7,
	DPLL_ID_TGL_MGPLL6 = 8,
	DPLL_ID_DG1_DPLL0 = 0,
	DPLL_ID_DG1_DPLL1 = 1,
	DPLL_ID_DG1_DPLL2 = 2,
	DPLL_ID_DG1_DPLL3 = 3,
};
#define I915_NUM_PLLS 9
enum icl_port_dpll_id {
	ICL_PORT_DPLL_DEFAULT,
	ICL_PORT_DPLL_MG_PHY,
	ICL_PORT_DPLL_COUNT,
};
struct intel_dpll_hw_state {
	u32 dpll;
	u32 dpll_md;
	u32 fp0;
	u32 fp1;
	u32 wrpll;
	u32 spll;
	u32 ctrl1;
	u32 cfgcr1, cfgcr2;
	u32 cfgcr0;
	u32 div0;
	u32 ebb0, ebb4, pll0, pll1, pll2, pll3, pll6, pll8, pll9, pll10, pcsdw12;
	u32 mg_refclkin_ctl;
	u32 mg_clktop2_coreclkctl1;
	u32 mg_clktop2_hsclkctl;
	u32 mg_pll_div0;
	u32 mg_pll_div1;
	u32 mg_pll_lf;
	u32 mg_pll_frac_lock;
	u32 mg_pll_ssc;
	u32 mg_pll_bias;
	u32 mg_pll_tdc_coldst_bias;
	u32 mg_pll_bias_mask;
	u32 mg_pll_tdc_coldst_bias_mask;
};
struct intel_shared_dpll_state {
	u8 pipe_mask;
	struct intel_dpll_hw_state hw_state;
};
struct dpll_info {
	const char *name;
	const struct intel_shared_dpll_funcs *funcs;
	enum intel_dpll_id id;
#define INTEL_DPLL_ALWAYS_ON	(1 << 0)
	u32 flags;
};
struct intel_shared_dpll {
	struct intel_shared_dpll_state state;
	u8 active_mask;
	bool on;
	const struct dpll_info *info;
	intel_wakeref_t wakeref;
};
#define SKL_DPLL0 0
#define SKL_DPLL1 1
#define SKL_DPLL2 2
#define SKL_DPLL3 3
struct intel_shared_dpll *
intel_get_shared_dpll_by_id(struct drm_i915_private *dev_priv,
			    enum intel_dpll_id id);
void assert_shared_dpll(struct drm_i915_private *dev_priv,
			struct intel_shared_dpll *pll,
			bool state);
#define assert_shared_dpll_enabled(d, p) assert_shared_dpll(d, p, true)
#define assert_shared_dpll_disabled(d, p) assert_shared_dpll(d, p, false)
int intel_compute_shared_dplls(struct intel_atomic_state *state,
			       struct intel_crtc *crtc,
			       struct intel_encoder *encoder);
int intel_reserve_shared_dplls(struct intel_atomic_state *state,
			       struct intel_crtc *crtc,
			       struct intel_encoder *encoder);
void intel_release_shared_dplls(struct intel_atomic_state *state,
				struct intel_crtc *crtc);
void intel_unreference_shared_dpll_crtc(const struct intel_crtc *crtc,
					const struct intel_shared_dpll *pll,
					struct intel_shared_dpll_state *shared_dpll_state);
void icl_set_active_port_dpll(struct intel_crtc_state *crtc_state,
			      enum icl_port_dpll_id port_dpll_id);
void intel_update_active_dpll(struct intel_atomic_state *state,
			      struct intel_crtc *crtc,
			      struct intel_encoder *encoder);
int intel_dpll_get_freq(struct drm_i915_private *i915,
			const struct intel_shared_dpll *pll,
			const struct intel_dpll_hw_state *pll_state);
bool intel_dpll_get_hw_state(struct drm_i915_private *i915,
			     struct intel_shared_dpll *pll,
			     struct intel_dpll_hw_state *hw_state);
void intel_enable_shared_dpll(const struct intel_crtc_state *crtc_state);
void intel_disable_shared_dpll(const struct intel_crtc_state *crtc_state);
void intel_shared_dpll_swap_state(struct intel_atomic_state *state);
void intel_shared_dpll_init(struct drm_i915_private *dev_priv);
void intel_dpll_update_ref_clks(struct drm_i915_private *dev_priv);
void intel_dpll_readout_hw_state(struct drm_i915_private *dev_priv);
void intel_dpll_sanitize_state(struct drm_i915_private *dev_priv);
void intel_dpll_dump_hw_state(struct drm_i915_private *dev_priv,
			      const struct intel_dpll_hw_state *hw_state);
enum intel_dpll_id icl_tc_port_to_pll_id(enum tc_port tc_port);
bool intel_dpll_is_combophy(enum intel_dpll_id id);
void intel_shared_dpll_state_verify(struct intel_crtc *crtc,
				    struct intel_crtc_state *old_crtc_state,
				    struct intel_crtc_state *new_crtc_state);
void intel_shared_dpll_verify_disabled(struct drm_i915_private *i915);
#endif  
