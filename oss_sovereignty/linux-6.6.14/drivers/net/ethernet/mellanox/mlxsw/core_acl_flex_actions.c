
 

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/rhashtable.h>
#include <linux/list.h>
#include <linux/idr.h>
#include <linux/refcount.h>
#include <net/flow_offload.h>

#include "item.h"
#include "trap.h"
#include "core_acl_flex_actions.h"

enum mlxsw_afa_set_type {
	MLXSW_AFA_SET_TYPE_NEXT,
	MLXSW_AFA_SET_TYPE_GOTO,
};

 
MLXSW_ITEM32(afa, set, type, 0xA0, 28, 4);

 
MLXSW_ITEM32(afa, set, next_action_set_ptr, 0xA4, 0, 24);

 
MLXSW_ITEM32(afa, set, goto_g, 0xA4, 29, 1);

enum mlxsw_afa_set_goto_binding_cmd {
	 
	MLXSW_AFA_SET_GOTO_BINDING_CMD_NONE,
	 
	MLXSW_AFA_SET_GOTO_BINDING_CMD_JUMP,
	 
	MLXSW_AFA_SET_GOTO_BINDING_CMD_TERM = 4,
};

 
MLXSW_ITEM32(afa, set, goto_binding_cmd, 0xA4, 24, 3);

 
MLXSW_ITEM32(afa, set, goto_next_binding, 0xA4, 0, 16);

 
MLXSW_ITEM32(afa, all, action_type, 0x00, 24, 6);

struct mlxsw_afa {
	unsigned int max_acts_per_set;
	const struct mlxsw_afa_ops *ops;
	void *ops_priv;
	struct rhashtable set_ht;
	struct rhashtable fwd_entry_ht;
	struct rhashtable cookie_ht;
	struct rhashtable policer_ht;
	struct idr cookie_idr;
	struct list_head policer_list;
};

#define MLXSW_AFA_SET_LEN 0xA8

struct mlxsw_afa_set_ht_key {
	char enc_actions[MLXSW_AFA_SET_LEN];  
	bool is_first;
};

 

struct mlxsw_afa_set {
	struct rhash_head ht_node;
	struct mlxsw_afa_set_ht_key ht_key;
	u32 kvdl_index;
	u8 shared:1,  
	   has_trap:1,
	   has_police:1;
	unsigned int ref_count;
	struct mlxsw_afa_set *next;  
	struct mlxsw_afa_set *prev;  
};

static const struct rhashtable_params mlxsw_afa_set_ht_params = {
	.key_len = sizeof(struct mlxsw_afa_set_ht_key),
	.key_offset = offsetof(struct mlxsw_afa_set, ht_key),
	.head_offset = offsetof(struct mlxsw_afa_set, ht_node),
	.automatic_shrinking = true,
};

struct mlxsw_afa_fwd_entry_ht_key {
	u16 local_port;
};

struct mlxsw_afa_fwd_entry {
	struct rhash_head ht_node;
	struct mlxsw_afa_fwd_entry_ht_key ht_key;
	u32 kvdl_index;
	unsigned int ref_count;
};

static const struct rhashtable_params mlxsw_afa_fwd_entry_ht_params = {
	.key_len = sizeof(struct mlxsw_afa_fwd_entry_ht_key),
	.key_offset = offsetof(struct mlxsw_afa_fwd_entry, ht_key),
	.head_offset = offsetof(struct mlxsw_afa_fwd_entry, ht_node),
	.automatic_shrinking = true,
};

struct mlxsw_afa_cookie {
	struct rhash_head ht_node;
	refcount_t ref_count;
	struct rcu_head rcu;
	u32 cookie_index;
	struct flow_action_cookie fa_cookie;
};

static u32 mlxsw_afa_cookie_hash(const struct flow_action_cookie *fa_cookie,
				 u32 seed)
{
	return jhash2((u32 *) fa_cookie->cookie,
		      fa_cookie->cookie_len / sizeof(u32), seed);
}

static u32 mlxsw_afa_cookie_key_hashfn(const void *data, u32 len, u32 seed)
{
	const struct flow_action_cookie *fa_cookie = data;

	return mlxsw_afa_cookie_hash(fa_cookie, seed);
}

static u32 mlxsw_afa_cookie_obj_hashfn(const void *data, u32 len, u32 seed)
{
	const struct mlxsw_afa_cookie *cookie = data;

	return mlxsw_afa_cookie_hash(&cookie->fa_cookie, seed);
}

static int mlxsw_afa_cookie_obj_cmpfn(struct rhashtable_compare_arg *arg,
				      const void *obj)
{
	const struct flow_action_cookie *fa_cookie = arg->key;
	const struct mlxsw_afa_cookie *cookie = obj;

	if (cookie->fa_cookie.cookie_len == fa_cookie->cookie_len)
		return memcmp(cookie->fa_cookie.cookie, fa_cookie->cookie,
			      fa_cookie->cookie_len);
	return 1;
}

static const struct rhashtable_params mlxsw_afa_cookie_ht_params = {
	.head_offset = offsetof(struct mlxsw_afa_cookie, ht_node),
	.hashfn	= mlxsw_afa_cookie_key_hashfn,
	.obj_hashfn = mlxsw_afa_cookie_obj_hashfn,
	.obj_cmpfn = mlxsw_afa_cookie_obj_cmpfn,
	.automatic_shrinking = true,
};

struct mlxsw_afa_policer {
	struct rhash_head ht_node;
	struct list_head list;  
	refcount_t ref_count;
	u32 fa_index;
	u16 policer_index;
};

static const struct rhashtable_params mlxsw_afa_policer_ht_params = {
	.key_len = sizeof(u32),
	.key_offset = offsetof(struct mlxsw_afa_policer, fa_index),
	.head_offset = offsetof(struct mlxsw_afa_policer, ht_node),
	.automatic_shrinking = true,
};

