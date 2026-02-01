 
 
 
 


#define pr_fmt(fmt) "[TTM] " fmt

#include "ttm_object.h"
#include "vmwgfx_drv.h"

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/hashtable.h>

MODULE_IMPORT_NS(DMA_BUF);

#define VMW_TTM_OBJECT_REF_HT_ORDER 10

 
struct ttm_object_file {
	struct ttm_object_device *tdev;
	spinlock_t lock;
	struct list_head ref_list;
	DECLARE_HASHTABLE(ref_hash, VMW_TTM_OBJECT_REF_HT_ORDER);
	struct kref refcount;
};

 

struct ttm_object_device {
	spinlock_t object_lock;
	atomic_t object_count;
	struct dma_buf_ops ops;
	void (*dmabuf_release)(struct dma_buf *dma_buf);
	struct idr idr;
};

 

struct ttm_ref_object {
	struct rcu_head rcu_head;
	struct vmwgfx_hash_item hash;
	struct list_head head;
	struct kref kref;
	struct ttm_base_object *obj;
	struct ttm_object_file *tfile;
};

static void ttm_prime_dmabuf_release(struct dma_buf *dma_buf);

static inline struct ttm_object_file *
ttm_object_file_ref(struct ttm_object_file *tfile)
{
	kref_get(&tfile->refcount);
	return tfile;
}

static int ttm_tfile_find_ref_rcu(struct ttm_object_file *tfile,
				  uint64_t key,
				  struct vmwgfx_hash_item **p_hash)
{
	struct vmwgfx_hash_item *hash;

	hash_for_each_possible_rcu(tfile->ref_hash, hash, head, key) {
		if (hash->key == key) {
			*p_hash = hash;
			return 0;
		}
	}
	return -EINVAL;
}

static int ttm_tfile_find_ref(struct ttm_object_file *tfile,
			      uint64_t key,
			      struct vmwgfx_hash_item **p_hash)
{
	struct vmwgfx_hash_item *hash;

	hash_for_each_possible(tfile->ref_hash, hash, head, key) {
		if (hash->key == key) {
			*p_hash = hash;
			return 0;
		}
	}
	return -EINVAL;
}

static void ttm_object_file_destroy(struct kref *kref)
{
	struct ttm_object_file *tfile =
		container_of(kref, struct ttm_object_file, refcount);

	kfree(tfile);
}


static inline void ttm_object_file_unref(struct ttm_object_file **p_tfile)
{
	struct ttm_object_file *tfile = *p_tfile;

	*p_tfile = NULL;
	kref_put(&tfile->refcount, ttm_object_file_destroy);
}


int ttm_base_object_init(struct ttm_object_file *tfile,
			 struct ttm_base_object *base,
			 bool shareable,
			 enum ttm_object_type object_type,
			 void (*refcount_release) (struct ttm_base_object **))
{
	struct ttm_object_device *tdev = tfile->tdev;
	int ret;

	base->shareable = shareable;
	base->tfile = ttm_object_file_ref(tfile);
	base->refcount_release = refcount_release;
	base->object_type = object_type;
	kref_init(&base->refcount);
	idr_preload(GFP_KERNEL);
	spin_lock(&tdev->object_lock);
	ret = idr_alloc(&tdev->idr, base, 1, 0, GFP_NOWAIT);
	spin_unlock(&tdev->object_lock);
	idr_preload_end();
	if (ret < 0)
		return ret;

	base->handle = ret;
	ret = ttm_ref_object_add(tfile, base, NULL, false);
	if (unlikely(ret != 0))
		goto out_err1;

	ttm_base_object_unref(&base);

	return 0;
out_err1:
	spin_lock(&tdev->object_lock);
	idr_remove(&tdev->idr, base->handle);
	spin_unlock(&tdev->object_lock);
	return ret;
}

static void ttm_release_base(struct kref *kref)
{
	struct ttm_base_object *base =
	    container_of(kref, struct ttm_base_object, refcount);
	struct ttm_object_device *tdev = base->tfile->tdev;

	spin_lock(&tdev->object_lock);
	idr_remove(&tdev->idr, base->handle);
	spin_unlock(&tdev->object_lock);

	 

	ttm_object_file_unref(&base->tfile);
	if (base->refcount_release)
		base->refcount_release(&base);
}

void ttm_base_object_unref(struct ttm_base_object **p_base)
{
	struct ttm_base_object *base = *p_base;

	*p_base = NULL;

	kref_put(&base->refcount, ttm_release_base);
}

struct ttm_base_object *ttm_base_object_lookup(struct ttm_object_file *tfile,
					       uint64_t key)
{
	struct ttm_base_object *base = NULL;
	struct vmwgfx_hash_item *hash;
	int ret;

	spin_lock(&tfile->lock);
	ret = ttm_tfile_find_ref(tfile, key, &hash);

	if (likely(ret == 0)) {
		base = hlist_entry(hash, struct ttm_ref_object, hash)->obj;
		if (!kref_get_unless_zero(&base->refcount))
			base = NULL;
	}
	spin_unlock(&tfile->lock);


	return base;
}

