 

#include "dm_services_types.h"
#include "dc.h"

#include "amdgpu.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_irq.h"

 

 

 
struct amdgpu_dm_irq_handler_data {
	struct list_head list;
	interrupt_handler handler;
	void *handler_arg;

	struct amdgpu_display_manager *dm;
	 
	enum dc_irq_source irq_source;
	struct work_struct work;
};

#define DM_IRQ_TABLE_LOCK(adev, flags) \
	spin_lock_irqsave(&adev->dm.irq_handler_list_table_lock, flags)

#define DM_IRQ_TABLE_UNLOCK(adev, flags) \
	spin_unlock_irqrestore(&adev->dm.irq_handler_list_table_lock, flags)

 

static void init_handler_common_data(struct amdgpu_dm_irq_handler_data *hcd,
				     void (*ih)(void *),
				     void *args,
				     struct amdgpu_display_manager *dm)
{
	hcd->handler = ih;
	hcd->handler_arg = args;
	hcd->dm = dm;
}

 
static void dm_irq_work_func(struct work_struct *work)
{
	struct amdgpu_dm_irq_handler_data *handler_data =
		container_of(work, struct amdgpu_dm_irq_handler_data, work);

	handler_data->handler(handler_data->handler_arg);

	 
}

 
static struct list_head *remove_irq_handler(struct amdgpu_device *adev,
					    void *ih,
					    const struct dc_interrupt_params *int_params)
{
	struct list_head *hnd_list;
	struct list_head *entry, *tmp;
	struct amdgpu_dm_irq_handler_data *handler;
	unsigned long irq_table_flags;
	bool handler_removed = false;
	enum dc_irq_source irq_source;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	irq_source = int_params->irq_source;

	switch (int_params->int_context) {
	case INTERRUPT_HIGH_IRQ_CONTEXT:
		hnd_list = &adev->dm.irq_handler_list_high_tab[irq_source];
		break;
	case INTERRUPT_LOW_IRQ_CONTEXT:
	default:
		hnd_list = &adev->dm.irq_handler_list_low_tab[irq_source];
		break;
	}

	list_for_each_safe(entry, tmp, hnd_list) {

		handler = list_entry(entry, struct amdgpu_dm_irq_handler_data,
				     list);

		if (handler == NULL)
			continue;

		if (ih == handler->handler) {
			 
			list_del(&handler->list);
			handler_removed = true;
			break;
		}
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);

	if (handler_removed == false) {
		 
		return NULL;
	}

	kfree(handler);

	DRM_DEBUG_KMS(
	"DM_IRQ: removed irq handler: %p for: dal_src=%d, irq context=%d\n",
		ih, int_params->irq_source, int_params->int_context);

	return hnd_list;
}

 
static void unregister_all_irq_handlers(struct amdgpu_device *adev)
{
	struct list_head *hnd_list_low;
	struct list_head *hnd_list_high;
	struct list_head *entry, *tmp;
	struct amdgpu_dm_irq_handler_data *handler;
	unsigned long irq_table_flags;
	int i;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	for (i = 0; i < DAL_IRQ_SOURCES_NUMBER; i++) {
		hnd_list_low = &adev->dm.irq_handler_list_low_tab[i];
		hnd_list_high = &adev->dm.irq_handler_list_high_tab[i];

		list_for_each_safe(entry, tmp, hnd_list_low) {

			handler = list_entry(entry, struct amdgpu_dm_irq_handler_data,
					     list);

			if (handler == NULL || handler->handler == NULL)
				continue;

			list_del(&handler->list);
			kfree(handler);
		}

		list_for_each_safe(entry, tmp, hnd_list_high) {

			handler = list_entry(entry, struct amdgpu_dm_irq_handler_data,
					     list);

			if (handler == NULL || handler->handler == NULL)
				continue;

			list_del(&handler->list);
			kfree(handler);
		}
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);
}