struct mlxsw_afa *mlxsw_afa_create(unsigned int max_acts_per_set,
				   const struct mlxsw_afa_ops *ops,
				   void *ops_priv)
{
	struct mlxsw_afa *mlxsw_afa;
	int err;

	mlxsw_afa = kzalloc(sizeof(*mlxsw_afa), GFP_KERNEL);
	if (!mlxsw_afa)
		return ERR_PTR(-ENOMEM);
	err = rhashtable_init(&mlxsw_afa->set_ht, &mlxsw_afa_set_ht_params);
	if (err)
		goto err_set_rhashtable_init;
	err = rhashtable_init(&mlxsw_afa->fwd_entry_ht,
			      &mlxsw_afa_fwd_entry_ht_params);
	if (err)
		goto err_fwd_entry_rhashtable_init;
	err = rhashtable_init(&mlxsw_afa->cookie_ht,
			      &mlxsw_afa_cookie_ht_params);
	if (err)
		goto err_cookie_rhashtable_init;
	err = rhashtable_init(&mlxsw_afa->policer_ht,
			      &mlxsw_afa_policer_ht_params);
	if (err)
		goto err_policer_rhashtable_init;
	idr_init(&mlxsw_afa->cookie_idr);
	INIT_LIST_HEAD(&mlxsw_afa->policer_list);
	mlxsw_afa->max_acts_per_set = max_acts_per_set;
	mlxsw_afa->ops = ops;
	mlxsw_afa->ops_priv = ops_priv;
	return mlxsw_afa;

err_policer_rhashtable_init:
	rhashtable_destroy(&mlxsw_afa->cookie_ht);
err_cookie_rhashtable_init:
	rhashtable_destroy(&mlxsw_afa->fwd_entry_ht);
err_fwd_entry_rhashtable_init:
	rhashtable_destroy(&mlxsw_afa->set_ht);
err_set_rhashtable_init:
	kfree(mlxsw_afa);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(mlxsw_afa_create);

void mlxsw_afa_destroy(struct mlxsw_afa *mlxsw_afa)
{
	WARN_ON(!list_empty(&mlxsw_afa->policer_list));
	WARN_ON(!idr_is_empty(&mlxsw_afa->cookie_idr));
	idr_destroy(&mlxsw_afa->cookie_idr);
	rhashtable_destroy(&mlxsw_afa->policer_ht);
	rhashtable_destroy(&mlxsw_afa->cookie_ht);
	rhashtable_destroy(&mlxsw_afa->fwd_entry_ht);
	rhashtable_destroy(&mlxsw_afa->set_ht);
	kfree(mlxsw_afa);
}
EXPORT_SYMBOL(mlxsw_afa_destroy);

static void mlxsw_afa_set_goto_set(struct mlxsw_afa_set *set,
				   enum mlxsw_afa_set_goto_binding_cmd cmd,
				   u16 group_id)
{
	char *actions = set->ht_key.enc_actions;

	mlxsw_afa_set_type_set(actions, MLXSW_AFA_SET_TYPE_GOTO);
	mlxsw_afa_set_goto_g_set(actions, true);
	mlxsw_afa_set_goto_binding_cmd_set(actions, cmd);
	mlxsw_afa_set_goto_next_binding_set(actions, group_id);
}

static void mlxsw_afa_set_next_set(struct mlxsw_afa_set *set,
				   u32 next_set_kvdl_index)
{
	char *actions = set->ht_key.enc_actions;

	mlxsw_afa_set_type_set(actions, MLXSW_AFA_SET_TYPE_NEXT);
	mlxsw_afa_set_next_action_set_ptr_set(actions, next_set_kvdl_index);
}

static struct mlxsw_afa_set *mlxsw_afa_set_create(bool is_first)
{
	struct mlxsw_afa_set *set;

	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		return NULL;
	 
	mlxsw_afa_set_goto_set(set, MLXSW_AFA_SET_GOTO_BINDING_CMD_TERM, 0);
	set->ht_key.is_first = is_first;
	set->ref_count = 1;
	return set;
}

static void mlxsw_afa_set_destroy(struct mlxsw_afa_set *set)
{
	kfree(set);
}

static int mlxsw_afa_set_share(struct mlxsw_afa *mlxsw_afa,
			       struct mlxsw_afa_set *set)
{
	int err;

	err = rhashtable_insert_fast(&mlxsw_afa->set_ht, &set->ht_node,
				     mlxsw_afa_set_ht_params);
	if (err)
		return err;
	err = mlxsw_afa->ops->kvdl_set_add(mlxsw_afa->ops_priv,
					   &set->kvdl_index,
					   set->ht_key.enc_actions,
					   set->ht_key.is_first);
	if (err)
		goto err_kvdl_set_add;
	set->shared = true;
	set->prev = NULL;
	return 0;

err_kvdl_set_add:
	rhashtable_remove_fast(&mlxsw_afa->set_ht, &set->ht_node,
			       mlxsw_afa_set_ht_params);
	return err;
}

static void mlxsw_afa_set_unshare(struct mlxsw_afa *mlxsw_afa,
				  struct mlxsw_afa_set *set)
{
	mlxsw_afa->ops->kvdl_set_del(mlxsw_afa->ops_priv,
				     set->kvdl_index,
				     set->ht_key.is_first);
	rhashtable_remove_fast(&mlxsw_afa->set_ht, &set->ht_node,
			       mlxsw_afa_set_ht_params);
	set->shared = false;
}

static void mlxsw_afa_set_put(struct mlxsw_afa *mlxsw_afa,
			      struct mlxsw_afa_set *set)
{
	if (--set->ref_count)
		return;
	if (set->shared)
		mlxsw_afa_set_unshare(mlxsw_afa, set);
	mlxsw_afa_set_destroy(set);
}

static struct mlxsw_afa_set *mlxsw_afa_set_get(struct mlxsw_afa *mlxsw_afa,
					       struct mlxsw_afa_set *orig_set)
{
	struct mlxsw_afa_set *set;
	int err;

	 
	set = rhashtable_lookup_fast(&mlxsw_afa->set_ht, &orig_set->ht_key,
				     mlxsw_afa_set_ht_params);
	if (set) {
		set->ref_count++;
		mlxsw_afa_set_put(mlxsw_afa, orig_set);
	} else {
		set = orig_set;
		err = mlxsw_afa_set_share(mlxsw_afa, set);
		if (err)
			return ERR_PTR(err);
	}
	return set;
}

 

struct mlxsw_afa_block {
	struct mlxsw_afa *afa;
	bool finished;
	struct mlxsw_afa_set *first_set;
	struct mlxsw_afa_set *cur_set;
	unsigned int cur_act_index;  
	struct list_head resource_list;  
};

struct mlxsw_afa_resource {
	struct list_head list;
	void (*destructor)(struct mlxsw_afa_block *block,
			   struct mlxsw_afa_resource *resource);
};

static void mlxsw_afa_resource_add(struct mlxsw_afa_block *block,
				   struct mlxsw_afa_resource *resource)
{
	list_add(&resource->list, &block->resource_list);
}

static void mlxsw_afa_resource_del(struct mlxsw_afa_resource *resource)
{
	list_del(&resource->list);
}

static void mlxsw_afa_resources_destroy(struct mlxsw_afa_block *block)
{
	struct mlxsw_afa_resource *resource, *tmp;

	list_for_each_entry_safe(resource, tmp, &block->resource_list, list) {
		resource->destructor(block, resource);
	}
}

struct mlxsw_afa_block *mlxsw_afa_block_create(struct mlxsw_afa *mlxsw_afa)
{
	struct mlxsw_afa_block *block;

	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (!block)
		return ERR_PTR(-ENOMEM);
	INIT_LIST_HEAD(&block->resource_list);
	block->afa = mlxsw_afa;

	 
	block->first_set = mlxsw_afa_set_create(true);
	if (!block->first_set)
		goto err_first_set_create;

	 
	if (mlxsw_afa->ops->dummy_first_set) {
		block->cur_set = mlxsw_afa_set_create(false);
		if (!block->cur_set)
			goto err_second_set_create;
		block->cur_set->prev = block->first_set;
		block->first_set->next = block->cur_set;
	} else {
		block->cur_set = block->first_set;
	}

	return block;

err_second_set_create:
	mlxsw_afa_set_destroy(block->first_set);
err_first_set_create:
	kfree(block);
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(mlxsw_afa_block_create);

void mlxsw_afa_block_destroy(struct mlxsw_afa_block *block)
{
	struct mlxsw_afa_set *set = block->first_set;
	struct mlxsw_afa_set *next_set;

	do {
		next_set = set->next;
		mlxsw_afa_set_put(block->afa, set);
		set = next_set;
	} while (set);
	mlxsw_afa_resources_destroy(block);
	kfree(block);
}
EXPORT_SYMBOL(mlxsw_afa_block_destroy);

int mlxsw_afa_block_commit(struct mlxsw_afa_block *block)
{
	struct mlxsw_afa_set *set = block->cur_set;
	struct mlxsw_afa_set *prev_set;

	block->cur_set = NULL;
	block->finished = true;

	 
	do {
		prev_set = set->prev;
		set = mlxsw_afa_set_get(block->afa, set);
		if (IS_ERR(set))
			 
			return PTR_ERR(set);
		if (prev_set) {
			prev_set->next = set;
			mlxsw_afa_set_next_set(prev_set, set->kvdl_index);
			set = prev_set;
		}
	} while (prev_set);

	block->first_set = set;
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_commit);

char *mlxsw_afa_block_first_set(struct mlxsw_afa_block *block)
{
	return block->first_set->ht_key.enc_actions;
}
EXPORT_SYMBOL(mlxsw_afa_block_first_set);

char *mlxsw_afa_block_cur_set(struct mlxsw_afa_block *block)
{
	return block->cur_set->ht_key.enc_actions;
}
EXPORT_SYMBOL(mlxsw_afa_block_cur_set);

u32 mlxsw_afa_block_first_kvdl_index(struct mlxsw_afa_block *block)
{
	 
	if (WARN_ON(!block->first_set->next))
		return 0;
	return block->first_set->next->kvdl_index;
}
EXPORT_SYMBOL(mlxsw_afa_block_first_kvdl_index);

int mlxsw_afa_block_activity_get(struct mlxsw_afa_block *block, bool *activity)
{
	u32 kvdl_index = mlxsw_afa_block_first_kvdl_index(block);

	return block->afa->ops->kvdl_set_activity_get(block->afa->ops_priv,
						      kvdl_index, activity);
}
EXPORT_SYMBOL(mlxsw_afa_block_activity_get);

int mlxsw_afa_block_continue(struct mlxsw_afa_block *block)
{
	if (block->finished)
		return -EINVAL;
	mlxsw_afa_set_goto_set(block->cur_set,
			       MLXSW_AFA_SET_GOTO_BINDING_CMD_NONE, 0);
	block->finished = true;
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_continue);

int mlxsw_afa_block_jump(struct mlxsw_afa_block *block, u16 group_id)
{
	if (block->finished)
		return -EINVAL;
	mlxsw_afa_set_goto_set(block->cur_set,
			       MLXSW_AFA_SET_GOTO_BINDING_CMD_JUMP, group_id);
	block->finished = true;
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_jump);

int mlxsw_afa_block_terminate(struct mlxsw_afa_block *block)
{
	if (block->finished)
		return -EINVAL;
	mlxsw_afa_set_goto_set(block->cur_set,
			       MLXSW_AFA_SET_GOTO_BINDING_CMD_TERM, 0);
	block->finished = true;
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_terminate);

static struct mlxsw_afa_fwd_entry *
mlxsw_afa_fwd_entry_create(struct mlxsw_afa *mlxsw_afa, u16 local_port)
{
	struct mlxsw_afa_fwd_entry *fwd_entry;
	int err;

	fwd_entry = kzalloc(sizeof(*fwd_entry), GFP_KERNEL);
	if (!fwd_entry)
		return ERR_PTR(-ENOMEM);
	fwd_entry->ht_key.local_port = local_port;
	fwd_entry->ref_count = 1;

	err = rhashtable_insert_fast(&mlxsw_afa->fwd_entry_ht,
				     &fwd_entry->ht_node,
				     mlxsw_afa_fwd_entry_ht_params);
	if (err)
		goto err_rhashtable_insert;

	err = mlxsw_afa->ops->kvdl_fwd_entry_add(mlxsw_afa->ops_priv,
						 &fwd_entry->kvdl_index,
						 local_port);
	if (err)
		goto err_kvdl_fwd_entry_add;
	return fwd_entry;

err_kvdl_fwd_entry_add:
	rhashtable_remove_fast(&mlxsw_afa->fwd_entry_ht, &fwd_entry->ht_node,
			       mlxsw_afa_fwd_entry_ht_params);
err_rhashtable_insert:
	kfree(fwd_entry);
	return ERR_PTR(err);
}

static void mlxsw_afa_fwd_entry_destroy(struct mlxsw_afa *mlxsw_afa,
					struct mlxsw_afa_fwd_entry *fwd_entry)
{
	mlxsw_afa->ops->kvdl_fwd_entry_del(mlxsw_afa->ops_priv,
					   fwd_entry->kvdl_index);
	rhashtable_remove_fast(&mlxsw_afa->fwd_entry_ht, &fwd_entry->ht_node,
			       mlxsw_afa_fwd_entry_ht_params);
	kfree(fwd_entry);
}

static struct mlxsw_afa_fwd_entry *
mlxsw_afa_fwd_entry_get(struct mlxsw_afa *mlxsw_afa, u16 local_port)
{
	struct mlxsw_afa_fwd_entry_ht_key ht_key = {0};
	struct mlxsw_afa_fwd_entry *fwd_entry;

	ht_key.local_port = local_port;
	fwd_entry = rhashtable_lookup_fast(&mlxsw_afa->fwd_entry_ht, &ht_key,
					   mlxsw_afa_fwd_entry_ht_params);
	if (fwd_entry) {
		fwd_entry->ref_count++;
		return fwd_entry;
	}
	return mlxsw_afa_fwd_entry_create(mlxsw_afa, local_port);
}

static void mlxsw_afa_fwd_entry_put(struct mlxsw_afa *mlxsw_afa,
				    struct mlxsw_afa_fwd_entry *fwd_entry)
{
	if (--fwd_entry->ref_count)
		return;
	mlxsw_afa_fwd_entry_destroy(mlxsw_afa, fwd_entry);
}

struct mlxsw_afa_fwd_entry_ref {
	struct mlxsw_afa_resource resource;
	struct mlxsw_afa_fwd_entry *fwd_entry;
};

static void
mlxsw_afa_fwd_entry_ref_destroy(struct mlxsw_afa_block *block,
				struct mlxsw_afa_fwd_entry_ref *fwd_entry_ref)
{
	mlxsw_afa_resource_del(&fwd_entry_ref->resource);
	mlxsw_afa_fwd_entry_put(block->afa, fwd_entry_ref->fwd_entry);
	kfree(fwd_entry_ref);
}

static void
mlxsw_afa_fwd_entry_ref_destructor(struct mlxsw_afa_block *block,
				   struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_fwd_entry_ref *fwd_entry_ref;

	fwd_entry_ref = container_of(resource, struct mlxsw_afa_fwd_entry_ref,
				     resource);
	mlxsw_afa_fwd_entry_ref_destroy(block, fwd_entry_ref);
}

static struct mlxsw_afa_fwd_entry_ref *
mlxsw_afa_fwd_entry_ref_create(struct mlxsw_afa_block *block, u16 local_port)
{
	struct mlxsw_afa_fwd_entry_ref *fwd_entry_ref;
	struct mlxsw_afa_fwd_entry *fwd_entry;
	int err;

	fwd_entry_ref = kzalloc(sizeof(*fwd_entry_ref), GFP_KERNEL);
	if (!fwd_entry_ref)
		return ERR_PTR(-ENOMEM);
	fwd_entry = mlxsw_afa_fwd_entry_get(block->afa, local_port);
	if (IS_ERR(fwd_entry)) {
		err = PTR_ERR(fwd_entry);
		goto err_fwd_entry_get;
	}
	fwd_entry_ref->fwd_entry = fwd_entry;
	fwd_entry_ref->resource.destructor = mlxsw_afa_fwd_entry_ref_destructor;
	mlxsw_afa_resource_add(block, &fwd_entry_ref->resource);
	return fwd_entry_ref;

err_fwd_entry_get:
	kfree(fwd_entry_ref);
	return ERR_PTR(err);
}

struct mlxsw_afa_counter {
	struct mlxsw_afa_resource resource;
	u32 counter_index;
};

static void
mlxsw_afa_counter_destroy(struct mlxsw_afa_block *block,
			  struct mlxsw_afa_counter *counter)
{
	mlxsw_afa_resource_del(&counter->resource);
	block->afa->ops->counter_index_put(block->afa->ops_priv,
					   counter->counter_index);
	kfree(counter);
}

static void
mlxsw_afa_counter_destructor(struct mlxsw_afa_block *block,
			     struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_counter *counter;

	counter = container_of(resource, struct mlxsw_afa_counter, resource);
	mlxsw_afa_counter_destroy(block, counter);
}

static struct mlxsw_afa_counter *
mlxsw_afa_counter_create(struct mlxsw_afa_block *block)
{
	struct mlxsw_afa_counter *counter;
	int err;

	counter = kzalloc(sizeof(*counter), GFP_KERNEL);
	if (!counter)
		return ERR_PTR(-ENOMEM);

	err = block->afa->ops->counter_index_get(block->afa->ops_priv,
						 &counter->counter_index);
	if (err)
		goto err_counter_index_get;
	counter->resource.destructor = mlxsw_afa_counter_destructor;
	mlxsw_afa_resource_add(block, &counter->resource);
	return counter;

err_counter_index_get:
	kfree(counter);
	return ERR_PTR(err);
}

 
#define MLXSW_AFA_COOKIE_INDEX_BITS 20
#define MLXSW_AFA_COOKIE_INDEX_MAX ((1 << MLXSW_AFA_COOKIE_INDEX_BITS) - 1)

static struct mlxsw_afa_cookie *
mlxsw_afa_cookie_create(struct mlxsw_afa *mlxsw_afa,
			const struct flow_action_cookie *fa_cookie)
{
	struct mlxsw_afa_cookie *cookie;
	u32 cookie_index;
	int err;

	cookie = kzalloc(sizeof(*cookie) + fa_cookie->cookie_len, GFP_KERNEL);
	if (!cookie)
		return ERR_PTR(-ENOMEM);
	refcount_set(&cookie->ref_count, 1);
	cookie->fa_cookie = *fa_cookie;
	memcpy(cookie->fa_cookie.cookie, fa_cookie->cookie,
	       fa_cookie->cookie_len);

	err = rhashtable_insert_fast(&mlxsw_afa->cookie_ht, &cookie->ht_node,
				     mlxsw_afa_cookie_ht_params);
	if (err)
		goto err_rhashtable_insert;

	 
	cookie_index = 1;
	err = idr_alloc_u32(&mlxsw_afa->cookie_idr, cookie, &cookie_index,
			    MLXSW_AFA_COOKIE_INDEX_MAX, GFP_KERNEL);
	if (err)
		goto err_idr_alloc;
	cookie->cookie_index = cookie_index;
	return cookie;

err_idr_alloc:
	rhashtable_remove_fast(&mlxsw_afa->cookie_ht, &cookie->ht_node,
			       mlxsw_afa_cookie_ht_params);
err_rhashtable_insert:
	kfree(cookie);
	return ERR_PTR(err);
}

static void mlxsw_afa_cookie_destroy(struct mlxsw_afa *mlxsw_afa,
				     struct mlxsw_afa_cookie *cookie)
{
	idr_remove(&mlxsw_afa->cookie_idr, cookie->cookie_index);
	rhashtable_remove_fast(&mlxsw_afa->cookie_ht, &cookie->ht_node,
			       mlxsw_afa_cookie_ht_params);
	kfree_rcu(cookie, rcu);
}

static struct mlxsw_afa_cookie *
mlxsw_afa_cookie_get(struct mlxsw_afa *mlxsw_afa,
		     const struct flow_action_cookie *fa_cookie)
{
	struct mlxsw_afa_cookie *cookie;

	cookie = rhashtable_lookup_fast(&mlxsw_afa->cookie_ht, fa_cookie,
					mlxsw_afa_cookie_ht_params);
	if (cookie) {
		refcount_inc(&cookie->ref_count);
		return cookie;
	}
	return mlxsw_afa_cookie_create(mlxsw_afa, fa_cookie);
}

static void mlxsw_afa_cookie_put(struct mlxsw_afa *mlxsw_afa,
				 struct mlxsw_afa_cookie *cookie)
{
	if (!refcount_dec_and_test(&cookie->ref_count))
		return;
	mlxsw_afa_cookie_destroy(mlxsw_afa, cookie);
}

 
const struct flow_action_cookie *
mlxsw_afa_cookie_lookup(struct mlxsw_afa *mlxsw_afa, u32 cookie_index)
{
	struct mlxsw_afa_cookie *cookie;

	 
	if (!cookie_index)
		return NULL;
	cookie = idr_find(&mlxsw_afa->cookie_idr, cookie_index);
	if (!cookie)
		return NULL;
	return &cookie->fa_cookie;
}
EXPORT_SYMBOL(mlxsw_afa_cookie_lookup);

struct mlxsw_afa_cookie_ref {
	struct mlxsw_afa_resource resource;
	struct mlxsw_afa_cookie *cookie;
};

static void
mlxsw_afa_cookie_ref_destroy(struct mlxsw_afa_block *block,
			     struct mlxsw_afa_cookie_ref *cookie_ref)
{
	mlxsw_afa_resource_del(&cookie_ref->resource);
	mlxsw_afa_cookie_put(block->afa, cookie_ref->cookie);
	kfree(cookie_ref);
}

static void
mlxsw_afa_cookie_ref_destructor(struct mlxsw_afa_block *block,
				struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_cookie_ref *cookie_ref;

	cookie_ref = container_of(resource, struct mlxsw_afa_cookie_ref,
				  resource);
	mlxsw_afa_cookie_ref_destroy(block, cookie_ref);
}

static struct mlxsw_afa_cookie_ref *
mlxsw_afa_cookie_ref_create(struct mlxsw_afa_block *block,
			    const struct flow_action_cookie *fa_cookie)
{
	struct mlxsw_afa_cookie_ref *cookie_ref;
	struct mlxsw_afa_cookie *cookie;
	int err;

	cookie_ref = kzalloc(sizeof(*cookie_ref), GFP_KERNEL);
	if (!cookie_ref)
		return ERR_PTR(-ENOMEM);
	cookie = mlxsw_afa_cookie_get(block->afa, fa_cookie);
	if (IS_ERR(cookie)) {
		err = PTR_ERR(cookie);
		goto err_cookie_get;
	}
	cookie_ref->cookie = cookie;
	cookie_ref->resource.destructor = mlxsw_afa_cookie_ref_destructor;
	mlxsw_afa_resource_add(block, &cookie_ref->resource);
	return cookie_ref;

err_cookie_get:
	kfree(cookie_ref);
	return ERR_PTR(err);
}

static struct mlxsw_afa_policer *
mlxsw_afa_policer_create(struct mlxsw_afa *mlxsw_afa, u32 fa_index,
			 u64 rate_bytes_ps, u32 burst,
			 struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_policer *policer;
	int err;

	policer = kzalloc(sizeof(*policer), GFP_KERNEL);
	if (!policer)
		return ERR_PTR(-ENOMEM);

	err = mlxsw_afa->ops->policer_add(mlxsw_afa->ops_priv, rate_bytes_ps,
					  burst, &policer->policer_index,
					  extack);
	if (err)
		goto err_policer_add;

	refcount_set(&policer->ref_count, 1);
	policer->fa_index = fa_index;

	err = rhashtable_insert_fast(&mlxsw_afa->policer_ht, &policer->ht_node,
				     mlxsw_afa_policer_ht_params);
	if (err)
		goto err_rhashtable_insert;

	list_add_tail(&policer->list, &mlxsw_afa->policer_list);

	return policer;

err_rhashtable_insert:
	mlxsw_afa->ops->policer_del(mlxsw_afa->ops_priv,
				    policer->policer_index);
err_policer_add:
	kfree(policer);
	return ERR_PTR(err);
}

static void mlxsw_afa_policer_destroy(struct mlxsw_afa *mlxsw_afa,
				      struct mlxsw_afa_policer *policer)
{
	list_del(&policer->list);
	rhashtable_remove_fast(&mlxsw_afa->policer_ht, &policer->ht_node,
			       mlxsw_afa_policer_ht_params);
	mlxsw_afa->ops->policer_del(mlxsw_afa->ops_priv,
				    policer->policer_index);
	kfree(policer);
}

static struct mlxsw_afa_policer *
mlxsw_afa_policer_get(struct mlxsw_afa *mlxsw_afa, u32 fa_index,
		      u64 rate_bytes_ps, u32 burst,
		      struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_policer *policer;

	policer = rhashtable_lookup_fast(&mlxsw_afa->policer_ht, &fa_index,
					 mlxsw_afa_policer_ht_params);
	if (policer) {
		refcount_inc(&policer->ref_count);
		return policer;
	}

	return mlxsw_afa_policer_create(mlxsw_afa, fa_index, rate_bytes_ps,
					burst, extack);
}

static void mlxsw_afa_policer_put(struct mlxsw_afa *mlxsw_afa,
				  struct mlxsw_afa_policer *policer)
{
	if (!refcount_dec_and_test(&policer->ref_count))
		return;
	mlxsw_afa_policer_destroy(mlxsw_afa, policer);
}

struct mlxsw_afa_policer_ref {
	struct mlxsw_afa_resource resource;
	struct mlxsw_afa_policer *policer;
};

static void
mlxsw_afa_policer_ref_destroy(struct mlxsw_afa_block *block,
			      struct mlxsw_afa_policer_ref *policer_ref)
{
	mlxsw_afa_resource_del(&policer_ref->resource);
	mlxsw_afa_policer_put(block->afa, policer_ref->policer);
	kfree(policer_ref);
}

static void
mlxsw_afa_policer_ref_destructor(struct mlxsw_afa_block *block,
				 struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_policer_ref *policer_ref;

	policer_ref = container_of(resource, struct mlxsw_afa_policer_ref,
				   resource);
	mlxsw_afa_policer_ref_destroy(block, policer_ref);
}

static struct mlxsw_afa_policer_ref *
mlxsw_afa_policer_ref_create(struct mlxsw_afa_block *block, u32 fa_index,
			     u64 rate_bytes_ps, u32 burst,
			     struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_policer_ref *policer_ref;
	struct mlxsw_afa_policer *policer;
	int err;

	policer_ref = kzalloc(sizeof(*policer_ref), GFP_KERNEL);
	if (!policer_ref)
		return ERR_PTR(-ENOMEM);

	policer = mlxsw_afa_policer_get(block->afa, fa_index, rate_bytes_ps,
					burst, extack);
	if (IS_ERR(policer)) {
		err = PTR_ERR(policer);
		goto err_policer_get;
	}

	policer_ref->policer = policer;
	policer_ref->resource.destructor = mlxsw_afa_policer_ref_destructor;
	mlxsw_afa_resource_add(block, &policer_ref->resource);

	return policer_ref;

err_policer_get:
	kfree(policer_ref);
	return ERR_PTR(err);
}

#define MLXSW_AFA_ONE_ACTION_LEN 32
#define MLXSW_AFA_PAYLOAD_OFFSET 4

enum mlxsw_afa_action_type {
	MLXSW_AFA_ACTION_TYPE_TRAP,
	MLXSW_AFA_ACTION_TYPE_POLICE,
	MLXSW_AFA_ACTION_TYPE_OTHER,
};

static bool
mlxsw_afa_block_need_split(const struct mlxsw_afa_block *block,
			   enum mlxsw_afa_action_type type)
{
	struct mlxsw_afa_set *cur_set = block->cur_set;

	 
	return (cur_set->has_trap && type == MLXSW_AFA_ACTION_TYPE_POLICE) ||
	       (cur_set->has_police && type == MLXSW_AFA_ACTION_TYPE_TRAP);
}

static char *mlxsw_afa_block_append_action_ext(struct mlxsw_afa_block *block,
					       u8 action_code, u8 action_size,
					       enum mlxsw_afa_action_type type)
{
	char *oneact;
	char *actions;

	if (block->finished)
		return ERR_PTR(-EINVAL);
	if (block->cur_act_index + action_size > block->afa->max_acts_per_set ||
	    mlxsw_afa_block_need_split(block, type)) {
		struct mlxsw_afa_set *set;

		 
		set = mlxsw_afa_set_create(false);
		if (!set)
			return ERR_PTR(-ENOBUFS);
		set->prev = block->cur_set;
		block->cur_act_index = 0;
		block->cur_set->next = set;
		block->cur_set = set;
	}

	switch (type) {
	case MLXSW_AFA_ACTION_TYPE_TRAP:
		block->cur_set->has_trap = true;
		break;
	case MLXSW_AFA_ACTION_TYPE_POLICE:
		block->cur_set->has_police = true;
		break;
	default:
		break;
	}

	actions = block->cur_set->ht_key.enc_actions;
	oneact = actions + block->cur_act_index * MLXSW_AFA_ONE_ACTION_LEN;
	block->cur_act_index += action_size;
	mlxsw_afa_all_action_type_set(oneact, action_code);
	return oneact + MLXSW_AFA_PAYLOAD_OFFSET;
}

static char *mlxsw_afa_block_append_action(struct mlxsw_afa_block *block,
					   u8 action_code, u8 action_size)
{
	return mlxsw_afa_block_append_action_ext(block, action_code,
						 action_size,
						 MLXSW_AFA_ACTION_TYPE_OTHER);
}

 

#define MLXSW_AFA_VLAN_CODE 0x02
#define MLXSW_AFA_VLAN_SIZE 1

enum mlxsw_afa_vlan_vlan_tag_cmd {
	MLXSW_AFA_VLAN_VLAN_TAG_CMD_NOP,
	MLXSW_AFA_VLAN_VLAN_TAG_CMD_PUSH_TAG,
	MLXSW_AFA_VLAN_VLAN_TAG_CMD_POP_TAG,
};

enum mlxsw_afa_vlan_cmd {
	MLXSW_AFA_VLAN_CMD_NOP,
	MLXSW_AFA_VLAN_CMD_SET_OUTER,
	MLXSW_AFA_VLAN_CMD_SET_INNER,
	MLXSW_AFA_VLAN_CMD_COPY_OUTER_TO_INNER,
	MLXSW_AFA_VLAN_CMD_COPY_INNER_TO_OUTER,
	MLXSW_AFA_VLAN_CMD_SWAP,
};

 
MLXSW_ITEM32(afa, vlan, vlan_tag_cmd, 0x00, 29, 3);

 
MLXSW_ITEM32(afa, vlan, vid_cmd, 0x04, 29, 3);

 
MLXSW_ITEM32(afa, vlan, vid, 0x04, 0, 12);

 
MLXSW_ITEM32(afa, vlan, ethertype_cmd, 0x08, 29, 3);

 
MLXSW_ITEM32(afa, vlan, ethertype, 0x08, 24, 3);

 
MLXSW_ITEM32(afa, vlan, pcp_cmd, 0x08, 13, 3);

 
MLXSW_ITEM32(afa, vlan, pcp, 0x08, 8, 3);

static inline void
mlxsw_afa_vlan_pack(char *payload,
		    enum mlxsw_afa_vlan_vlan_tag_cmd vlan_tag_cmd,
		    enum mlxsw_afa_vlan_cmd vid_cmd, u16 vid,
		    enum mlxsw_afa_vlan_cmd pcp_cmd, u8 pcp,
		    enum mlxsw_afa_vlan_cmd ethertype_cmd, u8 ethertype)
{
	mlxsw_afa_vlan_vlan_tag_cmd_set(payload, vlan_tag_cmd);
	mlxsw_afa_vlan_vid_cmd_set(payload, vid_cmd);
	mlxsw_afa_vlan_vid_set(payload, vid);
	mlxsw_afa_vlan_pcp_cmd_set(payload, pcp_cmd);
	mlxsw_afa_vlan_pcp_set(payload, pcp);
	mlxsw_afa_vlan_ethertype_cmd_set(payload, ethertype_cmd);
	mlxsw_afa_vlan_ethertype_set(payload, ethertype);
}

int mlxsw_afa_block_append_vlan_modify(struct mlxsw_afa_block *block,
				       u16 vid, u8 pcp, u8 et,
				       struct netlink_ext_ack *extack)
{
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_VLAN_CODE,
						  MLXSW_AFA_VLAN_SIZE);

	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append vlan_modify action");
		return PTR_ERR(act);
	}
	mlxsw_afa_vlan_pack(act, MLXSW_AFA_VLAN_VLAN_TAG_CMD_NOP,
			    MLXSW_AFA_VLAN_CMD_SET_OUTER, vid,
			    MLXSW_AFA_VLAN_CMD_SET_OUTER, pcp,
			    MLXSW_AFA_VLAN_CMD_SET_OUTER, et);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_vlan_modify);

 

