 

#include <linux/dim.h>
#include "en.h"

static void
mlx5e_complete_dim_work(struct dim *dim, struct dim_cq_moder moder,
			struct mlx5_core_dev *mdev, struct mlx5_core_cq *mcq)
{
	mlx5_core_modify_cq_moderation(mdev, mcq, moder.usec, moder.pkts);
	dim->state = DIM_START_MEASURE;
}

void mlx5e_rx_dim_work(struct work_struct *work)
{
	struct dim *dim = container_of(work, struct dim, work);
	struct mlx5e_rq *rq = container_of(dim, struct mlx5e_rq, dim);
	struct dim_cq_moder cur_moder =
		net_dim_get_rx_moderation(dim->mode, dim->profile_ix);

	mlx5e_complete_dim_work(dim, cur_moder, rq->mdev, &rq->cq.mcq);
}

void mlx5e_tx_dim_work(struct work_struct *work)
{
	struct dim *dim = container_of(work, struct dim, work);
	struct mlx5e_txqsq *sq = container_of(dim, struct mlx5e_txqsq, dim);
	struct dim_cq_moder cur_moder =
		net_dim_get_tx_moderation(dim->mode, dim->profile_ix);

	mlx5e_complete_dim_work(dim, cur_moder, sq->cq.mdev, &sq->cq.mcq);
}
