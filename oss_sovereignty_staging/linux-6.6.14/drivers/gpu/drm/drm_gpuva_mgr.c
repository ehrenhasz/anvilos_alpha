
 

#include <drm/drm_gpuva_mgr.h>

#include <linux/interval_tree_generic.h>
#include <linux/mm.h>

 

 

 

 

#define to_drm_gpuva(__node)	container_of((__node), struct drm_gpuva, rb.node)

#define GPUVA_START(node) ((node)->va.addr)
#define GPUVA_LAST(node) ((node)->va.addr + (node)->va.range - 1)

 
INTERVAL_TREE_DEFINE(struct drm_gpuva, rb.node, u64, rb.__subtree_last,
		     GPUVA_START, GPUVA_LAST, static __maybe_unused,
		     drm_gpuva_it)

static int __drm_gpuva_insert(struct drm_gpuva_manager *mgr,
			      struct drm_gpuva *va);
static void __drm_gpuva_remove(struct drm_gpuva *va);

static bool
drm_gpuva_check_overflow(u64 addr, u64 range)
{
	u64 end;

	return WARN(check_add_overflow(addr, range, &end),
		    "GPUVA address limited to %zu bytes.\n", sizeof(end));
}

static bool
drm_gpuva_in_mm_range(struct drm_gpuva_manager *mgr, u64 addr, u64 range)
{
	u64 end = addr + range;
	u64 mm_start = mgr->mm_start;
	u64 mm_end = mm_start + mgr->mm_range;

	return addr >= mm_start && end <= mm_end;
}

static bool
drm_gpuva_in_kernel_node(struct drm_gpuva_manager *mgr, u64 addr, u64 range)
{
	u64 end = addr + range;
	u64 kstart = mgr->kernel_alloc_node.va.addr;
	u64 krange = mgr->kernel_alloc_node.va.range;
	u64 kend = kstart + krange;

	return krange && addr < kend && kstart < end;
}

static bool
drm_gpuva_range_valid(struct drm_gpuva_manager *mgr,
		      u64 addr, u64 range)
{
	return !drm_gpuva_check_overflow(addr, range) &&
	       drm_gpuva_in_mm_range(mgr, addr, range) &&
	       !drm_gpuva_in_kernel_node(mgr, addr, range);
}

 
void
drm_gpuva_manager_init(struct drm_gpuva_manager *mgr,
		       const char *name,
		       u64 start_offset, u64 range,
		       u64 reserve_offset, u64 reserve_range,
		       const struct drm_gpuva_fn_ops *ops)
{
	mgr->rb.tree = RB_ROOT_CACHED;
	INIT_LIST_HEAD(&mgr->rb.list);

	drm_gpuva_check_overflow(start_offset, range);
	mgr->mm_start = start_offset;
	mgr->mm_range = range;

	mgr->name = name ? name : "unknown";
	mgr->ops = ops;

	memset(&mgr->kernel_alloc_node, 0, sizeof(struct drm_gpuva));

	if (reserve_range) {
		mgr->kernel_alloc_node.va.addr = reserve_offset;
		mgr->kernel_alloc_node.va.range = reserve_range;

		if (likely(!drm_gpuva_check_overflow(reserve_offset,
						     reserve_range)))
			__drm_gpuva_insert(mgr, &mgr->kernel_alloc_node);
	}
}
EXPORT_SYMBOL_GPL(drm_gpuva_manager_init);

 
void
drm_gpuva_manager_destroy(struct drm_gpuva_manager *mgr)
{
	mgr->name = NULL;

	if (mgr->kernel_alloc_node.va.range)
		__drm_gpuva_remove(&mgr->kernel_alloc_node);

	WARN(!RB_EMPTY_ROOT(&mgr->rb.tree.rb_root),
	     "GPUVA tree is not empty, potentially leaking memory.");
}
EXPORT_SYMBOL_GPL(drm_gpuva_manager_destroy);

static int
__drm_gpuva_insert(struct drm_gpuva_manager *mgr,
		   struct drm_gpuva *va)
{
	struct rb_node *node;
	struct list_head *head;

	if (drm_gpuva_it_iter_first(&mgr->rb.tree,
				    GPUVA_START(va),
				    GPUVA_LAST(va)))
		return -EEXIST;

	va->mgr = mgr;

	drm_gpuva_it_insert(va, &mgr->rb.tree);

	node = rb_prev(&va->rb.node);
	if (node)
		head = &(to_drm_gpuva(node))->rb.entry;
	else
		head = &mgr->rb.list;

