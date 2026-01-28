#ifndef __AMDGPU_BO_LIST_H__
#define __AMDGPU_BO_LIST_H__
#include <drm/amdgpu_drm.h>
struct hmm_range;
struct drm_file;
struct amdgpu_device;
struct amdgpu_bo;
struct amdgpu_bo_va;
struct amdgpu_fpriv;
struct amdgpu_bo_list_entry {
	struct amdgpu_bo		*bo;
	struct amdgpu_bo_va		*bo_va;
	uint32_t			priority;
	struct page			**user_pages;
	struct hmm_range		*range;
	bool				user_invalidated;
};
struct amdgpu_bo_list {
	struct rcu_head rhead;
	struct kref refcount;
	struct amdgpu_bo *gds_obj;
	struct amdgpu_bo *gws_obj;
	struct amdgpu_bo *oa_obj;
	unsigned first_userptr;
	unsigned num_entries;
	struct mutex bo_list_mutex;
};
int amdgpu_bo_list_get(struct amdgpu_fpriv *fpriv, int id,
		       struct amdgpu_bo_list **result);
void amdgpu_bo_list_put(struct amdgpu_bo_list *list);
int amdgpu_bo_create_list_entry_array(struct drm_amdgpu_bo_list_in *in,
				      struct drm_amdgpu_bo_list_entry **info_param);
int amdgpu_bo_list_create(struct amdgpu_device *adev,
				 struct drm_file *filp,
				 struct drm_amdgpu_bo_list_entry *info,
				 size_t num_entries,
				 struct amdgpu_bo_list **list);
static inline struct amdgpu_bo_list_entry *
amdgpu_bo_list_array_entry(struct amdgpu_bo_list *list, unsigned index)
{
	struct amdgpu_bo_list_entry *array = (void *)&list[1];
	return &array[index];
}
#define amdgpu_bo_list_for_each_entry(e, list) \
	for (e = amdgpu_bo_list_array_entry(list, 0); \
	     e != amdgpu_bo_list_array_entry(list, (list)->num_entries); \
	     ++e)
#define amdgpu_bo_list_for_each_userptr_entry(e, list) \
	for (e = amdgpu_bo_list_array_entry(list, (list)->first_userptr); \
	     e != amdgpu_bo_list_array_entry(list, (list)->num_entries); \
	     ++e)
#endif
