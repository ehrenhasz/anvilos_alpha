 

#include <linux/ethtool.h>
#include <net/sock.h>

#include "en.h"
#include "fpga/sdk.h"
#include "en_accel/ktls.h"

static const struct counter_desc mlx5e_ktls_sw_stats_desc[] = {
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, tx_tls_ctx) },
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, tx_tls_del) },
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, tx_tls_pool_alloc) },
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, tx_tls_pool_free) },
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, rx_tls_ctx) },
	{ MLX5E_DECLARE_STAT(struct mlx5e_tls_sw_stats, rx_tls_del) },
};

#define MLX5E_READ_CTR_ATOMIC64(ptr, dsc, i) \
	atomic64_read((atomic64_t *)((char *)(ptr) + (dsc)[i].offset))

int mlx5e_ktls_get_count(struct mlx5e_priv *priv)
{
	if (!priv->tls)
		return 0;

	return ARRAY_SIZE(mlx5e_ktls_sw_stats_desc);
}

int mlx5e_ktls_get_strings(struct mlx5e_priv *priv, uint8_t *data)
{
	unsigned int i, n, idx = 0;

	if (!priv->tls)
		return 0;

	n = mlx5e_ktls_get_count(priv);

	for (i = 0; i < n; i++)
		strcpy(data + (idx++) * ETH_GSTRING_LEN,
		       mlx5e_ktls_sw_stats_desc[i].format);

	return n;
}

int mlx5e_ktls_get_stats(struct mlx5e_priv *priv, u64 *data)
{
	unsigned int i, n, idx = 0;

	if (!priv->tls)
		return 0;

	n = mlx5e_ktls_get_count(priv);

	for (i = 0; i < n; i++)
		data[idx++] = MLX5E_READ_CTR_ATOMIC64(&priv->tls->sw_stats,
						      mlx5e_ktls_sw_stats_desc,
						      i);

	return n;
}