	list_add(&va->rb.entry, head);

	return 0;
}

 
int
drm_gpuva_insert(struct drm_gpuva_manager *mgr,
		 struct drm_gpuva *va)
{
	u64 addr = va->va.addr;
	u64 range = va->va.range;

	if (unlikely(!drm_gpuva_range_valid(mgr, addr, range)))
		return -EINVAL;

	return __drm_gpuva_insert(mgr, va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_insert);

static void
__drm_gpuva_remove(struct drm_gpuva *va)
{
	drm_gpuva_it_remove(va, &va->mgr->rb.tree);
	list_del_init(&va->rb.entry);
}

 
void
drm_gpuva_remove(struct drm_gpuva *va)
{
	struct drm_gpuva_manager *mgr = va->mgr;

	if (unlikely(va == &mgr->kernel_alloc_node)) {
		WARN(1, "Can't destroy kernel reserved node.\n");
		return;
	}

	__drm_gpuva_remove(va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_remove);

 
void
drm_gpuva_link(struct drm_gpuva *va)
{
	struct drm_gem_object *obj = va->gem.obj;

	if (unlikely(!obj))
		return;

	drm_gem_gpuva_assert_lock_held(obj);

	list_add_tail(&va->gem.entry, &obj->gpuva.list);
}
EXPORT_SYMBOL_GPL(drm_gpuva_link);

 
void
drm_gpuva_unlink(struct drm_gpuva *va)
{
	struct drm_gem_object *obj = va->gem.obj;

	if (unlikely(!obj))
		return;

	drm_gem_gpuva_assert_lock_held(obj);

	list_del_init(&va->gem.entry);
}
EXPORT_SYMBOL_GPL(drm_gpuva_unlink);

 
struct drm_gpuva *
drm_gpuva_find_first(struct drm_gpuva_manager *mgr,
		     u64 addr, u64 range)
{
	u64 last = addr + range - 1;

	return drm_gpuva_it_iter_first(&mgr->rb.tree, addr, last);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_first);

 
struct drm_gpuva *
drm_gpuva_find(struct drm_gpuva_manager *mgr,
	       u64 addr, u64 range)
{
	struct drm_gpuva *va;

	va = drm_gpuva_find_first(mgr, addr, range);
	if (!va)
		goto out;

	if (va->va.addr != addr ||
	    va->va.range != range)
		goto out;

	return va;

out:
	return NULL;
}
EXPORT_SYMBOL_GPL(drm_gpuva_find);

 
struct drm_gpuva *
drm_gpuva_find_prev(struct drm_gpuva_manager *mgr, u64 start)
{
	if (!drm_gpuva_range_valid(mgr, start - 1, 1))
		return NULL;

	return drm_gpuva_it_iter_first(&mgr->rb.tree, start - 1, start);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_prev);

 
struct drm_gpuva *
drm_gpuva_find_next(struct drm_gpuva_manager *mgr, u64 end)
{
	if (!drm_gpuva_range_valid(mgr, end, 1))
		return NULL;

	return drm_gpuva_it_iter_first(&mgr->rb.tree, end, end + 1);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_next);

 
bool
drm_gpuva_interval_empty(struct drm_gpuva_manager *mgr, u64 addr, u64 range)
{
	return !drm_gpuva_find_first(mgr, addr, range);
}
EXPORT_SYMBOL_GPL(drm_gpuva_interval_empty);

 
void
drm_gpuva_map(struct drm_gpuva_manager *mgr,
	      struct drm_gpuva *va,
	      struct drm_gpuva_op_map *op)
{
	drm_gpuva_init_from_op(va, op);
	drm_gpuva_insert(mgr, va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_map);

 
void
drm_gpuva_remap(struct drm_gpuva *prev,
		struct drm_gpuva *next,
		struct drm_gpuva_op_remap *op)
{
	struct drm_gpuva *curr = op->unmap->va;
	struct drm_gpuva_manager *mgr = curr->mgr;

	drm_gpuva_remove(curr);

	if (op->prev) {
		drm_gpuva_init_from_op(prev, op->prev);
		drm_gpuva_insert(mgr, prev);
	}

	if (op->next) {
		drm_gpuva_init_from_op(next, op->next);
		drm_gpuva_insert(mgr, next);
	}
}
EXPORT_SYMBOL_GPL(drm_gpuva_remap);

 
void
drm_gpuva_unmap(struct drm_gpuva_op_unmap *op)
{
	drm_gpuva_remove(op->va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_unmap);

static int
op_map_cb(const struct drm_gpuva_fn_ops *fn, void *priv,
	  u64 addr, u64 range,
	  struct drm_gem_object *obj, u64 offset)
{
	struct drm_gpuva_op op = {};

	op.op = DRM_GPUVA_OP_MAP;
	op.map.va.addr = addr;
	op.map.va.range = range;
	op.map.gem.obj = obj;
	op.map.gem.offset = offset;

	return fn->sm_step_map(&op, priv);
}

static int
op_remap_cb(const struct drm_gpuva_fn_ops *fn, void *priv,
	    struct drm_gpuva_op_map *prev,
	    struct drm_gpuva_op_map *next,
	    struct drm_gpuva_op_unmap *unmap)
{
	struct drm_gpuva_op op = {};
	struct drm_gpuva_op_remap *r;

	op.op = DRM_GPUVA_OP_REMAP;
	r = &op.remap;
	r->prev = prev;
	r->next = next;
	r->unmap = unmap;

	return fn->sm_step_remap(&op, priv);
}

static int
op_unmap_cb(const struct drm_gpuva_fn_ops *fn, void *priv,
	    struct drm_gpuva *va, bool merge)
{
	struct drm_gpuva_op op = {};

	op.op = DRM_GPUVA_OP_UNMAP;
	op.unmap.va = va;
	op.unmap.keep = merge;

	return fn->sm_step_unmap(&op, priv);
}

static int
__drm_gpuva_sm_map(struct drm_gpuva_manager *mgr,
		   const struct drm_gpuva_fn_ops *ops, void *priv,
		   u64 req_addr, u64 req_range,
		   struct drm_gem_object *req_obj, u64 req_offset)
{
	struct drm_gpuva *va, *next;
	u64 req_end = req_addr + req_range;
	int ret;

	if (unlikely(!drm_gpuva_range_valid(mgr, req_addr, req_range)))
		return -EINVAL;

	drm_gpuva_for_each_va_range_safe(va, next, mgr, req_addr, req_end) {
		struct drm_gem_object *obj = va->gem.obj;
		u64 offset = va->gem.offset;
		u64 addr = va->va.addr;
		u64 range = va->va.range;
		u64 end = addr + range;
		bool merge = !!va->gem.obj;

		if (addr == req_addr) {
			merge &= obj == req_obj &&
				 offset == req_offset;

			if (end == req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = range - req_range,
					.gem.obj = obj,
					.gem.offset = offset + req_range,
				};
				struct drm_gpuva_op_unmap u = {
					.va = va,
					.keep = merge,
				};

				ret = op_remap_cb(ops, priv, NULL, &n, &u);
				if (ret)
					return ret;
				break;
			}
		} else if (addr < req_addr) {
			u64 ls_range = req_addr - addr;
			struct drm_gpuva_op_map p = {
				.va.addr = addr,
				.va.range = ls_range,
				.gem.obj = obj,
				.gem.offset = offset,
			};
			struct drm_gpuva_op_unmap u = { .va = va };

			merge &= obj == req_obj &&
				 offset + ls_range == req_offset;
			u.keep = merge;

			if (end == req_end) {
				ret = op_remap_cb(ops, priv, &p, NULL, &u);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_remap_cb(ops, priv, &p, NULL, &u);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = end - req_end,
					.gem.obj = obj,
					.gem.offset = offset + ls_range +
						      req_range,
				};

				ret = op_remap_cb(ops, priv, &p, &n, &u);
				if (ret)
					return ret;
				break;
			}
		} else if (addr > req_addr) {
			merge &= obj == req_obj &&
				 offset == req_offset +
					   (addr - req_addr);

			if (end == req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = end - req_end,
					.gem.obj = obj,
					.gem.offset = offset + req_end - addr,
				};
				struct drm_gpuva_op_unmap u = {
					.va = va,
					.keep = merge,
				};

				ret = op_remap_cb(ops, priv, NULL, &n, &u);
				if (ret)
					return ret;
				break;
			}
		}
	}

	return op_map_cb(ops, priv,
			 req_addr, req_range,
			 req_obj, req_offset);
}

static int
__drm_gpuva_sm_unmap(struct drm_gpuva_manager *mgr,
		     const struct drm_gpuva_fn_ops *ops, void *priv,
		     u64 req_addr, u64 req_range)
{
	struct drm_gpuva *va, *next;
	u64 req_end = req_addr + req_range;
	int ret;

	if (unlikely(!drm_gpuva_range_valid(mgr, req_addr, req_range)))
		return -EINVAL;

	drm_gpuva_for_each_va_range_safe(va, next, mgr, req_addr, req_end) {
		struct drm_gpuva_op_map prev = {}, next = {};
		bool prev_split = false, next_split = false;
		struct drm_gem_object *obj = va->gem.obj;
		u64 offset = va->gem.offset;
		u64 addr = va->va.addr;
		u64 range = va->va.range;
		u64 end = addr + range;

		if (addr < req_addr) {
			prev.va.addr = addr;
			prev.va.range = req_addr - addr;
			prev.gem.obj = obj;
			prev.gem.offset = offset;

			prev_split = true;
		}

		if (end > req_end) {
			next.va.addr = req_end;
			next.va.range = end - req_end;
			next.gem.obj = obj;
			next.gem.offset = offset + (req_end - addr);

			next_split = true;
		}

		if (prev_split || next_split) {
			struct drm_gpuva_op_unmap unmap = { .va = va };

			ret = op_remap_cb(ops, priv,
					  prev_split ? &prev : NULL,
					  next_split ? &next : NULL,
					  &unmap);
			if (ret)
				return ret;
		} else {
			ret = op_unmap_cb(ops, priv, va, false);
			if (ret)
				return ret;
		}
	}

	return 0;
}

 
int
drm_gpuva_sm_map(struct drm_gpuva_manager *mgr, void *priv,
		 u64 req_addr, u64 req_range,
		 struct drm_gem_object *req_obj, u64 req_offset)
{
	const struct drm_gpuva_fn_ops *ops = mgr->ops;

