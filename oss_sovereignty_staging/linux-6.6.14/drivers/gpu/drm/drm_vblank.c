 

#include <linux/export.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>

#include <drm/drm_crtc.h>
#include <drm/drm_drv.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_managed.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include "drm_internal.h"
#include "drm_trace.h"

 

 
#define DRM_TIMESTAMP_MAXRETRIES 3

 
#define DRM_REDUNDANT_VBLIRQ_THRESH_NS 1000000

static bool
drm_get_last_vbltimestamp(struct drm_device *dev, unsigned int pipe,
			  ktime_t *tvblank, bool in_vblank_irq);

static unsigned int drm_timestamp_precision = 20;   

static int drm_vblank_offdelay = 5000;     

module_param_named(vblankoffdelay, drm_vblank_offdelay, int, 0600);
module_param_named(timestamp_precision_usec, drm_timestamp_precision, int, 0600);
MODULE_PARM_DESC(vblankoffdelay, "Delay until vblank irq auto-disable [msecs] (0: never disable, <0: disable immediately)");
MODULE_PARM_DESC(timestamp_precision_usec, "Max. error on timestamps [usecs]");

static void store_vblank(struct drm_device *dev, unsigned int pipe,
			 u32 vblank_count_inc,
			 ktime_t t_vblank, u32 last)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	assert_spin_locked(&dev->vblank_time_lock);

	vblank->last = last;

	write_seqlock(&vblank->seqlock);
	vblank->time = t_vblank;
	atomic64_add(vblank_count_inc, &vblank->count);
	write_sequnlock(&vblank->seqlock);
}

static u32 drm_max_vblank_count(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	return vblank->max_vblank_count ?: dev->max_vblank_count;
}

 
static u32 drm_vblank_no_hw_counter(struct drm_device *dev, unsigned int pipe)
{
	drm_WARN_ON_ONCE(dev, drm_max_vblank_count(dev, pipe) != 0);
	return 0;
}

static u32 __get_vblank_counter(struct drm_device *dev, unsigned int pipe)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);

		if (drm_WARN_ON(dev, !crtc))
			return 0;

		if (crtc->funcs->get_vblank_counter)
			return crtc->funcs->get_vblank_counter(crtc);
	}
#ifdef CONFIG_DRM_LEGACY
	else if (dev->driver->get_vblank_counter) {
		return dev->driver->get_vblank_counter(dev, pipe);
	}
#endif

	return drm_vblank_no_hw_counter(dev, pipe);
}

 
static void drm_reset_vblank_timestamp(struct drm_device *dev, unsigned int pipe)
{
	u32 cur_vblank;
	bool rc;
	ktime_t t_vblank;
	int count = DRM_TIMESTAMP_MAXRETRIES;

	spin_lock(&dev->vblank_time_lock);

	 
	do {
		cur_vblank = __get_vblank_counter(dev, pipe);
		rc = drm_get_last_vbltimestamp(dev, pipe, &t_vblank, false);
	} while (cur_vblank != __get_vblank_counter(dev, pipe) && --count > 0);

	 
	if (!rc)
		t_vblank = 0;

	 
	store_vblank(dev, pipe, 1, t_vblank, cur_vblank);

	spin_unlock(&dev->vblank_time_lock);
}

 
static void drm_update_vblank_count(struct drm_device *dev, unsigned int pipe,
				    bool in_vblank_irq)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	u32 cur_vblank, diff;
	bool rc;
	ktime_t t_vblank;
	int count = DRM_TIMESTAMP_MAXRETRIES;
	int framedur_ns = vblank->framedur_ns;
	u32 max_vblank_count = drm_max_vblank_count(dev, pipe);

	 
	do {
		cur_vblank = __get_vblank_counter(dev, pipe);
		rc = drm_get_last_vbltimestamp(dev, pipe, &t_vblank, in_vblank_irq);
	} while (cur_vblank != __get_vblank_counter(dev, pipe) && --count > 0);

	if (max_vblank_count) {
		 
		diff = (cur_vblank - vblank->last) & max_vblank_count;
	} else if (rc && framedur_ns) {
		u64 diff_ns = ktime_to_ns(ktime_sub(t_vblank, vblank->time));

		 

		drm_dbg_vbl(dev, "crtc %u: Calculating number of vblanks."
			    " diff_ns = %lld, framedur_ns = %d)\n",
			    pipe, (long long)diff_ns, framedur_ns);

		diff = DIV_ROUND_CLOSEST_ULL(diff_ns, framedur_ns);

		if (diff == 0 && in_vblank_irq)
			drm_dbg_vbl(dev, "crtc %u: Redundant vblirq ignored\n",
				    pipe);
	} else {
		 
		diff = in_vblank_irq ? 1 : 0;
	}

	 
	if (diff > 1 && (vblank->inmodeset & 0x2)) {
		drm_dbg_vbl(dev,
			    "clamping vblank bump to 1 on crtc %u: diffr=%u"
			    " due to pre-modeset.\n", pipe, diff);
		diff = 1;
	}

	drm_dbg_vbl(dev, "updating vblank count on crtc %u:"
		    " current=%llu, diff=%u, hw=%u hw_last=%u\n",
		    pipe, (unsigned long long)atomic64_read(&vblank->count),
		    diff, cur_vblank, vblank->last);

	if (diff == 0) {
		drm_WARN_ON_ONCE(dev, cur_vblank != vblank->last);
		return;
	}

	 
	if (!rc && !in_vblank_irq)
		t_vblank = 0;

	store_vblank(dev, pipe, diff, t_vblank, cur_vblank);
}

