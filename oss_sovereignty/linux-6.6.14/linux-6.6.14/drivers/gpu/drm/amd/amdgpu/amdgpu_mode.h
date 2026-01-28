#ifndef AMDGPU_MODE_H
#define AMDGPU_MODE_H
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fixed.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_probe_helper.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/hrtimer.h>
#include "amdgpu_irq.h"
#include <drm/display/drm_dp_mst_helper.h>
#include "modules/inc/mod_freesync.h"
#include "amdgpu_dm_irq_params.h"
struct amdgpu_bo;
struct amdgpu_device;
struct amdgpu_encoder;
struct amdgpu_router;
struct amdgpu_hpd;
#define to_amdgpu_crtc(x) container_of(x, struct amdgpu_crtc, base)
#define to_amdgpu_connector(x) container_of(x, struct amdgpu_connector, base)
#define to_amdgpu_encoder(x) container_of(x, struct amdgpu_encoder, base)
#define to_amdgpu_framebuffer(x) container_of(x, struct amdgpu_framebuffer, base)
#define to_dm_plane_state(x)	container_of(x, struct dm_plane_state, base)
#define AMDGPU_MAX_HPD_PINS 6
#define AMDGPU_MAX_CRTCS 6
#define AMDGPU_MAX_PLANES 6
#define AMDGPU_MAX_AFMT_BLOCKS 9
enum amdgpu_rmx_type {
	RMX_OFF,
	RMX_FULL,
	RMX_CENTER,
	RMX_ASPECT
};
enum amdgpu_underscan_type {
	UNDERSCAN_OFF,
	UNDERSCAN_ON,
	UNDERSCAN_AUTO,
};
#define AMDGPU_HPD_CONNECT_INT_DELAY_IN_MS 50
#define AMDGPU_HPD_DISCONNECT_INT_DELAY_IN_MS 10
enum amdgpu_hpd_id {
	AMDGPU_HPD_1 = 0,
	AMDGPU_HPD_2,
	AMDGPU_HPD_3,
	AMDGPU_HPD_4,
	AMDGPU_HPD_5,
	AMDGPU_HPD_6,
	AMDGPU_HPD_NONE = 0xff,
};
enum amdgpu_crtc_irq {
	AMDGPU_CRTC_IRQ_VBLANK1 = 0,
	AMDGPU_CRTC_IRQ_VBLANK2,
	AMDGPU_CRTC_IRQ_VBLANK3,
	AMDGPU_CRTC_IRQ_VBLANK4,
	AMDGPU_CRTC_IRQ_VBLANK5,
	AMDGPU_CRTC_IRQ_VBLANK6,
	AMDGPU_CRTC_IRQ_VLINE1,
	AMDGPU_CRTC_IRQ_VLINE2,
	AMDGPU_CRTC_IRQ_VLINE3,
	AMDGPU_CRTC_IRQ_VLINE4,
	AMDGPU_CRTC_IRQ_VLINE5,
	AMDGPU_CRTC_IRQ_VLINE6,
	AMDGPU_CRTC_IRQ_NONE = 0xff
};
enum amdgpu_pageflip_irq {
	AMDGPU_PAGEFLIP_IRQ_D1 = 0,
	AMDGPU_PAGEFLIP_IRQ_D2,
	AMDGPU_PAGEFLIP_IRQ_D3,
	AMDGPU_PAGEFLIP_IRQ_D4,
	AMDGPU_PAGEFLIP_IRQ_D5,
	AMDGPU_PAGEFLIP_IRQ_D6,
	AMDGPU_PAGEFLIP_IRQ_NONE = 0xff
};
enum amdgpu_flip_status {
	AMDGPU_FLIP_NONE,
	AMDGPU_FLIP_PENDING,
	AMDGPU_FLIP_SUBMITTED
};
#define AMDGPU_MAX_I2C_BUS 16
struct amdgpu_i2c_bus_rec {
	bool valid;
	uint8_t i2c_id;
	enum amdgpu_hpd_id hpd;
	bool hw_capable;
	bool mm_i2c;
	uint32_t mask_clk_reg;
	uint32_t mask_data_reg;
	uint32_t a_clk_reg;
	uint32_t a_data_reg;
	uint32_t en_clk_reg;
	uint32_t en_data_reg;
	uint32_t y_clk_reg;
	uint32_t y_data_reg;
	uint32_t mask_clk_mask;
	uint32_t mask_data_mask;
	uint32_t a_clk_mask;
	uint32_t a_data_mask;
	uint32_t en_clk_mask;
	uint32_t en_data_mask;
	uint32_t y_clk_mask;
	uint32_t y_data_mask;
};
#define AMDGPU_MAX_BIOS_CONNECTOR 16
#define AMDGPU_PLL_USE_BIOS_DIVS        (1 << 0)
#define AMDGPU_PLL_NO_ODD_POST_DIV      (1 << 1)
#define AMDGPU_PLL_USE_REF_DIV          (1 << 2)
#define AMDGPU_PLL_LEGACY               (1 << 3)
#define AMDGPU_PLL_PREFER_LOW_REF_DIV   (1 << 4)
#define AMDGPU_PLL_PREFER_HIGH_REF_DIV  (1 << 5)
#define AMDGPU_PLL_PREFER_LOW_FB_DIV    (1 << 6)
#define AMDGPU_PLL_PREFER_HIGH_FB_DIV   (1 << 7)
#define AMDGPU_PLL_PREFER_LOW_POST_DIV  (1 << 8)
#define AMDGPU_PLL_PREFER_HIGH_POST_DIV (1 << 9)
#define AMDGPU_PLL_USE_FRAC_FB_DIV      (1 << 10)
#define AMDGPU_PLL_PREFER_CLOSEST_LOWER (1 << 11)
#define AMDGPU_PLL_USE_POST_DIV         (1 << 12)
#define AMDGPU_PLL_IS_LCD               (1 << 13)
#define AMDGPU_PLL_PREFER_MINM_OVER_MAXP (1 << 14)
struct amdgpu_pll {
	uint32_t reference_freq;
	uint32_t reference_div;
	uint32_t post_div;
	uint32_t pll_in_min;
	uint32_t pll_in_max;
	uint32_t pll_out_min;
	uint32_t pll_out_max;
	uint32_t lcd_pll_out_min;
	uint32_t lcd_pll_out_max;
	uint32_t best_vco;
	uint32_t min_ref_div;
	uint32_t max_ref_div;
	uint32_t min_post_div;
	uint32_t max_post_div;
	uint32_t min_feedback_div;
	uint32_t max_feedback_div;
	uint32_t min_frac_feedback_div;
	uint32_t max_frac_feedback_div;
	uint32_t flags;
	uint32_t id;
};
struct amdgpu_i2c_chan {
	struct i2c_adapter adapter;
	struct drm_device *dev;
	struct i2c_algo_bit_data bit;
	struct amdgpu_i2c_bus_rec rec;
	struct drm_dp_aux aux;
	bool has_aux;
	struct mutex mutex;
};
struct amdgpu_afmt {
	bool enabled;
	int offset;
	bool last_buffer_filled_status;
	int id;
	struct amdgpu_audio_pin *pin;
};
struct amdgpu_audio_pin {
	int			channels;
	int			rate;
	int			bits_per_sample;
	u8			status_bits;
	u8			category_code;
	u32			offset;
	bool			connected;
	u32			id;
};
struct amdgpu_audio {
	bool enabled;
	struct amdgpu_audio_pin pin[AMDGPU_MAX_AFMT_BLOCKS];
	int num_pins;
};
struct amdgpu_display_funcs {
	void (*bandwidth_update)(struct amdgpu_device *adev);
	u32 (*vblank_get_counter)(struct amdgpu_device *adev, int crtc);
	void (*backlight_set_level)(struct amdgpu_encoder *amdgpu_encoder,
				    u8 level);
	u8 (*backlight_get_level)(struct amdgpu_encoder *amdgpu_encoder);
	bool (*hpd_sense)(struct amdgpu_device *adev, enum amdgpu_hpd_id hpd);
	void (*hpd_set_polarity)(struct amdgpu_device *adev,
				 enum amdgpu_hpd_id hpd);
	u32 (*hpd_get_gpio_reg)(struct amdgpu_device *adev);
	void (*page_flip)(struct amdgpu_device *adev,
			  int crtc_id, u64 crtc_base, bool async);
	int (*page_flip_get_scanoutpos)(struct amdgpu_device *adev, int crtc,
					u32 *vbl, u32 *position);
	void (*add_encoder)(struct amdgpu_device *adev,
			    uint32_t encoder_enum,
			    uint32_t supported_device,
			    u16 caps);
	void (*add_connector)(struct amdgpu_device *adev,
			      uint32_t connector_id,
			      uint32_t supported_device,
			      int connector_type,
			      struct amdgpu_i2c_bus_rec *i2c_bus,
			      uint16_t connector_object_id,
			      struct amdgpu_hpd *hpd,
			      struct amdgpu_router *router);
};
struct amdgpu_framebuffer {
	struct drm_framebuffer base;
	uint64_t tiling_flags;
	bool tmz_surface;
	uint64_t address;
};
struct amdgpu_mode_info {
	struct atom_context *atom_context;
	struct card_info *atom_card_info;
	bool mode_config_initialized;
	struct amdgpu_crtc *crtcs[AMDGPU_MAX_CRTCS];
	struct drm_plane *planes[AMDGPU_MAX_PLANES];
	struct amdgpu_afmt *afmt[AMDGPU_MAX_AFMT_BLOCKS];
	struct drm_property *coherent_mode_property;
	struct drm_property *load_detect_property;
	struct drm_property *underscan_property;
	struct drm_property *underscan_hborder_property;
	struct drm_property *underscan_vborder_property;
	struct drm_property *audio_property;
	struct drm_property *dither_property;
	struct drm_property *abm_level_property;
	struct edid *bios_hardcoded_edid;
	int bios_hardcoded_edid_size;
	u32 firmware_flags;
	struct amdgpu_encoder *bl_encoder;
	u8 bl_level;  
	struct amdgpu_audio	audio;  
	int			num_crtc;  
	int			num_hpd;  
	int			num_dig;  
	bool			gpu_vm_support;  
	int			disp_priority;
	const struct amdgpu_display_funcs *funcs;
	const enum drm_plane_type *plane_type;
};
#define AMDGPU_MAX_BL_LEVEL 0xFF
struct amdgpu_backlight_privdata {
	struct amdgpu_encoder *encoder;
	uint8_t negative;
};
struct amdgpu_atom_ss {
	uint16_t percentage;
	uint16_t percentage_divider;
	uint8_t type;
	uint16_t step;
	uint8_t delay;
	uint8_t range;
	uint8_t refdiv;
	uint16_t rate;
	uint16_t amount;
};
struct amdgpu_crtc {
	struct drm_crtc base;
	int crtc_id;
	bool enabled;
	bool can_tile;
	uint32_t crtc_offset;
	struct drm_gem_object *cursor_bo;
	uint64_t cursor_addr;
	int cursor_x;
	int cursor_y;
	int cursor_hot_x;
	int cursor_hot_y;
	int cursor_width;
	int cursor_height;
	int max_cursor_width;
	int max_cursor_height;
	enum amdgpu_rmx_type rmx_type;
	u8 h_border;
	u8 v_border;
	fixed20_12 vsc;
	fixed20_12 hsc;
	struct drm_display_mode native_mode;
	u32 pll_id;
	struct amdgpu_flip_work *pflip_works;
	enum amdgpu_flip_status pflip_status;
	int deferred_flip_completion;
	struct dm_irq_params dm_irq_params;
	struct amdgpu_atom_ss ss;
	bool ss_enabled;
	u32 adjusted_clock;
	int bpc;
	u32 pll_reference_div;
	u32 pll_post_div;
	u32 pll_flags;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	u32 line_time;
	u32 wm_low;
	u32 wm_high;
	u32 lb_vblank_lead_lines;
	struct drm_display_mode hw_mode;
	struct hrtimer vblank_timer;
	enum amdgpu_interrupt_state vsync_timer_enabled;
	int otg_inst;
	struct drm_pending_vblank_event *event;
};
struct amdgpu_encoder_atom_dig {
	bool linkb;
	bool coherent_mode;
	int dig_encoder;  
	uint32_t lcd_misc;
	uint16_t panel_pwr_delay;
	uint32_t lcd_ss_id;
	struct drm_display_mode native_mode;
	struct backlight_device *bl_dev;
	int dpms_mode;
	uint8_t backlight_level;
	int panel_mode;
	struct amdgpu_afmt *afmt;
};
struct amdgpu_encoder {
	struct drm_encoder base;
	uint32_t encoder_enum;
	uint32_t encoder_id;
	uint32_t devices;
	uint32_t active_device;
	uint32_t flags;
	uint32_t pixel_clock;
	enum amdgpu_rmx_type rmx_type;
	enum amdgpu_underscan_type underscan_type;
	uint32_t underscan_hborder;
	uint32_t underscan_vborder;
	struct drm_display_mode native_mode;
	void *enc_priv;
	int audio_polling_active;
	bool is_ext_encoder;
	u16 caps;
};
struct amdgpu_connector_atom_dig {
	u8 dpcd[DP_RECEIVER_CAP_SIZE];
	u8 downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
	u8 dp_sink_type;
	int dp_clock;
	int dp_lane_count;
	bool edp_on;
};
struct amdgpu_gpio_rec {
	bool valid;
	u8 id;
	u32 reg;
	u32 mask;
	u32 shift;
};
struct amdgpu_hpd {
	enum amdgpu_hpd_id hpd;
	u8 plugged_state;
	struct amdgpu_gpio_rec gpio;
};
struct amdgpu_router {
	u32 router_id;
	struct amdgpu_i2c_bus_rec i2c_info;
	u8 i2c_addr;
	bool ddc_valid;
	u8 ddc_mux_type;
	u8 ddc_mux_control_pin;
	u8 ddc_mux_state;
	bool cd_valid;
	u8 cd_mux_type;
	u8 cd_mux_control_pin;
	u8 cd_mux_state;
};
enum amdgpu_connector_audio {
	AMDGPU_AUDIO_DISABLE = 0,
	AMDGPU_AUDIO_ENABLE = 1,
	AMDGPU_AUDIO_AUTO = 2
};
enum amdgpu_connector_dither {
	AMDGPU_FMT_DITHER_DISABLE = 0,
	AMDGPU_FMT_DITHER_ENABLE = 1,
};
struct amdgpu_dm_dp_aux {
	struct drm_dp_aux aux;
	struct ddc_service *ddc_service;
};
struct amdgpu_i2c_adapter {
	struct i2c_adapter base;
	struct ddc_service *ddc_service;
};
#define TO_DM_AUX(x) container_of((x), struct amdgpu_dm_dp_aux, aux)
struct amdgpu_connector {
	struct drm_connector base;
	uint32_t connector_id;
	uint32_t devices;
	struct amdgpu_i2c_chan *ddc_bus;
	bool shared_ddc;
	bool use_digital;
	struct edid *edid;
	void *con_priv;
	bool dac_load_detect;
	bool detected_by_load;  
	bool detected_hpd_without_ddc;  
	uint16_t connector_object_id;
	struct amdgpu_hpd hpd;
	struct amdgpu_router router;
	struct amdgpu_i2c_chan *router_bus;
	enum amdgpu_connector_audio audio;
	enum amdgpu_connector_dither dither;
	unsigned pixelclock_for_modeset;
};
struct amdgpu_mst_connector {
	struct amdgpu_connector base;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct amdgpu_dm_dp_aux dm_dp_aux;
	struct drm_dp_mst_port *mst_output_port;
	struct amdgpu_connector *mst_root;
	bool is_mst_connector;
	struct amdgpu_encoder *mst_encoder;
};
#define ENCODER_MODE_IS_DP(em) (((em) == ATOM_ENCODER_MODE_DP) || \
				((em) == ATOM_ENCODER_MODE_DP_MST))
