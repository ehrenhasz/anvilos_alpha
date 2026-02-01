 

#ifndef __INTEL_DISPLAY_TYPES_H__
#define __INTEL_DISPLAY_TYPES_H__

#include <linux/i2c.h>
#include <linux/pm_qos.h>
#include <linux/pwm.h>
#include <linux/sched/clock.h>

#include <drm/display/drm_dp_dual_mode_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/display/drm_dsc.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_vblank.h>
#include <drm/drm_vblank_work.h>
#include <drm/i915_hdcp_interface.h>
#include <media/cec-notifier.h>

#include "i915_vma.h"
#include "i915_vma_types.h"
#include "intel_bios.h"
#include "intel_display.h"
#include "intel_display_limits.h"
#include "intel_display_power.h"
#include "intel_dpll_mgr.h"
#include "intel_wm_types.h"

struct drm_printer;
struct __intel_global_objs_state;
struct intel_ddi_buf_trans;
struct intel_fbc;
struct intel_connector;
struct intel_tc_port;

 

 
enum intel_output_type {
	INTEL_OUTPUT_UNUSED = 0,
	INTEL_OUTPUT_ANALOG = 1,
	INTEL_OUTPUT_DVO = 2,
	INTEL_OUTPUT_SDVO = 3,
	INTEL_OUTPUT_LVDS = 4,
	INTEL_OUTPUT_TVOUT = 5,
	INTEL_OUTPUT_HDMI = 6,
	INTEL_OUTPUT_DP = 7,
	INTEL_OUTPUT_EDP = 8,
	INTEL_OUTPUT_DSI = 9,
	INTEL_OUTPUT_DDI = 10,
	INTEL_OUTPUT_DP_MST = 11,
};

enum hdmi_force_audio {
	HDMI_AUDIO_OFF_DVI = -2,	 
	HDMI_AUDIO_OFF,			 
	HDMI_AUDIO_AUTO,		 
	HDMI_AUDIO_ON,			 
};

 
enum intel_broadcast_rgb {
	INTEL_BROADCAST_RGB_AUTO,
	INTEL_BROADCAST_RGB_FULL,
	INTEL_BROADCAST_RGB_LIMITED,
};

struct intel_fb_view {
	 
	struct i915_gtt_view gtt;

	 
	struct i915_color_plane_view {
		u32 offset;
		unsigned int x, y;
		 
		unsigned int mapping_stride;
		unsigned int scanout_stride;
	} color_plane[4];
};

struct intel_framebuffer {
	struct drm_framebuffer base;
	struct intel_frontbuffer *frontbuffer;

	 
	struct intel_fb_view normal_view;
	union {
		struct intel_fb_view rotated_view;
		struct intel_fb_view remapped_view;
	};

	struct i915_address_space *dpt_vm;
};

enum intel_hotplug_state {
	INTEL_HOTPLUG_UNCHANGED,
	INTEL_HOTPLUG_CHANGED,
	INTEL_HOTPLUG_RETRY,
};

struct intel_encoder {
	struct drm_encoder base;

	enum intel_output_type type;
	enum port port;
	u16 cloneable;
	u8 pipe_mask;
	enum intel_hotplug_state (*hotplug)(struct intel_encoder *encoder,
					    struct intel_connector *connector);
	enum intel_output_type (*compute_output_type)(struct intel_encoder *,
						      struct intel_crtc_state *,
						      struct drm_connector_state *);
	int (*compute_config)(struct intel_encoder *,
			      struct intel_crtc_state *,
			      struct drm_connector_state *);
	int (*compute_config_late)(struct intel_encoder *,
				   struct intel_crtc_state *,
				   struct drm_connector_state *);
	void (*pre_pll_enable)(struct intel_atomic_state *,
			       struct intel_encoder *,
			       const struct intel_crtc_state *,
			       const struct drm_connector_state *);
	void (*pre_enable)(struct intel_atomic_state *,
			   struct intel_encoder *,
			   const struct intel_crtc_state *,
			   const struct drm_connector_state *);
	void (*enable)(struct intel_atomic_state *,
		       struct intel_encoder *,
		       const struct intel_crtc_state *,
		       const struct drm_connector_state *);
	void (*disable)(struct intel_atomic_state *,
			struct intel_encoder *,
			const struct intel_crtc_state *,
			const struct drm_connector_state *);
	void (*post_disable)(struct intel_atomic_state *,
			     struct intel_encoder *,
			     const struct intel_crtc_state *,
			     const struct drm_connector_state *);
	void (*post_pll_disable)(struct intel_atomic_state *,
				 struct intel_encoder *,
				 const struct intel_crtc_state *,
				 const struct drm_connector_state *);
	void (*update_pipe)(struct intel_atomic_state *,
			    struct intel_encoder *,
			    const struct intel_crtc_state *,
			    const struct drm_connector_state *);
	 
	bool (*get_hw_state)(struct intel_encoder *, enum pipe *pipe);
	 
	void (*get_config)(struct intel_encoder *,
			   struct intel_crtc_state *pipe_config);

	 
	void (*sync_state)(struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state);

	 
	bool (*initial_fastset_check)(struct intel_encoder *encoder,
				      struct intel_crtc_state *crtc_state);

	 
	void (*get_power_domains)(struct intel_encoder *encoder,
				  struct intel_crtc_state *crtc_state);
	 
	void (*suspend)(struct intel_encoder *);
	 
	void (*suspend_complete)(struct intel_encoder *encoder);
	 
	void (*shutdown)(struct intel_encoder *encoder);
	 
	void (*shutdown_complete)(struct intel_encoder *encoder);
	 
	void (*enable_clock)(struct intel_encoder *encoder,
			     const struct intel_crtc_state *crtc_state);
	void (*disable_clock)(struct intel_encoder *encoder);
	 
	bool (*is_clock_enabled)(struct intel_encoder *encoder);
	 