u64 drm_vblank_count(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	u64 count;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return 0;

	count = atomic64_read(&vblank->count);

	 
	smp_rmb();

	return count;
}

 
u64 drm_crtc_accurate_vblank_count(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	u64 vblank;
	unsigned long flags;

	drm_WARN_ONCE(dev, drm_debug_enabled(DRM_UT_VBL) &&
		      !crtc->funcs->get_vblank_timestamp,
		      "This function requires support for accurate vblank timestamps.");

	spin_lock_irqsave(&dev->vblank_time_lock, flags);

	drm_update_vblank_count(dev, pipe, false);
	vblank = drm_vblank_count(dev, pipe);

	spin_unlock_irqrestore(&dev->vblank_time_lock, flags);

	return vblank;
}
EXPORT_SYMBOL(drm_crtc_accurate_vblank_count);

static void __disable_vblank(struct drm_device *dev, unsigned int pipe)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);

		if (drm_WARN_ON(dev, !crtc))
			return;

		if (crtc->funcs->disable_vblank)
			crtc->funcs->disable_vblank(crtc);
	}
#ifdef CONFIG_DRM_LEGACY
	else {
		dev->driver->disable_vblank(dev, pipe);
	}
#endif
}

 
void drm_vblank_disable_and_save(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	unsigned long irqflags;

	assert_spin_locked(&dev->vbl_lock);

	 
	spin_lock_irqsave(&dev->vblank_time_lock, irqflags);

	 
	if (!vblank->enabled)
		goto out;

	 
	drm_update_vblank_count(dev, pipe, false);
	__disable_vblank(dev, pipe);
	vblank->enabled = false;

out:
	spin_unlock_irqrestore(&dev->vblank_time_lock, irqflags);
}

static void vblank_disable_fn(struct timer_list *t)
{
	struct drm_vblank_crtc *vblank = from_timer(vblank, t, disable_timer);
	struct drm_device *dev = vblank->dev;
	unsigned int pipe = vblank->pipe;
	unsigned long irqflags;

	spin_lock_irqsave(&dev->vbl_lock, irqflags);
	if (atomic_read(&vblank->refcount) == 0 && vblank->enabled) {
		drm_dbg_core(dev, "disabling vblank on crtc %u\n", pipe);
		drm_vblank_disable_and_save(dev, pipe);
	}
	spin_unlock_irqrestore(&dev->vbl_lock, irqflags);
}

