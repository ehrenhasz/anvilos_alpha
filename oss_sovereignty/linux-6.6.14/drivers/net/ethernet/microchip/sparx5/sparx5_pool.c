
 

#include "sparx5_main_regs.h"
#include "sparx5_main.h"

static u32 sparx5_pool_id_to_idx(u32 id)
{
	return --id;
}

u32 sparx5_pool_idx_to_id(u32 idx)
{
	return ++idx;
}

 
int sparx5_pool_put(struct sparx5_pool_entry *pool, int size, u32 id)
{
	struct sparx5_pool_entry *e_itr;

	e_itr = (pool + sparx5_pool_id_to_idx(id));
	if (e_itr->ref_cnt == 0)
		return -EINVAL;

	return --e_itr->ref_cnt;
}

 
int sparx5_pool_get(struct sparx5_pool_entry *pool, int size, u32 *id)
{
	struct sparx5_pool_entry *e_itr;
	int i;

	for (i = 0, e_itr = pool; i < size; i++, e_itr++) {
		if (e_itr->ref_cnt == 0) {
			*id = sparx5_pool_idx_to_id(i);
			return ++e_itr->ref_cnt;
		}
	}

	return -ENOSPC;
}

 
int sparx5_pool_get_with_idx(struct sparx5_pool_entry *pool, int size, u32 idx,
			     u32 *id)
{
	struct sparx5_pool_entry *e_itr;
	int i, ret = -ENOSPC;

	for (i = 0, e_itr = pool; i < size; i++, e_itr++) {
		 
		if (e_itr->ref_cnt == 0 && ret == -ENOSPC)
			ret = i;
		 
		if (e_itr->idx == idx && e_itr->ref_cnt > 0) {
			ret = i;
			break;
		}
	}

	 
	if (ret >= 0) {
		*id = sparx5_pool_idx_to_id(ret);
		e_itr = (pool + ret);
		e_itr->idx = idx;
		return ++e_itr->ref_cnt;
	}

	return ret;
}
