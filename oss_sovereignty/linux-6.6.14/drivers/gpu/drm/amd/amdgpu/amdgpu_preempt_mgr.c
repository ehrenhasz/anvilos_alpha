
 

#include "amdgpu.h"

 
static ssize_t mem_info_preempt_used_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct ttm_resource_manager *man = &adev->mman.preempt_mgr;

	return sysfs_emit(buf, "%llu\n", ttm_resource_manager_usage(man));
}

static DEVICE_ATTR_RO(mem_info_preempt_used);

 
static int amdgpu_preempt_mgr_new(struct ttm_resource_manager *man,
				  struct ttm_buffer_object *tbo,
				  const struct ttm_place *place,
				  struct ttm_resource **res)
{
	*res = kzalloc(sizeof(**res), GFP_KERNEL);
	if (!*res)
		return -ENOMEM;

	ttm_resource_init(tbo, place, *res);
	(*res)->start = AMDGPU_BO_INVALID_OFFSET;
	return 0;
}

 
static void amdgpu_preempt_mgr_del(struct ttm_resource_manager *man,
				   struct ttm_resource *res)
{
	ttm_resource_fini(man, res);
	kfree(res);
}

static const struct ttm_resource_manager_func amdgpu_preempt_mgr_func = {
	.alloc = amdgpu_preempt_mgr_new,
	.free = amdgpu_preempt_mgr_del,
};

 
int amdgpu_preempt_mgr_init(struct amdgpu_device *adev)
{
	struct ttm_resource_manager *man = &adev->mman.preempt_mgr;
	int ret;

	man->use_tt = true;
	man->func = &amdgpu_preempt_mgr_func;

	ttm_resource_manager_init(man, &adev->mman.bdev, (1 << 30));

	ret = device_create_file(adev->dev, &dev_attr_mem_info_preempt_used);
	if (ret) {
		DRM_ERROR("Failed to create device file mem_info_preempt_used\n");
		return ret;
	}

	ttm_set_driver_manager(&adev->mman.bdev, AMDGPU_PL_PREEMPT, man);
	ttm_resource_manager_set_used(man, true);
	return 0;
}

 
void amdgpu_preempt_mgr_fini(struct amdgpu_device *adev)
{
	struct ttm_resource_manager *man = &adev->mman.preempt_mgr;
	int ret;

	ttm_resource_manager_set_used(man, false);

	ret = ttm_resource_manager_evict_all(&adev->mman.bdev, man);
	if (ret)
		return;

	device_remove_file(adev->dev, &dev_attr_mem_info_preempt_used);

	ttm_resource_manager_cleanup(man);
	ttm_set_driver_manager(&adev->mman.bdev, AMDGPU_PL_PREEMPT, NULL);
}