	enum icl_port_dpll_id (*port_pll_type)(struct intel_encoder *encoder,
					       const struct intel_crtc_state *crtc_state);
	const struct intel_ddi_buf_trans *(*get_buf_trans)(struct intel_encoder *encoder,
							   const struct intel_crtc_state *crtc_state,
							   int *n_entries);
	void (*set_signal_levels)(struct intel_encoder *encoder,
				  const struct intel_crtc_state *crtc_state);

	enum hpd_pin hpd_pin;
	enum intel_display_power_domain power_domain;

	 
	const struct intel_bios_encoder_data *devdata;
};

struct intel_panel_bl_funcs {
	 
	int (*setup)(struct intel_connector *connector, enum pipe pipe);
	u32 (*get)(struct intel_connector *connector, enum pipe pipe);
	void (*set)(const struct drm_connector_state *conn_state, u32 level);
	void (*disable)(const struct drm_connector_state *conn_state, u32 level);
	void (*enable)(const struct intel_crtc_state *crtc_state,
		       const struct drm_connector_state *conn_state, u32 level);
	u32 (*hz_to_pwm)(struct intel_connector *connector, u32 hz);
};

enum drrs_type {
	DRRS_TYPE_NONE,
	DRRS_TYPE_STATIC,
	DRRS_TYPE_SEAMLESS,
};

struct intel_vbt_panel_data {
	struct drm_display_mode *lfp_lvds_vbt_mode;  
	struct drm_display_mode *sdvo_lvds_vbt_mode;  

	 
	int panel_type;
	unsigned int lvds_dither:1;
	unsigned int bios_lvds_val;  

	bool vrr;

	u8 seamless_drrs_min_refresh_rate;
	enum drrs_type drrs_type;

	struct {
		int max_link_rate;
		int rate;
		int lanes;
		int preemphasis;
		int vswing;
		int bpp;
		struct edp_power_seq pps;
		u8 drrs_msa_timing_delay;
		bool low_vswing;
		bool initialized;
		bool hobl;
	} edp;

	struct {
		bool enable;
		bool full_link;
		bool require_aux_wakeup;
		int idle_frames;
		int tp1_wakeup_time_us;
		int tp2_tp3_wakeup_time_us;
		int psr2_tp2_tp3_wakeup_time_us;
	} psr;

	struct {
		u16 pwm_freq_hz;
		u16 brightness_precision_bits;
		u16 hdr_dpcd_refresh_timeout;
		bool present;
		bool active_low_pwm;
		u8 min_brightness;	 
		s8 controller;		 
		enum intel_backlight_type type;
	} backlight;

	 
	struct {
		u16 panel_id;
		struct mipi_config *config;
		struct mipi_pps_data *pps;
		u16 bl_ports;
		u16 cabc_ports;
		u8 seq_version;
		u32 size;
		u8 *data;
		const u8 *sequence[MIPI_SEQ_MAX];
		u8 *deassert_seq;  
		enum drm_panel_orientation orientation;
	} dsi;
};

struct intel_panel {
	 
	const struct drm_edid *fixed_edid;

	struct list_head fixed_modes;

	 
	struct {
		bool present;
		u32 level;
		u32 min;
		u32 max;
		bool enabled;
		bool combination_mode;	 
		bool active_low_pwm;
		bool alternate_pwm_increment;	 

		 
		u32 pwm_level_min;
		u32 pwm_level_max;
		bool pwm_enabled;
		bool util_pin_active_low;	 
		u8 controller;		 
		struct pwm_device *pwm;
		struct pwm_state pwm_state;

		 
		union {
			struct {
				struct drm_edp_backlight_info info;
			} vesa;
			struct {
				bool sdr_uses_aux;
			} intel;
		} edp;

		struct backlight_device *device;

		const struct intel_panel_bl_funcs *funcs;
		const struct intel_panel_bl_funcs *pwm_funcs;
		void (*power)(struct intel_connector *, bool enable);
	} backlight;

	struct intel_vbt_panel_data vbt;
};

struct intel_digital_port;

enum check_link_response {
	HDCP_LINK_PROTECTED	= 0,
	HDCP_TOPOLOGY_CHANGE,
	HDCP_LINK_INTEGRITY_FAILURE,
	HDCP_REAUTH_REQUEST
};

 
struct intel_hdcp_shim {
	 
	int (*write_an_aksv)(struct intel_digital_port *dig_port, u8 *an);

	 
	int (*read_bksv)(struct intel_digital_port *dig_port, u8 *bksv);

	 
	int (*read_bstatus)(struct intel_digital_port *dig_port,
			    u8 *bstatus);

	 
	int (*repeater_present)(struct intel_digital_port *dig_port,
				bool *repeater_present);

	 
	int (*read_ri_prime)(struct intel_digital_port *dig_port, u8 *ri);

	 
	int (*read_ksv_ready)(struct intel_digital_port *dig_port,
			      bool *ksv_ready);

	 
	int (*read_ksv_fifo)(struct intel_digital_port *dig_port,
			     int num_downstream, u8 *ksv_fifo);

	 
	int (*read_v_prime_part)(struct intel_digital_port *dig_port,
				 int i, u32 *part);

	 
	int (*toggle_signalling)(struct intel_digital_port *dig_port,
				 enum transcoder cpu_transcoder,
				 bool enable);

	 
	int (*stream_encryption)(struct intel_connector *connector,
				 bool enable);

	 
	bool (*check_link)(struct intel_digital_port *dig_port,
			   struct intel_connector *connector);

	 
	int (*hdcp_capable)(struct intel_digital_port *dig_port,
			    bool *hdcp_capable);

	 
	enum hdcp_wired_protocol protocol;

	 
	int (*hdcp_2_2_capable)(struct intel_digital_port *dig_port,
				bool *capable);

	 
	int (*write_2_2_msg)(struct intel_digital_port *dig_port,
			     void *buf, size_t size);

	 
	int (*read_2_2_msg)(struct intel_digital_port *dig_port,
			    u8 msg_id, void *buf, size_t size);

	 
	int (*config_stream_type)(struct intel_digital_port *dig_port,
				  bool is_repeater, u8 type);

	 
	int (*stream_2_2_encryption)(struct intel_connector *connector,
				     bool enable);

	 
	int (*check_2_2_link)(struct intel_digital_port *dig_port,
			      struct intel_connector *connector);
};

