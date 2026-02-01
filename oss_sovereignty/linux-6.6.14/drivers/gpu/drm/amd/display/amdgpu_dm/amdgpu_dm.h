 

#ifndef __AMDGPU_DM_H__
#define __AMDGPU_DM_H__

#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_plane.h>
#include "link_service_types.h"

 

#define AMDGPU_DM_MAX_DISPLAY_INDEX 31

#define AMDGPU_DM_MAX_CRTC 6

#define AMDGPU_DM_MAX_NUM_EDP 2

#define AMDGPU_DMUB_NOTIFICATION_MAX 5

#define HDMI_AMD_VENDOR_SPECIFIC_DATA_BLOCK_IEEE_REGISTRATION_ID 0x00001A
#define AMD_VSDB_VERSION_3_FEATURECAP_REPLAYMODE 0x40
#define HDMI_AMD_VENDOR_SPECIFIC_DATA_BLOCK_VERSION_3 0x3
 

#include "irq_types.h"
#include "signal_types.h"
#include "amdgpu_dm_crc.h"
#include "mod_info_packet.h"
struct aux_payload;
struct set_config_cmd_payload;
enum aux_return_code_type;
enum set_config_status;

 
struct amdgpu_device;
struct amdgpu_crtc;
struct drm_device;
struct dc;
struct amdgpu_bo;
struct dmub_srv;
struct dc_plane_state;
struct dmub_notification;

struct amd_vsdb_block {
	unsigned char ieee_id[3];
	unsigned char version;
	unsigned char feature_caps;
};

struct common_irq_params {
	struct amdgpu_device *adev;
	enum dc_irq_source irq_src;
	atomic64_t previous_timestamp;
};

 
struct dm_compressor_info {
	void *cpu_addr;
	struct amdgpu_bo *bo_ptr;
	uint64_t gpu_addr;
};

typedef void (*dmub_notify_interrupt_callback_t)(struct amdgpu_device *adev, struct dmub_notification *notify);

 
struct dmub_hpd_work {
	struct work_struct handle_hpd_work;
	struct dmub_notification *dmub_notify;
	struct amdgpu_device *adev;
};

 
struct vblank_control_work {
	struct work_struct work;
	struct amdgpu_display_manager *dm;
	struct amdgpu_crtc *acrtc;
	struct dc_stream_state *stream;
	bool enable;
};

 
struct amdgpu_dm_backlight_caps {
	 
	union dpcd_sink_ext_caps *ext_caps;
	 
	u32 aux_min_input_signal;
	 
	u32 aux_max_input_signal;
	 
	int min_input_signal;
	 
	int max_input_signal;
	 
	bool caps_valid;
	 
	bool aux_support;
};

 
struct dal_allocation {
	struct list_head list;
	struct amdgpu_bo *bo;
	void *cpu_ptr;
	u64 gpu_addr;
};

 
struct hpd_rx_irq_offload_work_queue {
	 
	struct workqueue_struct *wq;
	 
	spinlock_t offload_lock;
	 
	bool is_handling_link_loss;
	 
	bool is_handling_mst_msg_rdy_event;
	 
	struct amdgpu_dm_connector *aconnector;
};

 
struct hpd_rx_irq_offload_work {
	 
	struct work_struct work;
	 
	union hpd_irq_data data;
	 
	struct hpd_rx_irq_offload_work_queue *offload_wq;
};

 
struct amdgpu_display_manager {

	struct dc *dc;

	 
	struct dmub_srv *dmub_srv;

	 

	struct dmub_notification *dmub_notify;

	 

	dmub_notify_interrupt_callback_t dmub_callback[AMDGPU_DMUB_NOTIFICATION_MAX];

	 

	bool dmub_thread_offload[AMDGPU_DMUB_NOTIFICATION_MAX];

	 
	struct dmub_srv_fb_info *dmub_fb_info;

	 
	const struct firmware *dmub_fw;

	 
	struct amdgpu_bo *dmub_bo;

	 
	u64 dmub_bo_gpu_addr;

	 
	void *dmub_bo_cpu_addr;

	 
	uint32_t dmcub_fw_version;

	 
	struct cgs_device *cgs_device;

	struct amdgpu_device *adev;
	struct drm_device *ddev;
	u16 display_indexes_num;

	 
	struct drm_private_obj atomic_obj;

	 
	struct mutex dc_lock;

	 
	struct mutex audio_lock;

	 
	struct drm_audio_component *audio_component;

	 
	bool audio_registered;

	 
	struct list_head irq_handler_list_low_tab[DAL_IRQ_SOURCES_NUMBER];

	 
	struct list_head irq_handler_list_high_tab[DAL_IRQ_SOURCES_NUMBER];

	 
	struct common_irq_params
	pflip_params[DC_IRQ_SOURCE_PFLIP_LAST - DC_IRQ_SOURCE_PFLIP_FIRST + 1];

	 
	struct common_irq_params
	vblank_params[DC_IRQ_SOURCE_VBLANK6 - DC_IRQ_SOURCE_VBLANK1 + 1];

	 
	struct common_irq_params
	vline0_params[DC_IRQ_SOURCE_DC6_VLINE0 - DC_IRQ_SOURCE_DC1_VLINE0 + 1];

	 
	struct common_irq_params
	vupdate_params[DC_IRQ_SOURCE_VUPDATE6 - DC_IRQ_SOURCE_VUPDATE1 + 1];

	 
	struct common_irq_params
	dmub_trace_params[1];

