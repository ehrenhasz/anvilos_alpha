 

#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_print.h>

 

static DEFINE_WW_CLASS(crtc_ww_class);

#if IS_ENABLED(CONFIG_DRM_DEBUG_MODESET_LOCK)
static noinline depot_stack_handle_t __drm_stack_depot_save(void)
{
	unsigned long entries[8];
	unsigned int n;

	n = stack_trace_save(entries, ARRAY_SIZE(entries), 1);

	return stack_depot_save(entries, n, GFP_NOWAIT | __GFP_NOWARN);
}

static void __drm_stack_depot_print(depot_stack_handle_t stack_depot)
{
	struct drm_printer p = drm_debug_printer("drm_modeset_lock");
	unsigned long *entries;
	unsigned int nr_entries;
	char *buf;

	buf = kmalloc(PAGE_SIZE, GFP_NOWAIT | __GFP_NOWARN);
	if (!buf)
		return;

	nr_entries = stack_depot_fetch(stack_depot, &entries);
	stack_trace_snprint(buf, PAGE_SIZE, entries, nr_entries, 2);

	drm_printf(&p, "attempting to lock a contended lock without backoff:\n%s", buf);

	kfree(buf);
}

static void __drm_stack_depot_init(void)
{
	stack_depot_init();
}
#else  
static depot_stack_handle_t __drm_stack_depot_save(void)
{
	return 0;
}
static void __drm_stack_depot_print(depot_stack_handle_t stack_depot)
{
}
static void __drm_stack_depot_init(void)
{
}
#endif  

 
void drm_modeset_lock_all(struct drm_device *dev)
{
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_modeset_acquire_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL | __GFP_NOFAIL);
	if (WARN_ON(!ctx))
		return;

	mutex_lock(&config->mutex);

	drm_modeset_acquire_init(ctx, 0);

retry:
	ret = drm_modeset_lock_all_ctx(dev, ctx);
	if (ret < 0) {
		if (ret == -EDEADLK) {
			drm_modeset_backoff(ctx);
			goto retry;
		}

		drm_modeset_acquire_fini(ctx);
		kfree(ctx);
		return;
	}
	ww_acquire_done(&ctx->ww_ctx);

	WARN_ON(config->acquire_ctx);

	 
	config->acquire_ctx = ctx;

	drm_warn_on_modeset_not_all_locked(dev);
}
EXPORT_SYMBOL(drm_modeset_lock_all);

 
void drm_modeset_unlock_all(struct drm_device *dev)
{
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_modeset_acquire_ctx *ctx = config->acquire_ctx;

	if (WARN_ON(!ctx))
		return;

	config->acquire_ctx = NULL;
	drm_modeset_drop_locks(ctx);
	drm_modeset_acquire_fini(ctx);

	kfree(ctx);

	mutex_unlock(&dev->mode_config.mutex);
}
EXPORT_SYMBOL(drm_modeset_unlock_all);

 
void drm_warn_on_modeset_not_all_locked(struct drm_device *dev)
{
	struct drm_crtc *crtc;

	 
	if (oops_in_progress)
		return;

	drm_for_each_crtc(crtc, dev)
		WARN_ON(!drm_modeset_is_locked(&crtc->mutex));

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));
	WARN_ON(!mutex_is_locked(&dev->mode_config.mutex));
}
EXPORT_SYMBOL(drm_warn_on_modeset_not_all_locked);

 
void drm_modeset_acquire_init(struct drm_modeset_acquire_ctx *ctx,
		uint32_t flags)
{
	memset(ctx, 0, sizeof(*ctx));
	ww_acquire_init(&ctx->ww_ctx, &crtc_ww_class);
	INIT_LIST_HEAD(&ctx->locked);

	if (flags & DRM_MODESET_ACQUIRE_INTERRUPTIBLE)
		ctx->interruptible = true;
}
EXPORT_SYMBOL(drm_modeset_acquire_init);

 
void drm_modeset_acquire_fini(struct drm_modeset_acquire_ctx *ctx)
{
	ww_acquire_fini(&ctx->ww_ctx);
}
EXPORT_SYMBOL(drm_modeset_acquire_fini);

 
void drm_modeset_drop_locks(struct drm_modeset_acquire_ctx *ctx)
{
	if (WARN_ON(ctx->contended))
		__drm_stack_depot_print(ctx->stack_depot);

	while (!list_empty(&ctx->locked)) {
		struct drm_modeset_lock *lock;

		lock = list_first_entry(&ctx->locked,
				struct drm_modeset_lock, head);

		drm_modeset_unlock(lock);
	}
}
EXPORT_SYMBOL(drm_modeset_drop_locks);

