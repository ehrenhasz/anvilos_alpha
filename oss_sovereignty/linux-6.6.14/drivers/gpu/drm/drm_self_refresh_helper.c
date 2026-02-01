
 
#include <linux/average.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_print.h>
#include <drm/drm_self_refresh_helper.h>

 

#define SELF_REFRESH_AVG_SEED_MS 200

DECLARE_EWMA(psr_time, 4, 4)

struct drm_self_refresh_data {
	struct drm_crtc *crtc;
	struct delayed_work entry_work;

	struct mutex avg_mutex;
	struct ewma_psr_time entry_avg_ms;
	struct ewma_psr_time exit_avg_ms;
};

static void drm_self_refresh_helper_entry_work(struct work_struct *work)
{
	struct drm_self_refresh_data *sr_data = container_of(
				to_delayed_work(work),
				struct drm_self_refresh_data, entry_work);
	struct drm_crtc *crtc = sr_data->crtc;
	struct drm_device *dev = crtc->dev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state;
	int i, ret = 0;

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_drop_locks;
	}

retry:
	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	if (!crtc_state->enable)
		goto out;

	ret = drm_atomic_add_affected_connectors(state, crtc);
	if (ret)
		goto out;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		if (!conn_state->self_refresh_aware)
			goto out;
	}

	crtc_state->active = false;
	crtc_state->self_refresh_active = true;

	ret = drm_atomic_commit(state);
	if (ret)
		goto out;

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret)
			goto retry;
	}

	drm_atomic_state_put(state);

out_drop_locks:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

 
void
drm_self_refresh_helper_update_avg_times(struct drm_atomic_state *state,
					 unsigned int commit_time_ms,
					 unsigned int new_self_refresh_mask)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		bool new_self_refresh_active = new_self_refresh_mask & BIT(i);
		struct drm_self_refresh_data *sr_data = crtc->self_refresh_data;
		struct ewma_psr_time *time;

		if (old_crtc_state->self_refresh_active ==
		    new_self_refresh_active)
			continue;

		if (new_self_refresh_active)
			time = &sr_data->entry_avg_ms;
		else
			time = &sr_data->exit_avg_ms;

		mutex_lock(&sr_data->avg_mutex);
		ewma_psr_time_add(time, commit_time_ms);
		mutex_unlock(&sr_data->avg_mutex);
	}
}
EXPORT_SYMBOL(drm_self_refresh_helper_update_avg_times);

 
void drm_self_refresh_helper_alter_state(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;

	if (state->async_update || !state->allow_modeset) {
		for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
			if (crtc_state->self_refresh_active) {
				state->async_update = false;
				state->allow_modeset = true;
				break;
			}
		}
	}

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		struct drm_self_refresh_data *sr_data;
		unsigned int delay;

		 
		if (crtc_state->self_refresh_active)
			continue;

		sr_data = crtc->self_refresh_data;
		if (!sr_data)
			continue;

		mutex_lock(&sr_data->avg_mutex);
		delay = (ewma_psr_time_read(&sr_data->entry_avg_ms) +
			 ewma_psr_time_read(&sr_data->exit_avg_ms)) * 2;
		mutex_unlock(&sr_data->avg_mutex);

		mod_delayed_work(system_wq, &sr_data->entry_work,
				 msecs_to_jiffies(delay));
	}
}
EXPORT_SYMBOL(drm_self_refresh_helper_alter_state);

 
int drm_self_refresh_helper_init(struct drm_crtc *crtc)
{
	struct drm_self_refresh_data *sr_data = crtc->self_refresh_data;

	 
	if (WARN_ON(sr_data))
		return -EINVAL;

	sr_data = kzalloc(sizeof(*sr_data), GFP_KERNEL);
	if (!sr_data)
		return -ENOMEM;

	INIT_DELAYED_WORK(&sr_data->entry_work,
			  drm_self_refresh_helper_entry_work);
	sr_data->crtc = crtc;
	mutex_init(&sr_data->avg_mutex);
	ewma_psr_time_init(&sr_data->entry_avg_ms);
	ewma_psr_time_init(&sr_data->exit_avg_ms);

	 
	ewma_psr_time_add(&sr_data->entry_avg_ms, SELF_REFRESH_AVG_SEED_MS);
	ewma_psr_time_add(&sr_data->exit_avg_ms, SELF_REFRESH_AVG_SEED_MS);

	crtc->self_refresh_data = sr_data;
	return 0;
}
EXPORT_SYMBOL(drm_self_refresh_helper_init);

 
void drm_self_refresh_helper_cleanup(struct drm_crtc *crtc)
{
	struct drm_self_refresh_data *sr_data = crtc->self_refresh_data;

	 
	if (!sr_data)
		return;

	crtc->self_refresh_data = NULL;

	cancel_delayed_work_sync(&sr_data->entry_work);
	kfree(sr_data);
}
EXPORT_SYMBOL(drm_self_refresh_helper_cleanup);