static void drm_vblank_init_release(struct drm_device *dev, void *ptr)
{
	struct drm_vblank_crtc *vblank = ptr;

	drm_WARN_ON(dev, READ_ONCE(vblank->enabled) &&
		    drm_core_check_feature(dev, DRIVER_MODESET));

	drm_vblank_destroy_worker(vblank);
	del_timer_sync(&vblank->disable_timer);
}

 
int drm_vblank_init(struct drm_device *dev, unsigned int num_crtcs)
{
	int ret;
	unsigned int i;

	spin_lock_init(&dev->vbl_lock);
	spin_lock_init(&dev->vblank_time_lock);

	dev->vblank = drmm_kcalloc(dev, num_crtcs, sizeof(*dev->vblank), GFP_KERNEL);
	if (!dev->vblank)
		return -ENOMEM;

	dev->num_crtcs = num_crtcs;

	for (i = 0; i < num_crtcs; i++) {
		struct drm_vblank_crtc *vblank = &dev->vblank[i];

		vblank->dev = dev;
		vblank->pipe = i;
		init_waitqueue_head(&vblank->queue);
		timer_setup(&vblank->disable_timer, vblank_disable_fn, 0);
		seqlock_init(&vblank->seqlock);

		ret = drmm_add_action_or_reset(dev, drm_vblank_init_release,
					       vblank);
		if (ret)
			return ret;

		ret = drm_vblank_worker_init(vblank);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_vblank_init);

 
bool drm_dev_has_vblank(const struct drm_device *dev)
{
	return dev->num_crtcs != 0;
}
EXPORT_SYMBOL(drm_dev_has_vblank);

 
wait_queue_head_t *drm_crtc_vblank_waitqueue(struct drm_crtc *crtc)
{
	return &crtc->dev->vblank[drm_crtc_index(crtc)].queue;
}
EXPORT_SYMBOL(drm_crtc_vblank_waitqueue);


 
void drm_calc_timestamping_constants(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	int linedur_ns = 0, framedur_ns = 0;
	int dotclock = mode->crtc_clock;

	if (!drm_dev_has_vblank(dev))
		return;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	 
	if (dotclock > 0) {
		int frame_size = mode->crtc_htotal * mode->crtc_vtotal;

		 
		linedur_ns  = div_u64((u64) mode->crtc_htotal * 1000000, dotclock);
		framedur_ns = div_u64((u64) frame_size * 1000000, dotclock);

		 
		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			framedur_ns /= 2;
	} else {
		drm_err(dev, "crtc %u: Can't calculate constants, dotclock = 0!\n",
			crtc->base.id);
	}

	vblank->linedur_ns  = linedur_ns;
	vblank->framedur_ns = framedur_ns;
	drm_mode_copy(&vblank->hwmode, mode);

	drm_dbg_core(dev,
		     "crtc %u: hwmode: htotal %d, vtotal %d, vdisplay %d\n",
		     crtc->base.id, mode->crtc_htotal,
		     mode->crtc_vtotal, mode->crtc_vdisplay);
	drm_dbg_core(dev, "crtc %u: clock %d kHz framedur %d linedur %d\n",
		     crtc->base.id, dotclock, framedur_ns, linedur_ns);
}
EXPORT_SYMBOL(drm_calc_timestamping_constants);

 
bool
drm_crtc_vblank_helper_get_vblank_timestamp_internal(
	struct drm_crtc *crtc, int *max_error, ktime_t *vblank_time,
	bool in_vblank_irq,
	drm_vblank_get_scanout_position_func get_scanout_position)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = crtc->index;
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	struct timespec64 ts_etime, ts_vblank_time;
	ktime_t stime, etime;
	bool vbl_status;
	const struct drm_display_mode *mode;
	int vpos, hpos, i;
	int delta_ns, duration_ns;

	if (pipe >= dev->num_crtcs) {
		drm_err(dev, "Invalid crtc %u\n", pipe);
		return false;
	}

	 
	if (!get_scanout_position) {
		drm_err(dev, "Called from CRTC w/o get_scanout_position()!?\n");
		return false;
	}

	if (drm_drv_uses_atomic_modeset(dev))
		mode = &vblank->hwmode;
	else
		mode = &crtc->hwmode;

	 
	if (mode->crtc_clock == 0) {
		drm_dbg_core(dev, "crtc %u: Noop due to uninitialized mode.\n",
			     pipe);
		drm_WARN_ON_ONCE(dev, drm_drv_uses_atomic_modeset(dev));
		return false;
	}

	 
	for (i = 0; i < DRM_TIMESTAMP_MAXRETRIES; i++) {
		 
		vbl_status = get_scanout_position(crtc, in_vblank_irq,
						  &vpos, &hpos,
						  &stime, &etime,
						  mode);

		 
		if (!vbl_status) {
			drm_dbg_core(dev,
				     "crtc %u : scanoutpos query failed.\n",
				     pipe);
			return false;
		}

		 
		duration_ns = ktime_to_ns(etime) - ktime_to_ns(stime);

		 
		if (duration_ns <= *max_error)
			break;
	}

	 
	if (i == DRM_TIMESTAMP_MAXRETRIES) {
		drm_dbg_core(dev,
			     "crtc %u: Noisy timestamp %d us > %d us [%d reps].\n",
			     pipe, duration_ns / 1000, *max_error / 1000, i);
	}

	 
	*max_error = duration_ns;

	 
	delta_ns = div_s64(1000000LL * (vpos * mode->crtc_htotal + hpos),
			   mode->crtc_clock);

	 
	*vblank_time = ktime_sub_ns(etime, delta_ns);

	if (!drm_debug_enabled(DRM_UT_VBL))
		return true;

	ts_etime = ktime_to_timespec64(etime);
	ts_vblank_time = ktime_to_timespec64(*vblank_time);

	drm_dbg_vbl(dev,
		    "crtc %u : v p(%d,%d)@ %lld.%06ld -> %lld.%06ld [e %d us, %d rep]\n",
		    pipe, hpos, vpos,
		    (u64)ts_etime.tv_sec, ts_etime.tv_nsec / 1000,
		    (u64)ts_vblank_time.tv_sec, ts_vblank_time.tv_nsec / 1000,
		    duration_ns / 1000, i);

	return true;
}
EXPORT_SYMBOL(drm_crtc_vblank_helper_get_vblank_timestamp_internal);

 
bool drm_crtc_vblank_helper_get_vblank_timestamp(struct drm_crtc *crtc,
						 int *max_error,
						 ktime_t *vblank_time,
						 bool in_vblank_irq)
{
	return drm_crtc_vblank_helper_get_vblank_timestamp_internal(
		crtc, max_error, vblank_time, in_vblank_irq,
		crtc->helper_private->get_scanout_position);
}
EXPORT_SYMBOL(drm_crtc_vblank_helper_get_vblank_timestamp);

 
static bool
drm_crtc_get_last_vbltimestamp(struct drm_crtc *crtc, ktime_t *tvblank,
			       bool in_vblank_irq)
{
	bool ret = false;

	 
	int max_error = (int) drm_timestamp_precision * 1000;

	 
	if (crtc && crtc->funcs->get_vblank_timestamp && max_error > 0) {
		ret = crtc->funcs->get_vblank_timestamp(crtc, &max_error,
							tvblank, in_vblank_irq);
	}

	 
	if (!ret)
		*tvblank = ktime_get();

	return ret;
}

