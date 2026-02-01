
 

#include <linux/dma-fence.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_property.h>
#include <drm/drm_writeback.h>

 

#define fence_to_wb_connector(x) container_of(x->lock, \
					      struct drm_writeback_connector, \
					      fence_lock)

static const char *drm_writeback_fence_get_driver_name(struct dma_fence *fence)
{
	struct drm_writeback_connector *wb_connector =
		fence_to_wb_connector(fence);

	return wb_connector->base.dev->driver->name;
}

static const char *
drm_writeback_fence_get_timeline_name(struct dma_fence *fence)
{
	struct drm_writeback_connector *wb_connector =
		fence_to_wb_connector(fence);

	return wb_connector->timeline_name;
}

static bool drm_writeback_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static const struct dma_fence_ops drm_writeback_fence_ops = {
	.get_driver_name = drm_writeback_fence_get_driver_name,
	.get_timeline_name = drm_writeback_fence_get_timeline_name,
	.enable_signaling = drm_writeback_fence_enable_signaling,
};

static int create_writeback_properties(struct drm_device *dev)
{
	struct drm_property *prop;

	if (!dev->mode_config.writeback_fb_id_property) {
		prop = drm_property_create_object(dev, DRM_MODE_PROP_ATOMIC,
						  "WRITEBACK_FB_ID",
						  DRM_MODE_OBJECT_FB);
		if (!prop)
			return -ENOMEM;
		dev->mode_config.writeback_fb_id_property = prop;
	}

	if (!dev->mode_config.writeback_pixel_formats_property) {
		prop = drm_property_create(dev, DRM_MODE_PROP_BLOB |
					   DRM_MODE_PROP_ATOMIC |
					   DRM_MODE_PROP_IMMUTABLE,
					   "WRITEBACK_PIXEL_FORMATS", 0);
		if (!prop)
			return -ENOMEM;
		dev->mode_config.writeback_pixel_formats_property = prop;
	}

	if (!dev->mode_config.writeback_out_fence_ptr_property) {
		prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
						 "WRITEBACK_OUT_FENCE_PTR", 0,
						 U64_MAX);
		if (!prop)
			return -ENOMEM;
		dev->mode_config.writeback_out_fence_ptr_property = prop;
	}

	return 0;
}

static const struct drm_encoder_funcs drm_writeback_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

 
int drm_writeback_connector_init(struct drm_device *dev,
				 struct drm_writeback_connector *wb_connector,
				 const struct drm_connector_funcs *con_funcs,
				 const struct drm_encoder_helper_funcs *enc_helper_funcs,
				 const u32 *formats, int n_formats,
				 u32 possible_crtcs)
{
	int ret = 0;

	drm_encoder_helper_add(&wb_connector->encoder, enc_helper_funcs);

	wb_connector->encoder.possible_crtcs = possible_crtcs;

	ret = drm_encoder_init(dev, &wb_connector->encoder,
			       &drm_writeback_encoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret)
		return ret;

	ret = drm_writeback_connector_init_with_encoder(dev, wb_connector, &wb_connector->encoder,
			con_funcs, formats, n_formats);

	if (ret)
		drm_encoder_cleanup(&wb_connector->encoder);

	return ret;
}
EXPORT_SYMBOL(drm_writeback_connector_init);

 
int drm_writeback_connector_init_with_encoder(struct drm_device *dev,
		struct drm_writeback_connector *wb_connector, struct drm_encoder *enc,
		const struct drm_connector_funcs *con_funcs, const u32 *formats,
		int n_formats)
{
	struct drm_property_blob *blob;
	struct drm_connector *connector = &wb_connector->base;
	struct drm_mode_config *config = &dev->mode_config;
	int ret = create_writeback_properties(dev);

	if (ret != 0)
		return ret;

	blob = drm_property_create_blob(dev, n_formats * sizeof(*formats),
					formats);
	if (IS_ERR(blob))
		return PTR_ERR(blob);


	connector->interlace_allowed = 0;

	ret = drm_connector_init(dev, connector, con_funcs,
				 DRM_MODE_CONNECTOR_WRITEBACK);
	if (ret)
		goto connector_fail;

	ret = drm_connector_attach_encoder(connector, enc);
	if (ret)
		goto attach_fail;

	INIT_LIST_HEAD(&wb_connector->job_queue);
	spin_lock_init(&wb_connector->job_lock);

	wb_connector->fence_context = dma_fence_context_alloc(1);
	spin_lock_init(&wb_connector->fence_lock);
	snprintf(wb_connector->timeline_name,
		 sizeof(wb_connector->timeline_name),
		 "CONNECTOR:%d-%s", connector->base.id, connector->name);

	drm_object_attach_property(&connector->base,
				   config->writeback_out_fence_ptr_property, 0);

	drm_object_attach_property(&connector->base,
				   config->writeback_fb_id_property, 0);

	drm_object_attach_property(&connector->base,
				   config->writeback_pixel_formats_property,
				   blob->base.id);
	wb_connector->pixel_formats_blob_ptr = blob;

	return 0;

attach_fail:
	drm_connector_cleanup(connector);
connector_fail:
	drm_property_blob_put(blob);
	return ret;
}
EXPORT_SYMBOL(drm_writeback_connector_init_with_encoder);

