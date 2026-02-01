
 

#include "ice_pf_vsi_vlan_ops.h"
#include "ice_vf_vsi_vlan_ops.h"
#include "ice_lib.h"
#include "ice.h"

static int
op_unsupported_vlan_arg(struct ice_vsi * __always_unused vsi,
			struct ice_vlan * __always_unused vlan)
{
	return -EOPNOTSUPP;
}

static int
op_unsupported_tpid_arg(struct ice_vsi *__always_unused vsi,
			u16 __always_unused tpid)
{
	return -EOPNOTSUPP;
}

static int op_unsupported(struct ice_vsi *__always_unused vsi)
{
	return -EOPNOTSUPP;
}

 
static struct ice_vsi_vlan_ops ops_unsupported = {
	.add_vlan = op_unsupported_vlan_arg,
	.del_vlan = op_unsupported_vlan_arg,
	.ena_stripping = op_unsupported_tpid_arg,
	.dis_stripping = op_unsupported,
	.ena_insertion = op_unsupported_tpid_arg,
	.dis_insertion = op_unsupported,
	.ena_rx_filtering = op_unsupported,
	.dis_rx_filtering = op_unsupported,
	.ena_tx_filtering = op_unsupported,
	.dis_tx_filtering = op_unsupported,
	.set_port_vlan = op_unsupported_vlan_arg,
};

 
static void ice_vsi_init_unsupported_vlan_ops(struct ice_vsi *vsi)
{
	vsi->outer_vlan_ops = ops_unsupported;
	vsi->inner_vlan_ops = ops_unsupported;
}

 
void ice_vsi_init_vlan_ops(struct ice_vsi *vsi)
{
	 
	ice_vsi_init_unsupported_vlan_ops(vsi);

	switch (vsi->type) {
	case ICE_VSI_PF:
	case ICE_VSI_SWITCHDEV_CTRL:
		ice_pf_vsi_init_vlan_ops(vsi);
		break;
	case ICE_VSI_VF:
		ice_vf_vsi_init_vlan_ops(vsi);
		break;
	default:
		dev_dbg(ice_pf_to_dev(vsi->back), "%s does not support VLAN operations\n",
			ice_vsi_type_str(vsi->type));
		break;
	}
}

 
struct ice_vsi_vlan_ops *ice_get_compat_vsi_vlan_ops(struct ice_vsi *vsi)
{
	if (ice_is_dvm_ena(&vsi->back->hw))
		return &vsi->outer_vlan_ops;
	else
		return &vsi->inner_vlan_ops;
}
