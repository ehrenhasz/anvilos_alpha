 
 
 

#ifndef _TTM_OBJECT_H_
#define _TTM_OBJECT_H_

#include <linux/dma-buf.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/rcupdate.h>

#include <drm/ttm/ttm_bo.h>

 

enum ttm_object_type {
	ttm_fence_type,
	ttm_lock_type,
	ttm_prime_type,
	ttm_driver_type0 = 256,
	ttm_driver_type1,
	ttm_driver_type2,
	ttm_driver_type3,
	ttm_driver_type4,
	ttm_driver_type5
};

struct ttm_object_file;
struct ttm_object_device;

 

struct ttm_base_object {
	struct rcu_head rhead;
	struct ttm_object_file *tfile;
	struct kref refcount;
	void (*refcount_release) (struct ttm_base_object **base);
	u64 handle;
	enum ttm_object_type object_type;
	u32 shareable;
};


 

struct ttm_prime_object {
	struct ttm_base_object base;
	struct mutex mutex;
	size_t size;
	enum ttm_object_type real_type;
	struct dma_buf *dma_buf;
	void (*refcount_release) (struct ttm_base_object **);
};

 

extern int ttm_base_object_init(struct ttm_object_file *tfile,
				struct ttm_base_object *base,
				bool shareable,
				enum ttm_object_type type,
				void (*refcount_release) (struct ttm_base_object
							  **));

 

extern struct ttm_base_object *ttm_base_object_lookup(struct ttm_object_file
						      *tfile, uint64_t key);

 

extern struct ttm_base_object *
ttm_base_object_lookup_for_ref(struct ttm_object_device *tdev, uint64_t key);

 

extern void ttm_base_object_unref(struct ttm_base_object **p_base);

 
extern int ttm_ref_object_add(struct ttm_object_file *tfile,
			      struct ttm_base_object *base,
			      bool *existed,
			      bool require_existed);

 
extern int ttm_ref_object_base_unref(struct ttm_object_file *tfile,
				     unsigned long key);

 

extern struct ttm_object_file *ttm_object_file_init(struct ttm_object_device
						    *tdev);

 

extern void ttm_object_file_release(struct ttm_object_file **p_tfile);

 

extern struct ttm_object_device *
ttm_object_device_init(const struct dma_buf_ops *ops);

 

extern void ttm_object_device_release(struct ttm_object_device **p_tdev);

#define ttm_base_object_kfree(__object, __base)\
	kfree_rcu(__object, __base.rhead)

extern int ttm_prime_object_init(struct ttm_object_file *tfile,
				 size_t size,
				 struct ttm_prime_object *prime,
				 bool shareable,
				 enum ttm_object_type type,
				 void (*refcount_release)
				 (struct ttm_base_object **));

static inline enum ttm_object_type
ttm_base_object_type(struct ttm_base_object *base)
{
	return (base->object_type == ttm_prime_type) ?
		container_of(base, struct ttm_prime_object, base)->real_type :
		base->object_type;
}
extern int ttm_prime_fd_to_handle(struct ttm_object_file *tfile,
				  int fd, u32 *handle);
extern int ttm_prime_handle_to_fd(struct ttm_object_file *tfile,
				  uint32_t handle, uint32_t flags,
				  int *prime_fd);

#define ttm_prime_object_kfree(__obj, __prime)		\
	kfree_rcu(__obj, __prime.base.rhead)

static inline int ttm_bo_wait(struct ttm_buffer_object *bo, bool intr,
			      bool no_wait)
{
	struct ttm_operation_ctx ctx = { intr, no_wait };

	return ttm_bo_wait_ctx(bo, &ctx);
}

#endif