static bool
drm_get_last_vbltimestamp(struct drm_device *dev, unsigned int pipe,
			  ktime_t *tvblank, bool in_vblank_irq)
{
	struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);

	return drm_crtc_get_last_vbltimestamp(crtc, tvblank, in_vblank_irq);
}

 
u64 drm_crtc_vblank_count(struct drm_crtc *crtc)
{
	return drm_vblank_count(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_vblank_count);

 
static u64 drm_vblank_count_and_time(struct drm_device *dev, unsigned int pipe,
				     ktime_t *vblanktime)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	u64 vblank_count;
	unsigned int seq;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs)) {
		*vblanktime = 0;
		return 0;
	}

	do {
		seq = read_seqbegin(&vblank->seqlock);
		vblank_count = atomic64_read(&vblank->count);
		*vblanktime = vblank->time;
	} while (read_seqretry(&vblank->seqlock, seq));

	return vblank_count;
}

 
u64 drm_crtc_vblank_count_and_time(struct drm_crtc *crtc,
				   ktime_t *vblanktime)
{
	return drm_vblank_count_and_time(crtc->dev, drm_crtc_index(crtc),
					 vblanktime);
}
EXPORT_SYMBOL(drm_crtc_vblank_count_and_time);

 
int drm_crtc_next_vblank_start(struct drm_crtc *crtc, ktime_t *vblanktime)
{
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank;
	struct drm_display_mode *mode;
	u64 vblank_start;

	if (!drm_dev_has_vblank(crtc->dev))
		return -EINVAL;

	vblank = &crtc->dev->vblank[pipe];
	mode = &vblank->hwmode;

	if (!vblank->framedur_ns || !vblank->linedur_ns)
		return -EINVAL;

	if (!drm_crtc_get_last_vbltimestamp(crtc, vblanktime, false))
		return -EINVAL;

	vblank_start = DIV_ROUND_DOWN_ULL(
			(u64)vblank->framedur_ns * mode->crtc_vblank_start,
			mode->crtc_vtotal);
	*vblanktime  = ktime_add(*vblanktime, ns_to_ktime(vblank_start));

	return 0;
}
EXPORT_SYMBOL(drm_crtc_next_vblank_start);

static void send_vblank_event(struct drm_device *dev,
		struct drm_pending_vblank_event *e,
		u64 seq, ktime_t now)
{
	struct timespec64 tv;

	switch (e->event.base.type) {
	case DRM_EVENT_VBLANK:
	case DRM_EVENT_FLIP_COMPLETE:
		tv = ktime_to_timespec64(now);
		e->event.vbl.sequence = seq;
		 
		e->event.vbl.tv_sec = tv.tv_sec;
		e->event.vbl.tv_usec = tv.tv_nsec / 1000;
		break;
	case DRM_EVENT_CRTC_SEQUENCE:
		if (seq)
			e->event.seq.sequence = seq;
		e->event.seq.time_ns = ktime_to_ns(now);
		break;
	}
	trace_drm_vblank_event_delivered(e->base.file_priv, e->pipe, seq);
	 
	drm_send_event_timestamp_locked(dev, &e->base, now);
}

 
void drm_crtc_arm_vblank_event(struct drm_crtc *crtc,
			       struct drm_pending_vblank_event *e)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);

	assert_spin_locked(&dev->event_lock);

	e->pipe = pipe;
	e->sequence = drm_crtc_accurate_vblank_count(crtc) + 1;
	list_add_tail(&e->base.link, &dev->vblank_event_list);
}
EXPORT_SYMBOL(drm_crtc_arm_vblank_event);

 
void drm_crtc_send_vblank_event(struct drm_crtc *crtc,
				struct drm_pending_vblank_event *e)
{
	struct drm_device *dev = crtc->dev;
	u64 seq;
	unsigned int pipe = drm_crtc_index(crtc);
	ktime_t now;

	if (drm_dev_has_vblank(dev)) {
		seq = drm_vblank_count_and_time(dev, pipe, &now);
	} else {
		seq = 0;

		now = ktime_get();
	}
	e->pipe = pipe;
	send_vblank_event(dev, e, seq, now);
}
EXPORT_SYMBOL(drm_crtc_send_vblank_event);

static int __enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);

		if (drm_WARN_ON(dev, !crtc))
			return 0;

		if (crtc->funcs->enable_vblank)
			return crtc->funcs->enable_vblank(crtc);
	}
#ifdef CONFIG_DRM_LEGACY
	else if (dev->driver->enable_vblank) {
		return dev->driver->enable_vblank(dev, pipe);
	}
#endif

	return -EINVAL;
}

static int drm_vblank_enable(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	int ret = 0;

	assert_spin_locked(&dev->vbl_lock);

	spin_lock(&dev->vblank_time_lock);

	if (!vblank->enabled) {
		 
		ret = __enable_vblank(dev, pipe);
		drm_dbg_core(dev, "enabling vblank on crtc %u, ret: %d\n",
			     pipe, ret);
		if (ret) {
			atomic_dec(&vblank->refcount);
		} else {
			drm_update_vblank_count(dev, pipe, 0);
			 
			WRITE_ONCE(vblank->enabled, true);
		}
	}

	spin_unlock(&dev->vblank_time_lock);

	return ret;
}