static bool
validate_irq_registration_params(struct dc_interrupt_params *int_params,
				 void (*ih)(void *))
{
	if (NULL == int_params || NULL == ih) {
		DRM_ERROR("DM_IRQ: invalid input!\n");
		return false;
	}

	if (int_params->int_context >= INTERRUPT_CONTEXT_NUMBER) {
		DRM_ERROR("DM_IRQ: invalid context: %d!\n",
				int_params->int_context);
		return false;
	}

	if (!DAL_VALID_IRQ_SRC_NUM(int_params->irq_source)) {
		DRM_ERROR("DM_IRQ: invalid irq_source: %d!\n",
				int_params->irq_source);
		return false;
	}

	return true;
}

static bool validate_irq_unregistration_params(enum dc_irq_source irq_source,
					       irq_handler_idx handler_idx)
{
	if (handler_idx == DAL_INVALID_IRQ_HANDLER_IDX) {
		DRM_ERROR("DM_IRQ: invalid handler_idx==NULL!\n");
		return false;
	}

	if (!DAL_VALID_IRQ_SRC_NUM(irq_source)) {
		DRM_ERROR("DM_IRQ: invalid irq_source:%d!\n", irq_source);
		return false;
	}

	return true;
}
 

 
void *amdgpu_dm_irq_register_interrupt(struct amdgpu_device *adev,
				       struct dc_interrupt_params *int_params,
				       void (*ih)(void *),
				       void *handler_args)
{
	struct list_head *hnd_list;
	struct amdgpu_dm_irq_handler_data *handler_data;
	unsigned long irq_table_flags;
	enum dc_irq_source irq_source;

	if (false == validate_irq_registration_params(int_params, ih))
		return DAL_INVALID_IRQ_HANDLER_IDX;

	handler_data = kzalloc(sizeof(*handler_data), GFP_KERNEL);
	if (!handler_data) {
		DRM_ERROR("DM_IRQ: failed to allocate irq handler!\n");
		return DAL_INVALID_IRQ_HANDLER_IDX;
	}

	init_handler_common_data(handler_data, ih, handler_args, &adev->dm);

	irq_source = int_params->irq_source;

	handler_data->irq_source = irq_source;

	 
	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	switch (int_params->int_context) {
	case INTERRUPT_HIGH_IRQ_CONTEXT:
		hnd_list = &adev->dm.irq_handler_list_high_tab[irq_source];
		break;
	case INTERRUPT_LOW_IRQ_CONTEXT:
	default:
		hnd_list = &adev->dm.irq_handler_list_low_tab[irq_source];
		INIT_WORK(&handler_data->work, dm_irq_work_func);
		break;
	}

	list_add_tail(&handler_data->list, hnd_list);

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);

	 

	DRM_DEBUG_KMS(
		"DM_IRQ: added irq handler: %p for: dal_src=%d, irq context=%d\n",
		handler_data,
		irq_source,
		int_params->int_context);

	return handler_data;
}

 
void amdgpu_dm_irq_unregister_interrupt(struct amdgpu_device *adev,
					enum dc_irq_source irq_source,
					void *ih)
{
	struct list_head *handler_list;
	struct dc_interrupt_params int_params;
	int i;

	if (false == validate_irq_unregistration_params(irq_source, ih))
		return;

	memset(&int_params, 0, sizeof(int_params));

	int_params.irq_source = irq_source;

	for (i = 0; i < INTERRUPT_CONTEXT_NUMBER; i++) {

		int_params.int_context = i;

		handler_list = remove_irq_handler(adev, ih, &int_params);

		if (handler_list != NULL)
			break;
	}

	if (handler_list == NULL) {
		 
		DRM_ERROR(
		"DM_IRQ: failed to find irq handler:%p for irq_source:%d!\n",
			ih, irq_source);
	}
}

 
int amdgpu_dm_irq_init(struct amdgpu_device *adev)
{
	int src;
	struct list_head *lh;

	DRM_DEBUG_KMS("DM_IRQ\n");

	spin_lock_init(&adev->dm.irq_handler_list_table_lock);

	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++) {
		 
		lh = &adev->dm.irq_handler_list_low_tab[src];
		INIT_LIST_HEAD(lh);
		 
		INIT_LIST_HEAD(&adev->dm.irq_handler_list_high_tab[src]);
	}

	return 0;
}

 
void amdgpu_dm_irq_fini(struct amdgpu_device *adev)
{
	int src;
	struct list_head *lh;
	struct list_head *entry, *tmp;
	struct amdgpu_dm_irq_handler_data *handler;
	unsigned long irq_table_flags;

	DRM_DEBUG_KMS("DM_IRQ: releasing resources.\n");
	for (src = 0; src < DAL_IRQ_SOURCES_NUMBER; src++) {
		DM_IRQ_TABLE_LOCK(adev, irq_table_flags);
		 
		lh = &adev->dm.irq_handler_list_low_tab[src];
		DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);

		if (!list_empty(lh)) {
			list_for_each_safe(entry, tmp, lh) {
				handler = list_entry(
					entry,
					struct amdgpu_dm_irq_handler_data,
					list);
				flush_work(&handler->work);
			}
		}
	}
	 
	unregister_all_irq_handlers(adev);
}