#define MLXSW_AFA_TRAP_CODE 0x03
#define MLXSW_AFA_TRAP_SIZE 1

#define MLXSW_AFA_TRAPWU_CODE 0x04
#define MLXSW_AFA_TRAPWU_SIZE 2

enum mlxsw_afa_trap_trap_action {
	MLXSW_AFA_TRAP_TRAP_ACTION_NOP = 0,
	MLXSW_AFA_TRAP_TRAP_ACTION_TRAP = 2,
};

 
MLXSW_ITEM32(afa, trap, trap_action, 0x00, 24, 4);

enum mlxsw_afa_trap_forward_action {
	MLXSW_AFA_TRAP_FORWARD_ACTION_FORWARD = 1,
	MLXSW_AFA_TRAP_FORWARD_ACTION_DISCARD = 3,
};

 
MLXSW_ITEM32(afa, trap, forward_action, 0x00, 0, 4);

 
MLXSW_ITEM32(afa, trap, trap_id, 0x04, 0, 9);

 
MLXSW_ITEM32(afa, trap, mirror_agent, 0x08, 29, 3);

 
MLXSW_ITEM32(afa, trap, mirror_enable, 0x08, 24, 1);

 
MLXSW_ITEM32(afa, trap, user_def_val, 0x0C, 0, 20);