struct ttm_base_object *
ttm_base_object_lookup_for_ref(struct ttm_object_device *tdev, uint64_t key)
{
	struct ttm_base_object *base;

	rcu_read_lock();
	base = idr_find(&tdev->idr, key);

	if (base && !kref_get_unless_zero(&base->refcount))
		base = NULL;
	rcu_read_unlock();

	return base;
}

int ttm_ref_object_add(struct ttm_object_file *tfile,
		       struct ttm_base_object *base,
		       bool *existed,
		       bool require_existed)
{
	struct ttm_ref_object *ref;
	struct vmwgfx_hash_item *hash;
	int ret = -EINVAL;

	if (base->tfile != tfile && !base->shareable)
		return -EPERM;

	if (existed != NULL)
		*existed = true;

	while (ret == -EINVAL) {
		rcu_read_lock();
		ret = ttm_tfile_find_ref_rcu(tfile, base->handle, &hash);

		if (ret == 0) {
			ref = hlist_entry(hash, struct ttm_ref_object, hash);
			if (kref_get_unless_zero(&ref->kref)) {
				rcu_read_unlock();
				break;
			}
		}

		rcu_read_unlock();
		if (require_existed)
			return -EPERM;

		ref = kmalloc(sizeof(*ref), GFP_KERNEL);
		if (unlikely(ref == NULL)) {
			return -ENOMEM;
		}

		ref->hash.key = base->handle;
		ref->obj = base;
		ref->tfile = tfile;
		kref_init(&ref->kref);

		spin_lock(&tfile->lock);
		hash_add_rcu(tfile->ref_hash, &ref->hash.head, ref->hash.key);
		ret = 0;

		list_add_tail(&ref->head, &tfile->ref_list);
		kref_get(&base->refcount);
		spin_unlock(&tfile->lock);
		if (existed != NULL)
			*existed = false;
	}

	return ret;
}

static void __releases(tfile->lock) __acquires(tfile->lock)
ttm_ref_object_release(struct kref *kref)
{
	struct ttm_ref_object *ref =
	    container_of(kref, struct ttm_ref_object, kref);
	struct ttm_object_file *tfile = ref->tfile;

	hash_del_rcu(&ref->hash.head);
	list_del(&ref->head);
	spin_unlock(&tfile->lock);

	ttm_base_object_unref(&ref->obj);
	kfree_rcu(ref, rcu_head);
	spin_lock(&tfile->lock);
}

int ttm_ref_object_base_unref(struct ttm_object_file *tfile,
			      unsigned long key)
{
	struct ttm_ref_object *ref;
	struct vmwgfx_hash_item *hash;
	int ret;

	spin_lock(&tfile->lock);
	ret = ttm_tfile_find_ref(tfile, key, &hash);
	if (unlikely(ret != 0)) {
		spin_unlock(&tfile->lock);
		return -EINVAL;
	}
	ref = hlist_entry(hash, struct ttm_ref_object, hash);
	kref_put(&ref->kref, ttm_ref_object_release);
	spin_unlock(&tfile->lock);
	return 0;
}

void ttm_object_file_release(struct ttm_object_file **p_tfile)
{
	struct ttm_ref_object *ref;
	struct list_head *list;
	struct ttm_object_file *tfile = *p_tfile;

	*p_tfile = NULL;
	spin_lock(&tfile->lock);

	 

	while (!list_empty(&tfile->ref_list)) {
		list = tfile->ref_list.next;
		ref = list_entry(list, struct ttm_ref_object, head);
		ttm_ref_object_release(&ref->kref);
	}

	spin_unlock(&tfile->lock);

	ttm_object_file_unref(&tfile);
}

struct ttm_object_file *ttm_object_file_init(struct ttm_object_device *tdev)
{
	struct ttm_object_file *tfile = kmalloc(sizeof(*tfile), GFP_KERNEL);

	if (unlikely(tfile == NULL))
		return NULL;

	spin_lock_init(&tfile->lock);
	tfile->tdev = tdev;
	kref_init(&tfile->refcount);
	INIT_LIST_HEAD(&tfile->ref_list);

	hash_init(tfile->ref_hash);

	return tfile;
}

struct ttm_object_device *
ttm_object_device_init(const struct dma_buf_ops *ops)
{
	struct ttm_object_device *tdev = kmalloc(sizeof(*tdev), GFP_KERNEL);

	if (unlikely(tdev == NULL))
		return NULL;

	spin_lock_init(&tdev->object_lock);
	atomic_set(&tdev->object_count, 0);

	 
	idr_init_base(&tdev->idr, VMWGFX_NUM_MOB + 1);
	tdev->ops = *ops;
	tdev->dmabuf_release = tdev->ops.release;
	tdev->ops.release = ttm_prime_dmabuf_release;
	return tdev;
}

