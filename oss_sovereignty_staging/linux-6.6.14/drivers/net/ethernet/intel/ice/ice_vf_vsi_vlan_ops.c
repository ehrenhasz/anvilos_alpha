
 

#include "ice_vsi_vlan_ops.h"
#include "ice_vsi_vlan_lib.h"
#include "ice_vlan_mode.h"
#include "ice.h"
#include "ice_vf_vsi_vlan_ops.h"
#include "ice_sriov.h"

static int
noop_vlan_arg(struct ice_vsi __always_unused *vsi,
	      struct ice_vlan __always_unused *vlan)
{
	return 0;
}

static int
noop_vlan(struct ice_vsi __always_unused *vsi)
{
	return 0;
}

static void ice_port_vlan_on(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_pf *pf = vsi->back;

	if (ice_is_dvm_ena(&pf->hw)) {
		vlan_ops = &vsi->outer_vlan_ops;

		 
		vlan_ops->set_port_vlan = ice_vsi_set_outer_port_vlan;
		vlan_ops->clear_port_vlan = ice_vsi_clear_outer_port_vlan;

		 
		vlan_ops = &vsi->inner_vlan_ops;
		vlan_ops->add_vlan = noop_vlan_arg;
		vlan_ops->del_vlan = noop_vlan_arg;
		vlan_ops->ena_stripping = ice_vsi_ena_inner_stripping;
		vlan_ops->dis_stripping = ice_vsi_dis_inner_stripping;
		vlan_ops->ena_insertion = ice_vsi_ena_inner_insertion;
		vlan_ops->dis_insertion = ice_vsi_dis_inner_insertion;
	} else {
		vlan_ops = &vsi->inner_vlan_ops;

		vlan_ops->set_port_vlan = ice_vsi_set_inner_port_vlan;
		vlan_ops->clear_port_vlan = ice_vsi_clear_inner_port_vlan;
	}

	 
	vlan_ops->dis_rx_filtering = noop_vlan;

	vlan_ops->ena_rx_filtering = ice_vsi_ena_rx_vlan_filtering;
}