static inline void
mlxsw_afa_trap_pack(char *payload,
		    enum mlxsw_afa_trap_trap_action trap_action,
		    enum mlxsw_afa_trap_forward_action forward_action,
		    u16 trap_id)
{
	mlxsw_afa_trap_trap_action_set(payload, trap_action);
	mlxsw_afa_trap_forward_action_set(payload, forward_action);
	mlxsw_afa_trap_trap_id_set(payload, trap_id);
}

static inline void
mlxsw_afa_trapwu_pack(char *payload,
		      enum mlxsw_afa_trap_trap_action trap_action,
		      enum mlxsw_afa_trap_forward_action forward_action,
		      u16 trap_id, u32 user_def_val)
{
	mlxsw_afa_trap_pack(payload, trap_action, forward_action, trap_id);
	mlxsw_afa_trap_user_def_val_set(payload, user_def_val);
}

static inline void
mlxsw_afa_trap_mirror_pack(char *payload, bool mirror_enable,
			   u8 mirror_agent)
{
	mlxsw_afa_trap_mirror_enable_set(payload, mirror_enable);
	mlxsw_afa_trap_mirror_agent_set(payload, mirror_agent);
}

static char *mlxsw_afa_block_append_action_trap(struct mlxsw_afa_block *block,
						u8 action_code, u8 action_size)
{
	return mlxsw_afa_block_append_action_ext(block, action_code,
						 action_size,
						 MLXSW_AFA_ACTION_TYPE_TRAP);
}

