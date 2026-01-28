#ifndef _MLX5_IB_UMR_H
#define _MLX5_IB_UMR_H
#include "mlx5_ib.h"
#define MLX5_MAX_UMR_SHIFT 16
#define MLX5_MAX_UMR_PAGES (1 << MLX5_MAX_UMR_SHIFT)
#define MLX5_IB_UMR_OCTOWORD	       16
#define MLX5_IB_UMR_XLT_ALIGNMENT      64
int mlx5r_umr_resource_init(struct mlx5_ib_dev *dev);
void mlx5r_umr_resource_cleanup(struct mlx5_ib_dev *dev);
static inline bool mlx5r_umr_can_load_pas(struct mlx5_ib_dev *dev,
					  size_t length)
{
	if (MLX5_CAP_GEN(dev->mdev, umr_modify_entity_size_disabled))
		return false;
	if (!MLX5_CAP_GEN(dev->mdev, umr_extended_translation_offset) &&
	    length >= MLX5_MAX_UMR_PAGES * PAGE_SIZE)
		return false;
	return true;
}
static inline bool mlx5r_umr_can_reconfig(struct mlx5_ib_dev *dev,
					  unsigned int current_access_flags,
					  unsigned int target_access_flags)
{
	unsigned int diffs = current_access_flags ^ target_access_flags;
	if ((diffs & IB_ACCESS_REMOTE_ATOMIC) &&
	    MLX5_CAP_GEN(dev->mdev, atomic) &&
	    MLX5_CAP_GEN(dev->mdev, umr_modify_atomic_disabled))
		return false;
	if ((diffs & IB_ACCESS_RELAXED_ORDERING) &&
	    MLX5_CAP_GEN(dev->mdev, relaxed_ordering_write) &&
	    !MLX5_CAP_GEN(dev->mdev, relaxed_ordering_write_umr))
		return false;
	if ((diffs & IB_ACCESS_RELAXED_ORDERING) &&
	    (MLX5_CAP_GEN(dev->mdev, relaxed_ordering_read) ||
	     MLX5_CAP_GEN(dev->mdev, relaxed_ordering_read_pci_enabled)) &&
	    !MLX5_CAP_GEN(dev->mdev, relaxed_ordering_read_umr))
		return false;
	return true;
}
static inline u64 mlx5r_umr_get_xlt_octo(u64 bytes)
{
	return ALIGN(bytes, MLX5_IB_UMR_XLT_ALIGNMENT) /
	       MLX5_IB_UMR_OCTOWORD;
}
struct mlx5r_umr_context {
	struct ib_cqe cqe;
	enum ib_wc_status status;
	struct completion done;
};
struct mlx5r_umr_wqe {
	struct mlx5_wqe_umr_ctrl_seg ctrl_seg;
	struct mlx5_mkey_seg mkey_seg;
	struct mlx5_wqe_data_seg data_seg;
};
int mlx5r_umr_revoke_mr(struct mlx5_ib_mr *mr);
int mlx5r_umr_rereg_pd_access(struct mlx5_ib_mr *mr, struct ib_pd *pd,
			      int access_flags);
int mlx5r_umr_update_mr_pas(struct mlx5_ib_mr *mr, unsigned int flags);
int mlx5r_umr_update_xlt(struct mlx5_ib_mr *mr, u64 idx, int npages,
			 int page_shift, int flags);
#endif  