int amdgpu_dm_irq_suspend(struct amdgpu_device *adev)
{
	int src;
	struct list_head *hnd_list_h;
	struct list_head *hnd_list_l;
	unsigned long irq_table_flags;
	struct list_head *entry, *tmp;
	struct amdgpu_dm_irq_handler_data *handler;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	DRM_DEBUG_KMS("DM_IRQ: suspend\n");

	 
	for (src = DC_IRQ_SOURCE_HPD1; src <= DC_IRQ_SOURCE_HPD6RX; src++) {
		hnd_list_l = &adev->dm.irq_handler_list_low_tab[src];
		hnd_list_h = &adev->dm.irq_handler_list_high_tab[src];
		if (!list_empty(hnd_list_l) || !list_empty(hnd_list_h))
			dc_interrupt_set(adev->dm.dc, src, false);

		DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);

		if (!list_empty(hnd_list_l)) {
			list_for_each_safe(entry, tmp, hnd_list_l) {
				handler = list_entry(
					entry,
					struct amdgpu_dm_irq_handler_data,
					list);
				flush_work(&handler->work);
			}
		}
		DM_IRQ_TABLE_LOCK(adev, irq_table_flags);
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);
	return 0;
}

int amdgpu_dm_irq_resume_early(struct amdgpu_device *adev)
{
	int src;
	struct list_head *hnd_list_h, *hnd_list_l;
	unsigned long irq_table_flags;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	DRM_DEBUG_KMS("DM_IRQ: early resume\n");

	 
	for (src = DC_IRQ_SOURCE_HPD1RX; src <= DC_IRQ_SOURCE_HPD6RX; src++) {
		hnd_list_l = &adev->dm.irq_handler_list_low_tab[src];
		hnd_list_h = &adev->dm.irq_handler_list_high_tab[src];
		if (!list_empty(hnd_list_l) || !list_empty(hnd_list_h))
			dc_interrupt_set(adev->dm.dc, src, true);
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);

	return 0;
}

int amdgpu_dm_irq_resume_late(struct amdgpu_device *adev)
{
	int src;
	struct list_head *hnd_list_h, *hnd_list_l;
	unsigned long irq_table_flags;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	DRM_DEBUG_KMS("DM_IRQ: resume\n");

	 
	for (src = DC_IRQ_SOURCE_HPD1; src <= DC_IRQ_SOURCE_HPD6; src++) {
		hnd_list_l = &adev->dm.irq_handler_list_low_tab[src];
		hnd_list_h = &adev->dm.irq_handler_list_high_tab[src];
		if (!list_empty(hnd_list_l) || !list_empty(hnd_list_h))
			dc_interrupt_set(adev->dm.dc, src, true);
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);
	return 0;
}

 
static void amdgpu_dm_irq_schedule_work(struct amdgpu_device *adev,
					enum dc_irq_source irq_source)
{
	struct  list_head *handler_list = &adev->dm.irq_handler_list_low_tab[irq_source];
	struct  amdgpu_dm_irq_handler_data *handler_data;
	bool    work_queued = false;

