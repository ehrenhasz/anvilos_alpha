
 

#include "dr_types.h"

int mlx5dr_buddy_init(struct mlx5dr_icm_buddy_mem *buddy,
		      unsigned int max_order)
{
	int i;

	buddy->max_order = max_order;

	INIT_LIST_HEAD(&buddy->list_node);

	buddy->bitmap = kcalloc(buddy->max_order + 1,
				sizeof(*buddy->bitmap),
				GFP_KERNEL);
	buddy->num_free = kcalloc(buddy->max_order + 1,
				  sizeof(*buddy->num_free),
				  GFP_KERNEL);

	if (!buddy->bitmap || !buddy->num_free)
		goto err_free_all;

	 

	for (i = 0; i <= buddy->max_order; ++i) {
		unsigned int size = 1 << (buddy->max_order - i);

		buddy->bitmap[i] = bitmap_zalloc(size, GFP_KERNEL);
		if (!buddy->bitmap[i])
			goto err_out_free_each_bit_per_order;
	}

	 

	bitmap_set(buddy->bitmap[buddy->max_order], 0, 1);

	buddy->num_free[buddy->max_order] = 1;

	return 0;

err_out_free_each_bit_per_order:
	for (i = 0; i <= buddy->max_order; ++i)
		bitmap_free(buddy->bitmap[i]);

err_free_all:
	kfree(buddy->num_free);
	kfree(buddy->bitmap);
	return -ENOMEM;
}

void mlx5dr_buddy_cleanup(struct mlx5dr_icm_buddy_mem *buddy)
{
	int i;

	list_del(&buddy->list_node);

	for (i = 0; i <= buddy->max_order; ++i)
		bitmap_free(buddy->bitmap[i]);

	kfree(buddy->num_free);
	kfree(buddy->bitmap);
}

static int dr_buddy_find_free_seg(struct mlx5dr_icm_buddy_mem *buddy,
				  unsigned int start_order,
				  unsigned int *segment,
				  unsigned int *order)
{
	unsigned int seg, order_iter, m;

	for (order_iter = start_order;
	     order_iter <= buddy->max_order; ++order_iter) {
		if (!buddy->num_free[order_iter])
			continue;

		m = 1 << (buddy->max_order - order_iter);
		seg = find_first_bit(buddy->bitmap[order_iter], m);

		if (WARN(seg >= m,
			 "ICM Buddy: failed finding free mem for order %d\n",
			 order_iter))
			return -ENOMEM;

		break;
	}

	if (order_iter > buddy->max_order)
		return -ENOMEM;

	*segment = seg;
	*order = order_iter;
	return 0;
}

 
int mlx5dr_buddy_alloc_mem(struct mlx5dr_icm_buddy_mem *buddy,
			   unsigned int order,
			   unsigned int *segment)
{
	unsigned int seg, order_iter;
	int err;

	err = dr_buddy_find_free_seg(buddy, order, &seg, &order_iter);
	if (err)
		return err;

	bitmap_clear(buddy->bitmap[order_iter], seg, 1);
	--buddy->num_free[order_iter];

	 
	while (order_iter > order) {
		--order_iter;
		seg <<= 1;
		bitmap_set(buddy->bitmap[order_iter], seg ^ 1, 1);
		++buddy->num_free[order_iter];
	}

	seg <<= order;
	*segment = seg;

	return 0;
}

void mlx5dr_buddy_free_mem(struct mlx5dr_icm_buddy_mem *buddy,
			   unsigned int seg, unsigned int order)
{
	seg >>= order;

	 
	while (test_bit(seg ^ 1, buddy->bitmap[order])) {
		bitmap_clear(buddy->bitmap[order], seg ^ 1, 1);
		--buddy->num_free[order];
		seg >>= 1;
		++order;
	}
	bitmap_set(buddy->bitmap[order], seg, 1);

	++buddy->num_free[order];
}