struct intel_hdcp {
	const struct intel_hdcp_shim *shim;
	 
	struct mutex mutex;
	u64 value;
	struct delayed_work check_work;
	struct work_struct prop_work;

	 
	bool hdcp_encrypted;

	 
	 
	bool hdcp2_supported;

	 
	bool hdcp2_encrypted;

	 
	u8 content_type;

	bool is_paired;
	bool is_repeater;

	 
	u32 seq_num_v;

	 
	u32 seq_num_m;

	 
	wait_queue_head_t cp_irq_queue;
	atomic_t cp_irq_count;
	int cp_irq_count_cached;

	 
	enum transcoder cpu_transcoder;
	 
	enum transcoder stream_transcoder;
};

struct intel_connector {
	struct drm_connector base;
	 
	struct intel_encoder *encoder;

	 
	u32 acpi_device_id;

	 
	bool (*get_hw_state)(struct intel_connector *);

	 
	struct intel_panel panel;

	 
	const struct drm_edid *detect_edid;

	 
	int hotplug_retries;

	 
	u8 polled;

	struct drm_dp_mst_port *port;

	struct intel_dp *mst_port;

	 
	struct work_struct modeset_retry_work;

	struct intel_hdcp hdcp;
};

struct intel_digital_connector_state {
	struct drm_connector_state base;

	enum hdmi_force_audio force_audio;
	int broadcast_rgb;
};

#define to_intel_digital_connector_state(x) container_of(x, struct intel_digital_connector_state, base)

struct dpll {
	 
	int n;
	int m1, m2;
	int p1, p2;
	 
	int	dot;
	int	vco;
	int	m;
	int	p;
};

struct intel_atomic_state {
	struct drm_atomic_state base;

	intel_wakeref_t wakeref;

	struct __intel_global_objs_state *global_objs;
	int num_global_objs;

	 
	bool internal;

	bool dpll_set, modeset;

	struct intel_shared_dpll_state shared_dpll[I915_NUM_PLLS];

	 
	bool skip_intermediate_wm;

	bool rps_interactive;

	struct i915_sw_fence commit_ready;

	struct llist_node freed;
};

struct intel_plane_state {
	struct drm_plane_state uapi;

	 
	struct {
		struct drm_crtc *crtc;
		struct drm_framebuffer *fb;

		u16 alpha;
		u16 pixel_blend_mode;
		unsigned int rotation;
		enum drm_color_encoding color_encoding;
		enum drm_color_range color_range;
		enum drm_scaling_filter scaling_filter;
	} hw;

	struct i915_vma *ggtt_vma;
	struct i915_vma *dpt_vma;
	unsigned long flags;
#define PLANE_HAS_FENCE BIT(0)

	struct intel_fb_view view;

	 
	bool decrypt;

	 
	bool force_black;

	 
	u32 ctl;

	 
	u32 color_ctl;

	 
	u32 cus_ctl;

	 
	int scaler_id;

	 
	struct intel_plane *planar_linked_plane;

	 
	u32 planar_slave;

	struct drm_intel_sprite_colorkey ckey;

	struct drm_rect psr2_sel_fetch_area;

	 
	u64 ccval;

	const char *no_fbc_reason;
};

struct intel_initial_plane_config {
	struct intel_framebuffer *fb;
	struct i915_vma *vma;
	unsigned int tiling;
	int size;
	u32 base;
	u8 rotation;
};

struct intel_scaler {
	int in_use;
	u32 mode;
};

struct intel_crtc_scaler_state {
#define SKL_NUM_SCALERS 2
	struct intel_scaler scalers[SKL_NUM_SCALERS];

	 
#define SKL_CRTC_INDEX 31
	unsigned scaler_users;

	 
	int scaler_id;
};

 
 
#define I915_MODE_FLAG_GET_SCANLINE_FROM_TIMESTAMP (1<<1)
 
#define I915_MODE_FLAG_USE_SCANLINE_COUNTER (1<<2)
 
#define I915_MODE_FLAG_DSI_USE_TE0 (1<<3)
 
#define I915_MODE_FLAG_DSI_USE_TE1 (1<<4)
 
#define I915_MODE_FLAG_DSI_PERIODIC_CMD_MODE (1<<5)
 
#define I915_MODE_FLAG_VRR (1<<6)

struct intel_wm_level {
	bool enable;
	u32 pri_val;
	u32 spr_val;
	u32 cur_val;
	u32 fbc_val;
};

struct intel_pipe_wm {
	struct intel_wm_level wm[5];
	bool fbc_wm_enabled;
	bool pipe_enabled;
	bool sprites_enabled;
	bool sprites_scaled;
};

struct skl_wm_level {
	u16 min_ddb_alloc;
	u16 blocks;
	u8 lines;
	bool enable;
	bool ignore_lines;
	bool can_sagv;
};

struct skl_plane_wm {
	struct skl_wm_level wm[8];
	struct skl_wm_level uv_wm[8];
	struct skl_wm_level trans_wm;
	struct {
		struct skl_wm_level wm0;
		struct skl_wm_level trans_wm;
	} sagv;
	bool is_planar;
};

struct skl_pipe_wm {
	struct skl_plane_wm planes[I915_MAX_PLANES];
	bool use_sagv_wm;
};

enum vlv_wm_level {
	VLV_WM_LEVEL_PM2,
	VLV_WM_LEVEL_PM5,
	VLV_WM_LEVEL_DDR_DVFS,
	NUM_VLV_WM_LEVELS,
};

struct vlv_wm_state {
	struct g4x_pipe_wm wm[NUM_VLV_WM_LEVELS];
	struct g4x_sr_wm sr[NUM_VLV_WM_LEVELS];
	u8 num_levels;
	bool cxsr;
};