	if (unlikely(!(ops && ops->sm_step_map &&
		       ops->sm_step_remap &&
		       ops->sm_step_unmap)))
		return -EINVAL;

	return __drm_gpuva_sm_map(mgr, ops, priv,
				  req_addr, req_range,
				  req_obj, req_offset);
}
EXPORT_SYMBOL_GPL(drm_gpuva_sm_map);

 
int
drm_gpuva_sm_unmap(struct drm_gpuva_manager *mgr, void *priv,
		   u64 req_addr, u64 req_range)
{
	const struct drm_gpuva_fn_ops *ops = mgr->ops;

	if (unlikely(!(ops && ops->sm_step_remap &&
		       ops->sm_step_unmap)))
		return -EINVAL;

	return __drm_gpuva_sm_unmap(mgr, ops, priv,
				    req_addr, req_range);
}
EXPORT_SYMBOL_GPL(drm_gpuva_sm_unmap);

static struct drm_gpuva_op *
gpuva_op_alloc(struct drm_gpuva_manager *mgr)
{
	const struct drm_gpuva_fn_ops *fn = mgr->ops;
	struct drm_gpuva_op *op;

	if (fn && fn->op_alloc)
		op = fn->op_alloc();
	else
		op = kzalloc(sizeof(*op), GFP_KERNEL);

	if (unlikely(!op))
		return NULL;

	return op;
}

static void
gpuva_op_free(struct drm_gpuva_manager *mgr,
	      struct drm_gpuva_op *op)
{
	const struct drm_gpuva_fn_ops *fn = mgr->ops;