int drm_vblank_get(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	unsigned long irqflags;
	int ret = 0;

	if (!drm_dev_has_vblank(dev))
		return -EINVAL;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return -EINVAL;

	spin_lock_irqsave(&dev->vbl_lock, irqflags);
	 
	if (atomic_add_return(1, &vblank->refcount) == 1) {
		ret = drm_vblank_enable(dev, pipe);
	} else {
		if (!vblank->enabled) {
			atomic_dec(&vblank->refcount);
			ret = -EINVAL;
		}
	}
	spin_unlock_irqrestore(&dev->vbl_lock, irqflags);

	return ret;
}

 
int drm_crtc_vblank_get(struct drm_crtc *crtc)
{
	return drm_vblank_get(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_vblank_get);

void drm_vblank_put(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	if (drm_WARN_ON(dev, atomic_read(&vblank->refcount) == 0))
		return;

	 
	if (atomic_dec_and_test(&vblank->refcount)) {
		if (drm_vblank_offdelay == 0)
			return;
		else if (drm_vblank_offdelay < 0)
			vblank_disable_fn(&vblank->disable_timer);
		else if (!dev->vblank_disable_immediate)
			mod_timer(&vblank->disable_timer,
				  jiffies + ((drm_vblank_offdelay * HZ)/1000));
	}
}

 
void drm_crtc_vblank_put(struct drm_crtc *crtc)
{
	drm_vblank_put(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_vblank_put);

 
void drm_wait_one_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	int ret;
	u64 last;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	ret = drm_vblank_get(dev, pipe);
	if (drm_WARN(dev, ret, "vblank not available on crtc %i, ret=%i\n",
		     pipe, ret))
		return;

	last = drm_vblank_count(dev, pipe);

	ret = wait_event_timeout(vblank->queue,
				 last != drm_vblank_count(dev, pipe),
				 msecs_to_jiffies(100));

	drm_WARN(dev, ret == 0, "vblank wait timed out on crtc %i\n", pipe);

	drm_vblank_put(dev, pipe);
}
EXPORT_SYMBOL(drm_wait_one_vblank);

 
void drm_crtc_wait_one_vblank(struct drm_crtc *crtc)
{
	drm_wait_one_vblank(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_wait_one_vblank);

 
void drm_crtc_vblank_off(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	struct drm_pending_vblank_event *e, *t;
	ktime_t now;
	u64 seq;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	 
	spin_lock_irq(&dev->event_lock);

	spin_lock(&dev->vbl_lock);
	drm_dbg_vbl(dev, "crtc %d, vblank enabled %d, inmodeset %d\n",
		    pipe, vblank->enabled, vblank->inmodeset);

	 
	if (drm_core_check_feature(dev, DRIVER_ATOMIC) || !vblank->inmodeset)
		drm_vblank_disable_and_save(dev, pipe);

	wake_up(&vblank->queue);

	 
	if (!vblank->inmodeset) {
		atomic_inc(&vblank->refcount);
		vblank->inmodeset = 1;
	}
	spin_unlock(&dev->vbl_lock);

	 
	seq = drm_vblank_count_and_time(dev, pipe, &now);

	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != pipe)
			continue;
		drm_dbg_core(dev, "Sending premature vblank event on disable: "
			     "wanted %llu, current %llu\n",
			     e->sequence, seq);
		list_del(&e->base.link);
		drm_vblank_put(dev, pipe);
		send_vblank_event(dev, e, seq, now);
	}

	 
	drm_vblank_cancel_pending_works(vblank);

	spin_unlock_irq(&dev->event_lock);

	 
	vblank->hwmode.crtc_clock = 0;

	 
	drm_vblank_flush_worker(vblank);
}
EXPORT_SYMBOL(drm_crtc_vblank_off);

 
void drm_crtc_vblank_reset(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	spin_lock_irq(&dev->vbl_lock);
	 
	if (!vblank->inmodeset) {
		atomic_inc(&vblank->refcount);
		vblank->inmodeset = 1;
	}
	spin_unlock_irq(&dev->vbl_lock);

	drm_WARN_ON(dev, !list_empty(&dev->vblank_event_list));
	drm_WARN_ON(dev, !list_empty(&vblank->pending_work));
}
EXPORT_SYMBOL(drm_crtc_vblank_reset);

 
void drm_crtc_set_max_vblank_count(struct drm_crtc *crtc,
				   u32 max_vblank_count)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	drm_WARN_ON(dev, dev->max_vblank_count);
	drm_WARN_ON(dev, !READ_ONCE(vblank->inmodeset));

	vblank->max_vblank_count = max_vblank_count;
}
EXPORT_SYMBOL(drm_crtc_set_max_vblank_count);

 
void drm_crtc_vblank_on(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	spin_lock_irq(&dev->vbl_lock);
	drm_dbg_vbl(dev, "crtc %d, vblank enabled %d, inmodeset %d\n",
		    pipe, vblank->enabled, vblank->inmodeset);

	 
	if (vblank->inmodeset) {
		atomic_dec(&vblank->refcount);
		vblank->inmodeset = 0;
	}

	drm_reset_vblank_timestamp(dev, pipe);

	 
	if (atomic_read(&vblank->refcount) != 0 || drm_vblank_offdelay == 0)
		drm_WARN_ON(dev, drm_vblank_enable(dev, pipe));
	spin_unlock_irq(&dev->vbl_lock);
}
EXPORT_SYMBOL(drm_crtc_vblank_on);