struct vlv_fifo_state {
	u16 plane[I915_MAX_PLANES];
};

enum g4x_wm_level {
	G4X_WM_LEVEL_NORMAL,
	G4X_WM_LEVEL_SR,
	G4X_WM_LEVEL_HPLL,
	NUM_G4X_WM_LEVELS,
};

struct g4x_wm_state {
	struct g4x_pipe_wm wm;
	struct g4x_sr_wm sr;
	struct g4x_sr_wm hpll;
	bool cxsr;
	bool hpll_en;
	bool fbc_en;
};

struct intel_crtc_wm_state {
	union {
		 
		struct {
			struct intel_pipe_wm intermediate;
			struct intel_pipe_wm optimal;
		} ilk;

		struct {
			struct skl_pipe_wm raw;
			 
			struct skl_pipe_wm optimal;
			struct skl_ddb_entry ddb;
			 
			struct skl_ddb_entry plane_ddb[I915_MAX_PLANES];
			 
			struct skl_ddb_entry plane_ddb_y[I915_MAX_PLANES];
		} skl;

		struct {
			struct g4x_pipe_wm raw[NUM_VLV_WM_LEVELS];  
			struct vlv_wm_state intermediate;  
			struct vlv_wm_state optimal;  
			struct vlv_fifo_state fifo_state;
		} vlv;

		struct {
			struct g4x_pipe_wm raw[NUM_G4X_WM_LEVELS];
			struct g4x_wm_state intermediate;
			struct g4x_wm_state optimal;
		} g4x;
	};

	 
	bool need_postvbl_update;
};

enum intel_output_format {
	INTEL_OUTPUT_FORMAT_RGB,
	INTEL_OUTPUT_FORMAT_YCBCR420,
	INTEL_OUTPUT_FORMAT_YCBCR444,
};

struct intel_mpllb_state {
	u32 clock;  
	u32 ref_control;
	u32 mpllb_cp;
	u32 mpllb_div;
	u32 mpllb_div2;
	u32 mpllb_fracn1;
	u32 mpllb_fracn2;
	u32 mpllb_sscen;
	u32 mpllb_sscstep;
};

 
struct intel_link_m_n {
	u32 tu;
	u32 data_m;
	u32 data_n;
	u32 link_m;
	u32 link_n;
};

struct intel_csc_matrix {
	u16 coeff[9];
	u16 preoff[3];
	u16 postoff[3];
};

struct intel_c10pll_state {
	u32 clock;  
	u8 tx;
	u8 cmn;
	u8 pll[20];
};

struct intel_c20pll_state {
	u32 link_bit_rate;
	u32 clock;  
	u16 tx[3];
	u16 cmn[4];
	union {
		u16 mplla[10];
		u16 mpllb[11];
	};
};

struct intel_cx0pll_state {
	union {
		struct intel_c10pll_state c10;
		struct intel_c20pll_state c20;
	};
	bool ssc_enabled;
};

struct intel_crtc_state {
	 
	struct drm_crtc_state uapi;

	 
	struct {
		bool active, enable;
		 
		struct drm_property_blob *degamma_lut, *gamma_lut, *ctm;
		struct drm_display_mode mode, pipe_mode, adjusted_mode;
		enum drm_scaling_filter scaling_filter;
	} hw;

	 
	struct drm_property_blob *pre_csc_lut, *post_csc_lut;

	struct intel_csc_matrix csc, output_csc;

	 
#define PIPE_CONFIG_QUIRK_MODE_SYNC_FLAGS	(1<<0)  
	unsigned long quirks;

	unsigned fb_bits;  
	bool update_pipe;  
	bool disable_cxsr;
	bool update_wm_pre, update_wm_post;  
	bool fifo_changed;  
	bool preload_luts;
	bool inherited;  

	 
	bool do_async_flip;

	 
	struct drm_rect pipe_src;

	 
	unsigned int pixel_rate;

	 
	bool has_pch_encoder;

	 
	bool has_infoframe;

	 
	enum transcoder cpu_transcoder;

	 
	bool limited_color_range;

	 
	unsigned int output_types;

	 
	bool has_hdmi_sink;

	 
	bool has_audio;

	 
	bool dither;

	 
	bool dither_force_disable;

	 
	bool clock_set;

	 
	bool sdvo_tv_clock;

	 
	bool bw_constrained;

	 
	struct dpll dpll;

	 
	struct intel_shared_dpll *shared_dpll;

	 
	union {
		struct intel_dpll_hw_state dpll_hw_state;
		struct intel_mpllb_state mpllb_state;
		struct intel_cx0pll_state cx0pll_state;
	};

	 
	struct icl_port_dpll {
		struct intel_shared_dpll *pll;
		struct intel_dpll_hw_state hw_state;
	} icl_port_dplls[ICL_PORT_DPLL_COUNT];

	 
	struct {
		u32 ctrl, div;
	} dsi_pll;

	int pipe_bpp;
	struct intel_link_m_n dp_m_n;

	 
	struct intel_link_m_n dp_m2_n2;
	bool has_drrs;
	bool seamless_m_n;

	 
	bool has_psr;
	bool has_psr2;
	bool enable_psr2_sel_fetch;
	bool req_psr2_sdp_prior_scanline;
	bool wm_level_disabled;
	u32 dc3co_exitline;
	u16 su_y_granularity;
	struct drm_dp_vsc_sdp psr_vsc;

	 
	int port_clock;

	 
	unsigned pixel_multiplier;

	 
	u8 mode_flags;

	u8 lane_count;

	 
	u8 lane_lat_optim_mask;

	 
	u8 min_voltage_level;

	 
	struct {
		u32 control;
		u32 pgm_ratios;
		u32 lvds_border_bits;
	} gmch_pfit;

	 
	struct {
		struct drm_rect dst;
		bool enabled;
		bool force_thru;
	} pch_pfit;

	 
	int fdi_lanes;
	struct intel_link_m_n fdi_m_n;

	bool ips_enabled;

	bool crc_enabled;

	bool double_wide;

	int pbn;

