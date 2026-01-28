#ifndef AMDGPU_DM_AMDGPU_DM_HDCP_H_
#define AMDGPU_DM_AMDGPU_DM_HDCP_H_
#include "mod_hdcp.h"
#include "hdcp.h"
#include "dc.h"
#include "dm_cp_psp.h"
#include "amdgpu.h"
struct mod_hdcp;
struct mod_hdcp_link;
struct mod_hdcp_display;
struct cp_psp;
struct hdcp_workqueue {
	struct work_struct cpirq_work;
	struct work_struct property_update_work;
	struct delayed_work callback_dwork;
	struct delayed_work watchdog_timer_dwork;
	struct delayed_work property_validate_dwork;
	struct amdgpu_dm_connector *aconnector[AMDGPU_DM_MAX_DISPLAY_INDEX];
	struct mutex mutex;
	struct mod_hdcp hdcp;
	struct mod_hdcp_output output;
	struct mod_hdcp_display display;
	struct mod_hdcp_link link;
	enum mod_hdcp_encryption_status encryption_status[AMDGPU_DM_MAX_DISPLAY_INDEX];
	unsigned int content_protection[AMDGPU_DM_MAX_DISPLAY_INDEX];
	unsigned int hdcp_content_type[AMDGPU_DM_MAX_DISPLAY_INDEX];
	uint8_t max_link;
	uint8_t *srm;
	uint8_t *srm_temp;
	uint32_t srm_version;
	uint32_t srm_size;
	struct bin_attribute attr;
};
void hdcp_update_display(struct hdcp_workqueue *hdcp_work,
			 unsigned int link_index,
			 struct amdgpu_dm_connector *aconnector,
			 uint8_t content_type,
			 bool enable_encryption);
void hdcp_reset_display(struct hdcp_workqueue *work, unsigned int link_index);
void hdcp_handle_cpirq(struct hdcp_workqueue *work, unsigned int link_index);
void hdcp_destroy(struct kobject *kobj, struct hdcp_workqueue *work);
struct hdcp_workqueue *hdcp_create_workqueue(struct amdgpu_device *adev, struct cp_psp *cp_psp, struct dc *dc);
#endif  