static void drm_vblank_restore(struct drm_device *dev, unsigned int pipe)
{
	ktime_t t_vblank;
	struct drm_vblank_crtc *vblank;
	int framedur_ns;
	u64 diff_ns;
	u32 cur_vblank, diff = 1;
	int count = DRM_TIMESTAMP_MAXRETRIES;
	u32 max_vblank_count = drm_max_vblank_count(dev, pipe);

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	assert_spin_locked(&dev->vbl_lock);
	assert_spin_locked(&dev->vblank_time_lock);

	vblank = &dev->vblank[pipe];
	drm_WARN_ONCE(dev,
		      drm_debug_enabled(DRM_UT_VBL) && !vblank->framedur_ns,
		      "Cannot compute missed vblanks without frame duration\n");
	framedur_ns = vblank->framedur_ns;

	do {
		cur_vblank = __get_vblank_counter(dev, pipe);
		drm_get_last_vbltimestamp(dev, pipe, &t_vblank, false);
	} while (cur_vblank != __get_vblank_counter(dev, pipe) && --count > 0);

	diff_ns = ktime_to_ns(ktime_sub(t_vblank, vblank->time));
	if (framedur_ns)
		diff = DIV_ROUND_CLOSEST_ULL(diff_ns, framedur_ns);


	drm_dbg_vbl(dev,
		    "missed %d vblanks in %lld ns, frame duration=%d ns, hw_diff=%d\n",
		    diff, diff_ns, framedur_ns, cur_vblank - vblank->last);
	vblank->last = (cur_vblank - diff) & max_vblank_count;
}

 
void drm_crtc_vblank_restore(struct drm_crtc *crtc)
{
	WARN_ON_ONCE(!crtc->funcs->get_vblank_timestamp);
	WARN_ON_ONCE(!crtc->dev->vblank_disable_immediate);

	drm_vblank_restore(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_vblank_restore);

static void drm_legacy_vblank_pre_modeset(struct drm_device *dev,
					  unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	 
	if (!drm_dev_has_vblank(dev))
		return;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	 
	if (!vblank->inmodeset) {
		vblank->inmodeset = 0x1;
		if (drm_vblank_get(dev, pipe) == 0)
			vblank->inmodeset |= 0x2;
	}
}

static void drm_legacy_vblank_post_modeset(struct drm_device *dev,
					   unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];

	 
	if (!drm_dev_has_vblank(dev))
		return;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return;

	if (vblank->inmodeset) {
		spin_lock_irq(&dev->vbl_lock);
		drm_reset_vblank_timestamp(dev, pipe);
		spin_unlock_irq(&dev->vbl_lock);

		if (vblank->inmodeset & 0x2)
			drm_vblank_put(dev, pipe);

		vblank->inmodeset = 0;
	}
}

int drm_legacy_modeset_ctl_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	unsigned int pipe;

	 
	if (!drm_dev_has_vblank(dev))
		return 0;

	 
	if (!drm_core_check_feature(dev, DRIVER_LEGACY))
		return 0;

	pipe = modeset->crtc;
	if (pipe >= dev->num_crtcs)
		return -EINVAL;

	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		drm_legacy_vblank_pre_modeset(dev, pipe);
		break;
	case _DRM_POST_MODESET:
		drm_legacy_vblank_post_modeset(dev, pipe);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int drm_queue_vblank_event(struct drm_device *dev, unsigned int pipe,
				  u64 req_seq,
				  union drm_wait_vblank *vblwait,
				  struct drm_file *file_priv)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	struct drm_pending_vblank_event *e;
	ktime_t now;
	u64 seq;
	int ret;

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (e == NULL) {
		ret = -ENOMEM;
		goto err_put;
	}

	e->pipe = pipe;
	e->event.base.type = DRM_EVENT_VBLANK;
	e->event.base.length = sizeof(e->event.vbl);
	e->event.vbl.user_data = vblwait->request.signal;
	e->event.vbl.crtc_id = 0;
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);

		if (crtc)
			e->event.vbl.crtc_id = crtc->base.id;
	}

	spin_lock_irq(&dev->event_lock);

	 
	if (!READ_ONCE(vblank->enabled)) {
		ret = -EINVAL;
		goto err_unlock;
	}

	ret = drm_event_reserve_init_locked(dev, file_priv, &e->base,
					    &e->event.base);

	if (ret)
		goto err_unlock;

	seq = drm_vblank_count_and_time(dev, pipe, &now);

	drm_dbg_core(dev, "event on vblank count %llu, current %llu, crtc %u\n",
		     req_seq, seq, pipe);

	trace_drm_vblank_event_queued(file_priv, pipe, req_seq);

	e->sequence = req_seq;
	if (drm_vblank_passed(seq, req_seq)) {
		drm_vblank_put(dev, pipe);
		send_vblank_event(dev, e, seq, now);
		vblwait->reply.sequence = seq;
	} else {
		 
		list_add_tail(&e->base.link, &dev->vblank_event_list);
		vblwait->reply.sequence = req_seq;
	}

	spin_unlock_irq(&dev->event_lock);

	return 0;

err_unlock:
	spin_unlock_irq(&dev->event_lock);
	kfree(e);
err_put:
	drm_vblank_put(dev, pipe);
	return ret;
}

static bool drm_wait_vblank_is_query(union drm_wait_vblank *vblwait)
{
	if (vblwait->request.sequence)
		return false;

	return _DRM_VBLANK_RELATIVE ==
		(vblwait->request.type & (_DRM_VBLANK_TYPES_MASK |
					  _DRM_VBLANK_EVENT |
					  _DRM_VBLANK_NEXTONMISS));
}

 

static u64 widen_32_to_64(u32 narrow, u64 near)
{
	return near + (s32) (narrow - near);
}

static void drm_wait_vblank_reply(struct drm_device *dev, unsigned int pipe,
				  struct drm_wait_vblank_reply *reply)
{
	ktime_t now;
	struct timespec64 ts;

	 
	reply->sequence = drm_vblank_count_and_time(dev, pipe, &now);
	ts = ktime_to_timespec64(now);
	reply->tval_sec = (u32)ts.tv_sec;
	reply->tval_usec = ts.tv_nsec / 1000;
}

