

#include <drm/drm_exec.h>
#include <drm/drm_gem.h>
#include <linux/dma-resv.h>

 

 
#define DRM_EXEC_DUMMY ((void *)~0)

 
static void drm_exec_unlock_all(struct drm_exec *exec)
{
	struct drm_gem_object *obj;
	unsigned long index;

	drm_exec_for_each_locked_object_reverse(exec, index, obj) {
		dma_resv_unlock(obj->resv);
		drm_gem_object_put(obj);
	}

	drm_gem_object_put(exec->prelocked);
	exec->prelocked = NULL;
}

 
void drm_exec_init(struct drm_exec *exec, uint32_t flags)
{
	exec->flags = flags;
	exec->objects = kmalloc(PAGE_SIZE, GFP_KERNEL);

	 
	exec->max_objects = exec->objects ? PAGE_SIZE / sizeof(void *) : 0;
	exec->num_objects = 0;
	exec->contended = DRM_EXEC_DUMMY;
	exec->prelocked = NULL;
}
EXPORT_SYMBOL(drm_exec_init);

 
void drm_exec_fini(struct drm_exec *exec)
{
	drm_exec_unlock_all(exec);
	kvfree(exec->objects);
	if (exec->contended != DRM_EXEC_DUMMY) {
		drm_gem_object_put(exec->contended);
		ww_acquire_fini(&exec->ticket);
	}
}
EXPORT_SYMBOL(drm_exec_fini);

 
bool drm_exec_cleanup(struct drm_exec *exec)
{
	if (likely(!exec->contended)) {
		ww_acquire_done(&exec->ticket);
		return false;
	}

	if (likely(exec->contended == DRM_EXEC_DUMMY)) {
		exec->contended = NULL;
		ww_acquire_init(&exec->ticket, &reservation_ww_class);
		return true;
	}

	drm_exec_unlock_all(exec);
	exec->num_objects = 0;
	return true;
}
EXPORT_SYMBOL(drm_exec_cleanup);

 
static int drm_exec_obj_locked(struct drm_exec *exec,
			       struct drm_gem_object *obj)
{
	if (unlikely(exec->num_objects == exec->max_objects)) {
		size_t size = exec->max_objects * sizeof(void *);
		void *tmp;

		tmp = kvrealloc(exec->objects, size, size + PAGE_SIZE,
				GFP_KERNEL);
		if (!tmp)
			return -ENOMEM;

		exec->objects = tmp;
		exec->max_objects += PAGE_SIZE / sizeof(void *);
	}
	drm_gem_object_get(obj);
	exec->objects[exec->num_objects++] = obj;

	return 0;
}

 
static int drm_exec_lock_contended(struct drm_exec *exec)
{
	struct drm_gem_object *obj = exec->contended;
	int ret;

	if (likely(!obj))
		return 0;

	 
	exec->contended = NULL;
	if (exec->flags & DRM_EXEC_INTERRUPTIBLE_WAIT) {
		ret = dma_resv_lock_slow_interruptible(obj->resv,
						       &exec->ticket);
		if (unlikely(ret))
			goto error_dropref;
	} else {
		dma_resv_lock_slow(obj->resv, &exec->ticket);
	}

	ret = drm_exec_obj_locked(exec, obj);
	if (unlikely(ret))
		goto error_unlock;

	exec->prelocked = obj;
	return 0;

error_unlock:
	dma_resv_unlock(obj->resv);

error_dropref:
	drm_gem_object_put(obj);
	return ret;
}

 
int drm_exec_lock_obj(struct drm_exec *exec, struct drm_gem_object *obj)
{
	int ret;

	ret = drm_exec_lock_contended(exec);
	if (unlikely(ret))
		return ret;

	if (exec->prelocked == obj) {
		drm_gem_object_put(exec->prelocked);
		exec->prelocked = NULL;
		return 0;
	}

	if (exec->flags & DRM_EXEC_INTERRUPTIBLE_WAIT)
		ret = dma_resv_lock_interruptible(obj->resv, &exec->ticket);
	else
		ret = dma_resv_lock(obj->resv, &exec->ticket);

	if (unlikely(ret == -EDEADLK)) {
		drm_gem_object_get(obj);
		exec->contended = obj;
		return -EDEADLK;
	}

	if (unlikely(ret == -EALREADY) &&
	    exec->flags & DRM_EXEC_IGNORE_DUPLICATES)
		return 0;

	if (unlikely(ret))
		return ret;

	ret = drm_exec_obj_locked(exec, obj);
	if (ret)
		goto error_unlock;

	return 0;

error_unlock:
	dma_resv_unlock(obj->resv);
	return ret;
}
EXPORT_SYMBOL(drm_exec_lock_obj);

 
void drm_exec_unlock_obj(struct drm_exec *exec, struct drm_gem_object *obj)
{
	unsigned int i;

	for (i = exec->num_objects; i--;) {
		if (exec->objects[i] == obj) {
			dma_resv_unlock(obj->resv);
			for (++i; i < exec->num_objects; ++i)
				exec->objects[i - 1] = exec->objects[i];
			--exec->num_objects;
			drm_gem_object_put(obj);
			return;
		}

	}
}
EXPORT_SYMBOL(drm_exec_unlock_obj);

 
int drm_exec_prepare_obj(struct drm_exec *exec, struct drm_gem_object *obj,
			 unsigned int num_fences)
{
	int ret;

	ret = drm_exec_lock_obj(exec, obj);
	if (ret)
		return ret;

	ret = dma_resv_reserve_fences(obj->resv, num_fences);
	if (ret) {
		drm_exec_unlock_obj(exec, obj);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_exec_prepare_obj);

 
int drm_exec_prepare_array(struct drm_exec *exec,
			   struct drm_gem_object **objects,
			   unsigned int num_objects,
			   unsigned int num_fences)
{
	int ret;

	for (unsigned int i = 0; i < num_objects; ++i) {
		ret = drm_exec_prepare_obj(exec, objects[i], num_fences);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_exec_prepare_array);

MODULE_DESCRIPTION("DRM execution context");
MODULE_LICENSE("Dual MIT/GPL");