static void ice_port_vlan_off(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_pf *pf = vsi->back;

	 
	vlan_ops = &vsi->inner_vlan_ops;

	vlan_ops->ena_stripping = ice_vsi_ena_inner_stripping;
	vlan_ops->dis_stripping = ice_vsi_dis_inner_stripping;
	vlan_ops->ena_insertion = ice_vsi_ena_inner_insertion;
	vlan_ops->dis_insertion = ice_vsi_dis_inner_insertion;

	if (ice_is_dvm_ena(&pf->hw)) {
		vlan_ops = &vsi->outer_vlan_ops;

		vlan_ops->del_vlan = ice_vsi_del_vlan;
		vlan_ops->ena_stripping = ice_vsi_ena_outer_stripping;
		vlan_ops->dis_stripping = ice_vsi_dis_outer_stripping;
		vlan_ops->ena_insertion = ice_vsi_ena_outer_insertion;
		vlan_ops->dis_insertion = ice_vsi_dis_outer_insertion;
	} else {
		vlan_ops->del_vlan = ice_vsi_del_vlan;
	}

	vlan_ops->dis_rx_filtering = ice_vsi_dis_rx_vlan_filtering;

	if (!test_bit(ICE_FLAG_VF_VLAN_PRUNING, pf->flags))
		vlan_ops->ena_rx_filtering = noop_vlan;
	else
		vlan_ops->ena_rx_filtering =
			ice_vsi_ena_rx_vlan_filtering;
}

 
void ice_vf_vsi_enable_port_vlan(struct ice_vsi *vsi)
{
	if (WARN_ON_ONCE(!vsi->vf))
		return;

	ice_port_vlan_on(vsi);
}

 
void ice_vf_vsi_disable_port_vlan(struct ice_vsi *vsi)
{
	if (WARN_ON_ONCE(!vsi->vf))
		return;

	ice_port_vlan_off(vsi);
}

 
void ice_vf_vsi_init_vlan_ops(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_pf *pf = vsi->back;
	struct ice_vf *vf = vsi->vf;

	if (WARN_ON(!vf))
		return;

	if (ice_vf_is_port_vlan_ena(vf))
		ice_port_vlan_on(vsi);
	else
		ice_port_vlan_off(vsi);

	vlan_ops = ice_is_dvm_ena(&pf->hw) ?
		&vsi->outer_vlan_ops : &vsi->inner_vlan_ops;

	vlan_ops->add_vlan = ice_vsi_add_vlan;
	vlan_ops->ena_tx_filtering = ice_vsi_ena_tx_vlan_filtering;
	vlan_ops->dis_tx_filtering = ice_vsi_dis_tx_vlan_filtering;
}

 
void ice_vf_vsi_cfg_dvm_legacy_vlan_mode(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_vf *vf = vsi->vf;
	struct device *dev;

	if (WARN_ON(!vf))
		return;

	dev = ice_pf_to_dev(vf->pf);

	if (!ice_is_dvm_ena(&vsi->back->hw) || ice_vf_is_port_vlan_ena(vf))
		return;

	vlan_ops = &vsi->outer_vlan_ops;

	 
	vlan_ops->dis_rx_filtering = ice_vsi_dis_rx_vlan_filtering;
	 
	vlan_ops->ena_rx_filtering = noop_vlan;

	 
	vlan_ops->dis_tx_filtering = ice_vsi_dis_tx_vlan_filtering;
	 
	vlan_ops->ena_tx_filtering = noop_vlan;

	if (vlan_ops->dis_rx_filtering(vsi))
		dev_dbg(dev, "Failed to disable Rx VLAN filtering for old VF without VIRTCHNL_VF_OFFLOAD_VLAN_V2 support\n");
	if (vlan_ops->dis_tx_filtering(vsi))
		dev_dbg(dev, "Failed to disable Tx VLAN filtering for old VF without VIRTHCNL_VF_OFFLOAD_VLAN_V2 support\n");

	 
	vlan_ops->dis_stripping = ice_vsi_dis_outer_stripping;
	vlan_ops->dis_insertion = ice_vsi_dis_outer_insertion;

	if (vlan_ops->dis_stripping(vsi))
		dev_dbg(dev, "Failed to disable outer VLAN stripping for old VF without VIRTCHNL_VF_OFFLOAD_VLAN_V2 support\n");

	if (vlan_ops->dis_insertion(vsi))
		dev_dbg(dev, "Failed to disable outer VLAN insertion for old VF without VIRTCHNL_VF_OFFLOAD_VLAN_V2 support\n");

	 
	vlan_ops = &vsi->inner_vlan_ops;

	vlan_ops->dis_stripping = ice_vsi_dis_outer_stripping;
	vlan_ops->dis_insertion = ice_vsi_dis_outer_insertion;

	if (vlan_ops->dis_stripping(vsi))
		dev_dbg(dev, "Failed to disable inner VLAN stripping for old VF without VIRTCHNL_VF_OFFLOAD_VLAN_V2 support\n");

	if (vlan_ops->dis_insertion(vsi))
		dev_dbg(dev, "Failed to disable inner VLAN insertion for old VF without VIRTCHNL_VF_OFFLOAD_VLAN_V2 support\n");
}

 
void ice_vf_vsi_cfg_svm_legacy_vlan_mode(struct ice_vsi *vsi)
{
	struct ice_vf *vf = vsi->vf;

	if (WARN_ON(!vf))
		return;

	if (ice_is_dvm_ena(&vsi->back->hw) || ice_vf_is_port_vlan_ena(vf))
		return;

	if (vsi->inner_vlan_ops.dis_rx_filtering(vsi))
		dev_dbg(ice_pf_to_dev(vf->pf), "Failed to disable Rx VLAN filtering for old VF with VIRTCHNL_VF_OFFLOAD_VLAN support\n");
}