	struct common_irq_params
	dmub_outbox_params[1];

	spinlock_t irq_handler_list_table_lock;

	struct backlight_device *backlight_dev[AMDGPU_DM_MAX_NUM_EDP];

	const struct dc_link *backlight_link[AMDGPU_DM_MAX_NUM_EDP];

	uint8_t num_of_edps;

	struct amdgpu_dm_backlight_caps backlight_caps[AMDGPU_DM_MAX_NUM_EDP];

	struct mod_freesync *freesync_module;
	struct hdcp_workqueue *hdcp_workqueue;

	 
	struct workqueue_struct *vblank_control_workqueue;

	struct drm_atomic_state *cached_state;
	struct dc_state *cached_dc_state;

	struct dm_compressor_info compressor;

	const struct firmware *fw_dmcu;
	uint32_t dmcu_fw_version;
	 
	const struct gpu_info_soc_bounding_box_v1_0 *soc_bounding_box;

	 
	uint32_t active_vblank_irq_count;

#if defined(CONFIG_DRM_AMD_SECURE_DISPLAY)
	 
	struct secure_display_context *secure_display_ctxs;
#endif
	 
	struct hpd_rx_irq_offload_work_queue *hpd_rx_offload_wq;
	 
	struct amdgpu_encoder mst_encoders[AMDGPU_DM_MAX_CRTC];
	bool force_timing_sync;
	bool disable_hpd_irq;
	bool dmcub_trace_event_en;
	 
	struct list_head da_list;
	struct completion dmub_aux_transfer_done;
	struct workqueue_struct *delayed_hpd_wq;

	 
	u32 brightness[AMDGPU_DM_MAX_NUM_EDP];
	 
	u32 actual_brightness[AMDGPU_DM_MAX_NUM_EDP];

	 
	bool aux_hpd_discon_quirk;

	 
	struct mutex dpia_aux_lock;
};

enum dsc_clock_force_state {
	DSC_CLK_FORCE_DEFAULT = 0,
	DSC_CLK_FORCE_ENABLE,
	DSC_CLK_FORCE_DISABLE,
};

struct dsc_preferred_settings {
	enum dsc_clock_force_state dsc_force_enable;
	uint32_t dsc_num_slices_v;
	uint32_t dsc_num_slices_h;
	uint32_t dsc_bits_per_pixel;
	bool dsc_force_disable_passthrough;
};

enum mst_progress_status {
	MST_STATUS_DEFAULT = 0,
	MST_PROBE = BIT(0),
	MST_REMOTE_EDID = BIT(1),
	MST_ALLOCATE_NEW_PAYLOAD = BIT(2),
	MST_CLEAR_ALLOCATED_PAYLOAD = BIT(3),
};

 
struct amdgpu_hdmi_vsdb_info {
	 
	unsigned int amd_vsdb_version;

	 
	bool freesync_supported;

	 
	unsigned int min_refresh_rate_hz;

	 
	unsigned int max_refresh_rate_hz;

	 
	bool replay_mode;
};

struct amdgpu_dm_connector {

	struct drm_connector base;
	uint32_t connector_id;
	int bl_idx;

	 
	struct edid *edid;

	 
	struct amdgpu_hpd hpd;

	 
	int num_modes;

	 
	struct dc_sink *dc_sink;
	struct dc_link *dc_link;

	 
	struct dc_sink *dc_em_sink;

	 
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct amdgpu_dm_dp_aux dm_dp_aux;
	struct drm_dp_mst_port *mst_output_port;
	struct amdgpu_dm_connector *mst_root;
	struct drm_dp_aux *dsc_aux;
	struct mutex handle_mst_msg_ready;

	 
	struct amdgpu_i2c_adapter *i2c;

	 
	 
	int min_vfreq;

	 
	int max_vfreq ;
	int pixel_clock_mhz;

	 
	int audio_inst;

	struct mutex hpd_lock;

	bool fake_enable;
	bool force_yuv420_output;
	struct dsc_preferred_settings dsc_settings;
	union dp_downstream_port_present mst_downstream_port_present;
	 
	struct drm_display_mode freesync_vid_base;

	int psr_skip_count;

	 
	uint8_t mst_status;

	 
	bool timing_changed;
	struct dc_crtc_timing *timing_requested;

	 
	bool pack_sdp_v1_3;
	enum adaptive_sync_type as_type;
	struct amdgpu_hdmi_vsdb_info vsdb_info;
};