static inline int modeset_lock(struct drm_modeset_lock *lock,
		struct drm_modeset_acquire_ctx *ctx,
		bool interruptible, bool slow)
{
	int ret;

	if (WARN_ON(ctx->contended))
		__drm_stack_depot_print(ctx->stack_depot);

	if (ctx->trylock_only) {
		lockdep_assert_held(&ctx->ww_ctx);

		if (!ww_mutex_trylock(&lock->mutex, NULL))
			return -EBUSY;
		else
			return 0;
	} else if (interruptible && slow) {
		ret = ww_mutex_lock_slow_interruptible(&lock->mutex, &ctx->ww_ctx);
	} else if (interruptible) {
		ret = ww_mutex_lock_interruptible(&lock->mutex, &ctx->ww_ctx);
	} else if (slow) {
		ww_mutex_lock_slow(&lock->mutex, &ctx->ww_ctx);
		ret = 0;
	} else {
		ret = ww_mutex_lock(&lock->mutex, &ctx->ww_ctx);
	}
	if (!ret) {
		WARN_ON(!list_empty(&lock->head));
		list_add(&lock->head, &ctx->locked);
	} else if (ret == -EALREADY) {
		 
		ret = 0;
	} else if (ret == -EDEADLK) {
		ctx->contended = lock;
		ctx->stack_depot = __drm_stack_depot_save();
	}

	return ret;
}

 
int drm_modeset_backoff(struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_modeset_lock *contended = ctx->contended;

	ctx->contended = NULL;
	ctx->stack_depot = 0;

	if (WARN_ON(!contended))
		return 0;

	drm_modeset_drop_locks(ctx);

	return modeset_lock(contended, ctx, ctx->interruptible, true);
}
EXPORT_SYMBOL(drm_modeset_backoff);

 
void drm_modeset_lock_init(struct drm_modeset_lock *lock)
{
	ww_mutex_init(&lock->mutex, &crtc_ww_class);
	INIT_LIST_HEAD(&lock->head);
	__drm_stack_depot_init();
}
EXPORT_SYMBOL(drm_modeset_lock_init);

 
int drm_modeset_lock(struct drm_modeset_lock *lock,
		struct drm_modeset_acquire_ctx *ctx)
{
	if (ctx)
		return modeset_lock(lock, ctx, ctx->interruptible, false);

	ww_mutex_lock(&lock->mutex, NULL);
	return 0;
}
EXPORT_SYMBOL(drm_modeset_lock);

 
int drm_modeset_lock_single_interruptible(struct drm_modeset_lock *lock)
{
	return ww_mutex_lock_interruptible(&lock->mutex, NULL);
}
EXPORT_SYMBOL(drm_modeset_lock_single_interruptible);

 
void drm_modeset_unlock(struct drm_modeset_lock *lock)
{
	list_del_init(&lock->head);
	ww_mutex_unlock(&lock->mutex);
}
EXPORT_SYMBOL(drm_modeset_unlock);

 
int drm_modeset_lock_all_ctx(struct drm_device *dev,
			     struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_private_obj *privobj;
	struct drm_crtc *crtc;
	struct drm_plane *plane;
	int ret;

	ret = drm_modeset_lock(&dev->mode_config.connection_mutex, ctx);
	if (ret)
		return ret;

	drm_for_each_crtc(crtc, dev) {
		ret = drm_modeset_lock(&crtc->mutex, ctx);
		if (ret)
			return ret;
	}

	drm_for_each_plane(plane, dev) {
		ret = drm_modeset_lock(&plane->mutex, ctx);
		if (ret)
			return ret;
	}

	drm_for_each_privobj(privobj, dev) {
		ret = drm_modeset_lock(&privobj->lock, ctx);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_modeset_lock_all_ctx);