	if (list_empty(handler_list))
		return;

	list_for_each_entry(handler_data, handler_list, list) {
		if (queue_work(system_highpri_wq, &handler_data->work)) {
			work_queued = true;
			break;
		}
	}

	if (!work_queued) {
		struct  amdgpu_dm_irq_handler_data *handler_data_add;
		 
		handler_data = container_of(handler_list->next, struct amdgpu_dm_irq_handler_data, list);

		 
		handler_data_add = kzalloc(sizeof(*handler_data), GFP_ATOMIC);
		if (!handler_data_add) {
			DRM_ERROR("DM_IRQ: failed to allocate irq handler!\n");
			return;
		}

		 
		handler_data_add->handler       = handler_data->handler;
		handler_data_add->handler_arg   = handler_data->handler_arg;
		handler_data_add->dm            = handler_data->dm;
		handler_data_add->irq_source    = irq_source;

		list_add_tail(&handler_data_add->list, handler_list);

		INIT_WORK(&handler_data_add->work, dm_irq_work_func);

		if (queue_work(system_highpri_wq, &handler_data_add->work))
			DRM_DEBUG("Queued work for handling interrupt from "
				  "display for IRQ source %d\n",
				  irq_source);
		else
			DRM_ERROR("Failed to queue work for handling interrupt "
				  "from display for IRQ source %d\n",
				  irq_source);
	}
}

 
static void amdgpu_dm_irq_immediate_work(struct amdgpu_device *adev,
					 enum dc_irq_source irq_source)
{
	struct amdgpu_dm_irq_handler_data *handler_data;
	unsigned long irq_table_flags;

	DM_IRQ_TABLE_LOCK(adev, irq_table_flags);

	list_for_each_entry(handler_data,
			    &adev->dm.irq_handler_list_high_tab[irq_source],
			    list) {
		 
		handler_data->handler(handler_data->handler_arg);
	}

	DM_IRQ_TABLE_UNLOCK(adev, irq_table_flags);
}

 
static int amdgpu_dm_irq_handler(struct amdgpu_device *adev,
				 struct amdgpu_irq_src *source,
				 struct amdgpu_iv_entry *entry)
{

	enum dc_irq_source src =
		dc_interrupt_to_irq_source(
			adev->dm.dc,
			entry->src_id,
			entry->src_data[0]);

	dc_interrupt_ack(adev->dm.dc, src);

	 
	amdgpu_dm_irq_immediate_work(adev, src);
	 
	amdgpu_dm_irq_schedule_work(adev, src);

	return 0;
}

static enum dc_irq_source amdgpu_dm_hpd_to_dal_irq_source(unsigned int type)
{
	switch (type) {
	case AMDGPU_HPD_1:
		return DC_IRQ_SOURCE_HPD1;
	case AMDGPU_HPD_2:
		return DC_IRQ_SOURCE_HPD2;
	case AMDGPU_HPD_3:
		return DC_IRQ_SOURCE_HPD3;
	case AMDGPU_HPD_4:
		return DC_IRQ_SOURCE_HPD4;
	case AMDGPU_HPD_5:
		return DC_IRQ_SOURCE_HPD5;
	case AMDGPU_HPD_6:
		return DC_IRQ_SOURCE_HPD6;
	default:
		return DC_IRQ_SOURCE_INVALID;
	}
}