	struct intel_crtc_scaler_state scaler_state;

	 
	enum pipe hsw_workaround_pipe;

	 
	bool disable_lp_wm;

	struct intel_crtc_wm_state wm;

	int min_cdclk[I915_MAX_PLANES];

	 
	u32 data_rate[I915_MAX_PLANES];
	 
	u32 data_rate_y[I915_MAX_PLANES];

	 
	u64 rel_data_rate[I915_MAX_PLANES];
	u64 rel_data_rate_y[I915_MAX_PLANES];

	 
	u32 gamma_mode;

	union {
		 
		u32 csc_mode;

		 
		u32 cgm_mode;
	};

	 
	u8 enabled_planes;

	 
	u8 active_planes;
	u8 scaled_planes;
	u8 nv12_planes;
	u8 c8_planes;

	 
	u8 update_planes;

	 
	u8 async_flip_planes;

	u8 framestart_delay;  
	u8 msa_timing_delay;  

	struct {
		u32 enable;
		u32 gcp;
		union hdmi_infoframe avi;
		union hdmi_infoframe spd;
		union hdmi_infoframe hdmi;
		union hdmi_infoframe drm;
		struct drm_dp_vsc_sdp vsc;
	} infoframes;

	u8 eld[MAX_ELD_BYTES];

	 
	bool hdmi_scrambling;

	 
	bool hdmi_high_tmds_clock_ratio;

	 
	enum intel_output_format output_format;

	 
	enum intel_output_format sink_format;

	 
	bool gamma_enable;

	 
	bool csc_enable;

	 
	bool wgc_enable;

	 
	u8 bigjoiner_pipes;

	 
	struct {
		bool compression_enable;
		bool dsc_split;
		u16 compressed_bpp;
		u8 slice_count;
		struct drm_dsc_config config;
	} dsc;

	 
	u16 linetime;
	u16 ips_linetime;

	bool enhanced_framing;

	 
	bool fec_enable;

	bool sdp_split_enable;

	 
	enum transcoder master_transcoder;

	 
	u8 sync_mode_slaves_mask;

	 
	enum transcoder mst_master_transcoder;

	 
	struct intel_dsb *dsb;

	u32 psr2_man_track_ctl;

	 
	struct {
		bool enable;
		u8 pipeline_full;
		u16 flipline, vmin, vmax, guardband;
	} vrr;

	 
	struct {
		bool enable;
		u8 link_count;
		u8 pixel_overlap;
	} splitter;

	 
	struct drm_vblank_work vblank_work;
};

enum intel_pipe_crc_source {
	INTEL_PIPE_CRC_SOURCE_NONE,
	INTEL_PIPE_CRC_SOURCE_PLANE1,
	INTEL_PIPE_CRC_SOURCE_PLANE2,
	INTEL_PIPE_CRC_SOURCE_PLANE3,
	INTEL_PIPE_CRC_SOURCE_PLANE4,
	INTEL_PIPE_CRC_SOURCE_PLANE5,
	INTEL_PIPE_CRC_SOURCE_PLANE6,
	INTEL_PIPE_CRC_SOURCE_PLANE7,
	INTEL_PIPE_CRC_SOURCE_PIPE,
	 
	INTEL_PIPE_CRC_SOURCE_TV,
	INTEL_PIPE_CRC_SOURCE_DP_B,
	INTEL_PIPE_CRC_SOURCE_DP_C,
	INTEL_PIPE_CRC_SOURCE_DP_D,
	INTEL_PIPE_CRC_SOURCE_AUTO,
	INTEL_PIPE_CRC_SOURCE_MAX,
};

enum drrs_refresh_rate {
	DRRS_REFRESH_RATE_HIGH,
	DRRS_REFRESH_RATE_LOW,
};

#define INTEL_PIPE_CRC_ENTRIES_NR	128
struct intel_pipe_crc {
	spinlock_t lock;
	int skipped;
	enum intel_pipe_crc_source source;
};

struct intel_crtc {
	struct drm_crtc base;
	enum pipe pipe;
	 
	bool active;
	u8 plane_ids_mask;

	 
	u8 mode_flags;

	u16 vmax_vblank_start;

	struct intel_display_power_domain_set enabled_power_domains;
	struct intel_display_power_domain_set hw_readout_power_domains;
	struct intel_overlay *overlay;

	struct intel_crtc_state *config;

	 
	bool cpu_fifo_underrun_disabled;
	bool pch_fifo_underrun_disabled;

	 
	struct {
		 
		union {
			struct intel_pipe_wm ilk;
			struct vlv_wm_state vlv;
			struct g4x_wm_state g4x;
		} active;
	} wm;

	struct {
		struct mutex mutex;
		struct delayed_work work;
		enum drrs_refresh_rate refresh_rate;
		unsigned int frontbuffer_bits;
		unsigned int busy_frontbuffer_bits;
		enum transcoder cpu_transcoder;
		struct intel_link_m_n m_n, m2_n2;
	} drrs;

	int scanline_offset;

	struct {
		unsigned start_vbl_count;
		ktime_t start_vbl_time;
		int min_vbl, max_vbl;
		int scanline_start;
#ifdef CONFIG_DRM_I915_DEBUG_VBLANK_EVADE
		struct {
			u64 min;
			u64 max;
			u64 sum;
			unsigned int over;
			unsigned int times[17];  
		} vbl;
#endif
	} debug;

	 
	int num_scalers;

	 
	struct pm_qos_request vblank_pm_qos;

#ifdef CONFIG_DEBUG_FS
	struct intel_pipe_crc pipe_crc;
#endif
};

struct intel_plane {
	struct drm_plane base;
	enum i9xx_plane_id i9xx_plane;
	enum plane_id id;
	enum pipe pipe;
	bool need_async_flip_disable_wa;
	u32 frontbuffer_bit;

	struct {
		u32 base, cntl, size;
	} cursor;

	struct intel_fbc *fbc;

	 

