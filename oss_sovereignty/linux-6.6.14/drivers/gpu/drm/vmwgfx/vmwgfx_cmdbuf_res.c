
 

#include "vmwgfx_drv.h"
#include "vmwgfx_resource_priv.h"

#include <linux/hashtable.h>

#define VMW_CMDBUF_RES_MAN_HT_ORDER 12

 
struct vmw_cmdbuf_res {
	struct vmw_resource *res;
	struct vmwgfx_hash_item hash;
	struct list_head head;
	enum vmw_cmdbuf_res_state state;
	struct vmw_cmdbuf_res_manager *man;
};

 
struct vmw_cmdbuf_res_manager {
	DECLARE_HASHTABLE(resources, VMW_CMDBUF_RES_MAN_HT_ORDER);
	struct list_head list;
	struct vmw_private *dev_priv;
};


 
struct vmw_resource *
vmw_cmdbuf_res_lookup(struct vmw_cmdbuf_res_manager *man,
		      enum vmw_cmdbuf_res_type res_type,
		      u32 user_key)
{
	struct vmwgfx_hash_item *hash;
	unsigned long key = user_key | (res_type << 24);

	hash_for_each_possible_rcu(man->resources, hash, head, key) {
		if (hash->key == key)
			return hlist_entry(hash, struct vmw_cmdbuf_res, hash)->res;
	}
	return ERR_PTR(-EINVAL);
}

 
static void vmw_cmdbuf_res_free(struct vmw_cmdbuf_res_manager *man,
				struct vmw_cmdbuf_res *entry)
{
	list_del(&entry->head);
	hash_del_rcu(&entry->hash.head);
	vmw_resource_unreference(&entry->res);
	kfree(entry);
}

 
void vmw_cmdbuf_res_commit(struct list_head *list)
{
	struct vmw_cmdbuf_res *entry, *next;

	list_for_each_entry_safe(entry, next, list, head) {
		list_del(&entry->head);
		if (entry->res->func->commit_notify)
			entry->res->func->commit_notify(entry->res,
							entry->state);
		switch (entry->state) {
		case VMW_CMDBUF_RES_ADD:
			entry->state = VMW_CMDBUF_RES_COMMITTED;
			list_add_tail(&entry->head, &entry->man->list);
			break;
		case VMW_CMDBUF_RES_DEL:
			vmw_resource_unreference(&entry->res);
			kfree(entry);
			break;
		default:
			BUG();
			break;
		}
	}
}

 
void vmw_cmdbuf_res_revert(struct list_head *list)
{
	struct vmw_cmdbuf_res *entry, *next;

	list_for_each_entry_safe(entry, next, list, head) {
		switch (entry->state) {
		case VMW_CMDBUF_RES_ADD:
			vmw_cmdbuf_res_free(entry->man, entry);
			break;
		case VMW_CMDBUF_RES_DEL:
			hash_add_rcu(entry->man->resources, &entry->hash.head,
						entry->hash.key);
			list_move_tail(&entry->head, &entry->man->list);
			entry->state = VMW_CMDBUF_RES_COMMITTED;
			break;
		default:
			BUG();
			break;
		}
	}
}

 
int vmw_cmdbuf_res_add(struct vmw_cmdbuf_res_manager *man,
		       enum vmw_cmdbuf_res_type res_type,
		       u32 user_key,
		       struct vmw_resource *res,
		       struct list_head *list)
{
	struct vmw_cmdbuf_res *cres;

	cres = kzalloc(sizeof(*cres), GFP_KERNEL);
	if (unlikely(!cres))
		return -ENOMEM;

	cres->hash.key = user_key | (res_type << 24);
	hash_add_rcu(man->resources, &cres->hash.head, cres->hash.key);

	cres->state = VMW_CMDBUF_RES_ADD;
	cres->res = vmw_resource_reference(res);
	cres->man = man;
	list_add_tail(&cres->head, list);

	return 0;
}

 
int vmw_cmdbuf_res_remove(struct vmw_cmdbuf_res_manager *man,
			  enum vmw_cmdbuf_res_type res_type,
			  u32 user_key,
			  struct list_head *list,
			  struct vmw_resource **res_p)
{
	struct vmw_cmdbuf_res *entry = NULL;
	struct vmwgfx_hash_item *hash;
	unsigned long key = user_key | (res_type << 24);

	hash_for_each_possible_rcu(man->resources, hash, head, key) {
		if (hash->key == key) {
			entry = hlist_entry(hash, struct vmw_cmdbuf_res, hash);
			break;
		}
	}
	if (unlikely(!entry))
		return -EINVAL;

	switch (entry->state) {
	case VMW_CMDBUF_RES_ADD:
		vmw_cmdbuf_res_free(man, entry);
		*res_p = NULL;
		break;
	case VMW_CMDBUF_RES_COMMITTED:
		hash_del_rcu(&entry->hash.head);
		list_del(&entry->head);
		entry->state = VMW_CMDBUF_RES_DEL;
		list_add_tail(&entry->head, list);
		*res_p = entry->res;
		break;
	default:
		BUG();
		break;
	}

	return 0;
}

 
struct vmw_cmdbuf_res_manager *
vmw_cmdbuf_res_man_create(struct vmw_private *dev_priv)
{
	struct vmw_cmdbuf_res_manager *man;

	man = kzalloc(sizeof(*man), GFP_KERNEL);
	if (!man)
		return ERR_PTR(-ENOMEM);

	man->dev_priv = dev_priv;
	INIT_LIST_HEAD(&man->list);
	hash_init(man->resources);
	return man;
}

 
void vmw_cmdbuf_res_man_destroy(struct vmw_cmdbuf_res_manager *man)
{
	struct vmw_cmdbuf_res *entry, *next;

	list_for_each_entry_safe(entry, next, &man->list, head)
		vmw_cmdbuf_res_free(man, entry);

	kfree(man);
}