static int mlxsw_afa_block_append_drop_plain(struct mlxsw_afa_block *block,
					     bool ingress)
{
	char *act = mlxsw_afa_block_append_action_trap(block,
						       MLXSW_AFA_TRAP_CODE,
						       MLXSW_AFA_TRAP_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_trap_pack(act, MLXSW_AFA_TRAP_TRAP_ACTION_TRAP,
			    MLXSW_AFA_TRAP_FORWARD_ACTION_DISCARD,
			    ingress ? MLXSW_TRAP_ID_DISCARD_INGRESS_ACL :
				      MLXSW_TRAP_ID_DISCARD_EGRESS_ACL);
	return 0;
}

static int
mlxsw_afa_block_append_drop_with_cookie(struct mlxsw_afa_block *block,
					bool ingress,
					const struct flow_action_cookie *fa_cookie,
					struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_cookie_ref *cookie_ref;
	u32 cookie_index;
	char *act;
	int err;

	cookie_ref = mlxsw_afa_cookie_ref_create(block, fa_cookie);
	if (IS_ERR(cookie_ref)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot create cookie for drop action");
		return PTR_ERR(cookie_ref);
	}
	cookie_index = cookie_ref->cookie->cookie_index;

	act = mlxsw_afa_block_append_action_trap(block, MLXSW_AFA_TRAPWU_CODE,
						 MLXSW_AFA_TRAPWU_SIZE);
	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append drop with cookie action");
		err = PTR_ERR(act);
		goto err_append_action;
	}
	mlxsw_afa_trapwu_pack(act, MLXSW_AFA_TRAP_TRAP_ACTION_TRAP,
			      MLXSW_AFA_TRAP_FORWARD_ACTION_DISCARD,
			      ingress ? MLXSW_TRAP_ID_DISCARD_INGRESS_ACL :
					MLXSW_TRAP_ID_DISCARD_EGRESS_ACL,
			      cookie_index);
	return 0;

err_append_action:
	mlxsw_afa_cookie_ref_destroy(block, cookie_ref);
	return err;
}

int mlxsw_afa_block_append_drop(struct mlxsw_afa_block *block, bool ingress,
				const struct flow_action_cookie *fa_cookie,
				struct netlink_ext_ack *extack)
{
	return fa_cookie ?
	       mlxsw_afa_block_append_drop_with_cookie(block, ingress,
						       fa_cookie, extack) :
	       mlxsw_afa_block_append_drop_plain(block, ingress);
}
EXPORT_SYMBOL(mlxsw_afa_block_append_drop);