	if (fn && fn->op_free)
		fn->op_free(op);
	else
		kfree(op);
}

static int
drm_gpuva_sm_step(struct drm_gpuva_op *__op,
		  void *priv)
{
	struct {
		struct drm_gpuva_manager *mgr;
		struct drm_gpuva_ops *ops;
	} *args = priv;
	struct drm_gpuva_manager *mgr = args->mgr;
	struct drm_gpuva_ops *ops = args->ops;
	struct drm_gpuva_op *op;

	op = gpuva_op_alloc(mgr);
	if (unlikely(!op))
		goto err;

	memcpy(op, __op, sizeof(*op));

	if (op->op == DRM_GPUVA_OP_REMAP) {
		struct drm_gpuva_op_remap *__r = &__op->remap;
		struct drm_gpuva_op_remap *r = &op->remap;

		r->unmap = kmemdup(__r->unmap, sizeof(*r->unmap),
				   GFP_KERNEL);
		if (unlikely(!r->unmap))
			goto err_free_op;

		if (__r->prev) {
			r->prev = kmemdup(__r->prev, sizeof(*r->prev),
					  GFP_KERNEL);
			if (unlikely(!r->prev))
				goto err_free_unmap;
		}

		if (__r->next) {
			r->next = kmemdup(__r->next, sizeof(*r->next),
					  GFP_KERNEL);
			if (unlikely(!r->next))
				goto err_free_prev;
		}
	}

	list_add_tail(&op->entry, &ops->list);

	return 0;

err_free_unmap:
	kfree(op->remap.unmap);
err_free_prev:
	kfree(op->remap.prev);
err_free_op:
	gpuva_op_free(mgr, op);
err:
	return -ENOMEM;
}

static const struct drm_gpuva_fn_ops gpuva_list_ops = {
	.sm_step_map = drm_gpuva_sm_step,
	.sm_step_remap = drm_gpuva_sm_step,
	.sm_step_unmap = drm_gpuva_sm_step,
};

 
struct drm_gpuva_ops *
drm_gpuva_sm_map_ops_create(struct drm_gpuva_manager *mgr,
			    u64 req_addr, u64 req_range,
			    struct drm_gem_object *req_obj, u64 req_offset)
{
	struct drm_gpuva_ops *ops;
	struct {
		struct drm_gpuva_manager *mgr;
		struct drm_gpuva_ops *ops;
	} args;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (unlikely(!ops))
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	args.mgr = mgr;
	args.ops = ops;