#define DRM_SCANOUTPOS_VALID        (1 << 0)
#define DRM_SCANOUTPOS_IN_VBLANK    (1 << 1)
#define DRM_SCANOUTPOS_ACCURATE     (1 << 2)
#define USE_REAL_VBLANKSTART		(1 << 30)
#define GET_DISTANCE_TO_VBLANKSTART	(1 << 31)
void amdgpu_link_encoder_connector(struct drm_device *dev);
struct drm_connector *
amdgpu_get_connector_for_encoder(struct drm_encoder *encoder);
struct drm_connector *
amdgpu_get_connector_for_encoder_init(struct drm_encoder *encoder);
bool amdgpu_dig_monitor_is_duallink(struct drm_encoder *encoder,
				    u32 pixel_clock);
u16 amdgpu_encoder_get_dp_bridge_encoder_id(struct drm_encoder *encoder);
struct drm_encoder *amdgpu_get_external_encoder(struct drm_encoder *encoder);
bool amdgpu_display_ddc_probe(struct amdgpu_connector *amdgpu_connector,
			      bool use_aux);
void amdgpu_encoder_set_active_device(struct drm_encoder *encoder);
int amdgpu_display_get_crtc_scanoutpos(struct drm_device *dev,
			unsigned int pipe, unsigned int flags, int *vpos,
			int *hpos, ktime_t *stime, ktime_t *etime,
			const struct drm_display_mode *mode);