	int (*min_width)(const struct drm_framebuffer *fb,
			 int color_plane,
			 unsigned int rotation);
	int (*max_width)(const struct drm_framebuffer *fb,
			 int color_plane,
			 unsigned int rotation);
	int (*max_height)(const struct drm_framebuffer *fb,
			  int color_plane,
			  unsigned int rotation);
	unsigned int (*max_stride)(struct intel_plane *plane,
				   u32 pixel_format, u64 modifier,
				   unsigned int rotation);
	 
	void (*update_noarm)(struct intel_plane *plane,
			     const struct intel_crtc_state *crtc_state,
			     const struct intel_plane_state *plane_state);
	 
	void (*update_arm)(struct intel_plane *plane,
			   const struct intel_crtc_state *crtc_state,
			   const struct intel_plane_state *plane_state);
	 
	void (*disable_arm)(struct intel_plane *plane,
			    const struct intel_crtc_state *crtc_state);
	bool (*get_hw_state)(struct intel_plane *plane, enum pipe *pipe);
	int (*check_plane)(struct intel_crtc_state *crtc_state,
			   struct intel_plane_state *plane_state);
	int (*min_cdclk)(const struct intel_crtc_state *crtc_state,
			 const struct intel_plane_state *plane_state);
	void (*async_flip)(struct intel_plane *plane,
			   const struct intel_crtc_state *crtc_state,
			   const struct intel_plane_state *plane_state,
			   bool async_flip);
	void (*enable_flip_done)(struct intel_plane *plane);
	void (*disable_flip_done)(struct intel_plane *plane);
};

struct intel_watermark_params {
	u16 fifo_size;
	u16 max_wm;
	u8 default_wm;
	u8 guard_size;
	u8 cacheline_size;
};

#define to_intel_atomic_state(x) container_of(x, struct intel_atomic_state, base)
#define to_intel_crtc(x) container_of(x, struct intel_crtc, base)
#define to_intel_crtc_state(x) container_of(x, struct intel_crtc_state, uapi)
#define to_intel_connector(x) container_of(x, struct intel_connector, base)
#define to_intel_encoder(x) container_of(x, struct intel_encoder, base)
#define to_intel_framebuffer(x) container_of(x, struct intel_framebuffer, base)
#define to_intel_plane(x) container_of(x, struct intel_plane, base)
#define to_intel_plane_state(x) container_of(x, struct intel_plane_state, uapi)
#define intel_fb_obj(x) ((x) ? to_intel_bo((x)->obj[0]) : NULL)

struct intel_hdmi {
	i915_reg_t hdmi_reg;
	int ddc_bus;
	struct {
		enum drm_dp_dual_mode_type type;
		int max_tmds_clock;
	} dp_dual_mode;
	struct intel_connector *attached_connector;
	struct cec_notifier *cec_notifier;
};

struct intel_dp_mst_encoder;

struct intel_dp_compliance_data {
	unsigned long edid;
	u8 video_pattern;
	u16 hdisplay, vdisplay;
	u8 bpc;
	struct drm_dp_phy_test_params phytest;
};

struct intel_dp_compliance {
	unsigned long test_type;
	struct intel_dp_compliance_data test_data;
	bool test_active;
	int test_link_rate;
	u8 test_lane_count;
};

struct intel_dp_pcon_frl {
	bool is_trained;
	int trained_rate_gbps;
};

struct intel_pps {
	int panel_power_up_delay;
	int panel_power_down_delay;
	int panel_power_cycle_delay;
	int backlight_on_delay;
	int backlight_off_delay;
	struct delayed_work panel_vdd_work;
	bool want_panel_vdd;
	bool initializing;
	unsigned long last_power_on;
	unsigned long last_backlight_off;
	ktime_t panel_power_off_time;
	intel_wakeref_t vdd_wakeref;

	union {
		 
		enum pipe pps_pipe;

		 
		int pps_idx;
	};

	 
	enum pipe active_pipe;
	 
	bool pps_reset;
	struct edp_power_seq pps_delays;
	struct edp_power_seq bios_pps_delays;
};

struct intel_psr {
	 
	struct mutex lock;

#define I915_PSR_DEBUG_MODE_MASK	0x0f
#define I915_PSR_DEBUG_DEFAULT		0x00
#define I915_PSR_DEBUG_DISABLE		0x01
#define I915_PSR_DEBUG_ENABLE		0x02
#define I915_PSR_DEBUG_FORCE_PSR1	0x03
#define I915_PSR_DEBUG_ENABLE_SEL_FETCH	0x4
#define I915_PSR_DEBUG_IRQ		0x10

	u32 debug;
	bool sink_support;
	bool source_support;
	bool enabled;
	bool paused;
	enum pipe pipe;
	enum transcoder transcoder;
	bool active;
	struct work_struct work;
	unsigned int busy_frontbuffer_bits;
	bool sink_psr2_support;
	bool link_standby;
	bool colorimetry_support;
	bool psr2_enabled;
	bool psr2_sel_fetch_enabled;
	bool psr2_sel_fetch_cff_enabled;
	bool req_psr2_sdp_prior_scanline;
	u8 sink_sync_latency;
	u8 io_wake_lines;
	u8 fast_wake_lines;
	ktime_t last_entry_attempt;
	ktime_t last_exit;
	bool sink_not_reliable;
	bool irq_aux_error;
	u16 su_w_granularity;
	u16 su_y_granularity;
	u32 dc3co_exitline;
	u32 dc3co_exit_delay;
	struct delayed_work dc3co_work;
};

