#ifndef __INTEL_DISPLAY_CORE_H__
#define __INTEL_DISPLAY_CORE_H__
#include <linux/list.h>
#include <linux/llist.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>
#include "intel_cdclk.h"
#include "intel_display_device.h"
#include "intel_display_limits.h"
#include "intel_display_power.h"
#include "intel_dpll_mgr.h"
#include "intel_fbc.h"
#include "intel_global_state.h"
#include "intel_gmbus.h"
#include "intel_opregion.h"
#include "intel_wm_types.h"
struct drm_i915_private;
struct drm_property;
struct drm_property_blob;
struct i915_audio_component;
struct i915_hdcp_arbiter;
struct intel_atomic_state;
struct intel_audio_funcs;
struct intel_cdclk_funcs;
struct intel_cdclk_vals;
struct intel_color_funcs;
struct intel_crtc;
struct intel_crtc_state;
struct intel_dmc;
struct intel_dpll_funcs;
struct intel_dpll_mgr;
struct intel_fbdev;
struct intel_fdi_funcs;
struct intel_hotplug_funcs;
struct intel_initial_plane_config;
struct intel_overlay;
#define I915_NUM_QGV_POINTS 8
#define I915_NUM_PSF_GV_POINTS 3
struct intel_display_funcs {
	bool (*get_pipe_config)(struct intel_crtc *,
				struct intel_crtc_state *);
	void (*get_initial_plane_config)(struct intel_crtc *,
					 struct intel_initial_plane_config *);
	void (*crtc_enable)(struct intel_atomic_state *state,
			    struct intel_crtc *crtc);
	void (*crtc_disable)(struct intel_atomic_state *state,
			     struct intel_crtc *crtc);
	void (*commit_modeset_enables)(struct intel_atomic_state *state);
};
struct intel_wm_funcs {
	void (*update_wm)(struct drm_i915_private *dev_priv);
	int (*compute_pipe_wm)(struct intel_atomic_state *state,
			       struct intel_crtc *crtc);
	int (*compute_intermediate_wm)(struct intel_atomic_state *state,
				       struct intel_crtc *crtc);
	void (*initial_watermarks)(struct intel_atomic_state *state,
				   struct intel_crtc *crtc);
	void (*atomic_update_watermarks)(struct intel_atomic_state *state,
					 struct intel_crtc *crtc);
	void (*optimize_watermarks)(struct intel_atomic_state *state,
				    struct intel_crtc *crtc);
	int (*compute_global_watermarks)(struct intel_atomic_state *state);
	void (*get_hw_state)(struct drm_i915_private *i915);
};
struct intel_audio_state {
	struct intel_encoder *encoder;
	u8 eld[MAX_ELD_BYTES];
};
struct intel_audio {
	struct i915_audio_component *component;
	bool component_registered;
	struct mutex mutex;
	int power_refcount;
	u32 freq_cntrl;
	struct intel_audio_state state[I915_MAX_TRANSCODERS];
	struct {
		struct platform_device *platdev;
		int irq;
	} lpe;
};
struct intel_dpll {
	struct mutex lock;
	int num_shared_dpll;
	struct intel_shared_dpll shared_dplls[I915_NUM_PLLS];
	const struct intel_dpll_mgr *mgr;
	struct {
		int nssc;
		int ssc;
	} ref_clks;
	u8 pch_ssc_use;
};
struct intel_frontbuffer_tracking {
	spinlock_t lock;
	unsigned busy_bits;
	unsigned flip_bits;
};
struct intel_hotplug {
	struct delayed_work hotplug_work;
	const u32 *hpd, *pch_hpd;
	struct {
		unsigned long last_jiffies;
		int count;
		enum {
			HPD_ENABLED = 0,
			HPD_DISABLED = 1,
			HPD_MARK_DISABLED = 2
		} state;
	} stats[HPD_NUM_PINS];
	u32 event_bits;
	u32 retry_bits;
	struct delayed_work reenable_work;
	u32 long_port_mask;
	u32 short_port_mask;
	struct work_struct dig_port_work;
	struct work_struct poll_init_work;
	bool poll_enabled;
	unsigned int hpd_storm_threshold;
	u8 hpd_short_storm_enabled;
	struct workqueue_struct *dp_wq;
	bool ignore_long_hpd;
};
struct intel_vbt_data {
	u16 version;
	unsigned int int_tv_support:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	unsigned int int_lvds_support:1;
	unsigned int display_clock_mode:1;
	unsigned int fdi_rx_polarity_inverted:1;
	int lvds_ssc_freq;
	enum drm_panel_orientation orientation;
	bool override_afc_startup;
	u8 override_afc_startup_val;
	int crt_ddc_pin;
	struct list_head display_devices;
	struct list_head bdb_blocks;
	struct sdvo_device_mapping {
		u8 initialized;
		u8 dvo_port;
		u8 slave_addr;
		u8 dvo_wiring;
		u8 i2c_pin;
		u8 ddc_pin;
	} sdvo_mappings[2];
};
struct intel_wm {
	u16 pri_latency[5];
	u16 spr_latency[5];
	u16 cur_latency[5];
	u16 skl_latency[8];
	union {
		struct ilk_wm_values hw;
		struct vlv_wm_values vlv;
		struct g4x_wm_values g4x;
	};
	u8 num_levels;
	struct mutex wm_mutex;
	bool ipc_enabled;
};
struct intel_display {
	struct {
		const struct intel_display_funcs *display;
		const struct intel_cdclk_funcs *cdclk;
		const struct intel_dpll_funcs *dpll;
		const struct intel_hotplug_funcs *hotplug;
		const struct intel_wm_funcs *wm;
		const struct intel_fdi_funcs *fdi;
		const struct intel_color_funcs *color;
		const struct intel_audio_funcs *audio;
	} funcs;
	struct intel_atomic_helper {
		struct llist_head free_list;
		struct work_struct free_work;
	} atomic_helper;
	struct {
		struct mutex lock;
	} backlight;
	struct {
		struct intel_global_obj obj;
		struct intel_bw_info {
			unsigned int deratedbw[I915_NUM_QGV_POINTS];
			unsigned int psf_bw[I915_NUM_PSF_GV_POINTS];
			unsigned int peakbw[I915_NUM_QGV_POINTS];
			u8 num_qgv_points;
			u8 num_psf_gv_points;
			u8 num_planes;
		} max[6];
	} bw;
	struct {
		struct intel_cdclk_config hw;
		const struct intel_cdclk_vals *table;
		struct intel_global_obj obj;
		unsigned int max_cdclk_freq;
	} cdclk;
	struct {
		struct drm_property_blob *glk_linear_degamma_lut;
	} color;
	struct {
		u8 enabled_slices;
		struct intel_global_obj obj;
	} dbuf;
	struct {
		wait_queue_head_t waitqueue;
		struct mutex lock;
		struct intel_global_obj obj;
	} pmdemand;
	struct {
		spinlock_t phy_lock;
	} dkl;
	struct {
		struct intel_dmc *dmc;
		intel_wakeref_t wakeref;
	} dmc;
	struct {
		u32 mmio_base;
	} dsi;
	struct {
		struct intel_fbdev *fbdev;
		struct work_struct suspend_work;
	} fbdev;
	struct {
		unsigned int pll_freq;
		u32 rx_config;
	} fdi;
	struct {
		struct list_head obj_list;
	} global;
	struct {
		u32 mmio_base;
		struct mutex mutex;
		struct intel_gmbus *bus[GMBUS_NUM_PINS];
		wait_queue_head_t wait_queue;
	} gmbus;
	struct {
		struct i915_hdcp_arbiter *arbiter;
		bool comp_added;
		struct intel_hdcp_gsc_message *hdcp_message;
		struct mutex hdcp_mutex;
	} hdcp;
	struct {
		u32 state;
	} hti;
	struct {
		const struct intel_display_device_info *__device_info;
		struct intel_display_runtime_info __runtime_info;
	} info;
	struct {
		bool false_color;
	} ips;
	struct {
		struct i915_power_domains domains;
		u32 chv_phy_control;
		bool chv_phy_assert[2];
	} power;
	struct {
		u32 mmio_base;
		struct mutex mutex;
	} pps;
	struct {
		struct drm_property *broadcast_rgb;
		struct drm_property *force_audio;
	} properties;
	struct {
		unsigned long mask;
	} quirks;
	struct {
		struct drm_atomic_state *modeset_state;
		struct drm_modeset_acquire_ctx reset_ctx;
	} restore;
	struct {
		enum {
			I915_SAGV_UNKNOWN = 0,
			I915_SAGV_DISABLED,
			I915_SAGV_ENABLED,
			I915_SAGV_NOT_CONTROLLED
		} status;
		u32 block_time_us;
	} sagv;
	struct {
		u8 phy_failed_calibration;
	} snps;
	struct {
		u32 chv_dpll_md[I915_MAX_PIPES];
		u32 bxt_phy_grc;
	} state;
	struct {
		struct workqueue_struct *modeset;
		struct workqueue_struct *flip;
	} wq;
	struct intel_audio audio;
	struct intel_dpll dpll;
	struct intel_fbc *fbc[I915_MAX_FBCS];
	struct intel_frontbuffer_tracking fb_tracking;
	struct intel_hotplug hotplug;
	struct intel_opregion opregion;
	struct intel_overlay *overlay;
	struct intel_vbt_data vbt;
	struct intel_wm wm;
};
#endif  
