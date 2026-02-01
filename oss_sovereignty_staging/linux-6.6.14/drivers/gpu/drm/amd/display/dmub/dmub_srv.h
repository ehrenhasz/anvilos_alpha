 

#ifndef _DMUB_SRV_H_
#define _DMUB_SRV_H_

 

#include "inc/dmub_cmd.h"

#if defined(__cplusplus)
extern "C" {
#endif

 
struct dmub_srv;
struct dmub_srv_common_regs;
struct dmub_srv_dcn31_regs;

struct dmcub_trace_buf_entry;

 
enum dmub_status {
	DMUB_STATUS_OK = 0,
	DMUB_STATUS_NO_CTX,
	DMUB_STATUS_QUEUE_FULL,
	DMUB_STATUS_TIMEOUT,
	DMUB_STATUS_INVALID,
	DMUB_STATUS_HW_FAILURE,
};

 
enum dmub_asic {
	DMUB_ASIC_NONE = 0,
	DMUB_ASIC_DCN20,
	DMUB_ASIC_DCN21,
	DMUB_ASIC_DCN30,
	DMUB_ASIC_DCN301,
	DMUB_ASIC_DCN302,
	DMUB_ASIC_DCN303,
	DMUB_ASIC_DCN31,
	DMUB_ASIC_DCN31B,
	DMUB_ASIC_DCN314,
	DMUB_ASIC_DCN315,
	DMUB_ASIC_DCN316,
	DMUB_ASIC_DCN32,
	DMUB_ASIC_DCN321,
	DMUB_ASIC_MAX,
};

 
enum dmub_window_id {
	DMUB_WINDOW_0_INST_CONST = 0,
	DMUB_WINDOW_1_STACK,
	DMUB_WINDOW_2_BSS_DATA,
	DMUB_WINDOW_3_VBIOS,
	DMUB_WINDOW_4_MAILBOX,
	DMUB_WINDOW_5_TRACEBUFF,
	DMUB_WINDOW_6_FW_STATE,
	DMUB_WINDOW_7_SCRATCH_MEM,
	DMUB_WINDOW_TOTAL,
};

 
enum dmub_notification_type {
	DMUB_NOTIFICATION_NO_DATA = 0,
	DMUB_NOTIFICATION_AUX_REPLY,
	DMUB_NOTIFICATION_HPD,
	DMUB_NOTIFICATION_HPD_IRQ,
	DMUB_NOTIFICATION_SET_CONFIG_REPLY,
	DMUB_NOTIFICATION_DPIA_NOTIFICATION,
	DMUB_NOTIFICATION_MAX
};

 
enum dpia_notify_bw_alloc_status {

	DPIA_BW_REQ_FAILED = 0,
	DPIA_BW_REQ_SUCCESS,
	DPIA_EST_BW_CHANGED,
	DPIA_BW_ALLOC_CAPS_CHANGED
};

 
struct dmub_region {
	uint32_t base;
	uint32_t top;
};

 
struct dmub_window {
	union dmub_addr offset;
	struct dmub_region region;
};

 
struct dmub_fb {
	void *cpu_addr;
	uint64_t gpu_addr;
	uint32_t size;
};

 
struct dmub_srv_region_params {
	uint32_t inst_const_size;
	uint32_t bss_data_size;
	uint32_t vbios_size;
	const uint8_t *fw_inst_const;
	const uint8_t *fw_bss_data;
	bool is_mailbox_in_inbox;
};

 
struct dmub_srv_region_info {
	uint32_t fb_size;
	uint32_t inbox_size;
	uint8_t num_regions;
	struct dmub_region regions[DMUB_WINDOW_TOTAL];
};

 
struct dmub_srv_memory_params {
	const struct dmub_srv_region_info *region_info;
	void *cpu_fb_addr;
	void *cpu_inbox_addr;
	uint64_t gpu_fb_addr;
	uint64_t gpu_inbox_addr;
};

 
struct dmub_srv_fb_info {
	uint8_t num_fb;
	struct dmub_fb fb[DMUB_WINDOW_TOTAL];
};

 
struct dmub_srv_hw_params {
	struct dmub_fb *fb[DMUB_WINDOW_TOTAL];
	uint64_t fb_base;
	uint64_t fb_offset;
	uint32_t psp_version;
	bool load_inst_const;
	bool skip_panel_power_sequence;
	bool disable_z10;
	bool power_optimization;
	bool dpia_supported;
	bool disable_dpia;
	bool usb4_cm_version;
	bool fw_in_system_memory;
	bool dpia_hpd_int_enable_supported;
	bool disable_clock_gate;
	bool disallow_dispclk_dppclk_ds;
};

 
struct dmub_diagnostic_data {
	uint32_t dmcub_version;
	uint32_t scratch[17];
	uint32_t pc;
	uint32_t undefined_address_fault_addr;
	uint32_t inst_fetch_fault_addr;
	uint32_t data_write_fault_addr;
	uint32_t inbox1_rptr;
	uint32_t inbox1_wptr;
	uint32_t inbox1_size;
	uint32_t inbox0_rptr;
	uint32_t inbox0_wptr;
	uint32_t inbox0_size;
	uint32_t gpint_datain0;
	uint8_t is_dmcub_enabled : 1;
	uint8_t is_dmcub_soft_reset : 1;
	uint8_t is_dmcub_secure_reset : 1;
	uint8_t is_traceport_en : 1;
	uint8_t is_cw0_enabled : 1;
	uint8_t is_cw6_enabled : 1;
};

 
struct dmub_srv_base_funcs {
	 
	uint32_t (*reg_read)(void *ctx, uint32_t address);

	 
	void (*reg_write)(void *ctx, uint32_t address, uint32_t value);
};

 
struct dmub_srv_hw_funcs {
	 

	void (*init)(struct dmub_srv *dmub);

	void (*reset)(struct dmub_srv *dmub);

	void (*reset_release)(struct dmub_srv *dmub);

	void (*backdoor_load)(struct dmub_srv *dmub,
			      const struct dmub_window *cw0,
			      const struct dmub_window *cw1);

	void (*backdoor_load_zfb_mode)(struct dmub_srv *dmub,
			      const struct dmub_window *cw0,
			      const struct dmub_window *cw1);
	void (*setup_windows)(struct dmub_srv *dmub,
			      const struct dmub_window *cw2,
			      const struct dmub_window *cw3,
			      const struct dmub_window *cw4,
			      const struct dmub_window *cw5,
			      const struct dmub_window *cw6);

	void (*setup_mailbox)(struct dmub_srv *dmub,
			      const struct dmub_region *inbox1);

	uint32_t (*get_inbox1_wptr)(struct dmub_srv *dmub);

	uint32_t (*get_inbox1_rptr)(struct dmub_srv *dmub);

	void (*set_inbox1_wptr)(struct dmub_srv *dmub, uint32_t wptr_offset);

	void (*setup_out_mailbox)(struct dmub_srv *dmub,
			      const struct dmub_region *outbox1);

	uint32_t (*get_outbox1_wptr)(struct dmub_srv *dmub);

	void (*set_outbox1_rptr)(struct dmub_srv *dmub, uint32_t rptr_offset);

	void (*setup_outbox0)(struct dmub_srv *dmub,
			      const struct dmub_region *outbox0);

	uint32_t (*get_outbox0_wptr)(struct dmub_srv *dmub);

	void (*set_outbox0_rptr)(struct dmub_srv *dmub, uint32_t rptr_offset);

	uint32_t (*emul_get_inbox1_rptr)(struct dmub_srv *dmub);

	void (*emul_set_inbox1_wptr)(struct dmub_srv *dmub, uint32_t wptr_offset);

	bool (*is_supported)(struct dmub_srv *dmub);

	bool (*is_psrsu_supported)(struct dmub_srv *dmub);

	bool (*is_hw_init)(struct dmub_srv *dmub);

	void (*enable_dmub_boot_options)(struct dmub_srv *dmub,
				const struct dmub_srv_hw_params *params);

	void (*skip_dmub_panel_power_sequence)(struct dmub_srv *dmub, bool skip);

	union dmub_fw_boot_status (*get_fw_status)(struct dmub_srv *dmub);

	union dmub_fw_boot_options (*get_fw_boot_option)(struct dmub_srv *dmub);

	void (*set_gpint)(struct dmub_srv *dmub,
			  union dmub_gpint_data_register reg);

	bool (*is_gpint_acked)(struct dmub_srv *dmub,
			       union dmub_gpint_data_register reg);

	uint32_t (*get_gpint_response)(struct dmub_srv *dmub);

	uint32_t (*get_gpint_dataout)(struct dmub_srv *dmub);

	void (*configure_dmub_in_system_memory)(struct dmub_srv *dmub);
	void (*clear_inbox0_ack_register)(struct dmub_srv *dmub);
	uint32_t (*read_inbox0_ack_register)(struct dmub_srv *dmub);
	void (*send_inbox0_cmd)(struct dmub_srv *dmub, union dmub_inbox0_data_register data);
	uint32_t (*get_current_time)(struct dmub_srv *dmub);

	void (*get_diagnostic_data)(struct dmub_srv *dmub, struct dmub_diagnostic_data *dmub_oca);

	bool (*should_detect)(struct dmub_srv *dmub);
};

 
struct dmub_srv_create_params {
	struct dmub_srv_base_funcs funcs;
	struct dmub_srv_hw_funcs *hw_funcs;
	void *user_ctx;
	enum dmub_asic asic;
	uint32_t fw_version;
	bool is_virtual;
};

 
struct dmub_srv {
	enum dmub_asic asic;
	void *user_ctx;
	uint32_t fw_version;
	bool is_virtual;
	struct dmub_fb scratch_mem_fb;
	volatile const struct dmub_fw_state *fw_state;

	 
	const struct dmub_srv_common_regs *regs;
	const struct dmub_srv_dcn31_regs *regs_dcn31;
	const struct dmub_srv_dcn32_regs *regs_dcn32;

	struct dmub_srv_base_funcs funcs;
	struct dmub_srv_hw_funcs hw_funcs;
	struct dmub_rb inbox1_rb;
	uint32_t inbox1_last_wptr;
	 
	struct dmub_rb outbox1_rb;

	struct dmub_rb outbox0_rb;

	bool sw_init;
	bool hw_init;

	uint64_t fb_base;
	uint64_t fb_offset;
	uint32_t psp_version;

	 
	struct dmub_feature_caps feature_caps;
	struct dmub_visual_confirm_color visual_confirm_color;
};

 
struct dmub_notification {
	enum dmub_notification_type type;
	uint8_t link_index;
	uint8_t result;
	bool pending_notification;
	union {
		struct aux_reply_data aux_reply;
		enum dp_hpd_status hpd_status;
		enum set_config_status sc_status;
		 
		struct dmub_rb_cmd_dpia_notification dpia_notification;
	};
};

 
#define DMUB_FW_VERSION(major, minor, revision) \
	((((major) & 0xFF) << 24) | (((minor) & 0xFF) << 16) | (((revision) & 0xFF) << 8))

 
enum dmub_status dmub_srv_create(struct dmub_srv *dmub,
				 const struct dmub_srv_create_params *params);

 
void dmub_srv_destroy(struct dmub_srv *dmub);

 
enum dmub_status
dmub_srv_calc_region_info(struct dmub_srv *dmub,
			  const struct dmub_srv_region_params *params,
			  struct dmub_srv_region_info *out);

 
enum dmub_status dmub_srv_calc_mem_info(struct dmub_srv *dmub,
				       const struct dmub_srv_memory_params *params,
				       struct dmub_srv_fb_info *out);

 
enum dmub_status dmub_srv_has_hw_support(struct dmub_srv *dmub,
					 bool *is_supported);

 
enum dmub_status dmub_srv_is_hw_init(struct dmub_srv *dmub, bool *is_hw_init);

 
enum dmub_status dmub_srv_hw_init(struct dmub_srv *dmub,
				  const struct dmub_srv_hw_params *params);

 
enum dmub_status dmub_srv_hw_reset(struct dmub_srv *dmub);

 
enum dmub_status dmub_srv_sync_inbox1(struct dmub_srv *dmub);

 
enum dmub_status dmub_srv_cmd_queue(struct dmub_srv *dmub,
				    const union dmub_rb_cmd *cmd);

 
enum dmub_status dmub_srv_cmd_execute(struct dmub_srv *dmub);

 
enum dmub_status dmub_srv_wait_for_auto_load(struct dmub_srv *dmub,
					     uint32_t timeout_us);

 
enum dmub_status dmub_srv_wait_for_phy_init(struct dmub_srv *dmub,
					    uint32_t timeout_us);

 
enum dmub_status dmub_srv_wait_for_idle(struct dmub_srv *dmub,
					uint32_t timeout_us);

 
enum dmub_status
dmub_srv_send_gpint_command(struct dmub_srv *dmub,
			    enum dmub_gpint_command command_code,
			    uint16_t param, uint32_t timeout_us);

 
enum dmub_status dmub_srv_get_gpint_response(struct dmub_srv *dmub,
					     uint32_t *response);

 
enum dmub_status dmub_srv_get_gpint_dataout(struct dmub_srv *dmub,
					     uint32_t *dataout);

 
void dmub_flush_buffer_mem(const struct dmub_fb *fb);

 
enum dmub_status dmub_srv_get_fw_boot_status(struct dmub_srv *dmub,
					     union dmub_fw_boot_status *status);

enum dmub_status dmub_srv_get_fw_boot_option(struct dmub_srv *dmub,
					     union dmub_fw_boot_options *option);

enum dmub_status dmub_srv_cmd_with_reply_data(struct dmub_srv *dmub,
					      union dmub_rb_cmd *cmd);

enum dmub_status dmub_srv_set_skip_panel_power_sequence(struct dmub_srv *dmub,
					     bool skip);

bool dmub_srv_get_outbox0_msg(struct dmub_srv *dmub, struct dmcub_trace_buf_entry *entry);

bool dmub_srv_get_diagnostic_data(struct dmub_srv *dmub, struct dmub_diagnostic_data *diag_data);

bool dmub_srv_should_detect(struct dmub_srv *dmub);

 
enum dmub_status dmub_srv_send_inbox0_cmd(struct dmub_srv *dmub, union dmub_inbox0_data_register data);

 
enum dmub_status dmub_srv_wait_for_inbox0_ack(struct dmub_srv *dmub, uint32_t timeout_us);

 
enum dmub_status dmub_srv_clear_inbox0_ack(struct dmub_srv *dmub);

#if defined(__cplusplus)
}
#endif

#endif  