int amdgpufb_remove(struct drm_device *dev, struct drm_framebuffer *fb);
void amdgpu_enc_destroy(struct drm_encoder *encoder);
void amdgpu_copy_fb(struct drm_device *dev, struct drm_gem_object *dst_obj);
bool amdgpu_display_crtc_scaling_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
void amdgpu_panel_mode_fixup(struct drm_encoder *encoder,
			     struct drm_display_mode *adjusted_mode);
int amdgpu_display_crtc_idx_to_irq_type(struct amdgpu_device *adev, int crtc);
bool amdgpu_crtc_get_scanout_position(struct drm_crtc *crtc,
			bool in_vblank_irq, int *vpos,
			int *hpos, ktime_t *stime, ktime_t *etime,
			const struct drm_display_mode *mode);
void amdgpu_display_print_display_setup(struct drm_device *dev);
int amdgpu_display_modeset_create_props(struct amdgpu_device *adev);
int amdgpu_display_crtc_set_config(struct drm_mode_set *set,
				   struct drm_modeset_acquire_ctx *ctx);
int amdgpu_display_crtc_page_flip_target(struct drm_crtc *crtc,
				struct drm_framebuffer *fb,
				struct drm_pending_vblank_event *event,
				uint32_t page_flip_flags, uint32_t target,
				struct drm_modeset_acquire_ctx *ctx);
extern const struct drm_mode_config_funcs amdgpu_mode_funcs;
#endif