int drm_writeback_set_fb(struct drm_connector_state *conn_state,
			 struct drm_framebuffer *fb)
{
	WARN_ON(conn_state->connector->connector_type != DRM_MODE_CONNECTOR_WRITEBACK);

	if (!conn_state->writeback_job) {
		conn_state->writeback_job =
			kzalloc(sizeof(*conn_state->writeback_job), GFP_KERNEL);
		if (!conn_state->writeback_job)
			return -ENOMEM;

		conn_state->writeback_job->connector =
			drm_connector_to_writeback(conn_state->connector);
	}

	drm_framebuffer_assign(&conn_state->writeback_job->fb, fb);
	return 0;
}

int drm_writeback_prepare_job(struct drm_writeback_job *job)
{
	struct drm_writeback_connector *connector = job->connector;
	const struct drm_connector_helper_funcs *funcs =
		connector->base.helper_private;
	int ret;

	if (funcs->prepare_writeback_job) {
		ret = funcs->prepare_writeback_job(connector, job);
		if (ret < 0)
			return ret;
	}

	job->prepared = true;
	return 0;
}
EXPORT_SYMBOL(drm_writeback_prepare_job);

 
void drm_writeback_queue_job(struct drm_writeback_connector *wb_connector,
			     struct drm_connector_state *conn_state)
{
	struct drm_writeback_job *job;
	unsigned long flags;

	job = conn_state->writeback_job;
	conn_state->writeback_job = NULL;

	spin_lock_irqsave(&wb_connector->job_lock, flags);
	list_add_tail(&job->list_entry, &wb_connector->job_queue);
	spin_unlock_irqrestore(&wb_connector->job_lock, flags);
}
EXPORT_SYMBOL(drm_writeback_queue_job);

void drm_writeback_cleanup_job(struct drm_writeback_job *job)
{
	struct drm_writeback_connector *connector = job->connector;
	const struct drm_connector_helper_funcs *funcs =
		connector->base.helper_private;

	if (job->prepared && funcs->cleanup_writeback_job)
		funcs->cleanup_writeback_job(connector, job);

	if (job->fb)
		drm_framebuffer_put(job->fb);

	if (job->out_fence)
		dma_fence_put(job->out_fence);

	kfree(job);
}
EXPORT_SYMBOL(drm_writeback_cleanup_job);

 
static void cleanup_work(struct work_struct *work)
{
	struct drm_writeback_job *job = container_of(work,
						     struct drm_writeback_job,
						     cleanup_work);

	drm_writeback_cleanup_job(job);
}

 
void
drm_writeback_signal_completion(struct drm_writeback_connector *wb_connector,
				int status)
{
	unsigned long flags;
	struct drm_writeback_job *job;
	struct dma_fence *out_fence;

	spin_lock_irqsave(&wb_connector->job_lock, flags);
	job = list_first_entry_or_null(&wb_connector->job_queue,
				       struct drm_writeback_job,
				       list_entry);
	if (job)
		list_del(&job->list_entry);

	spin_unlock_irqrestore(&wb_connector->job_lock, flags);

	if (WARN_ON(!job))
		return;

	out_fence = job->out_fence;
	if (out_fence) {
		if (status)
			dma_fence_set_error(out_fence, status);
		dma_fence_signal(out_fence);
		dma_fence_put(out_fence);
		job->out_fence = NULL;
	}

	INIT_WORK(&job->cleanup_work, cleanup_work);
	queue_work(system_long_wq, &job->cleanup_work);
}
EXPORT_SYMBOL(drm_writeback_signal_completion);

struct dma_fence *
drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector)
{
	struct dma_fence *fence;

	if (WARN_ON(wb_connector->base.connector_type !=
		    DRM_MODE_CONNECTOR_WRITEBACK))
		return NULL;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &drm_writeback_fence_ops,
		       &wb_connector->fence_lock, wb_connector->fence_context,
		       ++wb_connector->fence_seqno);

	return fence;
}
EXPORT_SYMBOL(drm_writeback_get_out_fence);