static int amdgpu_dm_set_hpd_irq_state(struct amdgpu_device *adev,
				       struct amdgpu_irq_src *source,
				       unsigned int type,
				       enum amdgpu_interrupt_state state)
{
	enum dc_irq_source src = amdgpu_dm_hpd_to_dal_irq_source(type);
	bool st = (state == AMDGPU_IRQ_STATE_ENABLE);

	dc_interrupt_set(adev->dm.dc, src, st);
	return 0;
}

static inline int dm_irq_state(struct amdgpu_device *adev,
			       struct amdgpu_irq_src *source,
			       unsigned int crtc_id,
			       enum amdgpu_interrupt_state state,
			       const enum irq_type dal_irq_type,
			       const char *func)
{
	bool st;
	enum dc_irq_source irq_source;

	struct amdgpu_crtc *acrtc = adev->mode_info.crtcs[crtc_id];

	if (!acrtc) {
		DRM_ERROR(
			"%s: crtc is NULL at id :%d\n",
			func,
			crtc_id);
		return 0;
	}

	if (acrtc->otg_inst == -1)
		return 0;

	irq_source = dal_irq_type + acrtc->otg_inst;

	st = (state == AMDGPU_IRQ_STATE_ENABLE);

	dc_interrupt_set(adev->dm.dc, irq_source, st);
	return 0;
}

static int amdgpu_dm_set_pflip_irq_state(struct amdgpu_device *adev,
					 struct amdgpu_irq_src *source,
					 unsigned int crtc_id,
					 enum amdgpu_interrupt_state state)
{
	return dm_irq_state(
		adev,
		source,
		crtc_id,
		state,
		IRQ_TYPE_PFLIP,
		__func__);
}

static int amdgpu_dm_set_crtc_irq_state(struct amdgpu_device *adev,
					struct amdgpu_irq_src *source,
					unsigned int crtc_id,
					enum amdgpu_interrupt_state state)
{
	return dm_irq_state(
		adev,
		source,
		crtc_id,
		state,
		IRQ_TYPE_VBLANK,
		__func__);
}

static int amdgpu_dm_set_vline0_irq_state(struct amdgpu_device *adev,
					struct amdgpu_irq_src *source,
					unsigned int crtc_id,
					enum amdgpu_interrupt_state state)
{
	return dm_irq_state(
		adev,
		source,
		crtc_id,
		state,
		IRQ_TYPE_VLINE0,
		__func__);
}

static int amdgpu_dm_set_dmub_outbox_irq_state(struct amdgpu_device *adev,
					struct amdgpu_irq_src *source,
					unsigned int crtc_id,
					enum amdgpu_interrupt_state state)
{
	enum dc_irq_source irq_source = DC_IRQ_SOURCE_DMCUB_OUTBOX;
	bool st = (state == AMDGPU_IRQ_STATE_ENABLE);

	dc_interrupt_set(adev->dm.dc, irq_source, st);
	return 0;
}

static int amdgpu_dm_set_vupdate_irq_state(struct amdgpu_device *adev,
					   struct amdgpu_irq_src *source,
					   unsigned int crtc_id,
					   enum amdgpu_interrupt_state state)
{
	return dm_irq_state(
		adev,
		source,
		crtc_id,
		state,
		IRQ_TYPE_VUPDATE,
		__func__);
}

static int amdgpu_dm_set_dmub_trace_irq_state(struct amdgpu_device *adev,
					   struct amdgpu_irq_src *source,
					   unsigned int type,
					   enum amdgpu_interrupt_state state)
{
	enum dc_irq_source irq_source = DC_IRQ_SOURCE_DMCUB_OUTBOX0;
	bool st = (state == AMDGPU_IRQ_STATE_ENABLE);

	dc_interrupt_set(adev->dm.dc, irq_source, st);
	return 0;
}

