 

#include <linux/kernel.h>
#include <linux/refcount.h>
#include <linux/mlx5/driver.h>
#include <net/vxlan.h>
#include "mlx5_core.h"
#include "vxlan.h"

struct mlx5_vxlan {
	struct mlx5_core_dev		*mdev;
	 
	DECLARE_HASHTABLE(htable, 4);
	struct mutex                    sync_lock;  
};

struct mlx5_vxlan_port {
	struct hlist_node hlist;
	u16 udp_port;
};

static int mlx5_vxlan_core_add_port_cmd(struct mlx5_core_dev *mdev, u16 port)
{
	u32 in[MLX5_ST_SZ_DW(add_vxlan_udp_dport_in)] = {};

	MLX5_SET(add_vxlan_udp_dport_in, in, opcode,
		 MLX5_CMD_OP_ADD_VXLAN_UDP_DPORT);
	MLX5_SET(add_vxlan_udp_dport_in, in, vxlan_udp_port, port);
	return mlx5_cmd_exec_in(mdev, add_vxlan_udp_dport, in);
}

static int mlx5_vxlan_core_del_port_cmd(struct mlx5_core_dev *mdev, u16 port)
{
	u32 in[MLX5_ST_SZ_DW(delete_vxlan_udp_dport_in)] = {};

	MLX5_SET(delete_vxlan_udp_dport_in, in, opcode,
		 MLX5_CMD_OP_DELETE_VXLAN_UDP_DPORT);
	MLX5_SET(delete_vxlan_udp_dport_in, in, vxlan_udp_port, port);
	return mlx5_cmd_exec_in(mdev, delete_vxlan_udp_dport, in);
}

bool mlx5_vxlan_lookup_port(struct mlx5_vxlan *vxlan, u16 port)
{
	struct mlx5_vxlan_port *vxlanp;
	bool found = false;

	if (!mlx5_vxlan_allowed(vxlan))
		return NULL;

	rcu_read_lock();
	hash_for_each_possible_rcu(vxlan->htable, vxlanp, hlist, port)
		if (vxlanp->udp_port == port) {
			found = true;
			break;
		}
	rcu_read_unlock();

	return found;
}

static struct mlx5_vxlan_port *vxlan_lookup_port(struct mlx5_vxlan *vxlan, u16 port)
{
	struct mlx5_vxlan_port *vxlanp;

	hash_for_each_possible(vxlan->htable, vxlanp, hlist, port)
		if (vxlanp->udp_port == port)
			return vxlanp;
	return NULL;
}

int mlx5_vxlan_add_port(struct mlx5_vxlan *vxlan, u16 port)
{
	struct mlx5_vxlan_port *vxlanp;
	int ret;

	vxlanp = kzalloc(sizeof(*vxlanp), GFP_KERNEL);
	if (!vxlanp)
		return -ENOMEM;
	vxlanp->udp_port = port;

	ret = mlx5_vxlan_core_add_port_cmd(vxlan->mdev, port);
	if (ret) {
		kfree(vxlanp);
		return ret;
	}

	mutex_lock(&vxlan->sync_lock);
	hash_add_rcu(vxlan->htable, &vxlanp->hlist, port);
	mutex_unlock(&vxlan->sync_lock);

	return 0;
}

int mlx5_vxlan_del_port(struct mlx5_vxlan *vxlan, u16 port)
{
	struct mlx5_vxlan_port *vxlanp;
	int ret = 0;

	mutex_lock(&vxlan->sync_lock);

	vxlanp = vxlan_lookup_port(vxlan, port);
	if (WARN_ON(!vxlanp)) {
		ret = -ENOENT;
		goto out_unlock;
	}

	hash_del_rcu(&vxlanp->hlist);
	synchronize_rcu();
	mlx5_vxlan_core_del_port_cmd(vxlan->mdev, port);
	kfree(vxlanp);

out_unlock:
	mutex_unlock(&vxlan->sync_lock);
	return ret;
}

struct mlx5_vxlan *mlx5_vxlan_create(struct mlx5_core_dev *mdev)
{
	struct mlx5_vxlan *vxlan;

	if (!MLX5_CAP_ETH(mdev, tunnel_stateless_vxlan) || !mlx5_core_is_pf(mdev))
		return ERR_PTR(-ENOTSUPP);

	vxlan = kzalloc(sizeof(*vxlan), GFP_KERNEL);
	if (!vxlan)
		return ERR_PTR(-ENOMEM);

	vxlan->mdev = mdev;
	mutex_init(&vxlan->sync_lock);
	hash_init(vxlan->htable);

	 
	mlx5_vxlan_add_port(vxlan, IANA_VXLAN_UDP_PORT);

	return vxlan;
}

void mlx5_vxlan_destroy(struct mlx5_vxlan *vxlan)
{
	if (!mlx5_vxlan_allowed(vxlan))
		return;

	mlx5_vxlan_del_port(vxlan, IANA_VXLAN_UDP_PORT);
	WARN_ON(!hash_empty(vxlan->htable));

	kfree(vxlan);
}

void mlx5_vxlan_reset_to_default(struct mlx5_vxlan *vxlan)
{
	struct mlx5_vxlan_port *vxlanp;
	struct hlist_node *tmp;
	int bkt;

	if (!mlx5_vxlan_allowed(vxlan))
		return;

	hash_for_each_safe(vxlan->htable, bkt, tmp, vxlanp, hlist) {
		 
		if (vxlanp->udp_port == IANA_VXLAN_UDP_PORT)
			continue;
		mlx5_vxlan_del_port(vxlan, vxlanp->udp_port);
	}
}