struct intel_dp {
	i915_reg_t output_reg;
	u32 DP;
	int link_rate;
	u8 lane_count;
	u8 sink_count;
	bool link_trained;
	bool reset_link_params;
	bool use_max_params;
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	u8 psr_dpcd[EDP_PSR_RECEIVER_CAP_SIZE];
	u8 downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
	u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE];
	u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE];
	u8 lttpr_common_caps[DP_LTTPR_COMMON_CAP_SIZE];
	u8 lttpr_phy_caps[DP_MAX_LTTPR_COUNT][DP_LTTPR_PHY_CAP_SIZE];
	u8 fec_capable;
	u8 pcon_dsc_dpcd[DP_PCON_DSC_ENCODER_CAP_SIZE];
	 
	int num_source_rates;
	const int *source_rates;
	 
	int num_sink_rates;
	int sink_rates[DP_MAX_SUPPORTED_RATES];
	bool use_rate_select;
	 
	int max_sink_lane_count;
	 
	int num_common_rates;
	int common_rates[DP_MAX_SUPPORTED_RATES];
	 
	int max_link_lane_count;
	 
	int max_link_rate;
	int mso_link_count;
	int mso_pixel_overlap;
	 
	struct drm_dp_desc desc;
	struct drm_dp_aux aux;
	u32 aux_busy_last_status;
	u8 train_set[4];

	struct intel_pps pps;

	bool is_mst;
	int active_mst_links;

	 
	struct intel_connector *attached_connector;

	 
	struct intel_dp_mst_encoder *mst_encoders[I915_MAX_PIPES];
	struct drm_dp_mst_topology_mgr mst_mgr;

	u32 (*get_aux_clock_divider)(struct intel_dp *dp, int index);
	 
	u32 (*get_aux_send_ctl)(struct intel_dp *dp, int send_bytes,
				u32 aux_clock_divider);

	i915_reg_t (*aux_ch_ctl_reg)(struct intel_dp *dp);
	i915_reg_t (*aux_ch_data_reg)(struct intel_dp *dp, int index);

	 
	void (*prepare_link_retrain)(struct intel_dp *intel_dp,
				     const struct intel_crtc_state *crtc_state);
	void (*set_link_train)(struct intel_dp *intel_dp,
			       const struct intel_crtc_state *crtc_state,
			       u8 dp_train_pat);
	void (*set_idle_link_train)(struct intel_dp *intel_dp,
				    const struct intel_crtc_state *crtc_state);

	u8 (*preemph_max)(struct intel_dp *intel_dp);
	u8 (*voltage_max)(struct intel_dp *intel_dp,
			  const struct intel_crtc_state *crtc_state);

	 
	struct intel_dp_compliance compliance;

	 
	struct {
		int min_tmds_clock, max_tmds_clock;
		int max_dotclock;
		int pcon_max_frl_bw;
		u8 max_bpc;
		bool ycbcr_444_to_420;
		bool ycbcr420_passthrough;
		bool rgb_to_ycbcr;
	} dfp;

	 
	struct pm_qos_request pm_qos;

	 
	bool force_dsc_en;
	int force_dsc_output_format;
	int force_dsc_bpc;

	bool hobl_failed;
	bool hobl_active;

	struct intel_dp_pcon_frl frl;

	struct intel_psr psr;

	 
	unsigned long last_oui_write;
};

enum lspcon_vendor {
	LSPCON_VENDOR_MCA,
	LSPCON_VENDOR_PARADE
};

struct intel_lspcon {
	bool active;
	bool hdr_supported;
	enum drm_lspcon_mode mode;
	enum lspcon_vendor vendor;
};

struct intel_digital_port {
	struct intel_encoder base;
	u32 saved_port_bits;
	struct intel_dp dp;
	struct intel_hdmi hdmi;
	struct intel_lspcon lspcon;
	enum irqreturn (*hpd_pulse)(struct intel_digital_port *, bool);
	bool release_cl2_override;
	u8 max_lanes;
	 
	enum aux_ch aux_ch;
	enum intel_display_power_domain ddi_io_power_domain;
	intel_wakeref_t ddi_io_wakeref;
	intel_wakeref_t aux_wakeref;

	struct intel_tc_port *tc;

	 
	struct mutex hdcp_mutex;
	 
	unsigned int num_hdcp_streams;
	 
	bool hdcp_auth_status;
	 
	struct hdcp_port_data hdcp_port_data;
	 
	bool hdcp_mst_type1_capable;

	void (*write_infoframe)(struct intel_encoder *encoder,
				const struct intel_crtc_state *crtc_state,
				unsigned int type,
				const void *frame, ssize_t len);
	void (*read_infoframe)(struct intel_encoder *encoder,
			       const struct intel_crtc_state *crtc_state,
			       unsigned int type,
			       void *frame, ssize_t len);
	void (*set_infoframes)(struct intel_encoder *encoder,
			       bool enable,
			       const struct intel_crtc_state *crtc_state,
			       const struct drm_connector_state *conn_state);
	u32 (*infoframes_enabled)(struct intel_encoder *encoder,
				  const struct intel_crtc_state *pipe_config);
	bool (*connected)(struct intel_encoder *encoder);
};

struct intel_dp_mst_encoder {
	struct intel_encoder base;
	enum pipe pipe;
	struct intel_digital_port *primary;
	struct intel_connector *connector;
};

static inline struct intel_encoder *
intel_attached_encoder(struct intel_connector *connector)
{
	return connector->encoder;
}

static inline bool intel_encoder_is_dig_port(struct intel_encoder *encoder)
{
	switch (encoder->type) {
	case INTEL_OUTPUT_DDI:
	case INTEL_OUTPUT_DP:
	case INTEL_OUTPUT_EDP:
	case INTEL_OUTPUT_HDMI:
		return true;
	default:
		return false;
	}
}

static inline bool intel_encoder_is_mst(struct intel_encoder *encoder)
{
	return encoder->type == INTEL_OUTPUT_DP_MST;
}

static inline struct intel_dp_mst_encoder *
enc_to_mst(struct intel_encoder *encoder)
{
	return container_of(&encoder->base, struct intel_dp_mst_encoder,
			    base.base);
}

static inline struct intel_digital_port *
enc_to_dig_port(struct intel_encoder *encoder)
{
	struct intel_encoder *intel_encoder = encoder;

	if (intel_encoder_is_dig_port(intel_encoder))
		return container_of(&encoder->base, struct intel_digital_port,
				    base.base);
	else if (intel_encoder_is_mst(intel_encoder))
		return enc_to_mst(encoder)->primary;
	else
		return NULL;
}