static const struct amdgpu_irq_src_funcs dm_crtc_irq_funcs = {
	.set = amdgpu_dm_set_crtc_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_vline0_irq_funcs = {
	.set = amdgpu_dm_set_vline0_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_dmub_outbox_irq_funcs = {
	.set = amdgpu_dm_set_dmub_outbox_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_vupdate_irq_funcs = {
	.set = amdgpu_dm_set_vupdate_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_dmub_trace_irq_funcs = {
	.set = amdgpu_dm_set_dmub_trace_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_pageflip_irq_funcs = {
	.set = amdgpu_dm_set_pflip_irq_state,
	.process = amdgpu_dm_irq_handler,
};

static const struct amdgpu_irq_src_funcs dm_hpd_irq_funcs = {
	.set = amdgpu_dm_set_hpd_irq_state,
	.process = amdgpu_dm_irq_handler,
};

void amdgpu_dm_set_irq_funcs(struct amdgpu_device *adev)
{
	adev->crtc_irq.num_types = adev->mode_info.num_crtc;
	adev->crtc_irq.funcs = &dm_crtc_irq_funcs;

	adev->vline0_irq.num_types = adev->mode_info.num_crtc;
	adev->vline0_irq.funcs = &dm_vline0_irq_funcs;

	adev->dmub_outbox_irq.num_types = 1;
	adev->dmub_outbox_irq.funcs = &dm_dmub_outbox_irq_funcs;

	adev->vupdate_irq.num_types = adev->mode_info.num_crtc;
	adev->vupdate_irq.funcs = &dm_vupdate_irq_funcs;

	adev->dmub_trace_irq.num_types = 1;
	adev->dmub_trace_irq.funcs = &dm_dmub_trace_irq_funcs;

	adev->pageflip_irq.num_types = adev->mode_info.num_crtc;
	adev->pageflip_irq.funcs = &dm_pageflip_irq_funcs;

	adev->hpd_irq.num_types = adev->mode_info.num_hpd;
	adev->hpd_irq.funcs = &dm_hpd_irq_funcs;
}
void amdgpu_dm_outbox_init(struct amdgpu_device *adev)
{
	dc_interrupt_set(adev->dm.dc,
		DC_IRQ_SOURCE_DMCUB_OUTBOX,
		true);
}

 
void amdgpu_dm_hpd_init(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_connector *connector;
	struct drm_connector_list_iter iter;

	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter(connector, &iter) {
		struct amdgpu_dm_connector *amdgpu_dm_connector =
				to_amdgpu_dm_connector(connector);

		const struct dc_link *dc_link = amdgpu_dm_connector->dc_link;

		if (dc_link->irq_source_hpd != DC_IRQ_SOURCE_INVALID) {
			dc_interrupt_set(adev->dm.dc,
					dc_link->irq_source_hpd,
					true);
		}

		if (dc_link->irq_source_hpd_rx != DC_IRQ_SOURCE_INVALID) {
			dc_interrupt_set(adev->dm.dc,
					dc_link->irq_source_hpd_rx,
					true);
		}
	}
	drm_connector_list_iter_end(&iter);
}

 
void amdgpu_dm_hpd_fini(struct amdgpu_device *adev)
{
	struct drm_device *dev = adev_to_drm(adev);
	struct drm_connector *connector;
	struct drm_connector_list_iter iter;

	drm_connector_list_iter_begin(dev, &iter);
	drm_for_each_connector_iter(connector, &iter) {
		struct amdgpu_dm_connector *amdgpu_dm_connector =
				to_amdgpu_dm_connector(connector);
		const struct dc_link *dc_link = amdgpu_dm_connector->dc_link;

		if (dc_link->irq_source_hpd != DC_IRQ_SOURCE_INVALID) {
			dc_interrupt_set(adev->dm.dc,
					dc_link->irq_source_hpd,
					false);
		}

		if (dc_link->irq_source_hpd_rx != DC_IRQ_SOURCE_INVALID) {
			dc_interrupt_set(adev->dm.dc,
					dc_link->irq_source_hpd_rx,
					false);
		}
	}
	drm_connector_list_iter_end(&iter);
}