static inline void amdgpu_dm_set_mst_status(uint8_t *status,
		uint8_t flags, bool set)
{
	if (set)
		*status |= flags;
	else
		*status &= ~flags;
}

#define to_amdgpu_dm_connector(x) container_of(x, struct amdgpu_dm_connector, base)

extern const struct amdgpu_ip_block_version dm_ip_block;

struct dm_plane_state {
	struct drm_plane_state base;
	struct dc_plane_state *dc_state;
};

struct dm_crtc_state {
	struct drm_crtc_state base;
	struct dc_stream_state *stream;

	bool cm_has_degamma;
	bool cm_is_degamma_srgb;

	bool mpo_requested;

	int update_type;
	int active_planes;

	int crc_skip_count;

	bool freesync_vrr_info_changed;

	bool dsc_force_changed;
	bool vrr_supported;
	struct mod_freesync_config freesync_config;
	struct dc_info_packet vrr_infopacket;

	int abm_level;
};

#define to_dm_crtc_state(x) container_of(x, struct dm_crtc_state, base)

struct dm_atomic_state {
	struct drm_private_state base;

	struct dc_state *context;
};

#define to_dm_atomic_state(x) container_of(x, struct dm_atomic_state, base)

struct dm_connector_state {
	struct drm_connector_state base;

	enum amdgpu_rmx_type scaling;
	uint8_t underscan_vborder;
	uint8_t underscan_hborder;
	bool underscan_enable;
	bool freesync_capable;
	bool update_hdcp;
	uint8_t abm_level;
	int vcpi_slots;
	uint64_t pbn;
};

#define to_dm_connector_state(x)\
	container_of((x), struct dm_connector_state, base)

void amdgpu_dm_connector_funcs_reset(struct drm_connector *connector);
struct drm_connector_state *
amdgpu_dm_connector_atomic_duplicate_state(struct drm_connector *connector);
int amdgpu_dm_connector_atomic_set_property(struct drm_connector *connector,
					    struct drm_connector_state *state,
					    struct drm_property *property,
					    uint64_t val);

int amdgpu_dm_connector_atomic_get_property(struct drm_connector *connector,
					    const struct drm_connector_state *state,
					    struct drm_property *property,
					    uint64_t *val);

int amdgpu_dm_get_encoder_crtc_mask(struct amdgpu_device *adev);

void amdgpu_dm_connector_init_helper(struct amdgpu_display_manager *dm,
				     struct amdgpu_dm_connector *aconnector,
				     int connector_type,
				     struct dc_link *link,
				     int link_index);

enum drm_mode_status amdgpu_dm_connector_mode_valid(struct drm_connector *connector,
				   struct drm_display_mode *mode);

void dm_restore_drm_connector_state(struct drm_device *dev,
				    struct drm_connector *connector);

void amdgpu_dm_update_freesync_caps(struct drm_connector *connector,
					struct edid *edid);

void amdgpu_dm_trigger_timing_sync(struct drm_device *dev);

#define MAX_COLOR_LUT_ENTRIES 4096
 
#define MAX_COLOR_LEGACY_LUT_ENTRIES 256

void amdgpu_dm_init_color_mod(void);
int amdgpu_dm_verify_lut_sizes(const struct drm_crtc_state *crtc_state);
int amdgpu_dm_update_crtc_color_mgmt(struct dm_crtc_state *crtc);
int amdgpu_dm_update_plane_color_mgmt(struct dm_crtc_state *crtc,
				      struct dc_plane_state *dc_plane_state);

void amdgpu_dm_update_connector_after_detect(
		struct amdgpu_dm_connector *aconnector);

extern const struct drm_encoder_helper_funcs amdgpu_dm_encoder_helper_funcs;

int amdgpu_dm_process_dmub_aux_transfer_sync(struct dc_context *ctx, unsigned int link_index,
					struct aux_payload *payload, enum aux_return_code_type *operation_result);

int amdgpu_dm_process_dmub_set_config_sync(struct dc_context *ctx, unsigned int link_index,
					struct set_config_cmd_payload *payload, enum set_config_status *operation_result);

bool check_seamless_boot_capability(struct amdgpu_device *adev);

struct dc_stream_state *
	create_validate_stream_for_sink(struct amdgpu_dm_connector *aconnector,
					const struct drm_display_mode *drm_mode,
					const struct dm_connector_state *dm_state,
					const struct dc_stream_state *old_stream);

int dm_atomic_get_state(struct drm_atomic_state *state,
			struct dm_atomic_state **dm_state);

struct amdgpu_dm_connector *
amdgpu_dm_find_first_crtc_matching_connector(struct drm_atomic_state *state,
					     struct drm_crtc *crtc);

int convert_dc_color_depth_into_bpc(enum dc_color_depth display_color_depth);
#endif  