static bool drm_wait_vblank_supported(struct drm_device *dev)
{
#if IS_ENABLED(CONFIG_DRM_LEGACY)
	if (unlikely(drm_core_check_feature(dev, DRIVER_LEGACY)))
		return dev->irq_enabled;
#endif
	return drm_dev_has_vblank(dev);
}

int drm_wait_vblank_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct drm_vblank_crtc *vblank;
	union drm_wait_vblank *vblwait = data;
	int ret;
	u64 req_seq, seq;
	unsigned int pipe_index;
	unsigned int flags, pipe, high_pipe;

	if (!drm_wait_vblank_supported(dev))
		return -EOPNOTSUPP;

	if (vblwait->request.type & _DRM_VBLANK_SIGNAL)
		return -EINVAL;

	if (vblwait->request.type &
	    ~(_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK |
	      _DRM_VBLANK_HIGH_CRTC_MASK)) {
		drm_dbg_core(dev,
			     "Unsupported type value 0x%x, supported mask 0x%x\n",
			     vblwait->request.type,
			     (_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK |
			      _DRM_VBLANK_HIGH_CRTC_MASK));
		return -EINVAL;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	high_pipe = (vblwait->request.type & _DRM_VBLANK_HIGH_CRTC_MASK);
	if (high_pipe)
		pipe_index = high_pipe >> _DRM_VBLANK_HIGH_CRTC_SHIFT;
	else
		pipe_index = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;

	 
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		pipe = 0;
		drm_for_each_crtc(crtc, dev) {
			if (drm_lease_held(file_priv, crtc->base.id)) {
				if (pipe_index == 0)
					break;
				pipe_index--;
			}
			pipe++;
		}
	} else {
		pipe = pipe_index;
	}

	if (pipe >= dev->num_crtcs)
		return -EINVAL;

	vblank = &dev->vblank[pipe];

	 
	if (dev->vblank_disable_immediate &&
	    drm_wait_vblank_is_query(vblwait) &&
	    READ_ONCE(vblank->enabled)) {
		drm_wait_vblank_reply(dev, pipe, &vblwait->reply);
		return 0;
	}

	ret = drm_vblank_get(dev, pipe);
	if (ret) {
		drm_dbg_core(dev,
			     "crtc %d failed to acquire vblank counter, %d\n",
			     pipe, ret);
		return ret;
	}
	seq = drm_vblank_count(dev, pipe);

	switch (vblwait->request.type & _DRM_VBLANK_TYPES_MASK) {
	case _DRM_VBLANK_RELATIVE:
		req_seq = seq + vblwait->request.sequence;
		vblwait->request.sequence = req_seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
		break;
	case _DRM_VBLANK_ABSOLUTE:
		req_seq = widen_32_to_64(vblwait->request.sequence, seq);
		break;
	default:
		ret = -EINVAL;
		goto done;
	}

	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    drm_vblank_passed(seq, req_seq)) {
		req_seq = seq + 1;
		vblwait->request.type &= ~_DRM_VBLANK_NEXTONMISS;
		vblwait->request.sequence = req_seq;
	}

	if (flags & _DRM_VBLANK_EVENT) {
		 
		return drm_queue_vblank_event(dev, pipe, req_seq, vblwait, file_priv);
	}

	if (req_seq != seq) {
		int wait;

		drm_dbg_core(dev, "waiting on vblank count %llu, crtc %u\n",
			     req_seq, pipe);
		wait = wait_event_interruptible_timeout(vblank->queue,
			drm_vblank_passed(drm_vblank_count(dev, pipe), req_seq) ||
				      !READ_ONCE(vblank->enabled),
			msecs_to_jiffies(3000));

		switch (wait) {
		case 0:
			 
			ret = -EBUSY;
			break;
		case -ERESTARTSYS:
			 
			ret = -EINTR;
			break;
		default:
			ret = 0;
			break;
		}
	}

	if (ret != -EINTR) {
		drm_wait_vblank_reply(dev, pipe, &vblwait->reply);

		drm_dbg_core(dev, "crtc %d returning %u to client\n",
			     pipe, vblwait->reply.sequence);
	} else {
		drm_dbg_core(dev, "crtc %d vblank wait interrupted by signal\n",
			     pipe);
	}

done:
	drm_vblank_put(dev, pipe);
	return ret;
}