	ret = __drm_gpuva_sm_map(mgr, &gpuva_list_ops, &args,
				 req_addr, req_range,
				 req_obj, req_offset);
	if (ret)
		goto err_free_ops;

	return ops;

err_free_ops:
	drm_gpuva_ops_free(mgr, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuva_sm_map_ops_create);

 
struct drm_gpuva_ops *
drm_gpuva_sm_unmap_ops_create(struct drm_gpuva_manager *mgr,
			      u64 req_addr, u64 req_range)
{
	struct drm_gpuva_ops *ops;
	struct {
		struct drm_gpuva_manager *mgr;
		struct drm_gpuva_ops *ops;
	} args;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (unlikely(!ops))
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	args.mgr = mgr;
	args.ops = ops;

	ret = __drm_gpuva_sm_unmap(mgr, &gpuva_list_ops, &args,
				   req_addr, req_range);
	if (ret)
		goto err_free_ops;

	return ops;

err_free_ops:
	drm_gpuva_ops_free(mgr, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuva_sm_unmap_ops_create);

 
struct drm_gpuva_ops *
drm_gpuva_prefetch_ops_create(struct drm_gpuva_manager *mgr,
			      u64 addr, u64 range)
{
	struct drm_gpuva_ops *ops;
	struct drm_gpuva_op *op;
	struct drm_gpuva *va;
	u64 end = addr + range;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	drm_gpuva_for_each_va_range(va, mgr, addr, end) {
		op = gpuva_op_alloc(mgr);
		if (!op) {
			ret = -ENOMEM;
			goto err_free_ops;
		}

		op->op = DRM_GPUVA_OP_PREFETCH;
		op->prefetch.va = va;
		list_add_tail(&op->entry, &ops->list);
	}

	return ops;

err_free_ops:
	drm_gpuva_ops_free(mgr, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuva_prefetch_ops_create);

 
struct drm_gpuva_ops *
drm_gpuva_gem_unmap_ops_create(struct drm_gpuva_manager *mgr,
			       struct drm_gem_object *obj)
{
	struct drm_gpuva_ops *ops;
	struct drm_gpuva_op *op;
	struct drm_gpuva *va;
	int ret;

	drm_gem_gpuva_assert_lock_held(obj);

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	drm_gem_for_each_gpuva(va, obj) {
		op = gpuva_op_alloc(mgr);
		if (!op) {
			ret = -ENOMEM;
			goto err_free_ops;
		}

		op->op = DRM_GPUVA_OP_UNMAP;
		op->unmap.va = va;
		list_add_tail(&op->entry, &ops->list);
	}

	return ops;

err_free_ops:
	drm_gpuva_ops_free(mgr, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuva_gem_unmap_ops_create);

 
void
drm_gpuva_ops_free(struct drm_gpuva_manager *mgr,
		   struct drm_gpuva_ops *ops)
{
	struct drm_gpuva_op *op, *next;

	drm_gpuva_for_each_op_safe(op, next, ops) {
		list_del(&op->entry);

		if (op->op == DRM_GPUVA_OP_REMAP) {
			kfree(op->remap.prev);
			kfree(op->remap.next);
			kfree(op->remap.unmap);
		}

		gpuva_op_free(mgr, op);
	}

	kfree(ops);
}
EXPORT_SYMBOL_GPL(drm_gpuva_ops_free);