void ttm_object_device_release(struct ttm_object_device **p_tdev)
{
	struct ttm_object_device *tdev = *p_tdev;

	*p_tdev = NULL;

	WARN_ON_ONCE(!idr_is_empty(&tdev->idr));
	idr_destroy(&tdev->idr);

	kfree(tdev);
}

 
static bool __must_check get_dma_buf_unless_doomed(struct dma_buf *dmabuf)
{
	return atomic_long_inc_not_zero(&dmabuf->file->f_count) != 0L;
}

 
static void ttm_prime_refcount_release(struct ttm_base_object **p_base)
{
	struct ttm_base_object *base = *p_base;
	struct ttm_prime_object *prime;

	*p_base = NULL;
	prime = container_of(base, struct ttm_prime_object, base);
	BUG_ON(prime->dma_buf != NULL);
	mutex_destroy(&prime->mutex);
	if (prime->refcount_release)
		prime->refcount_release(&base);
}

 
static void ttm_prime_dmabuf_release(struct dma_buf *dma_buf)
{
	struct ttm_prime_object *prime =
		(struct ttm_prime_object *) dma_buf->priv;
	struct ttm_base_object *base = &prime->base;
	struct ttm_object_device *tdev = base->tfile->tdev;

	if (tdev->dmabuf_release)
		tdev->dmabuf_release(dma_buf);
	mutex_lock(&prime->mutex);
	if (prime->dma_buf == dma_buf)
		prime->dma_buf = NULL;
	mutex_unlock(&prime->mutex);
	ttm_base_object_unref(&base);
}

 
int ttm_prime_fd_to_handle(struct ttm_object_file *tfile,
			   int fd, u32 *handle)
{
	struct ttm_object_device *tdev = tfile->tdev;
	struct dma_buf *dma_buf;
	struct ttm_prime_object *prime;
	struct ttm_base_object *base;
	int ret;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	if (dma_buf->ops != &tdev->ops)
		return -ENOSYS;

	prime = (struct ttm_prime_object *) dma_buf->priv;
	base = &prime->base;
	*handle = base->handle;
	ret = ttm_ref_object_add(tfile, base, NULL, false);

	dma_buf_put(dma_buf);

	return ret;
}

 
int ttm_prime_handle_to_fd(struct ttm_object_file *tfile,
			   uint32_t handle, uint32_t flags,
			   int *prime_fd)
{
	struct ttm_object_device *tdev = tfile->tdev;
	struct ttm_base_object *base;
	struct dma_buf *dma_buf;
	struct ttm_prime_object *prime;
	int ret;

	base = ttm_base_object_lookup(tfile, handle);
	if (unlikely(base == NULL ||
		     base->object_type != ttm_prime_type)) {
		ret = -ENOENT;
		goto out_unref;
	}

	prime = container_of(base, struct ttm_prime_object, base);
	if (unlikely(!base->shareable)) {
		ret = -EPERM;
		goto out_unref;
	}

	ret = mutex_lock_interruptible(&prime->mutex);
	if (unlikely(ret != 0)) {
		ret = -ERESTARTSYS;
		goto out_unref;
	}

	dma_buf = prime->dma_buf;
	if (!dma_buf || !get_dma_buf_unless_doomed(dma_buf)) {
		DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
		exp_info.ops = &tdev->ops;
		exp_info.size = prime->size;
		exp_info.flags = flags;
		exp_info.priv = prime;

		 

		dma_buf = dma_buf_export(&exp_info);
		if (IS_ERR(dma_buf)) {
			ret = PTR_ERR(dma_buf);
			mutex_unlock(&prime->mutex);
			goto out_unref;
		}

		 
		base = NULL;
		prime->dma_buf = dma_buf;
	}
	mutex_unlock(&prime->mutex);

	ret = dma_buf_fd(dma_buf, flags);
	if (ret >= 0) {
		*prime_fd = ret;
		ret = 0;
	} else
		dma_buf_put(dma_buf);

out_unref:
	if (base)
		ttm_base_object_unref(&base);
	return ret;
}

 
int ttm_prime_object_init(struct ttm_object_file *tfile, size_t size,
			  struct ttm_prime_object *prime, bool shareable,
			  enum ttm_object_type type,
			  void (*refcount_release) (struct ttm_base_object **))
{
	mutex_init(&prime->mutex);
	prime->size = PAGE_ALIGN(size);
	prime->real_type = type;
	prime->dma_buf = NULL;
	prime->refcount_release = refcount_release;
	return ttm_base_object_init(tfile, &prime->base, shareable,
				    ttm_prime_type,
				    ttm_prime_refcount_release);
}