static void drm_handle_vblank_events(struct drm_device *dev, unsigned int pipe)
{
	struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);
	bool high_prec = false;
	struct drm_pending_vblank_event *e, *t;
	ktime_t now;
	u64 seq;

	assert_spin_locked(&dev->event_lock);

	seq = drm_vblank_count_and_time(dev, pipe, &now);

	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != pipe)
			continue;
		if (!drm_vblank_passed(seq, e->sequence))
			continue;

		drm_dbg_core(dev, "vblank event on %llu, current %llu\n",
			     e->sequence, seq);

		list_del(&e->base.link);
		drm_vblank_put(dev, pipe);
		send_vblank_event(dev, e, seq, now);
	}

	if (crtc && crtc->funcs->get_vblank_timestamp)
		high_prec = true;

	trace_drm_vblank_event(pipe, seq, now, high_prec);
}

 
bool drm_handle_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct drm_vblank_crtc *vblank = &dev->vblank[pipe];
	unsigned long irqflags;
	bool disable_irq;

	if (drm_WARN_ON_ONCE(dev, !drm_dev_has_vblank(dev)))
		return false;

	if (drm_WARN_ON(dev, pipe >= dev->num_crtcs))
		return false;

	spin_lock_irqsave(&dev->event_lock, irqflags);

	 
	spin_lock(&dev->vblank_time_lock);

	 
	if (!vblank->enabled) {
		spin_unlock(&dev->vblank_time_lock);
		spin_unlock_irqrestore(&dev->event_lock, irqflags);
		return false;
	}

	drm_update_vblank_count(dev, pipe, true);

	spin_unlock(&dev->vblank_time_lock);

	wake_up(&vblank->queue);

	 
	disable_irq = (dev->vblank_disable_immediate &&
		       drm_vblank_offdelay > 0 &&
		       !atomic_read(&vblank->refcount));

	drm_handle_vblank_events(dev, pipe);
	drm_handle_vblank_works(vblank);

	spin_unlock_irqrestore(&dev->event_lock, irqflags);

	if (disable_irq)
		vblank_disable_fn(&vblank->disable_timer);

	return true;
}
EXPORT_SYMBOL(drm_handle_vblank);

 
bool drm_crtc_handle_vblank(struct drm_crtc *crtc)
{
	return drm_handle_vblank(crtc->dev, drm_crtc_index(crtc));
}
EXPORT_SYMBOL(drm_crtc_handle_vblank);

 

int drm_crtc_get_sequence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct drm_vblank_crtc *vblank;
	int pipe;
	struct drm_crtc_get_sequence *get_seq = data;
	ktime_t now;
	bool vblank_enabled;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	if (!drm_dev_has_vblank(dev))
		return -EOPNOTSUPP;

	crtc = drm_crtc_find(dev, file_priv, get_seq->crtc_id);
	if (!crtc)
		return -ENOENT;

	pipe = drm_crtc_index(crtc);

	vblank = &dev->vblank[pipe];
	vblank_enabled = dev->vblank_disable_immediate && READ_ONCE(vblank->enabled);

	if (!vblank_enabled) {
		ret = drm_crtc_vblank_get(crtc);
		if (ret) {
			drm_dbg_core(dev,
				     "crtc %d failed to acquire vblank counter, %d\n",
				     pipe, ret);
			return ret;
		}
	}
	drm_modeset_lock(&crtc->mutex, NULL);
	if (crtc->state)
		get_seq->active = crtc->state->enable;
	else
		get_seq->active = crtc->enabled;
	drm_modeset_unlock(&crtc->mutex);
	get_seq->sequence = drm_vblank_count_and_time(dev, pipe, &now);
	get_seq->sequence_ns = ktime_to_ns(now);
	if (!vblank_enabled)
		drm_crtc_vblank_put(crtc);
	return 0;
}

 

int drm_crtc_queue_sequence_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct drm_vblank_crtc *vblank;
	int pipe;
	struct drm_crtc_queue_sequence *queue_seq = data;
	ktime_t now;
	struct drm_pending_vblank_event *e;
	u32 flags;
	u64 seq;
	u64 req_seq;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	if (!drm_dev_has_vblank(dev))
		return -EOPNOTSUPP;

	crtc = drm_crtc_find(dev, file_priv, queue_seq->crtc_id);
	if (!crtc)
		return -ENOENT;

	flags = queue_seq->flags;
	 
	if (flags & ~(DRM_CRTC_SEQUENCE_RELATIVE|
		      DRM_CRTC_SEQUENCE_NEXT_ON_MISS))
		return -EINVAL;

	pipe = drm_crtc_index(crtc);

	vblank = &dev->vblank[pipe];

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (e == NULL)
		return -ENOMEM;

	ret = drm_crtc_vblank_get(crtc);
	if (ret) {
		drm_dbg_core(dev,
			     "crtc %d failed to acquire vblank counter, %d\n",
			     pipe, ret);
		goto err_free;
	}

	seq = drm_vblank_count_and_time(dev, pipe, &now);
	req_seq = queue_seq->sequence;

	if (flags & DRM_CRTC_SEQUENCE_RELATIVE)
		req_seq += seq;

	if ((flags & DRM_CRTC_SEQUENCE_NEXT_ON_MISS) && drm_vblank_passed(seq, req_seq))
		req_seq = seq + 1;

	e->pipe = pipe;
	e->event.base.type = DRM_EVENT_CRTC_SEQUENCE;
	e->event.base.length = sizeof(e->event.seq);
	e->event.seq.user_data = queue_seq->user_data;

	spin_lock_irq(&dev->event_lock);

	 
	if (!READ_ONCE(vblank->enabled)) {
		ret = -EINVAL;
		goto err_unlock;
	}

	ret = drm_event_reserve_init_locked(dev, file_priv, &e->base,
					    &e->event.base);

	if (ret)
		goto err_unlock;

	e->sequence = req_seq;

	if (drm_vblank_passed(seq, req_seq)) {
		drm_crtc_vblank_put(crtc);
		send_vblank_event(dev, e, seq, now);
		queue_seq->sequence = seq;
	} else {
		 
		list_add_tail(&e->base.link, &dev->vblank_event_list);
		queue_seq->sequence = req_seq;
	}

	spin_unlock_irq(&dev->event_lock);
	return 0;

err_unlock:
	spin_unlock_irq(&dev->event_lock);
	drm_crtc_vblank_put(crtc);
err_free:
	kfree(e);
	return ret;
}

