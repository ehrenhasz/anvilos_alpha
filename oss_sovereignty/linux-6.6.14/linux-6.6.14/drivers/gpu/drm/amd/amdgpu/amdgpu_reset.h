#ifndef __AMDGPU_RESET_H__
#define __AMDGPU_RESET_H__
#include "amdgpu.h"
enum AMDGPU_RESET_FLAGS {
	AMDGPU_NEED_FULL_RESET = 0,
	AMDGPU_SKIP_HW_RESET = 1,
	AMDGPU_RESET_FOR_DEVICE_REMOVE = 2,
};
struct amdgpu_reset_context {
	enum amd_reset_method method;
	struct amdgpu_device *reset_req_dev;
	struct amdgpu_job *job;
	struct amdgpu_hive_info *hive;
	struct list_head *reset_device_list;
	unsigned long flags;
};
struct amdgpu_reset_handler {
	enum amd_reset_method reset_method;
	struct list_head handler_list;
	int (*prepare_env)(struct amdgpu_reset_control *reset_ctl,
			   struct amdgpu_reset_context *context);
	int (*prepare_hwcontext)(struct amdgpu_reset_control *reset_ctl,
				 struct amdgpu_reset_context *context);
	int (*perform_reset)(struct amdgpu_reset_control *reset_ctl,
			     struct amdgpu_reset_context *context);
	int (*restore_hwcontext)(struct amdgpu_reset_control *reset_ctl,
				 struct amdgpu_reset_context *context);
	int (*restore_env)(struct amdgpu_reset_control *reset_ctl,
			   struct amdgpu_reset_context *context);
	int (*do_reset)(struct amdgpu_device *adev);
};
struct amdgpu_reset_control {
	void *handle;
	struct work_struct reset_work;
	struct mutex reset_lock;
	struct list_head reset_handlers;
	atomic_t in_reset;
	enum amd_reset_method active_reset;
	struct amdgpu_reset_handler *(*get_reset_handler)(
		struct amdgpu_reset_control *reset_ctl,
		struct amdgpu_reset_context *context);
	void (*async_reset)(struct work_struct *work);
};
enum amdgpu_reset_domain_type {
	SINGLE_DEVICE,
	XGMI_HIVE
};
struct amdgpu_reset_domain {
	struct kref refcount;
	struct workqueue_struct *wq;
	enum amdgpu_reset_domain_type type;
	struct rw_semaphore sem;
	atomic_t in_gpu_reset;
	atomic_t reset_res;
};
int amdgpu_reset_init(struct amdgpu_device *adev);
int amdgpu_reset_fini(struct amdgpu_device *adev);
int amdgpu_reset_prepare_hwcontext(struct amdgpu_device *adev,
				   struct amdgpu_reset_context *reset_context);
int amdgpu_reset_perform_reset(struct amdgpu_device *adev,
			       struct amdgpu_reset_context *reset_context);
int amdgpu_reset_add_handler(struct amdgpu_reset_control *reset_ctl,
			     struct amdgpu_reset_handler *handler);
struct amdgpu_reset_domain *amdgpu_reset_create_reset_domain(enum amdgpu_reset_domain_type type,
							     char *wq_name);
void amdgpu_reset_destroy_reset_domain(struct kref *ref);
static inline bool amdgpu_reset_get_reset_domain(struct amdgpu_reset_domain *domain)
{
	return kref_get_unless_zero(&domain->refcount) != 0;
}
static inline void amdgpu_reset_put_reset_domain(struct amdgpu_reset_domain *domain)
{
	if (domain)
		kref_put(&domain->refcount, amdgpu_reset_destroy_reset_domain);
}
static inline bool amdgpu_reset_domain_schedule(struct amdgpu_reset_domain *domain,
						struct work_struct *work)
{
	return queue_work(domain->wq, work);
}
void amdgpu_device_lock_reset_domain(struct amdgpu_reset_domain *reset_domain);
void amdgpu_device_unlock_reset_domain(struct amdgpu_reset_domain *reset_domain);
#endif