int mlxsw_afa_block_append_trap(struct mlxsw_afa_block *block, u16 trap_id)
{
	char *act = mlxsw_afa_block_append_action_trap(block,
						       MLXSW_AFA_TRAP_CODE,
						       MLXSW_AFA_TRAP_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_trap_pack(act, MLXSW_AFA_TRAP_TRAP_ACTION_TRAP,
			    MLXSW_AFA_TRAP_FORWARD_ACTION_DISCARD, trap_id);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_trap);

int mlxsw_afa_block_append_trap_and_forward(struct mlxsw_afa_block *block,
					    u16 trap_id)
{
	char *act = mlxsw_afa_block_append_action_trap(block,
						       MLXSW_AFA_TRAP_CODE,
						       MLXSW_AFA_TRAP_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_trap_pack(act, MLXSW_AFA_TRAP_TRAP_ACTION_TRAP,
			    MLXSW_AFA_TRAP_FORWARD_ACTION_FORWARD, trap_id);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_trap_and_forward);

struct mlxsw_afa_mirror {
	struct mlxsw_afa_resource resource;
	int span_id;
	u16 local_in_port;
	bool ingress;
};

static void
mlxsw_afa_mirror_destroy(struct mlxsw_afa_block *block,
			 struct mlxsw_afa_mirror *mirror)
{
	mlxsw_afa_resource_del(&mirror->resource);
	block->afa->ops->mirror_del(block->afa->ops_priv,
				    mirror->local_in_port,
				    mirror->span_id,
				    mirror->ingress);
	kfree(mirror);
}

static void
mlxsw_afa_mirror_destructor(struct mlxsw_afa_block *block,
			    struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_mirror *mirror;

	mirror = container_of(resource, struct mlxsw_afa_mirror, resource);
	mlxsw_afa_mirror_destroy(block, mirror);
}

static struct mlxsw_afa_mirror *
mlxsw_afa_mirror_create(struct mlxsw_afa_block *block, u16 local_in_port,
			const struct net_device *out_dev, bool ingress)
{
	struct mlxsw_afa_mirror *mirror;
	int err;

	mirror = kzalloc(sizeof(*mirror), GFP_KERNEL);
	if (!mirror)
		return ERR_PTR(-ENOMEM);

	err = block->afa->ops->mirror_add(block->afa->ops_priv,
					  local_in_port, out_dev,
					  ingress, &mirror->span_id);
	if (err)
		goto err_mirror_add;

	mirror->ingress = ingress;
	mirror->local_in_port = local_in_port;
	mirror->resource.destructor = mlxsw_afa_mirror_destructor;
	mlxsw_afa_resource_add(block, &mirror->resource);
	return mirror;

err_mirror_add:
	kfree(mirror);
	return ERR_PTR(err);
}

static int
mlxsw_afa_block_append_allocated_mirror(struct mlxsw_afa_block *block,
					u8 mirror_agent)
{
	char *act = mlxsw_afa_block_append_action_trap(block,
						       MLXSW_AFA_TRAP_CODE,
						       MLXSW_AFA_TRAP_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_trap_pack(act, MLXSW_AFA_TRAP_TRAP_ACTION_NOP,
			    MLXSW_AFA_TRAP_FORWARD_ACTION_FORWARD, 0);
	mlxsw_afa_trap_mirror_pack(act, true, mirror_agent);
	return 0;
}

int
mlxsw_afa_block_append_mirror(struct mlxsw_afa_block *block, u16 local_in_port,
			      const struct net_device *out_dev, bool ingress,
			      struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_mirror *mirror;
	int err;

	mirror = mlxsw_afa_mirror_create(block, local_in_port, out_dev,
					 ingress);
	if (IS_ERR(mirror)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot create mirror action");
		return PTR_ERR(mirror);
	}
	err = mlxsw_afa_block_append_allocated_mirror(block, mirror->span_id);
	if (err) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append mirror action");
		goto err_append_allocated_mirror;
	}

	return 0;

err_append_allocated_mirror:
	mlxsw_afa_mirror_destroy(block, mirror);
	return err;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_mirror);

 

#define MLXSW_AFA_QOS_CODE 0x06
#define MLXSW_AFA_QOS_SIZE 1

enum mlxsw_afa_qos_ecn_cmd {
	 
	MLXSW_AFA_QOS_ECN_CMD_NOP,
	 
	MLXSW_AFA_QOS_ECN_CMD_SET,
};

 
MLXSW_ITEM32(afa, qos, ecn_cmd, 0x04, 29, 3);

 
MLXSW_ITEM32(afa, qos, ecn, 0x04, 24, 2);

enum mlxsw_afa_qos_dscp_cmd {
	 
	MLXSW_AFA_QOS_DSCP_CMD_NOP,
	 
	MLXSW_AFA_QOS_DSCP_CMD_SET_3LSB,
	 
	MLXSW_AFA_QOS_DSCP_CMD_SET_3MSB,
	 
	MLXSW_AFA_QOS_DSCP_CMD_SET_ALL,
};

 
MLXSW_ITEM32(afa, qos, dscp_cmd, 0x04, 14, 2);

 
MLXSW_ITEM32(afa, qos, dscp, 0x04, 0, 6);

enum mlxsw_afa_qos_switch_prio_cmd {
	 
	MLXSW_AFA_QOS_SWITCH_PRIO_CMD_NOP,
	 
	MLXSW_AFA_QOS_SWITCH_PRIO_CMD_SET,
};

 
MLXSW_ITEM32(afa, qos, switch_prio_cmd, 0x08, 14, 2);

 
MLXSW_ITEM32(afa, qos, switch_prio, 0x08, 0, 4);

enum mlxsw_afa_qos_dscp_rw {
	MLXSW_AFA_QOS_DSCP_RW_PRESERVE,
	MLXSW_AFA_QOS_DSCP_RW_SET,
	MLXSW_AFA_QOS_DSCP_RW_CLEAR,
};

 
MLXSW_ITEM32(afa, qos, dscp_rw, 0x0C, 30, 2);

static inline void
mlxsw_afa_qos_ecn_pack(char *payload,
		       enum mlxsw_afa_qos_ecn_cmd ecn_cmd, u8 ecn)
{
	mlxsw_afa_qos_ecn_cmd_set(payload, ecn_cmd);
	mlxsw_afa_qos_ecn_set(payload, ecn);
}

static inline void
mlxsw_afa_qos_dscp_pack(char *payload,
			enum mlxsw_afa_qos_dscp_cmd dscp_cmd, u8 dscp)
{
	mlxsw_afa_qos_dscp_cmd_set(payload, dscp_cmd);
	mlxsw_afa_qos_dscp_set(payload, dscp);
}

static inline void
mlxsw_afa_qos_switch_prio_pack(char *payload,
			       enum mlxsw_afa_qos_switch_prio_cmd prio_cmd,
			       u8 prio)
{
	mlxsw_afa_qos_switch_prio_cmd_set(payload, prio_cmd);
	mlxsw_afa_qos_switch_prio_set(payload, prio);
}

static int __mlxsw_afa_block_append_qos_dsfield(struct mlxsw_afa_block *block,
						bool set_dscp, u8 dscp,
						bool set_ecn, u8 ecn,
						struct netlink_ext_ack *extack)
{
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_QOS_CODE,
						  MLXSW_AFA_QOS_SIZE);

	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append QOS action");
		return PTR_ERR(act);
	}

	if (set_ecn)
		mlxsw_afa_qos_ecn_pack(act, MLXSW_AFA_QOS_ECN_CMD_SET, ecn);
	if (set_dscp) {
		mlxsw_afa_qos_dscp_pack(act, MLXSW_AFA_QOS_DSCP_CMD_SET_ALL,
					dscp);
		mlxsw_afa_qos_dscp_rw_set(act, MLXSW_AFA_QOS_DSCP_RW_CLEAR);
	}

	return 0;
}

int mlxsw_afa_block_append_qos_dsfield(struct mlxsw_afa_block *block,
				       u8 dsfield,
				       struct netlink_ext_ack *extack)
{
	return __mlxsw_afa_block_append_qos_dsfield(block,
						    true, dsfield >> 2,
						    true, dsfield & 0x03,
						    extack);
}
EXPORT_SYMBOL(mlxsw_afa_block_append_qos_dsfield);

int mlxsw_afa_block_append_qos_dscp(struct mlxsw_afa_block *block,
				    u8 dscp, struct netlink_ext_ack *extack)
{
	return __mlxsw_afa_block_append_qos_dsfield(block,
						    true, dscp,
						    false, 0,
						    extack);
}
EXPORT_SYMBOL(mlxsw_afa_block_append_qos_dscp);

int mlxsw_afa_block_append_qos_ecn(struct mlxsw_afa_block *block,
				   u8 ecn, struct netlink_ext_ack *extack)
{
	return __mlxsw_afa_block_append_qos_dsfield(block,
						    false, 0,
						    true, ecn,
						    extack);
}
EXPORT_SYMBOL(mlxsw_afa_block_append_qos_ecn);

int mlxsw_afa_block_append_qos_switch_prio(struct mlxsw_afa_block *block,
					   u8 prio,
					   struct netlink_ext_ack *extack)
{
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_QOS_CODE,
						  MLXSW_AFA_QOS_SIZE);

	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append QOS action");
		return PTR_ERR(act);
	}
	mlxsw_afa_qos_switch_prio_pack(act, MLXSW_AFA_QOS_SWITCH_PRIO_CMD_SET,
				       prio);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_qos_switch_prio);

 

#define MLXSW_AFA_FORWARD_CODE 0x07
#define MLXSW_AFA_FORWARD_SIZE 1

enum mlxsw_afa_forward_type {
	 
	MLXSW_AFA_FORWARD_TYPE_PBS,
	 
	MLXSW_AFA_FORWARD_TYPE_OUTPUT,
};

 
MLXSW_ITEM32(afa, forward, type, 0x00, 24, 2);

 
MLXSW_ITEM32(afa, forward, pbs_ptr, 0x08, 0, 24);

 
MLXSW_ITEM32(afa, forward, in_port, 0x0C, 0, 1);

