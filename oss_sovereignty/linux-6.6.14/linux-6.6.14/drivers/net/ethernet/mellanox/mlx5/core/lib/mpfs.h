#ifndef __MLX5_MPFS_H__
#define __MLX5_MPFS_H__
#include <linux/if_ether.h>
#include <linux/mlx5/device.h>
#define MLX5_L2_ADDR_HASH_SIZE (BIT(BITS_PER_BYTE))
#define MLX5_L2_ADDR_HASH(addr) (addr[5])
struct l2addr_node {
	struct hlist_node hlist;
	u8                addr[ETH_ALEN];
};
#define for_each_l2hash_node(hn, tmp, hash, i) \
	for (i = 0; i < MLX5_L2_ADDR_HASH_SIZE; i++) \
		hlist_for_each_entry_safe(hn, tmp, &(hash)[i], hlist)
#define l2addr_hash_find(hash, mac, type) ({                \
	int ix = MLX5_L2_ADDR_HASH(mac);                    \
	bool found = false;                                 \
	type *ptr = NULL;                                   \
							    \
	hlist_for_each_entry(ptr, &(hash)[ix], node.hlist)  \
		if (ether_addr_equal(ptr->node.addr, mac)) {\
			found = true;                       \
			break;                              \
		}                                           \
	if (!found)                                         \
		ptr = NULL;                                 \
	ptr;                                                \
})
#define l2addr_hash_add(hash, mac, type, gfp) ({            \
	int ix = MLX5_L2_ADDR_HASH(mac);                    \
	type *ptr = NULL;                                   \
							    \
	ptr = kzalloc(sizeof(type), gfp);                   \
	if (ptr) {                                          \
		ether_addr_copy(ptr->node.addr, mac);       \
		hlist_add_head(&ptr->node.hlist, &(hash)[ix]);\
	}                                                   \
	ptr;                                                \
})
#define l2addr_hash_del(ptr) ({                             \
	hlist_del(&(ptr)->node.hlist);                      \
	kfree(ptr);                                         \
})
#ifdef CONFIG_MLX5_MPFS
int  mlx5_mpfs_init(struct mlx5_core_dev *dev);
void mlx5_mpfs_cleanup(struct mlx5_core_dev *dev);
#else  
static inline int  mlx5_mpfs_init(struct mlx5_core_dev *dev) { return 0; }
static inline void mlx5_mpfs_cleanup(struct mlx5_core_dev *dev) {}
#endif
#endif