static inline struct intel_digital_port *
intel_attached_dig_port(struct intel_connector *connector)
{
	return enc_to_dig_port(intel_attached_encoder(connector));
}

static inline struct intel_hdmi *
enc_to_intel_hdmi(struct intel_encoder *encoder)
{
	return &enc_to_dig_port(encoder)->hdmi;
}

static inline struct intel_hdmi *
intel_attached_hdmi(struct intel_connector *connector)
{
	return enc_to_intel_hdmi(intel_attached_encoder(connector));
}

static inline struct intel_dp *enc_to_intel_dp(struct intel_encoder *encoder)
{
	return &enc_to_dig_port(encoder)->dp;
}

static inline struct intel_dp *intel_attached_dp(struct intel_connector *connector)
{
	return enc_to_intel_dp(intel_attached_encoder(connector));
}

static inline bool intel_encoder_is_dp(struct intel_encoder *encoder)
{
	switch (encoder->type) {
	case INTEL_OUTPUT_DP:
	case INTEL_OUTPUT_EDP:
		return true;
	case INTEL_OUTPUT_DDI:
		 
		return i915_mmio_reg_valid(enc_to_intel_dp(encoder)->output_reg);
	default:
		return false;
	}
}

static inline struct intel_lspcon *
enc_to_intel_lspcon(struct intel_encoder *encoder)
{
	return &enc_to_dig_port(encoder)->lspcon;
}

static inline struct intel_digital_port *
dp_to_dig_port(struct intel_dp *intel_dp)
{
	return container_of(intel_dp, struct intel_digital_port, dp);
}

static inline struct intel_lspcon *
dp_to_lspcon(struct intel_dp *intel_dp)
{
	return &dp_to_dig_port(intel_dp)->lspcon;
}

#define dp_to_i915(__intel_dp) to_i915(dp_to_dig_port(__intel_dp)->base.base.dev)

#define CAN_PSR(intel_dp) ((intel_dp)->psr.sink_support && \
			   (intel_dp)->psr.source_support)

static inline bool intel_encoder_can_psr(struct intel_encoder *encoder)
{
	if (!intel_encoder_is_dp(encoder))
		return false;

	return CAN_PSR(enc_to_intel_dp(encoder));
}

static inline struct intel_digital_port *
hdmi_to_dig_port(struct intel_hdmi *intel_hdmi)
{
	return container_of(intel_hdmi, struct intel_digital_port, hdmi);
}

static inline struct intel_plane_state *
intel_atomic_get_plane_state(struct intel_atomic_state *state,
				 struct intel_plane *plane)
{
	struct drm_plane_state *ret =
		drm_atomic_get_plane_state(&state->base, &plane->base);

	if (IS_ERR(ret))
		return ERR_CAST(ret);

	return to_intel_plane_state(ret);
}

static inline struct intel_plane_state *
intel_atomic_get_old_plane_state(struct intel_atomic_state *state,
				 struct intel_plane *plane)
{
	return to_intel_plane_state(drm_atomic_get_old_plane_state(&state->base,
								   &plane->base));
}

static inline struct intel_plane_state *
intel_atomic_get_new_plane_state(struct intel_atomic_state *state,
				 struct intel_plane *plane)
{
	return to_intel_plane_state(drm_atomic_get_new_plane_state(&state->base,
								   &plane->base));
}

static inline struct intel_crtc_state *
intel_atomic_get_old_crtc_state(struct intel_atomic_state *state,
				struct intel_crtc *crtc)
{
	return to_intel_crtc_state(drm_atomic_get_old_crtc_state(&state->base,
								 &crtc->base));
}

static inline struct intel_crtc_state *
intel_atomic_get_new_crtc_state(struct intel_atomic_state *state,
				struct intel_crtc *crtc)
{
	return to_intel_crtc_state(drm_atomic_get_new_crtc_state(&state->base,
								 &crtc->base));
}

static inline struct intel_digital_connector_state *
intel_atomic_get_new_connector_state(struct intel_atomic_state *state,
				     struct intel_connector *connector)
{
	return to_intel_digital_connector_state(
			drm_atomic_get_new_connector_state(&state->base,
			&connector->base));
}

static inline struct intel_digital_connector_state *
intel_atomic_get_old_connector_state(struct intel_atomic_state *state,
				     struct intel_connector *connector)
{
	return to_intel_digital_connector_state(
			drm_atomic_get_old_connector_state(&state->base,
			&connector->base));
}

 
static inline bool
intel_crtc_has_type(const struct intel_crtc_state *crtc_state,
		    enum intel_output_type type)
{
	return crtc_state->output_types & BIT(type);
}

static inline bool
intel_crtc_has_dp_encoder(const struct intel_crtc_state *crtc_state)
{
	return crtc_state->output_types &
		(BIT(INTEL_OUTPUT_DP) |
		 BIT(INTEL_OUTPUT_DP_MST) |
		 BIT(INTEL_OUTPUT_EDP));
}

static inline bool
intel_crtc_needs_modeset(const struct intel_crtc_state *crtc_state)
{
	return drm_atomic_crtc_needs_modeset(&crtc_state->uapi);
}

static inline bool
intel_crtc_needs_fastset(const struct intel_crtc_state *crtc_state)
{
	return crtc_state->update_pipe;
}

static inline bool
intel_crtc_needs_color_update(const struct intel_crtc_state *crtc_state)
{
	return crtc_state->uapi.color_mgmt_changed ||
		intel_crtc_needs_fastset(crtc_state) ||
		intel_crtc_needs_modeset(crtc_state);
}

static inline u32 intel_plane_ggtt_offset(const struct intel_plane_state *plane_state)
{
	return i915_ggtt_offset(plane_state->ggtt_vma);
}

static inline struct intel_frontbuffer *
to_intel_frontbuffer(struct drm_framebuffer *fb)
{
	return fb ? to_intel_framebuffer(fb)->frontbuffer : NULL;
}

#endif  