static inline void
mlxsw_afa_forward_pack(char *payload, enum mlxsw_afa_forward_type type,
		       u32 pbs_ptr, bool in_port)
{
	mlxsw_afa_forward_type_set(payload, type);
	mlxsw_afa_forward_pbs_ptr_set(payload, pbs_ptr);
	mlxsw_afa_forward_in_port_set(payload, in_port);
}

int mlxsw_afa_block_append_fwd(struct mlxsw_afa_block *block,
			       u16 local_port, bool in_port,
			       struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_fwd_entry_ref *fwd_entry_ref;
	u32 kvdl_index;
	char *act;
	int err;

	if (in_port) {
		NL_SET_ERR_MSG_MOD(extack, "Forwarding to ingress port is not supported");
		return -EOPNOTSUPP;
	}
	fwd_entry_ref = mlxsw_afa_fwd_entry_ref_create(block, local_port);
	if (IS_ERR(fwd_entry_ref)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot create forward action");
		return PTR_ERR(fwd_entry_ref);
	}
	kvdl_index = fwd_entry_ref->fwd_entry->kvdl_index;

	act = mlxsw_afa_block_append_action(block, MLXSW_AFA_FORWARD_CODE,
					    MLXSW_AFA_FORWARD_SIZE);
	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append forward action");
		err = PTR_ERR(act);
		goto err_append_action;
	}
	mlxsw_afa_forward_pack(act, MLXSW_AFA_FORWARD_TYPE_PBS,
			       kvdl_index, in_port);
	return 0;

err_append_action:
	mlxsw_afa_fwd_entry_ref_destroy(block, fwd_entry_ref);
	return err;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_fwd);

 

#define MLXSW_AFA_POLCNT_CODE 0x08
#define MLXSW_AFA_POLCNT_SIZE 1

enum {
	MLXSW_AFA_POLCNT_COUNTER,
	MLXSW_AFA_POLCNT_POLICER,
};

 
MLXSW_ITEM32(afa, polcnt, c_p, 0x00, 31, 1);

enum mlxsw_afa_polcnt_counter_set_type {
	 
	MLXSW_AFA_POLCNT_COUNTER_SET_TYPE_NO_COUNT = 0x00,
	 
	MLXSW_AFA_POLCNT_COUNTER_SET_TYPE_PACKETS_BYTES = 0x03,
	 
	MLXSW_AFA_POLCNT_COUNTER_SET_TYPE_PACKETS = 0x05,
};

 
MLXSW_ITEM32(afa, polcnt, counter_set_type, 0x04, 24, 8);

 
MLXSW_ITEM32(afa, polcnt, counter_index, 0x04, 0, 24);

 
MLXSW_ITEM32(afa, polcnt, pid, 0x08, 0, 14);

static inline void
mlxsw_afa_polcnt_pack(char *payload,
		      enum mlxsw_afa_polcnt_counter_set_type set_type,
		      u32 counter_index)
{
	mlxsw_afa_polcnt_c_p_set(payload, MLXSW_AFA_POLCNT_COUNTER);
	mlxsw_afa_polcnt_counter_set_type_set(payload, set_type);
	mlxsw_afa_polcnt_counter_index_set(payload, counter_index);
}

static void mlxsw_afa_polcnt_policer_pack(char *payload, u16 policer_index)
{
	mlxsw_afa_polcnt_c_p_set(payload, MLXSW_AFA_POLCNT_POLICER);
	mlxsw_afa_polcnt_pid_set(payload, policer_index);
}

int mlxsw_afa_block_append_allocated_counter(struct mlxsw_afa_block *block,
					     u32 counter_index)
{
	char *act = mlxsw_afa_block_append_action(block, MLXSW_AFA_POLCNT_CODE,
						  MLXSW_AFA_POLCNT_SIZE);
	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_polcnt_pack(act, MLXSW_AFA_POLCNT_COUNTER_SET_TYPE_PACKETS_BYTES,
			      counter_index);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_allocated_counter);

int mlxsw_afa_block_append_counter(struct mlxsw_afa_block *block,
				   u32 *p_counter_index,
				   struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_counter *counter;
	u32 counter_index;
	int err;

	counter = mlxsw_afa_counter_create(block);
	if (IS_ERR(counter)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot create count action");
		return PTR_ERR(counter);
	}
	counter_index = counter->counter_index;

	err = mlxsw_afa_block_append_allocated_counter(block, counter_index);
	if (err) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append count action");
		goto err_append_allocated_counter;
	}
	if (p_counter_index)
		*p_counter_index = counter_index;
	return 0;

err_append_allocated_counter:
	mlxsw_afa_counter_destroy(block, counter);
	return err;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_counter);

int mlxsw_afa_block_append_police(struct mlxsw_afa_block *block,
				  u32 fa_index, u64 rate_bytes_ps, u32 burst,
				  u16 *p_policer_index,
				  struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_policer_ref *policer_ref;
	char *act;
	int err;

	policer_ref = mlxsw_afa_policer_ref_create(block, fa_index,
						   rate_bytes_ps,
						   burst, extack);
	if (IS_ERR(policer_ref))
		return PTR_ERR(policer_ref);
	*p_policer_index = policer_ref->policer->policer_index;

	act = mlxsw_afa_block_append_action_ext(block, MLXSW_AFA_POLCNT_CODE,
						MLXSW_AFA_POLCNT_SIZE,
						MLXSW_AFA_ACTION_TYPE_POLICE);
	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append police action");
		err = PTR_ERR(act);
		goto err_append_action;
	}
	mlxsw_afa_polcnt_policer_pack(act, *p_policer_index);

	return 0;

err_append_action:
	mlxsw_afa_policer_ref_destroy(block, policer_ref);
	return err;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_police);

 

#define MLXSW_AFA_VIRFWD_CODE 0x0E
#define MLXSW_AFA_VIRFWD_SIZE 1

enum mlxsw_afa_virfwd_fid_cmd {
	 
	MLXSW_AFA_VIRFWD_FID_CMD_NOOP,
	 
	MLXSW_AFA_VIRFWD_FID_CMD_SET,
};

 
MLXSW_ITEM32(afa, virfwd, fid_cmd, 0x08, 29, 3);

 
MLXSW_ITEM32(afa, virfwd, fid, 0x08, 0, 16);

static inline void mlxsw_afa_virfwd_pack(char *payload,
					 enum mlxsw_afa_virfwd_fid_cmd fid_cmd,
					 u16 fid)
{
	mlxsw_afa_virfwd_fid_cmd_set(payload, fid_cmd);
	mlxsw_afa_virfwd_fid_set(payload, fid);
}

int mlxsw_afa_block_append_fid_set(struct mlxsw_afa_block *block, u16 fid,
				   struct netlink_ext_ack *extack)
{
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_VIRFWD_CODE,
						  MLXSW_AFA_VIRFWD_SIZE);
	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append fid_set action");
		return PTR_ERR(act);
	}
	mlxsw_afa_virfwd_pack(act, MLXSW_AFA_VIRFWD_FID_CMD_SET, fid);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_fid_set);

 

#define MLXSW_AFA_IGNORE_CODE 0x0F
#define MLXSW_AFA_IGNORE_SIZE 1

 
MLXSW_ITEM32(afa, ignore, disable_learning, 0x00, 29, 1);

 
MLXSW_ITEM32(afa, ignore, disable_security, 0x00, 28, 1);

static void mlxsw_afa_ignore_pack(char *payload, bool disable_learning,
				  bool disable_security)
{
	mlxsw_afa_ignore_disable_learning_set(payload, disable_learning);
	mlxsw_afa_ignore_disable_security_set(payload, disable_security);
}

int mlxsw_afa_block_append_ignore(struct mlxsw_afa_block *block,
				  bool disable_learning, bool disable_security)
{
	char *act = mlxsw_afa_block_append_action(block, MLXSW_AFA_IGNORE_CODE,
						  MLXSW_AFA_IGNORE_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_ignore_pack(act, disable_learning, disable_security);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_ignore);

 

#define MLXSW_AFA_MCROUTER_CODE 0x10
#define MLXSW_AFA_MCROUTER_SIZE 2

enum mlxsw_afa_mcrouter_rpf_action {
	MLXSW_AFA_MCROUTER_RPF_ACTION_NOP,
	MLXSW_AFA_MCROUTER_RPF_ACTION_TRAP,
	MLXSW_AFA_MCROUTER_RPF_ACTION_DISCARD_ERROR,
};

 
MLXSW_ITEM32(afa, mcrouter, rpf_action, 0x00, 28, 3);

 
MLXSW_ITEM32(afa, mcrouter, expected_irif, 0x00, 0, 16);

 
MLXSW_ITEM32(afa, mcrouter, min_mtu, 0x08, 0, 16);

enum mlxsw_afa_mrouter_vrmid {
	MLXSW_AFA_MCROUTER_VRMID_INVALID,
	MLXSW_AFA_MCROUTER_VRMID_VALID
};

 
MLXSW_ITEM32(afa, mcrouter, vrmid, 0x0C, 31, 1);

 
MLXSW_ITEM32(afa, mcrouter, rigr_rmid_index, 0x0C, 0, 24);

static inline void
mlxsw_afa_mcrouter_pack(char *payload,
			enum mlxsw_afa_mcrouter_rpf_action rpf_action,
			u16 expected_irif, u16 min_mtu,
			enum mlxsw_afa_mrouter_vrmid vrmid, u32 rigr_rmid_index)

{
	mlxsw_afa_mcrouter_rpf_action_set(payload, rpf_action);
	mlxsw_afa_mcrouter_expected_irif_set(payload, expected_irif);
	mlxsw_afa_mcrouter_min_mtu_set(payload, min_mtu);
	mlxsw_afa_mcrouter_vrmid_set(payload, vrmid);
	mlxsw_afa_mcrouter_rigr_rmid_index_set(payload, rigr_rmid_index);
}

int mlxsw_afa_block_append_mcrouter(struct mlxsw_afa_block *block,
				    u16 expected_irif, u16 min_mtu,
				    bool rmid_valid, u32 kvdl_index)
{
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_MCROUTER_CODE,
						  MLXSW_AFA_MCROUTER_SIZE);
	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_mcrouter_pack(act, MLXSW_AFA_MCROUTER_RPF_ACTION_TRAP,
				expected_irif, min_mtu, rmid_valid, kvdl_index);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_mcrouter);

 

#define MLXSW_AFA_IP_CODE 0x11
#define MLXSW_AFA_IP_SIZE 2

enum mlxsw_afa_ip_s_d {
	 
	MLXSW_AFA_IP_S_D_DIP,
	 
	MLXSW_AFA_IP_S_D_SIP,
};

 
MLXSW_ITEM32(afa, ip, s_d, 0x00, 31, 1);

enum mlxsw_afa_ip_m_l {
	 
	MLXSW_AFA_IP_M_L_LSB,
	 
	MLXSW_AFA_IP_M_L_MSB,
};

 
MLXSW_ITEM32(afa, ip, m_l, 0x00, 30, 1);

 
MLXSW_ITEM32(afa, ip, ip_63_32, 0x08, 0, 32);

 
MLXSW_ITEM32(afa, ip, ip_31_0, 0x0C, 0, 32);

static void mlxsw_afa_ip_pack(char *payload, enum mlxsw_afa_ip_s_d s_d,
			      enum mlxsw_afa_ip_m_l m_l, u32 ip_31_0,
			      u32 ip_63_32)
{
	mlxsw_afa_ip_s_d_set(payload, s_d);
	mlxsw_afa_ip_m_l_set(payload, m_l);
	mlxsw_afa_ip_ip_31_0_set(payload, ip_31_0);
	mlxsw_afa_ip_ip_63_32_set(payload, ip_63_32);
}

int mlxsw_afa_block_append_ip(struct mlxsw_afa_block *block, bool is_dip,
			      bool is_lsb, u32 val_31_0, u32 val_63_32,
			      struct netlink_ext_ack *extack)
{
	enum mlxsw_afa_ip_s_d s_d = is_dip ? MLXSW_AFA_IP_S_D_DIP :
					     MLXSW_AFA_IP_S_D_SIP;
	enum mlxsw_afa_ip_m_l m_l = is_lsb ? MLXSW_AFA_IP_M_L_LSB :
					     MLXSW_AFA_IP_M_L_MSB;
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_IP_CODE,
						  MLXSW_AFA_IP_SIZE);

	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append IP action");
		return PTR_ERR(act);
	}

	mlxsw_afa_ip_pack(act, s_d, m_l, val_31_0, val_63_32);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_ip);

 

#define MLXSW_AFA_L4PORT_CODE 0x12
#define MLXSW_AFA_L4PORT_SIZE 1

enum mlxsw_afa_l4port_s_d {
	 
	MLXSW_AFA_L4PORT_S_D_SRC,
	 
	MLXSW_AFA_L4PORT_S_D_DST,
};

 
MLXSW_ITEM32(afa, l4port, s_d, 0x00, 31, 1);

 
MLXSW_ITEM32(afa, l4port, l4_port, 0x08, 0, 16);

static void mlxsw_afa_l4port_pack(char *payload, enum mlxsw_afa_l4port_s_d s_d, u16 l4_port)
{
	mlxsw_afa_l4port_s_d_set(payload, s_d);
	mlxsw_afa_l4port_l4_port_set(payload, l4_port);
}

int mlxsw_afa_block_append_l4port(struct mlxsw_afa_block *block, bool is_dport, u16 l4_port,
				  struct netlink_ext_ack *extack)
{
	enum mlxsw_afa_l4port_s_d s_d = is_dport ? MLXSW_AFA_L4PORT_S_D_DST :
						   MLXSW_AFA_L4PORT_S_D_SRC;
	char *act = mlxsw_afa_block_append_action(block,
						  MLXSW_AFA_L4PORT_CODE,
						  MLXSW_AFA_L4PORT_SIZE);

	if (IS_ERR(act)) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append L4_PORT action");
		return PTR_ERR(act);
	}

	mlxsw_afa_l4port_pack(act, s_d, l4_port);
	return 0;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_l4port);

 

#define MLXSW_AFA_SAMPLER_CODE 0x13
#define MLXSW_AFA_SAMPLER_SIZE 1

 
MLXSW_ITEM32(afa, sampler, mirror_agent, 0x04, 0, 3);

#define MLXSW_AFA_SAMPLER_RATE_MAX (BIT(24) - 1)

 
MLXSW_ITEM32(afa, sampler, mirror_probability_rate, 0x08, 0, 24);

static void mlxsw_afa_sampler_pack(char *payload, u8 mirror_agent, u32 rate)
{
	mlxsw_afa_sampler_mirror_agent_set(payload, mirror_agent);
	mlxsw_afa_sampler_mirror_probability_rate_set(payload, rate);
}

struct mlxsw_afa_sampler {
	struct mlxsw_afa_resource resource;
	int span_id;
	u16 local_port;
	bool ingress;
};

static void mlxsw_afa_sampler_destroy(struct mlxsw_afa_block *block,
				      struct mlxsw_afa_sampler *sampler)
{
	mlxsw_afa_resource_del(&sampler->resource);
	block->afa->ops->sampler_del(block->afa->ops_priv, sampler->local_port,
				     sampler->span_id, sampler->ingress);
	kfree(sampler);
}

static void mlxsw_afa_sampler_destructor(struct mlxsw_afa_block *block,
					 struct mlxsw_afa_resource *resource)
{
	struct mlxsw_afa_sampler *sampler;

	sampler = container_of(resource, struct mlxsw_afa_sampler, resource);
	mlxsw_afa_sampler_destroy(block, sampler);
}

static struct mlxsw_afa_sampler *
mlxsw_afa_sampler_create(struct mlxsw_afa_block *block, u16 local_port,
			 struct psample_group *psample_group, u32 rate,
			 u32 trunc_size, bool truncate, bool ingress,
			 struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_sampler *sampler;
	int err;

	sampler = kzalloc(sizeof(*sampler), GFP_KERNEL);
	if (!sampler)
		return ERR_PTR(-ENOMEM);

	err = block->afa->ops->sampler_add(block->afa->ops_priv, local_port,
					   psample_group, rate, trunc_size,
					   truncate, ingress, &sampler->span_id,
					   extack);
	if (err)
		goto err_sampler_add;

	sampler->ingress = ingress;
	sampler->local_port = local_port;
	sampler->resource.destructor = mlxsw_afa_sampler_destructor;
	mlxsw_afa_resource_add(block, &sampler->resource);
	return sampler;

err_sampler_add:
	kfree(sampler);
	return ERR_PTR(err);
}

static int
mlxsw_afa_block_append_allocated_sampler(struct mlxsw_afa_block *block,
					 u8 mirror_agent, u32 rate)
{
	char *act = mlxsw_afa_block_append_action(block, MLXSW_AFA_SAMPLER_CODE,
						  MLXSW_AFA_SAMPLER_SIZE);

	if (IS_ERR(act))
		return PTR_ERR(act);
	mlxsw_afa_sampler_pack(act, mirror_agent, rate);
	return 0;
}

int mlxsw_afa_block_append_sampler(struct mlxsw_afa_block *block, u16 local_port,
				   struct psample_group *psample_group,
				   u32 rate, u32 trunc_size, bool truncate,
				   bool ingress,
				   struct netlink_ext_ack *extack)
{
	struct mlxsw_afa_sampler *sampler;
	int err;

	if (rate > MLXSW_AFA_SAMPLER_RATE_MAX) {
		NL_SET_ERR_MSG_MOD(extack, "Sampling rate is too high");
		return -EINVAL;
	}

	sampler = mlxsw_afa_sampler_create(block, local_port, psample_group,
					   rate, trunc_size, truncate, ingress,
					   extack);
	if (IS_ERR(sampler))
		return PTR_ERR(sampler);

	err = mlxsw_afa_block_append_allocated_sampler(block, sampler->span_id,
						       rate);
	if (err) {
		NL_SET_ERR_MSG_MOD(extack, "Cannot append sampler action");
		goto err_append_allocated_sampler;
	}

	return 0;

err_append_allocated_sampler:
	mlxsw_afa_sampler_destroy(block, sampler);
	return err;
}
EXPORT_SYMBOL(mlxsw_afa_block_append_sampler);
