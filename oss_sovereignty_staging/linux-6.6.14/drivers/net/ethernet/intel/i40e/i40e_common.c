
 

#include "i40e.h"
#include "i40e_type.h"
#include "i40e_adminq.h"
#include "i40e_prototype.h"
#include <linux/avf/virtchnl.h>

 
int i40e_set_mac_type(struct i40e_hw *hw)
{
	int status = 0;

	if (hw->vendor_id == PCI_VENDOR_ID_INTEL) {
		switch (hw->device_id) {
		case I40E_DEV_ID_SFP_XL710:
		case I40E_DEV_ID_QEMU:
		case I40E_DEV_ID_KX_B:
		case I40E_DEV_ID_KX_C:
		case I40E_DEV_ID_QSFP_A:
		case I40E_DEV_ID_QSFP_B:
		case I40E_DEV_ID_QSFP_C:
		case I40E_DEV_ID_1G_BASE_T_BC:
		case I40E_DEV_ID_5G_BASE_T_BC:
		case I40E_DEV_ID_10G_BASE_T:
		case I40E_DEV_ID_10G_BASE_T4:
		case I40E_DEV_ID_10G_BASE_T_BC:
		case I40E_DEV_ID_10G_B:
		case I40E_DEV_ID_10G_SFP:
		case I40E_DEV_ID_20G_KR2:
		case I40E_DEV_ID_20G_KR2_A:
		case I40E_DEV_ID_25G_B:
		case I40E_DEV_ID_25G_SFP28:
		case I40E_DEV_ID_X710_N3000:
		case I40E_DEV_ID_XXV710_N3000:
			hw->mac.type = I40E_MAC_XL710;
			break;
		case I40E_DEV_ID_KX_X722:
		case I40E_DEV_ID_QSFP_X722:
		case I40E_DEV_ID_SFP_X722:
		case I40E_DEV_ID_1G_BASE_T_X722:
		case I40E_DEV_ID_10G_BASE_T_X722:
		case I40E_DEV_ID_SFP_I_X722:
		case I40E_DEV_ID_SFP_X722_A:
			hw->mac.type = I40E_MAC_X722;
			break;
		default:
			hw->mac.type = I40E_MAC_GENERIC;
			break;
		}
	} else {
		status = -ENODEV;
	}

	hw_dbg(hw, "i40e_set_mac_type found mac: %d, returns: %d\n",
		  hw->mac.type, status);
	return status;
}

 
const char *i40e_aq_str(struct i40e_hw *hw, enum i40e_admin_queue_err aq_err)
{
	switch (aq_err) {
	case I40E_AQ_RC_OK:
		return "OK";
	case I40E_AQ_RC_EPERM:
		return "I40E_AQ_RC_EPERM";
	case I40E_AQ_RC_ENOENT:
		return "I40E_AQ_RC_ENOENT";
	case I40E_AQ_RC_ESRCH:
		return "I40E_AQ_RC_ESRCH";
	case I40E_AQ_RC_EINTR:
		return "I40E_AQ_RC_EINTR";
	case I40E_AQ_RC_EIO:
		return "I40E_AQ_RC_EIO";
	case I40E_AQ_RC_ENXIO:
		return "I40E_AQ_RC_ENXIO";
	case I40E_AQ_RC_E2BIG:
		return "I40E_AQ_RC_E2BIG";
	case I40E_AQ_RC_EAGAIN:
		return "I40E_AQ_RC_EAGAIN";
	case I40E_AQ_RC_ENOMEM:
		return "I40E_AQ_RC_ENOMEM";
	case I40E_AQ_RC_EACCES:
		return "I40E_AQ_RC_EACCES";
	case I40E_AQ_RC_EFAULT:
		return "I40E_AQ_RC_EFAULT";
	case I40E_AQ_RC_EBUSY:
		return "I40E_AQ_RC_EBUSY";
	case I40E_AQ_RC_EEXIST:
		return "I40E_AQ_RC_EEXIST";
	case I40E_AQ_RC_EINVAL:
		return "I40E_AQ_RC_EINVAL";
	case I40E_AQ_RC_ENOTTY:
		return "I40E_AQ_RC_ENOTTY";
	case I40E_AQ_RC_ENOSPC:
		return "I40E_AQ_RC_ENOSPC";
	case I40E_AQ_RC_ENOSYS:
		return "I40E_AQ_RC_ENOSYS";
	case I40E_AQ_RC_ERANGE:
		return "I40E_AQ_RC_ERANGE";
	case I40E_AQ_RC_EFLUSHED:
		return "I40E_AQ_RC_EFLUSHED";
	case I40E_AQ_RC_BAD_ADDR:
		return "I40E_AQ_RC_BAD_ADDR";
	case I40E_AQ_RC_EMODE:
		return "I40E_AQ_RC_EMODE";
	case I40E_AQ_RC_EFBIG:
		return "I40E_AQ_RC_EFBIG";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", aq_err);
	return hw->err_str;
}

 
void i40e_debug_aq(struct i40e_hw *hw, enum i40e_debug_mask mask, void *desc,
		   void *buffer, u16 buf_len)
{
	struct i40e_aq_desc *aq_desc = (struct i40e_aq_desc *)desc;
	u32 effective_mask = hw->debug_mask & mask;
	char prefix[27];
	u16 len;
	u8 *buf = (u8 *)buffer;

	if (!effective_mask || !desc)
		return;

	len = le16_to_cpu(aq_desc->datalen);

	i40e_debug(hw, mask & I40E_DEBUG_AQ_DESCRIPTOR,
		   "AQ CMD: opcode 0x%04X, flags 0x%04X, datalen 0x%04X, retval 0x%04X\n",
		   le16_to_cpu(aq_desc->opcode),
		   le16_to_cpu(aq_desc->flags),
		   le16_to_cpu(aq_desc->datalen),
		   le16_to_cpu(aq_desc->retval));
	i40e_debug(hw, mask & I40E_DEBUG_AQ_DESCRIPTOR,
		   "\tcookie (h,l) 0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->cookie_high),
		   le32_to_cpu(aq_desc->cookie_low));
	i40e_debug(hw, mask & I40E_DEBUG_AQ_DESCRIPTOR,
		   "\tparam (0,1)  0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->params.internal.param0),
		   le32_to_cpu(aq_desc->params.internal.param1));
	i40e_debug(hw, mask & I40E_DEBUG_AQ_DESCRIPTOR,
		   "\taddr (h,l)   0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->params.external.addr_high),
		   le32_to_cpu(aq_desc->params.external.addr_low));

	if (buffer && buf_len != 0 && len != 0 &&
	    (effective_mask & I40E_DEBUG_AQ_DESC_BUFFER)) {
		i40e_debug(hw, mask, "AQ CMD Buffer:\n");
		if (buf_len < len)
			len = buf_len;

		snprintf(prefix, sizeof(prefix),
			 "i40e %02x:%02x.%x: \t0x",
			 hw->bus.bus_id,
			 hw->bus.device,
			 hw->bus.func);

		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET,
			       16, 1, buf, len, false);
	}
}

 
bool i40e_check_asq_alive(struct i40e_hw *hw)
{
	if (hw->aq.asq.len)
		return !!(rd32(hw, hw->aq.asq.len) &
			  I40E_PF_ATQLEN_ATQENABLE_MASK);
	else
		return false;
}

 
int i40e_aq_queue_shutdown(struct i40e_hw *hw,
			   bool unloading)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_queue_shutdown *cmd =
		(struct i40e_aqc_queue_shutdown *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_queue_shutdown);

	if (unloading)
		cmd->driver_unloading = cpu_to_le32(I40E_AQ_DRIVER_UNLOADING);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, NULL);

	return status;
}

 
static int i40e_aq_get_set_rss_lut(struct i40e_hw *hw,
				   u16 vsi_id, bool pf_lut,
				   u8 *lut, u16 lut_size,
				   bool set)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_set_rss_lut *cmd_resp =
		   (struct i40e_aqc_get_set_rss_lut *)&desc.params.raw;
	int status;

	if (set)
		i40e_fill_default_direct_cmd_desc(&desc,
						  i40e_aqc_opc_set_rss_lut);
	else
		i40e_fill_default_direct_cmd_desc(&desc,
						  i40e_aqc_opc_get_rss_lut);

	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			cpu_to_le16((u16)((vsi_id <<
					  I40E_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
					  I40E_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd_resp->vsi_id |= cpu_to_le16((u16)I40E_AQC_SET_RSS_LUT_VSI_VALID);

	if (pf_lut)
		cmd_resp->flags |= cpu_to_le16((u16)
					((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_PF <<
					I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	else
		cmd_resp->flags |= cpu_to_le16((u16)
					((I40E_AQC_SET_RSS_LUT_TABLE_TYPE_VSI <<
					I40E_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					I40E_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));

	status = i40e_asq_send_command(hw, &desc, lut, lut_size, NULL);

	return status;
}

 
int i40e_aq_get_rss_lut(struct i40e_hw *hw, u16 vsi_id,
			bool pf_lut, u8 *lut, u16 lut_size)
{
	return i40e_aq_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size,
				       false);
}

 
int i40e_aq_set_rss_lut(struct i40e_hw *hw, u16 vsi_id,
			bool pf_lut, u8 *lut, u16 lut_size)
{
	return i40e_aq_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size, true);
}

 
static int i40e_aq_get_set_rss_key(struct i40e_hw *hw,
				   u16 vsi_id,
				   struct i40e_aqc_get_set_rss_key_data *key,
				   bool set)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_set_rss_key *cmd_resp =
			(struct i40e_aqc_get_set_rss_key *)&desc.params.raw;
	u16 key_size = sizeof(struct i40e_aqc_get_set_rss_key_data);
	int status;

	if (set)
		i40e_fill_default_direct_cmd_desc(&desc,
						  i40e_aqc_opc_set_rss_key);
	else
		i40e_fill_default_direct_cmd_desc(&desc,
						  i40e_aqc_opc_get_rss_key);

	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			cpu_to_le16((u16)((vsi_id <<
					  I40E_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
					  I40E_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd_resp->vsi_id |= cpu_to_le16((u16)I40E_AQC_SET_RSS_KEY_VSI_VALID);

	status = i40e_asq_send_command(hw, &desc, key, key_size, NULL);

	return status;
}

 
int i40e_aq_get_rss_key(struct i40e_hw *hw,
			u16 vsi_id,
			struct i40e_aqc_get_set_rss_key_data *key)
{
	return i40e_aq_get_set_rss_key(hw, vsi_id, key, false);
}

 
int i40e_aq_set_rss_key(struct i40e_hw *hw,
			u16 vsi_id,
			struct i40e_aqc_get_set_rss_key_data *key)
{
	return i40e_aq_get_set_rss_key(hw, vsi_id, key, true);
}

 

 
#define I40E_PTT(PTYPE, OUTER_IP, OUTER_IP_VER, OUTER_FRAG, T, TE, TEF, I, PL)\
	[PTYPE] = { \
		1, \
		I40E_RX_PTYPE_OUTER_##OUTER_IP, \
		I40E_RX_PTYPE_OUTER_##OUTER_IP_VER, \
		I40E_RX_PTYPE_##OUTER_FRAG, \
		I40E_RX_PTYPE_TUNNEL_##T, \
		I40E_RX_PTYPE_TUNNEL_END_##TE, \
		I40E_RX_PTYPE_##TEF, \
		I40E_RX_PTYPE_INNER_PROT_##I, \
		I40E_RX_PTYPE_PAYLOAD_LAYER_##PL }

#define I40E_PTT_UNUSED_ENTRY(PTYPE) [PTYPE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

 
#define I40E_RX_PTYPE_NOF		I40E_RX_PTYPE_NOT_FRAG
#define I40E_RX_PTYPE_FRG		I40E_RX_PTYPE_FRAG
#define I40E_RX_PTYPE_INNER_PROT_TS	I40E_RX_PTYPE_INNER_PROT_TIMESYNC

 
struct i40e_rx_ptype_decoded i40e_ptype_lookup[BIT(8)] = {
	 
	I40E_PTT_UNUSED_ENTRY(0),
	I40E_PTT(1,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	I40E_PTT(2,  L2, NONE, NOF, NONE, NONE, NOF, TS,   PAY2),
	I40E_PTT(3,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	I40E_PTT_UNUSED_ENTRY(4),
	I40E_PTT_UNUSED_ENTRY(5),
	I40E_PTT(6,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	I40E_PTT(7,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	I40E_PTT_UNUSED_ENTRY(8),
	I40E_PTT_UNUSED_ENTRY(9),
	I40E_PTT(10, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	I40E_PTT(11, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	I40E_PTT(12, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(13, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(14, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(15, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(16, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(17, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(18, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(19, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(20, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(21, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(22, IP, IPV4, FRG, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(23, IP, IPV4, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(24, IP, IPV4, NOF, NONE, NONE, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(25),
	I40E_PTT(26, IP, IPV4, NOF, NONE, NONE, NOF, TCP,  PAY4),
	I40E_PTT(27, IP, IPV4, NOF, NONE, NONE, NOF, SCTP, PAY4),
	I40E_PTT(28, IP, IPV4, NOF, NONE, NONE, NOF, ICMP, PAY4),

	 
	I40E_PTT(29, IP, IPV4, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	I40E_PTT(30, IP, IPV4, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	I40E_PTT(31, IP, IPV4, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(32),
	I40E_PTT(33, IP, IPV4, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(34, IP, IPV4, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(35, IP, IPV4, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(36, IP, IPV4, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	I40E_PTT(37, IP, IPV4, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	I40E_PTT(38, IP, IPV4, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(39),
	I40E_PTT(40, IP, IPV4, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(41, IP, IPV4, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(42, IP, IPV4, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(43, IP, IPV4, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(44, IP, IPV4, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	I40E_PTT(45, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	I40E_PTT(46, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(47),
	I40E_PTT(48, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(49, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(50, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(51, IP, IPV4, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	I40E_PTT(52, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	I40E_PTT(53, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(54),
	I40E_PTT(55, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(56, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(57, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(58, IP, IPV4, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(59, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	I40E_PTT(60, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	I40E_PTT(61, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(62),
	I40E_PTT(63, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(64, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(65, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(66, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	I40E_PTT(67, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	I40E_PTT(68, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(69),
	I40E_PTT(70, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(71, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(72, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(73, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(74, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	I40E_PTT(75, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	I40E_PTT(76, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(77),
	I40E_PTT(78, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(79, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(80, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(81, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	I40E_PTT(82, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	I40E_PTT(83, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(84),
	I40E_PTT(85, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(86, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(87, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(88, IP, IPV6, FRG, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(89, IP, IPV6, NOF, NONE, NONE, NOF, NONE, PAY3),
	I40E_PTT(90, IP, IPV6, NOF, NONE, NONE, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(91),
	I40E_PTT(92, IP, IPV6, NOF, NONE, NONE, NOF, TCP,  PAY4),
	I40E_PTT(93, IP, IPV6, NOF, NONE, NONE, NOF, SCTP, PAY4),
	I40E_PTT(94, IP, IPV6, NOF, NONE, NONE, NOF, ICMP, PAY4),

	 
	I40E_PTT(95,  IP, IPV6, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	I40E_PTT(96,  IP, IPV6, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	I40E_PTT(97,  IP, IPV6, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(98),
	I40E_PTT(99,  IP, IPV6, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(100, IP, IPV6, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(101, IP, IPV6, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(102, IP, IPV6, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	I40E_PTT(103, IP, IPV6, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	I40E_PTT(104, IP, IPV6, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(105),
	I40E_PTT(106, IP, IPV6, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(107, IP, IPV6, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(108, IP, IPV6, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(109, IP, IPV6, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(110, IP, IPV6, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	I40E_PTT(111, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	I40E_PTT(112, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(113),
	I40E_PTT(114, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(115, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(116, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(117, IP, IPV6, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	I40E_PTT(118, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	I40E_PTT(119, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(120),
	I40E_PTT(121, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(122, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(123, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(124, IP, IPV6, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(125, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	I40E_PTT(126, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	I40E_PTT(127, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(128),
	I40E_PTT(129, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(130, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(131, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(132, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	I40E_PTT(133, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	I40E_PTT(134, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(135),
	I40E_PTT(136, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(137, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(138, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	 
	I40E_PTT(139, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	 
	I40E_PTT(140, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	I40E_PTT(141, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	I40E_PTT(142, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(143),
	I40E_PTT(144, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	I40E_PTT(145, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	I40E_PTT(146, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	 
	I40E_PTT(147, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	I40E_PTT(148, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	I40E_PTT(149, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	I40E_PTT_UNUSED_ENTRY(150),
	I40E_PTT(151, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	I40E_PTT(152, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	I40E_PTT(153, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	 
	[154 ... 255] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

 
int i40e_init_shared_code(struct i40e_hw *hw)
{
	u32 port, ari, func_rid;
	int status = 0;

	i40e_set_mac_type(hw);

	switch (hw->mac.type) {
	case I40E_MAC_XL710:
	case I40E_MAC_X722:
		break;
	default:
		return -ENODEV;
	}

	hw->phy.get_link_info = true;

	 
	port = (rd32(hw, I40E_PFGEN_PORTNUM) & I40E_PFGEN_PORTNUM_PORT_NUM_MASK)
					   >> I40E_PFGEN_PORTNUM_PORT_NUM_SHIFT;
	hw->port = (u8)port;
	ari = (rd32(hw, I40E_GLPCI_CAPSUP) & I40E_GLPCI_CAPSUP_ARI_EN_MASK) >>
						 I40E_GLPCI_CAPSUP_ARI_EN_SHIFT;
	func_rid = rd32(hw, I40E_PF_FUNC_RID);
	if (ari)
		hw->pf_id = (u8)(func_rid & 0xff);
	else
		hw->pf_id = (u8)(func_rid & 0x7);

	status = i40e_init_nvm(hw);
	return status;
}

 
static int
i40e_aq_mac_address_read(struct i40e_hw *hw,
			 u16 *flags,
			 struct i40e_aqc_mac_address_read_data *addrs,
			 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_mac_address_read *cmd_data =
		(struct i40e_aqc_mac_address_read *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_mac_address_read);
	desc.flags |= cpu_to_le16(I40E_AQ_FLAG_BUF);

	status = i40e_asq_send_command(hw, &desc, addrs,
				       sizeof(*addrs), cmd_details);
	*flags = le16_to_cpu(cmd_data->command_flags);

	return status;
}

 
int i40e_aq_mac_address_write(struct i40e_hw *hw,
			      u16 flags, u8 *mac_addr,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_mac_address_write *cmd_data =
		(struct i40e_aqc_mac_address_write *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_mac_address_write);
	cmd_data->command_flags = cpu_to_le16(flags);
	cmd_data->mac_sah = cpu_to_le16((u16)mac_addr[0] << 8 | mac_addr[1]);
	cmd_data->mac_sal = cpu_to_le32(((u32)mac_addr[2] << 24) |
					((u32)mac_addr[3] << 16) |
					((u32)mac_addr[4] << 8) |
					mac_addr[5]);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_get_mac_addr(struct i40e_hw *hw, u8 *mac_addr)
{
	struct i40e_aqc_mac_address_read_data addrs;
	u16 flags = 0;
	int status;

	status = i40e_aq_mac_address_read(hw, &flags, &addrs, NULL);

	if (flags & I40E_AQC_LAN_ADDR_VALID)
		ether_addr_copy(mac_addr, addrs.pf_lan_mac);

	return status;
}

 
int i40e_get_port_mac_addr(struct i40e_hw *hw, u8 *mac_addr)
{
	struct i40e_aqc_mac_address_read_data addrs;
	u16 flags = 0;
	int status;

	status = i40e_aq_mac_address_read(hw, &flags, &addrs, NULL);
	if (status)
		return status;

	if (flags & I40E_AQC_PORT_ADDR_VALID)
		ether_addr_copy(mac_addr, addrs.port_mac);
	else
		status = -EINVAL;

	return status;
}

 
void i40e_pre_tx_queue_cfg(struct i40e_hw *hw, u32 queue, bool enable)
{
	u32 abs_queue_idx = hw->func_caps.base_queue + queue;
	u32 reg_block = 0;
	u32 reg_val;

	if (abs_queue_idx >= 128) {
		reg_block = abs_queue_idx / 128;
		abs_queue_idx %= 128;
	}

	reg_val = rd32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block));
	reg_val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
	reg_val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);

	if (enable)
		reg_val |= I40E_GLLAN_TXPRE_QDIS_CLEAR_QDIS_MASK;
	else
		reg_val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

	wr32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block), reg_val);
}

 
int i40e_read_pba_string(struct i40e_hw *hw, u8 *pba_num,
			 u32 pba_num_size)
{
	u16 pba_word = 0;
	u16 pba_size = 0;
	u16 pba_ptr = 0;
	int status = 0;
	u16 i = 0;

	status = i40e_read_nvm_word(hw, I40E_SR_PBA_FLAGS, &pba_word);
	if (status || (pba_word != 0xFAFA)) {
		hw_dbg(hw, "Failed to read PBA flags or flag is invalid.\n");
		return status;
	}

	status = i40e_read_nvm_word(hw, I40E_SR_PBA_BLOCK_PTR, &pba_ptr);
	if (status) {
		hw_dbg(hw, "Failed to read PBA Block pointer.\n");
		return status;
	}

	status = i40e_read_nvm_word(hw, pba_ptr, &pba_size);
	if (status) {
		hw_dbg(hw, "Failed to read PBA Block size.\n");
		return status;
	}

	 
	pba_size--;
	if (pba_num_size < (((u32)pba_size * 2) + 1)) {
		hw_dbg(hw, "Buffer too small for PBA data.\n");
		return -EINVAL;
	}

	for (i = 0; i < pba_size; i++) {
		status = i40e_read_nvm_word(hw, (pba_ptr + 1) + i, &pba_word);
		if (status) {
			hw_dbg(hw, "Failed to read PBA Block word %d.\n", i);
			return status;
		}

		pba_num[(i * 2)] = (pba_word >> 8) & 0xFF;
		pba_num[(i * 2) + 1] = pba_word & 0xFF;
	}
	pba_num[(pba_size * 2)] = '\0';

	return status;
}

 
static enum i40e_media_type i40e_get_media_type(struct i40e_hw *hw)
{
	enum i40e_media_type media;

	switch (hw->phy.link_info.phy_type) {
	case I40E_PHY_TYPE_10GBASE_SR:
	case I40E_PHY_TYPE_10GBASE_LR:
	case I40E_PHY_TYPE_1000BASE_SX:
	case I40E_PHY_TYPE_1000BASE_LX:
	case I40E_PHY_TYPE_40GBASE_SR4:
	case I40E_PHY_TYPE_40GBASE_LR4:
	case I40E_PHY_TYPE_25GBASE_LR:
	case I40E_PHY_TYPE_25GBASE_SR:
		media = I40E_MEDIA_TYPE_FIBER;
		break;
	case I40E_PHY_TYPE_100BASE_TX:
	case I40E_PHY_TYPE_1000BASE_T:
	case I40E_PHY_TYPE_2_5GBASE_T_LINK_STATUS:
	case I40E_PHY_TYPE_5GBASE_T_LINK_STATUS:
	case I40E_PHY_TYPE_10GBASE_T:
		media = I40E_MEDIA_TYPE_BASET;
		break;
	case I40E_PHY_TYPE_10GBASE_CR1_CU:
	case I40E_PHY_TYPE_40GBASE_CR4_CU:
	case I40E_PHY_TYPE_10GBASE_CR1:
	case I40E_PHY_TYPE_40GBASE_CR4:
	case I40E_PHY_TYPE_10GBASE_SFPP_CU:
	case I40E_PHY_TYPE_40GBASE_AOC:
	case I40E_PHY_TYPE_10GBASE_AOC:
	case I40E_PHY_TYPE_25GBASE_CR:
	case I40E_PHY_TYPE_25GBASE_AOC:
	case I40E_PHY_TYPE_25GBASE_ACC:
		media = I40E_MEDIA_TYPE_DA;
		break;
	case I40E_PHY_TYPE_1000BASE_KX:
	case I40E_PHY_TYPE_10GBASE_KX4:
	case I40E_PHY_TYPE_10GBASE_KR:
	case I40E_PHY_TYPE_40GBASE_KR4:
	case I40E_PHY_TYPE_20GBASE_KR2:
	case I40E_PHY_TYPE_25GBASE_KR:
		media = I40E_MEDIA_TYPE_BACKPLANE;
		break;
	case I40E_PHY_TYPE_SGMII:
	case I40E_PHY_TYPE_XAUI:
	case I40E_PHY_TYPE_XFI:
	case I40E_PHY_TYPE_XLAUI:
	case I40E_PHY_TYPE_XLPPI:
	default:
		media = I40E_MEDIA_TYPE_UNKNOWN;
		break;
	}

	return media;
}

 
static int i40e_poll_globr(struct i40e_hw *hw,
			   u32 retry_limit)
{
	u32 cnt, reg = 0;

	for (cnt = 0; cnt < retry_limit; cnt++) {
		reg = rd32(hw, I40E_GLGEN_RSTAT);
		if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			return 0;
		msleep(100);
	}

	hw_dbg(hw, "Global reset failed.\n");
	hw_dbg(hw, "I40E_GLGEN_RSTAT = 0x%x\n", reg);

	return -EIO;
}

#define I40E_PF_RESET_WAIT_COUNT_A0	200
#define I40E_PF_RESET_WAIT_COUNT	200
 
int i40e_pf_reset(struct i40e_hw *hw)
{
	u32 cnt = 0;
	u32 cnt1 = 0;
	u32 reg = 0;
	u32 grst_del;

	 
	grst_del = (rd32(hw, I40E_GLGEN_RSTCTL) &
		    I40E_GLGEN_RSTCTL_GRSTDEL_MASK) >>
		    I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;

	 
	grst_del = grst_del * 20;

	for (cnt = 0; cnt < grst_del; cnt++) {
		reg = rd32(hw, I40E_GLGEN_RSTAT);
		if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
		msleep(100);
	}
	if (reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
		hw_dbg(hw, "Global reset polling failed to complete.\n");
		return -EIO;
	}

	 
	for (cnt1 = 0; cnt1 < I40E_PF_RESET_WAIT_COUNT; cnt1++) {
		reg = rd32(hw, I40E_GLNVM_ULD);
		reg &= (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK);
		if (reg == (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
			    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK)) {
			hw_dbg(hw, "Core and Global modules ready %d\n", cnt1);
			break;
		}
		usleep_range(10000, 20000);
	}
	if (!(reg & (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
		     I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))) {
		hw_dbg(hw, "wait for FW Reset complete timedout\n");
		hw_dbg(hw, "I40E_GLNVM_ULD = 0x%x\n", reg);
		return -EIO;
	}

	 
	if (!cnt) {
		u32 reg2 = 0;
		if (hw->revision_id == 0)
			cnt = I40E_PF_RESET_WAIT_COUNT_A0;
		else
			cnt = I40E_PF_RESET_WAIT_COUNT;
		reg = rd32(hw, I40E_PFGEN_CTRL);
		wr32(hw, I40E_PFGEN_CTRL,
		     (reg | I40E_PFGEN_CTRL_PFSWR_MASK));
		for (; cnt; cnt--) {
			reg = rd32(hw, I40E_PFGEN_CTRL);
			if (!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
				break;
			reg2 = rd32(hw, I40E_GLGEN_RSTAT);
			if (reg2 & I40E_GLGEN_RSTAT_DEVSTATE_MASK)
				break;
			usleep_range(1000, 2000);
		}
		if (reg2 & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
			if (i40e_poll_globr(hw, grst_del))
				return -EIO;
		} else if (reg & I40E_PFGEN_CTRL_PFSWR_MASK) {
			hw_dbg(hw, "PF reset polling failed to complete.\n");
			return -EIO;
		}
	}

	i40e_clear_pxe_mode(hw);

	return 0;
}

 
void i40e_clear_hw(struct i40e_hw *hw)
{
	u32 num_queues, base_queue;
	u32 num_pf_int;
	u32 num_vf_int;
	u32 num_vfs;
	u32 i, j;
	u32 val;
	u32 eol = 0x7ff;

	 
	val = rd32(hw, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
		     I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
		     I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = rd32(hw, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
		     I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
	    I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK && j >= base_queue)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = rd32(hw, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
	    I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
	    I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK && j >= i)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	 
	wr32(hw, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		wr32(hw, I40E_PFINT_DYN_CTLN(i), val);

	 
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	wr32(hw, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		wr32(hw, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		wr32(hw, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		wr32(hw, I40E_VPINT_LNKLSTN(i), val);

	 
	for (i = 0; i < num_queues; i++) {
		u32 abs_queue_idx = base_queue + i;
		u32 reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = rd32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		wr32(hw, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	udelay(400);

	 
	for (i = 0; i < num_queues; i++) {
		wr32(hw, I40E_QINT_TQCTL(i), 0);
		wr32(hw, I40E_QTX_ENA(i), 0);
		wr32(hw, I40E_QINT_RQCTL(i), 0);
		wr32(hw, I40E_QRX_ENA(i), 0);
	}

	 
	udelay(50);
}

 
void i40e_clear_pxe_mode(struct i40e_hw *hw)
{
	u32 reg;

	if (i40e_check_asq_alive(hw))
		i40e_aq_clear_pxe_mode(hw, NULL);

	 
	reg = rd32(hw, I40E_GLLAN_RCTL_0);

	if (hw->revision_id == 0) {
		 
		wr32(hw, I40E_GLLAN_RCTL_0, (reg & (~I40E_GLLAN_RCTL_0_PXE_MODE_MASK)));
	} else {
		wr32(hw, I40E_GLLAN_RCTL_0, (reg | I40E_GLLAN_RCTL_0_PXE_MODE_MASK));
	}
}

 
static u32 i40e_led_is_mine(struct i40e_hw *hw, int idx)
{
	u32 gpio_val = 0;
	u32 port;

	if (!I40E_IS_X710TL_DEVICE(hw->device_id) &&
	    !hw->func_caps.led[idx])
		return 0;
	gpio_val = rd32(hw, I40E_GLGEN_GPIO_CTL(idx));
	port = (gpio_val & I40E_GLGEN_GPIO_CTL_PRT_NUM_MASK) >>
		I40E_GLGEN_GPIO_CTL_PRT_NUM_SHIFT;

	 
	if ((gpio_val & I40E_GLGEN_GPIO_CTL_PRT_NUM_NA_MASK) ||
	    (port != hw->port))
		return 0;

	return gpio_val;
}

#define I40E_FW_LED BIT(4)
#define I40E_LED_MODE_VALID (I40E_GLGEN_GPIO_CTL_LED_MODE_MASK >> \
			     I40E_GLGEN_GPIO_CTL_LED_MODE_SHIFT)

#define I40E_LED0 22

#define I40E_PIN_FUNC_SDP 0x0
#define I40E_PIN_FUNC_LED 0x1

 
u32 i40e_led_get(struct i40e_hw *hw)
{
	u32 mode = 0;
	int i;

	 
	for (i = I40E_LED0; i <= I40E_GLGEN_GPIO_CTL_MAX_INDEX; i++) {
		u32 gpio_val = i40e_led_is_mine(hw, i);

		if (!gpio_val)
			continue;

		mode = (gpio_val & I40E_GLGEN_GPIO_CTL_LED_MODE_MASK) >>
			I40E_GLGEN_GPIO_CTL_LED_MODE_SHIFT;
		break;
	}

	return mode;
}

 
void i40e_led_set(struct i40e_hw *hw, u32 mode, bool blink)
{
	int i;

	if (mode & ~I40E_LED_MODE_VALID) {
		hw_dbg(hw, "invalid mode passed in %X\n", mode);
		return;
	}

	 
	for (i = I40E_LED0; i <= I40E_GLGEN_GPIO_CTL_MAX_INDEX; i++) {
		u32 gpio_val = i40e_led_is_mine(hw, i);

		if (!gpio_val)
			continue;

		if (I40E_IS_X710TL_DEVICE(hw->device_id)) {
			u32 pin_func = 0;

			if (mode & I40E_FW_LED)
				pin_func = I40E_PIN_FUNC_SDP;
			else
				pin_func = I40E_PIN_FUNC_LED;

			gpio_val &= ~I40E_GLGEN_GPIO_CTL_PIN_FUNC_MASK;
			gpio_val |= ((pin_func <<
				     I40E_GLGEN_GPIO_CTL_PIN_FUNC_SHIFT) &
				     I40E_GLGEN_GPIO_CTL_PIN_FUNC_MASK);
		}
		gpio_val &= ~I40E_GLGEN_GPIO_CTL_LED_MODE_MASK;
		 
		gpio_val |= ((mode << I40E_GLGEN_GPIO_CTL_LED_MODE_SHIFT) &
			     I40E_GLGEN_GPIO_CTL_LED_MODE_MASK);

		if (blink)
			gpio_val |= BIT(I40E_GLGEN_GPIO_CTL_LED_BLINK_SHIFT);
		else
			gpio_val &= ~BIT(I40E_GLGEN_GPIO_CTL_LED_BLINK_SHIFT);

		wr32(hw, I40E_GLGEN_GPIO_CTL(i), gpio_val);
		break;
	}
}

 

 
int
i40e_aq_get_phy_capabilities(struct i40e_hw *hw,
			     bool qualified_modules, bool report_init,
			     struct i40e_aq_get_phy_abilities_resp *abilities,
			     struct i40e_asq_cmd_details *cmd_details)
{
	u16 abilities_size = sizeof(struct i40e_aq_get_phy_abilities_resp);
	u16 max_delay = I40E_MAX_PHY_TIMEOUT, total_delay = 0;
	struct i40e_aq_desc desc;
	int status;

	if (!abilities)
		return -EINVAL;

	do {
		i40e_fill_default_direct_cmd_desc(&desc,
					       i40e_aqc_opc_get_phy_abilities);

		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
		if (abilities_size > I40E_AQ_LARGE_BUF)
			desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

		if (qualified_modules)
			desc.params.external.param0 |=
			cpu_to_le32(I40E_AQ_PHY_REPORT_QUALIFIED_MODULES);

		if (report_init)
			desc.params.external.param0 |=
			cpu_to_le32(I40E_AQ_PHY_REPORT_INITIAL_VALUES);

		status = i40e_asq_send_command(hw, &desc, abilities,
					       abilities_size, cmd_details);

		switch (hw->aq.asq_last_status) {
		case I40E_AQ_RC_EIO:
			status = -EIO;
			break;
		case I40E_AQ_RC_EAGAIN:
			usleep_range(1000, 2000);
			total_delay++;
			status = -EIO;
			break;
		 
		default:
			break;
		}

	} while ((hw->aq.asq_last_status == I40E_AQ_RC_EAGAIN) &&
		(total_delay < max_delay));

	if (status)
		return status;

	if (report_init) {
		if (hw->mac.type ==  I40E_MAC_XL710 &&
		    hw->aq.api_maj_ver == I40E_FW_API_VERSION_MAJOR &&
		    hw->aq.api_min_ver >= I40E_MINOR_VER_GET_LINK_INFO_XL710) {
			status = i40e_aq_get_link_info(hw, true, NULL, NULL);
		} else {
			hw->phy.phy_types = le32_to_cpu(abilities->phy_type);
			hw->phy.phy_types |=
					((u64)abilities->phy_type_ext << 32);
		}
	}

	return status;
}

 
int i40e_aq_set_phy_config(struct i40e_hw *hw,
			   struct i40e_aq_set_phy_config *config,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aq_set_phy_config *cmd =
			(struct i40e_aq_set_phy_config *)&desc.params.raw;
	int status;

	if (!config)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_phy_config);

	*cmd = *config;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

static noinline_for_stack int
i40e_set_fc_status(struct i40e_hw *hw,
		   struct i40e_aq_get_phy_abilities_resp *abilities,
		   bool atomic_restart)
{
	struct i40e_aq_set_phy_config config;
	enum i40e_fc_mode fc_mode = hw->fc.requested_mode;
	u8 pause_mask = 0x0;

	switch (fc_mode) {
	case I40E_FC_FULL:
		pause_mask |= I40E_AQ_PHY_FLAG_PAUSE_TX;
		pause_mask |= I40E_AQ_PHY_FLAG_PAUSE_RX;
		break;
	case I40E_FC_RX_PAUSE:
		pause_mask |= I40E_AQ_PHY_FLAG_PAUSE_RX;
		break;
	case I40E_FC_TX_PAUSE:
		pause_mask |= I40E_AQ_PHY_FLAG_PAUSE_TX;
		break;
	default:
		break;
	}

	memset(&config, 0, sizeof(struct i40e_aq_set_phy_config));
	 
	config.abilities = abilities->abilities & ~(I40E_AQ_PHY_FLAG_PAUSE_TX) &
			   ~(I40E_AQ_PHY_FLAG_PAUSE_RX);
	 
	config.abilities |= pause_mask;
	 
	if (config.abilities == abilities->abilities)
		return 0;

	 
	if (atomic_restart)
		config.abilities |= I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	 
	config.phy_type = abilities->phy_type;
	config.phy_type_ext = abilities->phy_type_ext;
	config.link_speed = abilities->link_speed;
	config.eee_capability = abilities->eee_capability;
	config.eeer = abilities->eeer_val;
	config.low_power_ctrl = abilities->d3_lpan;
	config.fec_config = abilities->fec_cfg_curr_mod_ext_info &
			    I40E_AQ_PHY_FEC_CONFIG_MASK;

	return i40e_aq_set_phy_config(hw, &config, NULL);
}

 
int i40e_set_fc(struct i40e_hw *hw, u8 *aq_failures,
		bool atomic_restart)
{
	struct i40e_aq_get_phy_abilities_resp abilities;
	int status;

	*aq_failures = 0x0;

	 
	status = i40e_aq_get_phy_capabilities(hw, false, false, &abilities,
					      NULL);
	if (status) {
		*aq_failures |= I40E_SET_FC_AQ_FAIL_GET;
		return status;
	}

	status = i40e_set_fc_status(hw, &abilities, atomic_restart);
	if (status)
		*aq_failures |= I40E_SET_FC_AQ_FAIL_SET;

	 
	status = i40e_update_link_info(hw);
	if (status) {
		 
		msleep(1000);
		status = i40e_update_link_info(hw);
	}
	if (status)
		*aq_failures |= I40E_SET_FC_AQ_FAIL_UPDATE;

	return status;
}

 
int i40e_aq_clear_pxe_mode(struct i40e_hw *hw,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_clear_pxe *cmd =
		(struct i40e_aqc_clear_pxe *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_clear_pxe_mode);

	cmd->rx_cnt = 0x2;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	wr32(hw, I40E_GLLAN_RCTL_0, 0x1);

	return status;
}

 
int i40e_aq_set_link_restart_an(struct i40e_hw *hw,
				bool enable_link,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_link_restart_an *cmd =
		(struct i40e_aqc_set_link_restart_an *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_link_restart_an);

	cmd->command = I40E_AQ_PHY_RESTART_AN;
	if (enable_link)
		cmd->command |= I40E_AQ_PHY_LINK_ENABLE;
	else
		cmd->command &= ~I40E_AQ_PHY_LINK_ENABLE;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_get_link_info(struct i40e_hw *hw,
			  bool enable_lse, struct i40e_link_status *link,
			  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_link_status *resp =
		(struct i40e_aqc_get_link_status *)&desc.params.raw;
	struct i40e_link_status *hw_link_info = &hw->phy.link_info;
	bool tx_pause, rx_pause;
	u16 command_flags;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_get_link_status);

	if (enable_lse)
		command_flags = I40E_AQ_LSE_ENABLE;
	else
		command_flags = I40E_AQ_LSE_DISABLE;
	resp->command_flags = cpu_to_le16(command_flags);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (status)
		goto aq_get_link_info_exit;

	 
	hw->phy.link_info_old = *hw_link_info;

	 
	hw_link_info->phy_type = (enum i40e_aq_phy_type)resp->phy_type;
	hw->phy.media_type = i40e_get_media_type(hw);
	hw_link_info->link_speed = (enum i40e_aq_link_speed)resp->link_speed;
	hw_link_info->link_info = resp->link_info;
	hw_link_info->an_info = resp->an_info;
	hw_link_info->fec_info = resp->config & (I40E_AQ_CONFIG_FEC_KR_ENA |
						 I40E_AQ_CONFIG_FEC_RS_ENA);
	hw_link_info->ext_info = resp->ext_info;
	hw_link_info->loopback = resp->loopback & I40E_AQ_LOOPBACK_MASK;
	hw_link_info->max_frame_size = le16_to_cpu(resp->max_frame_size);
	hw_link_info->pacing = resp->config & I40E_AQ_CONFIG_PACING_MASK;

	 
	tx_pause = !!(resp->an_info & I40E_AQ_LINK_PAUSE_TX);
	rx_pause = !!(resp->an_info & I40E_AQ_LINK_PAUSE_RX);
	if (tx_pause & rx_pause)
		hw->fc.current_mode = I40E_FC_FULL;
	else if (tx_pause)
		hw->fc.current_mode = I40E_FC_TX_PAUSE;
	else if (rx_pause)
		hw->fc.current_mode = I40E_FC_RX_PAUSE;
	else
		hw->fc.current_mode = I40E_FC_NONE;

	if (resp->config & I40E_AQ_CONFIG_CRC_ENA)
		hw_link_info->crc_enable = true;
	else
		hw_link_info->crc_enable = false;

	if (resp->command_flags & cpu_to_le16(I40E_AQ_LSE_IS_ENABLED))
		hw_link_info->lse_enable = true;
	else
		hw_link_info->lse_enable = false;

	if ((hw->mac.type == I40E_MAC_XL710) &&
	    (hw->aq.fw_maj_ver < 4 || (hw->aq.fw_maj_ver == 4 &&
	     hw->aq.fw_min_ver < 40)) && hw_link_info->phy_type == 0xE)
		hw_link_info->phy_type = I40E_PHY_TYPE_10GBASE_SFPP_CU;

	if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE &&
	    hw->mac.type != I40E_MAC_X722) {
		__le32 tmp;

		memcpy(&tmp, resp->link_type, sizeof(tmp));
		hw->phy.phy_types = le32_to_cpu(tmp);
		hw->phy.phy_types |= ((u64)resp->link_type_ext << 32);
	}

	 
	if (link)
		*link = *hw_link_info;

	 
	hw->phy.get_link_info = false;

aq_get_link_info_exit:
	return status;
}

 
int i40e_aq_set_phy_int_mask(struct i40e_hw *hw,
			     u16 mask,
			     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_phy_int_mask *cmd =
		(struct i40e_aqc_set_phy_int_mask *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_phy_int_mask);

	cmd->event_mask = cpu_to_le16(mask);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_mac_loopback(struct i40e_hw *hw, bool ena_lpbk,
			     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_lb_mode *cmd =
		(struct i40e_aqc_set_lb_mode *)&desc.params.raw;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_set_lb_modes);
	if (ena_lpbk) {
		if (hw->nvm.version <= I40E_LEGACY_LOOPBACK_NVM_VER)
			cmd->lb_mode = cpu_to_le16(I40E_AQ_LB_MAC_LOCAL_LEGACY);
		else
			cmd->lb_mode = cpu_to_le16(I40E_AQ_LB_MAC_LOCAL);
	}

	return i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);
}

 
int i40e_aq_set_phy_debug(struct i40e_hw *hw, u8 cmd_flags,
			  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_phy_debug *cmd =
		(struct i40e_aqc_set_phy_debug *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_phy_debug);

	cmd->command_flags = cmd_flags;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
static bool i40e_is_aq_api_ver_ge(struct i40e_adminq_info *aq, u16 maj,
				  u16 min)
{
	return (aq->api_maj_ver > maj ||
		(aq->api_maj_ver == maj && aq->api_min_ver >= min));
}

 
int i40e_aq_add_vsi(struct i40e_hw *hw,
		    struct i40e_vsi_context *vsi_ctx,
		    struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_get_update_vsi *cmd =
		(struct i40e_aqc_add_get_update_vsi *)&desc.params.raw;
	struct i40e_aqc_add_get_update_vsi_completion *resp =
		(struct i40e_aqc_add_get_update_vsi_completion *)
		&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_add_vsi);

	cmd->uplink_seid = cpu_to_le16(vsi_ctx->uplink_seid);
	cmd->connection_type = vsi_ctx->connection_type;
	cmd->vf_id = vsi_ctx->vf_num;
	cmd->vsi_flags = cpu_to_le16(vsi_ctx->flags);

	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));

	status = i40e_asq_send_command_atomic(hw, &desc, &vsi_ctx->info,
					      sizeof(vsi_ctx->info),
					      cmd_details, true);

	if (status)
		goto aq_add_vsi_exit;

	vsi_ctx->seid = le16_to_cpu(resp->seid);
	vsi_ctx->vsi_number = le16_to_cpu(resp->vsi_number);
	vsi_ctx->vsis_allocated = le16_to_cpu(resp->vsi_used);
	vsi_ctx->vsis_unallocated = le16_to_cpu(resp->vsi_free);

aq_add_vsi_exit:
	return status;
}

 
int i40e_aq_set_default_vsi(struct i40e_hw *hw,
			    u16 seid,
			    struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)
		&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_vsi_promiscuous_modes);

	cmd->promiscuous_flags = cpu_to_le16(I40E_AQC_SET_VSI_DEFAULT);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_DEFAULT);
	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_clear_default_vsi(struct i40e_hw *hw,
			      u16 seid,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)
		&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_vsi_promiscuous_modes);

	cmd->promiscuous_flags = cpu_to_le16(0);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_DEFAULT);
	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_vsi_unicast_promiscuous(struct i40e_hw *hw,
					u16 seid, bool set,
					struct i40e_asq_cmd_details *cmd_details,
					bool rx_only_promisc)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (set) {
		flags |= I40E_AQC_SET_VSI_PROMISC_UNICAST;
		if (rx_only_promisc && i40e_is_aq_api_ver_ge(&hw->aq, 1, 5))
			flags |= I40E_AQC_SET_VSI_PROMISC_RX_ONLY;
	}

	cmd->promiscuous_flags = cpu_to_le16(flags);

	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_UNICAST);
	if (i40e_is_aq_api_ver_ge(&hw->aq, 1, 5))
		cmd->valid_flags |=
			cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_RX_ONLY);

	cmd->seid = cpu_to_le16(seid);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_vsi_multicast_promiscuous(struct i40e_hw *hw,
					  u16 seid, bool set,
					  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (set)
		flags |= I40E_AQC_SET_VSI_PROMISC_MULTICAST;

	cmd->promiscuous_flags = cpu_to_le16(flags);

	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_MULTICAST);

	cmd->seid = cpu_to_le16(seid);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_vsi_mc_promisc_on_vlan(struct i40e_hw *hw,
				       u16 seid, bool enable,
				       u16 vid,
				       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (enable)
		flags |= I40E_AQC_SET_VSI_PROMISC_MULTICAST;

	cmd->promiscuous_flags = cpu_to_le16(flags);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_MULTICAST);
	cmd->seid = cpu_to_le16(seid);
	cmd->vlan_tag = cpu_to_le16(vid | I40E_AQC_SET_VSI_VLAN_VALID);

	status = i40e_asq_send_command_atomic(hw, &desc, NULL, 0,
					      cmd_details, true);

	return status;
}

 
int i40e_aq_set_vsi_uc_promisc_on_vlan(struct i40e_hw *hw,
				       u16 seid, bool enable,
				       u16 vid,
				       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (enable) {
		flags |= I40E_AQC_SET_VSI_PROMISC_UNICAST;
		if (i40e_is_aq_api_ver_ge(&hw->aq, 1, 5))
			flags |= I40E_AQC_SET_VSI_PROMISC_RX_ONLY;
	}

	cmd->promiscuous_flags = cpu_to_le16(flags);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_UNICAST);
	if (i40e_is_aq_api_ver_ge(&hw->aq, 1, 5))
		cmd->valid_flags |=
			cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_RX_ONLY);
	cmd->seid = cpu_to_le16(seid);
	cmd->vlan_tag = cpu_to_le16(vid | I40E_AQC_SET_VSI_VLAN_VALID);

	status = i40e_asq_send_command_atomic(hw, &desc, NULL, 0,
					      cmd_details, true);

	return status;
}

 
int i40e_aq_set_vsi_bc_promisc_on_vlan(struct i40e_hw *hw,
				       u16 seid, bool enable, u16 vid,
				       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (enable)
		flags |= I40E_AQC_SET_VSI_PROMISC_BROADCAST;

	cmd->promiscuous_flags = cpu_to_le16(flags);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	cmd->seid = cpu_to_le16(seid);
	cmd->vlan_tag = cpu_to_le16(vid | I40E_AQC_SET_VSI_VLAN_VALID);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_vsi_broadcast(struct i40e_hw *hw,
			      u16 seid, bool set_filter,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					i40e_aqc_opc_set_vsi_promiscuous_modes);

	if (set_filter)
		cmd->promiscuous_flags
			    |= cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	else
		cmd->promiscuous_flags
			    &= cpu_to_le16(~I40E_AQC_SET_VSI_PROMISC_BROADCAST);

	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_BROADCAST);
	cmd->seid = cpu_to_le16(seid);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_set_vsi_vlan_promisc(struct i40e_hw *hw,
				 u16 seid, bool enable,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_vsi_promiscuous_modes *cmd =
		(struct i40e_aqc_set_vsi_promiscuous_modes *)&desc.params.raw;
	u16 flags = 0;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					i40e_aqc_opc_set_vsi_promiscuous_modes);
	if (enable)
		flags |= I40E_AQC_SET_VSI_PROMISC_VLAN;

	cmd->promiscuous_flags = cpu_to_le16(flags);
	cmd->valid_flags = cpu_to_le16(I40E_AQC_SET_VSI_PROMISC_VLAN);
	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_get_vsi_params(struct i40e_hw *hw,
			   struct i40e_vsi_context *vsi_ctx,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_get_update_vsi *cmd =
		(struct i40e_aqc_add_get_update_vsi *)&desc.params.raw;
	struct i40e_aqc_add_get_update_vsi_completion *resp =
		(struct i40e_aqc_add_get_update_vsi_completion *)
		&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_get_vsi_parameters);

	cmd->uplink_seid = cpu_to_le16(vsi_ctx->seid);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);

	status = i40e_asq_send_command(hw, &desc, &vsi_ctx->info,
				    sizeof(vsi_ctx->info), NULL);

	if (status)
		goto aq_get_vsi_params_exit;

	vsi_ctx->seid = le16_to_cpu(resp->seid);
	vsi_ctx->vsi_number = le16_to_cpu(resp->vsi_number);
	vsi_ctx->vsis_allocated = le16_to_cpu(resp->vsi_used);
	vsi_ctx->vsis_unallocated = le16_to_cpu(resp->vsi_free);

aq_get_vsi_params_exit:
	return status;
}

 
int i40e_aq_update_vsi_params(struct i40e_hw *hw,
			      struct i40e_vsi_context *vsi_ctx,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_get_update_vsi *cmd =
		(struct i40e_aqc_add_get_update_vsi *)&desc.params.raw;
	struct i40e_aqc_add_get_update_vsi_completion *resp =
		(struct i40e_aqc_add_get_update_vsi_completion *)
		&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_update_vsi_parameters);
	cmd->uplink_seid = cpu_to_le16(vsi_ctx->seid);

	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));

	status = i40e_asq_send_command_atomic(hw, &desc, &vsi_ctx->info,
					      sizeof(vsi_ctx->info),
					      cmd_details, true);

	vsi_ctx->vsis_allocated = le16_to_cpu(resp->vsi_used);
	vsi_ctx->vsis_unallocated = le16_to_cpu(resp->vsi_free);

	return status;
}

 
int i40e_aq_get_switch_config(struct i40e_hw *hw,
			      struct i40e_aqc_get_switch_config_resp *buf,
			      u16 buf_size, u16 *start_seid,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_switch_seid *scfg =
		(struct i40e_aqc_switch_seid *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_get_switch_config);
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (buf_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
	scfg->seid = cpu_to_le16(*start_seid);

	status = i40e_asq_send_command(hw, &desc, buf, buf_size, cmd_details);
	*start_seid = le16_to_cpu(scfg->seid);

	return status;
}

 
int i40e_aq_set_switch_config(struct i40e_hw *hw,
			      u16 flags,
			      u16 valid_flags, u8 mode,
			      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_switch_config *scfg =
		(struct i40e_aqc_set_switch_config *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_switch_config);
	scfg->flags = cpu_to_le16(flags);
	scfg->valid_flags = cpu_to_le16(valid_flags);
	scfg->mode = mode;
	if (hw->flags & I40E_HW_FLAG_802_1AD_CAPABLE) {
		scfg->switch_tag = cpu_to_le16(hw->switch_tag);
		scfg->first_tag = cpu_to_le16(hw->first_tag);
		scfg->second_tag = cpu_to_le16(hw->second_tag);
	}
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_get_firmware_version(struct i40e_hw *hw,
				 u16 *fw_major_version, u16 *fw_minor_version,
				 u32 *fw_build,
				 u16 *api_major_version, u16 *api_minor_version,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_version *resp =
		(struct i40e_aqc_get_version *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_get_version);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (!status) {
		if (fw_major_version)
			*fw_major_version = le16_to_cpu(resp->fw_major);
		if (fw_minor_version)
			*fw_minor_version = le16_to_cpu(resp->fw_minor);
		if (fw_build)
			*fw_build = le32_to_cpu(resp->fw_build);
		if (api_major_version)
			*api_major_version = le16_to_cpu(resp->api_major);
		if (api_minor_version)
			*api_minor_version = le16_to_cpu(resp->api_minor);
	}

	return status;
}

 
int i40e_aq_send_driver_version(struct i40e_hw *hw,
				struct i40e_driver_version *dv,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_driver_version *cmd =
		(struct i40e_aqc_driver_version *)&desc.params.raw;
	int status;
	u16 len;

	if (dv == NULL)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_driver_version);

	desc.flags |= cpu_to_le16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD);
	cmd->driver_major_ver = dv->major_version;
	cmd->driver_minor_ver = dv->minor_version;
	cmd->driver_build_ver = dv->build_version;
	cmd->driver_subbuild_ver = dv->subbuild_version;

	len = 0;
	while (len < sizeof(dv->driver_string) &&
	       (dv->driver_string[len] < 0x80) &&
	       dv->driver_string[len])
		len++;
	status = i40e_asq_send_command(hw, &desc, dv->driver_string,
				       len, cmd_details);

	return status;
}

 
int i40e_get_link_status(struct i40e_hw *hw, bool *link_up)
{
	int status = 0;

	if (hw->phy.get_link_info) {
		status = i40e_update_link_info(hw);

		if (status)
			i40e_debug(hw, I40E_DEBUG_LINK, "get link failed: status %d\n",
				   status);
	}

	*link_up = hw->phy.link_info.link_info & I40E_AQ_LINK_UP;

	return status;
}

 
noinline_for_stack int i40e_update_link_info(struct i40e_hw *hw)
{
	struct i40e_aq_get_phy_abilities_resp abilities;
	int status = 0;

	status = i40e_aq_get_link_info(hw, true, NULL, NULL);
	if (status)
		return status;

	 
	if ((hw->phy.link_info.link_info & I40E_AQ_MEDIA_AVAILABLE) &&
	    ((hw->phy.link_info.link_info & I40E_AQ_LINK_UP) ||
	     !(hw->phy.link_info_old.link_info & I40E_AQ_LINK_UP))) {
		status = i40e_aq_get_phy_capabilities(hw, false, false,
						      &abilities, NULL);
		if (status)
			return status;

		if (abilities.fec_cfg_curr_mod_ext_info &
		    I40E_AQ_ENABLE_FEC_AUTO)
			hw->phy.link_info.req_fec_info =
				(I40E_AQ_REQUEST_FEC_KR |
				 I40E_AQ_REQUEST_FEC_RS);
		else
			hw->phy.link_info.req_fec_info =
				abilities.fec_cfg_curr_mod_ext_info &
				(I40E_AQ_REQUEST_FEC_KR |
				 I40E_AQ_REQUEST_FEC_RS);

		memcpy(hw->phy.link_info.module_type, &abilities.module_type,
		       sizeof(hw->phy.link_info.module_type));
	}

	return status;
}

 
int i40e_aq_add_veb(struct i40e_hw *hw, u16 uplink_seid,
		    u16 downlink_seid, u8 enabled_tc,
		    bool default_port, u16 *veb_seid,
		    bool enable_stats,
		    struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_veb *cmd =
		(struct i40e_aqc_add_veb *)&desc.params.raw;
	struct i40e_aqc_add_veb_completion *resp =
		(struct i40e_aqc_add_veb_completion *)&desc.params.raw;
	u16 veb_flags = 0;
	int status;

	 
	if (!!uplink_seid != !!downlink_seid)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_add_veb);

	cmd->uplink_seid = cpu_to_le16(uplink_seid);
	cmd->downlink_seid = cpu_to_le16(downlink_seid);
	cmd->enable_tcs = enabled_tc;
	if (!uplink_seid)
		veb_flags |= I40E_AQC_ADD_VEB_FLOATING;
	if (default_port)
		veb_flags |= I40E_AQC_ADD_VEB_PORT_TYPE_DEFAULT;
	else
		veb_flags |= I40E_AQC_ADD_VEB_PORT_TYPE_DATA;

	 
	if (!enable_stats)
		veb_flags |= I40E_AQC_ADD_VEB_ENABLE_DISABLE_STATS;

	cmd->veb_flags = cpu_to_le16(veb_flags);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (!status && veb_seid)
		*veb_seid = le16_to_cpu(resp->veb_seid);

	return status;
}

 
int i40e_aq_get_veb_parameters(struct i40e_hw *hw,
			       u16 veb_seid, u16 *switch_id,
			       bool *floating, u16 *statistic_index,
			       u16 *vebs_used, u16 *vebs_free,
			       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_veb_parameters_completion *cmd_resp =
		(struct i40e_aqc_get_veb_parameters_completion *)
		&desc.params.raw;
	int status;

	if (veb_seid == 0)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_get_veb_parameters);
	cmd_resp->seid = cpu_to_le16(veb_seid);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);
	if (status)
		goto get_veb_exit;

	if (switch_id)
		*switch_id = le16_to_cpu(cmd_resp->switch_id);
	if (statistic_index)
		*statistic_index = le16_to_cpu(cmd_resp->statistic_index);
	if (vebs_used)
		*vebs_used = le16_to_cpu(cmd_resp->vebs_used);
	if (vebs_free)
		*vebs_free = le16_to_cpu(cmd_resp->vebs_free);
	if (floating) {
		u16 flags = le16_to_cpu(cmd_resp->veb_flags);

		if (flags & I40E_AQC_ADD_VEB_FLOATING)
			*floating = true;
		else
			*floating = false;
	}

get_veb_exit:
	return status;
}

 
static u16
i40e_prepare_add_macvlan(struct i40e_aqc_add_macvlan_element_data *mv_list,
			 struct i40e_aq_desc *desc, u16 count, u16 seid)
{
	struct i40e_aqc_macvlan *cmd =
		(struct i40e_aqc_macvlan *)&desc->params.raw;
	u16 buf_size;
	int i;

	buf_size = count * sizeof(*mv_list);

	 
	i40e_fill_default_direct_cmd_desc(desc, i40e_aqc_opc_add_macvlan);
	cmd->num_addresses = cpu_to_le16(count);
	cmd->seid[0] = cpu_to_le16(I40E_AQC_MACVLAN_CMD_SEID_VALID | seid);
	cmd->seid[1] = 0;
	cmd->seid[2] = 0;

	for (i = 0; i < count; i++)
		if (is_multicast_ether_addr(mv_list[i].mac_addr))
			mv_list[i].flags |=
			       cpu_to_le16(I40E_AQC_MACVLAN_ADD_USE_SHARED_MAC);

	desc->flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	if (buf_size > I40E_AQ_LARGE_BUF)
		desc->flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	return buf_size;
}

 
int
i40e_aq_add_macvlan(struct i40e_hw *hw, u16 seid,
		    struct i40e_aqc_add_macvlan_element_data *mv_list,
		    u16 count, struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	u16 buf_size;

	if (count == 0 || !mv_list || !hw)
		return -EINVAL;

	buf_size = i40e_prepare_add_macvlan(mv_list, &desc, count, seid);

	return i40e_asq_send_command_atomic(hw, &desc, mv_list, buf_size,
					    cmd_details, true);
}

 
int
i40e_aq_add_macvlan_v2(struct i40e_hw *hw, u16 seid,
		       struct i40e_aqc_add_macvlan_element_data *mv_list,
		       u16 count, struct i40e_asq_cmd_details *cmd_details,
		       enum i40e_admin_queue_err *aq_status)
{
	struct i40e_aq_desc desc;
	u16 buf_size;

	if (count == 0 || !mv_list || !hw)
		return -EINVAL;

	buf_size = i40e_prepare_add_macvlan(mv_list, &desc, count, seid);

	return i40e_asq_send_command_atomic_v2(hw, &desc, mv_list, buf_size,
					       cmd_details, true, aq_status);
}

 
int
i40e_aq_remove_macvlan(struct i40e_hw *hw, u16 seid,
		       struct i40e_aqc_remove_macvlan_element_data *mv_list,
		       u16 count, struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_macvlan *cmd =
		(struct i40e_aqc_macvlan *)&desc.params.raw;
	u16 buf_size;
	int status;

	if (count == 0 || !mv_list || !hw)
		return -EINVAL;

	buf_size = count * sizeof(*mv_list);

	 
	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_remove_macvlan);
	cmd->num_addresses = cpu_to_le16(count);
	cmd->seid[0] = cpu_to_le16(I40E_AQC_MACVLAN_CMD_SEID_VALID | seid);
	cmd->seid[1] = 0;
	cmd->seid[2] = 0;

	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	if (buf_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	status = i40e_asq_send_command_atomic(hw, &desc, mv_list, buf_size,
					      cmd_details, true);

	return status;
}

 
int
i40e_aq_remove_macvlan_v2(struct i40e_hw *hw, u16 seid,
			  struct i40e_aqc_remove_macvlan_element_data *mv_list,
			  u16 count, struct i40e_asq_cmd_details *cmd_details,
			  enum i40e_admin_queue_err *aq_status)
{
	struct i40e_aqc_macvlan *cmd;
	struct i40e_aq_desc desc;
	u16 buf_size;

	if (count == 0 || !mv_list || !hw)
		return -EINVAL;

	buf_size = count * sizeof(*mv_list);

	 
	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_remove_macvlan);
	cmd = (struct i40e_aqc_macvlan *)&desc.params.raw;
	cmd->num_addresses = cpu_to_le16(count);
	cmd->seid[0] = cpu_to_le16(I40E_AQC_MACVLAN_CMD_SEID_VALID | seid);
	cmd->seid[1] = 0;
	cmd->seid[2] = 0;

	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	if (buf_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	return i40e_asq_send_command_atomic_v2(hw, &desc, mv_list, buf_size,
						 cmd_details, true, aq_status);
}

 
static int i40e_mirrorrule_op(struct i40e_hw *hw,
			      u16 opcode, u16 sw_seid, u16 rule_type, u16 id,
			      u16 count, __le16 *mr_list,
			      struct i40e_asq_cmd_details *cmd_details,
			      u16 *rule_id, u16 *rules_used, u16 *rules_free)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_delete_mirror_rule *cmd =
		(struct i40e_aqc_add_delete_mirror_rule *)&desc.params.raw;
	struct i40e_aqc_add_delete_mirror_rule_completion *resp =
	(struct i40e_aqc_add_delete_mirror_rule_completion *)&desc.params.raw;
	u16 buf_size;
	int status;

	buf_size = count * sizeof(*mr_list);

	 
	i40e_fill_default_direct_cmd_desc(&desc, opcode);
	cmd->seid = cpu_to_le16(sw_seid);
	cmd->rule_type = cpu_to_le16(rule_type &
				     I40E_AQC_MIRROR_RULE_TYPE_MASK);
	cmd->num_entries = cpu_to_le16(count);
	 
	cmd->destination = cpu_to_le16(id);
	if (mr_list) {
		desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF |
						I40E_AQ_FLAG_RD));
		if (buf_size > I40E_AQ_LARGE_BUF)
			desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
	}

	status = i40e_asq_send_command(hw, &desc, mr_list, buf_size,
				       cmd_details);
	if (!status ||
	    hw->aq.asq_last_status == I40E_AQ_RC_ENOSPC) {
		if (rule_id)
			*rule_id = le16_to_cpu(resp->rule_id);
		if (rules_used)
			*rules_used = le16_to_cpu(resp->mirror_rules_used);
		if (rules_free)
			*rules_free = le16_to_cpu(resp->mirror_rules_free);
	}
	return status;
}

 
int i40e_aq_add_mirrorrule(struct i40e_hw *hw, u16 sw_seid,
			   u16 rule_type, u16 dest_vsi, u16 count,
			   __le16 *mr_list,
			   struct i40e_asq_cmd_details *cmd_details,
			   u16 *rule_id, u16 *rules_used, u16 *rules_free)
{
	if (!(rule_type == I40E_AQC_MIRROR_RULE_TYPE_ALL_INGRESS ||
	    rule_type == I40E_AQC_MIRROR_RULE_TYPE_ALL_EGRESS)) {
		if (count == 0 || !mr_list)
			return -EINVAL;
	}

	return i40e_mirrorrule_op(hw, i40e_aqc_opc_add_mirror_rule, sw_seid,
				  rule_type, dest_vsi, count, mr_list,
				  cmd_details, rule_id, rules_used, rules_free);
}

 
int i40e_aq_delete_mirrorrule(struct i40e_hw *hw, u16 sw_seid,
			      u16 rule_type, u16 rule_id, u16 count,
			      __le16 *mr_list,
			      struct i40e_asq_cmd_details *cmd_details,
			      u16 *rules_used, u16 *rules_free)
{
	 
	if (rule_type == I40E_AQC_MIRROR_RULE_TYPE_VLAN) {
		 
		if (count == 0 || !mr_list)
			return -EINVAL;
	}

	return i40e_mirrorrule_op(hw, i40e_aqc_opc_delete_mirror_rule, sw_seid,
				  rule_type, rule_id, count, mr_list,
				  cmd_details, NULL, rules_used, rules_free);
}

 
int i40e_aq_send_msg_to_vf(struct i40e_hw *hw, u16 vfid,
			   u32 v_opcode, u32 v_retval, u8 *msg, u16 msglen,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_pf_vf_message *cmd =
		(struct i40e_aqc_pf_vf_message *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_send_msg_to_vf);
	cmd->id = cpu_to_le32(vfid);
	desc.cookie_high = cpu_to_le32(v_opcode);
	desc.cookie_low = cpu_to_le32(v_retval);
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_SI);
	if (msglen) {
		desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF |
						I40E_AQ_FLAG_RD));
		if (msglen > I40E_AQ_LARGE_BUF)
			desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
		desc.datalen = cpu_to_le16(msglen);
	}
	status = i40e_asq_send_command(hw, &desc, msg, msglen, cmd_details);

	return status;
}

 
int i40e_aq_debug_read_register(struct i40e_hw *hw,
				u32 reg_addr, u64 *reg_val,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_debug_reg_read_write *cmd_resp =
		(struct i40e_aqc_debug_reg_read_write *)&desc.params.raw;
	int status;

	if (reg_val == NULL)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_debug_read_reg);

	cmd_resp->address = cpu_to_le32(reg_addr);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (!status) {
		*reg_val = ((u64)le32_to_cpu(cmd_resp->value_high) << 32) |
			   (u64)le32_to_cpu(cmd_resp->value_low);
	}

	return status;
}

 
int i40e_aq_debug_write_register(struct i40e_hw *hw,
				 u32 reg_addr, u64 reg_val,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_debug_reg_read_write *cmd =
		(struct i40e_aqc_debug_reg_read_write *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_debug_write_reg);

	cmd->address = cpu_to_le32(reg_addr);
	cmd->value_high = cpu_to_le32((u32)(reg_val >> 32));
	cmd->value_low = cpu_to_le32((u32)(reg_val & 0xFFFFFFFF));

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_request_resource(struct i40e_hw *hw,
			     enum i40e_aq_resources_ids resource,
			     enum i40e_aq_resource_access_type access,
			     u8 sdp_number, u64 *timeout,
			     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_request_resource *cmd_resp =
		(struct i40e_aqc_request_resource *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_request_resource);

	cmd_resp->resource_id = cpu_to_le16(resource);
	cmd_resp->access_type = cpu_to_le16(access);
	cmd_resp->resource_number = cpu_to_le32(sdp_number);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);
	 
	if (!status || hw->aq.asq_last_status == I40E_AQ_RC_EBUSY)
		*timeout = le32_to_cpu(cmd_resp->timeout);

	return status;
}

 
int i40e_aq_release_resource(struct i40e_hw *hw,
			     enum i40e_aq_resources_ids resource,
			     u8 sdp_number,
			     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_request_resource *cmd =
		(struct i40e_aqc_request_resource *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_release_resource);

	cmd->resource_id = cpu_to_le16(resource);
	cmd->resource_number = cpu_to_le32(sdp_number);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_read_nvm(struct i40e_hw *hw, u8 module_pointer,
		     u32 offset, u16 length, void *data,
		     bool last_command,
		     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_nvm_update *cmd =
		(struct i40e_aqc_nvm_update *)&desc.params.raw;
	int status;

	 
	if (offset & 0xFF000000) {
		status = -EINVAL;
		goto i40e_aq_read_nvm_exit;
	}

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_nvm_read);

	 
	if (last_command)
		cmd->command_flags |= I40E_AQ_NVM_LAST_CMD;
	cmd->module_pointer = module_pointer;
	cmd->offset = cpu_to_le32(offset);
	cmd->length = cpu_to_le16(length);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (length > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	status = i40e_asq_send_command(hw, &desc, data, length, cmd_details);

i40e_aq_read_nvm_exit:
	return status;
}

 
int i40e_aq_erase_nvm(struct i40e_hw *hw, u8 module_pointer,
		      u32 offset, u16 length, bool last_command,
		      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_nvm_update *cmd =
		(struct i40e_aqc_nvm_update *)&desc.params.raw;
	int status;

	 
	if (offset & 0xFF000000) {
		status = -EINVAL;
		goto i40e_aq_erase_nvm_exit;
	}

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_nvm_erase);

	 
	if (last_command)
		cmd->command_flags |= I40E_AQ_NVM_LAST_CMD;
	cmd->module_pointer = module_pointer;
	cmd->offset = cpu_to_le32(offset);
	cmd->length = cpu_to_le16(length);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

i40e_aq_erase_nvm_exit:
	return status;
}

 
static void i40e_parse_discover_capabilities(struct i40e_hw *hw, void *buff,
				     u32 cap_count,
				     enum i40e_admin_queue_opc list_type_opc)
{
	struct i40e_aqc_list_capabilities_element_resp *cap;
	u32 valid_functions, num_functions;
	u32 number, logical_id, phys_id;
	struct i40e_hw_capabilities *p;
	u16 id, ocp_cfg_word0;
	u8 major_rev;
	int status;
	u32 i = 0;

	cap = (struct i40e_aqc_list_capabilities_element_resp *) buff;

	if (list_type_opc == i40e_aqc_opc_list_dev_capabilities)
		p = &hw->dev_caps;
	else if (list_type_opc == i40e_aqc_opc_list_func_capabilities)
		p = &hw->func_caps;
	else
		return;

	for (i = 0; i < cap_count; i++, cap++) {
		id = le16_to_cpu(cap->id);
		number = le32_to_cpu(cap->number);
		logical_id = le32_to_cpu(cap->logical_id);
		phys_id = le32_to_cpu(cap->phys_id);
		major_rev = cap->major_rev;

		switch (id) {
		case I40E_AQ_CAP_ID_SWITCH_MODE:
			p->switch_mode = number;
			break;
		case I40E_AQ_CAP_ID_MNG_MODE:
			p->management_mode = number;
			if (major_rev > 1) {
				p->mng_protocols_over_mctp = logical_id;
				i40e_debug(hw, I40E_DEBUG_INIT,
					   "HW Capability: Protocols over MCTP = %d\n",
					   p->mng_protocols_over_mctp);
			} else {
				p->mng_protocols_over_mctp = 0;
			}
			break;
		case I40E_AQ_CAP_ID_NPAR_ACTIVE:
			p->npar_enable = number;
			break;
		case I40E_AQ_CAP_ID_OS2BMC_CAP:
			p->os2bmc = number;
			break;
		case I40E_AQ_CAP_ID_FUNCTIONS_VALID:
			p->valid_functions = number;
			break;
		case I40E_AQ_CAP_ID_SRIOV:
			if (number == 1)
				p->sr_iov_1_1 = true;
			break;
		case I40E_AQ_CAP_ID_VF:
			p->num_vfs = number;
			p->vf_base_id = logical_id;
			break;
		case I40E_AQ_CAP_ID_VMDQ:
			if (number == 1)
				p->vmdq = true;
			break;
		case I40E_AQ_CAP_ID_8021QBG:
			if (number == 1)
				p->evb_802_1_qbg = true;
			break;
		case I40E_AQ_CAP_ID_8021QBR:
			if (number == 1)
				p->evb_802_1_qbh = true;
			break;
		case I40E_AQ_CAP_ID_VSI:
			p->num_vsis = number;
			break;
		case I40E_AQ_CAP_ID_DCB:
			if (number == 1) {
				p->dcb = true;
				p->enabled_tcmap = logical_id;
				p->maxtc = phys_id;
			}
			break;
		case I40E_AQ_CAP_ID_FCOE:
			if (number == 1)
				p->fcoe = true;
			break;
		case I40E_AQ_CAP_ID_ISCSI:
			if (number == 1)
				p->iscsi = true;
			break;
		case I40E_AQ_CAP_ID_RSS:
			p->rss = true;
			p->rss_table_size = number;
			p->rss_table_entry_width = logical_id;
			break;
		case I40E_AQ_CAP_ID_RXQ:
			p->num_rx_qp = number;
			p->base_queue = phys_id;
			break;
		case I40E_AQ_CAP_ID_TXQ:
			p->num_tx_qp = number;
			p->base_queue = phys_id;
			break;
		case I40E_AQ_CAP_ID_MSIX:
			p->num_msix_vectors = number;
			i40e_debug(hw, I40E_DEBUG_INIT,
				   "HW Capability: MSIX vector count = %d\n",
				   p->num_msix_vectors);
			break;
		case I40E_AQ_CAP_ID_VF_MSIX:
			p->num_msix_vectors_vf = number;
			break;
		case I40E_AQ_CAP_ID_FLEX10:
			if (major_rev == 1) {
				if (number == 1) {
					p->flex10_enable = true;
					p->flex10_capable = true;
				}
			} else {
				 
				if (number & 1)
					p->flex10_enable = true;
				if (number & 2)
					p->flex10_capable = true;
			}
			p->flex10_mode = logical_id;
			p->flex10_status = phys_id;
			break;
		case I40E_AQ_CAP_ID_CEM:
			if (number == 1)
				p->mgmt_cem = true;
			break;
		case I40E_AQ_CAP_ID_IWARP:
			if (number == 1)
				p->iwarp = true;
			break;
		case I40E_AQ_CAP_ID_LED:
			if (phys_id < I40E_HW_CAP_MAX_GPIO)
				p->led[phys_id] = true;
			break;
		case I40E_AQ_CAP_ID_SDP:
			if (phys_id < I40E_HW_CAP_MAX_GPIO)
				p->sdp[phys_id] = true;
			break;
		case I40E_AQ_CAP_ID_MDIO:
			if (number == 1) {
				p->mdio_port_num = phys_id;
				p->mdio_port_mode = logical_id;
			}
			break;
		case I40E_AQ_CAP_ID_1588:
			if (number == 1)
				p->ieee_1588 = true;
			break;
		case I40E_AQ_CAP_ID_FLOW_DIRECTOR:
			p->fd = true;
			p->fd_filters_guaranteed = number;
			p->fd_filters_best_effort = logical_id;
			break;
		case I40E_AQ_CAP_ID_WSR_PROT:
			p->wr_csr_prot = (u64)number;
			p->wr_csr_prot |= (u64)logical_id << 32;
			break;
		case I40E_AQ_CAP_ID_NVM_MGMT:
			if (number & I40E_NVM_MGMT_SEC_REV_DISABLED)
				p->sec_rev_disabled = true;
			if (number & I40E_NVM_MGMT_UPDATE_DISABLED)
				p->update_disabled = true;
			break;
		default:
			break;
		}
	}

	if (p->fcoe)
		i40e_debug(hw, I40E_DEBUG_ALL, "device is FCoE capable\n");

	 
	if (p->npar_enable || p->flex10_enable)
		p->fcoe = false;

	 
	hw->num_ports = 0;
	for (i = 0; i < 4; i++) {
		u32 port_cfg_reg = I40E_PRTGEN_CNF + (4 * i);
		u64 port_cfg = 0;

		 
		i40e_aq_debug_read_register(hw, port_cfg_reg, &port_cfg, NULL);
		if (!(port_cfg & I40E_PRTGEN_CNF_PORT_DIS_MASK))
			hw->num_ports++;
	}

	 
	if (hw->mac.type == I40E_MAC_X722) {
		if (!i40e_acquire_nvm(hw, I40E_RESOURCE_READ)) {
			status = i40e_aq_read_nvm(hw, I40E_SR_EMP_MODULE_PTR,
						  2 * I40E_SR_OCP_CFG_WORD0,
						  sizeof(ocp_cfg_word0),
						  &ocp_cfg_word0, true, NULL);
			if (!status &&
			    (ocp_cfg_word0 & I40E_SR_OCP_ENABLED))
				hw->num_ports = 4;
			i40e_release_nvm(hw);
		}
	}

	valid_functions = p->valid_functions;
	num_functions = 0;
	while (valid_functions) {
		if (valid_functions & 1)
			num_functions++;
		valid_functions >>= 1;
	}

	 
	if (hw->num_ports != 0) {
		hw->partition_id = (hw->pf_id / hw->num_ports) + 1;
		hw->num_partitions = num_functions / hw->num_ports;
	}

	 
	p->rx_buf_chain_len = I40E_MAX_CHAINED_RX_BUFFERS;
}

 
int i40e_aq_discover_capabilities(struct i40e_hw *hw,
				  void *buff, u16 buff_size, u16 *data_size,
				  enum i40e_admin_queue_opc list_type_opc,
				  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aqc_list_capabilites *cmd;
	struct i40e_aq_desc desc;
	int status = 0;

	cmd = (struct i40e_aqc_list_capabilites *)&desc.params.raw;

	if (list_type_opc != i40e_aqc_opc_list_func_capabilities &&
		list_type_opc != i40e_aqc_opc_list_dev_capabilities) {
		status = -EINVAL;
		goto exit;
	}

	i40e_fill_default_direct_cmd_desc(&desc, list_type_opc);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	*data_size = le16_to_cpu(desc.datalen);

	if (status)
		goto exit;

	i40e_parse_discover_capabilities(hw, buff, le32_to_cpu(cmd->count),
					 list_type_opc);

exit:
	return status;
}

 
int i40e_aq_update_nvm(struct i40e_hw *hw, u8 module_pointer,
		       u32 offset, u16 length, void *data,
		       bool last_command, u8 preservation_flags,
		       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_nvm_update *cmd =
		(struct i40e_aqc_nvm_update *)&desc.params.raw;
	int status;

	 
	if (offset & 0xFF000000) {
		status = -EINVAL;
		goto i40e_aq_update_nvm_exit;
	}

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_nvm_update);

	 
	if (last_command)
		cmd->command_flags |= I40E_AQ_NVM_LAST_CMD;
	if (hw->mac.type == I40E_MAC_X722) {
		if (preservation_flags == I40E_NVM_PRESERVATION_FLAGS_SELECTED)
			cmd->command_flags |=
				(I40E_AQ_NVM_PRESERVATION_FLAGS_SELECTED <<
				 I40E_AQ_NVM_PRESERVATION_FLAGS_SHIFT);
		else if (preservation_flags == I40E_NVM_PRESERVATION_FLAGS_ALL)
			cmd->command_flags |=
				(I40E_AQ_NVM_PRESERVATION_FLAGS_ALL <<
				 I40E_AQ_NVM_PRESERVATION_FLAGS_SHIFT);
	}
	cmd->module_pointer = module_pointer;
	cmd->offset = cpu_to_le32(offset);
	cmd->length = cpu_to_le16(length);

	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	if (length > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	status = i40e_asq_send_command(hw, &desc, data, length, cmd_details);

i40e_aq_update_nvm_exit:
	return status;
}

 
int i40e_aq_rearrange_nvm(struct i40e_hw *hw,
			  u8 rearrange_nvm,
			  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aqc_nvm_update *cmd;
	struct i40e_aq_desc desc;
	int status;

	cmd = (struct i40e_aqc_nvm_update *)&desc.params.raw;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_nvm_update);

	rearrange_nvm &= (I40E_AQ_NVM_REARRANGE_TO_FLAT |
			 I40E_AQ_NVM_REARRANGE_TO_STRUCT);

	if (!rearrange_nvm) {
		status = -EINVAL;
		goto i40e_aq_rearrange_nvm_exit;
	}

	cmd->command_flags |= rearrange_nvm;
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

i40e_aq_rearrange_nvm_exit:
	return status;
}

 
int i40e_aq_get_lldp_mib(struct i40e_hw *hw, u8 bridge_type,
			 u8 mib_type, void *buff, u16 buff_size,
			 u16 *local_len, u16 *remote_len,
			 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_lldp_get_mib *cmd =
		(struct i40e_aqc_lldp_get_mib *)&desc.params.raw;
	struct i40e_aqc_lldp_get_mib *resp =
		(struct i40e_aqc_lldp_get_mib *)&desc.params.raw;
	int status;

	if (buff_size == 0 || !buff)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_lldp_get_mib);
	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);

	cmd->type = mib_type & I40E_AQ_LLDP_MIB_TYPE_MASK;
	cmd->type |= ((bridge_type << I40E_AQ_LLDP_BRIDGE_TYPE_SHIFT) &
		       I40E_AQ_LLDP_BRIDGE_TYPE_MASK);

	desc.datalen = cpu_to_le16(buff_size);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	if (!status) {
		if (local_len != NULL)
			*local_len = le16_to_cpu(resp->local_len);
		if (remote_len != NULL)
			*remote_len = le16_to_cpu(resp->remote_len);
	}

	return status;
}

 
int
i40e_aq_set_lldp_mib(struct i40e_hw *hw,
		     u8 mib_type, void *buff, u16 buff_size,
		     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aqc_lldp_set_local_mib *cmd;
	struct i40e_aq_desc desc;
	int status;

	cmd = (struct i40e_aqc_lldp_set_local_mib *)&desc.params.raw;
	if (buff_size == 0 || !buff)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_lldp_set_local_mib);
	 
	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
	desc.datalen = cpu_to_le16(buff_size);

	cmd->type = mib_type;
	cmd->length = cpu_to_le16(buff_size);
	cmd->address_high = cpu_to_le32(upper_32_bits((uintptr_t)buff));
	cmd->address_low = cpu_to_le32(lower_32_bits((uintptr_t)buff));

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	return status;
}

 
int i40e_aq_cfg_lldp_mib_change_event(struct i40e_hw *hw,
				      bool enable_update,
				      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_lldp_update_mib *cmd =
		(struct i40e_aqc_lldp_update_mib *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_lldp_update_mib);

	if (!enable_update)
		cmd->command |= I40E_AQ_LLDP_MIB_UPDATE_DISABLE;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int
i40e_aq_restore_lldp(struct i40e_hw *hw, u8 *setting, bool restore,
		     struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_lldp_restore *cmd =
		(struct i40e_aqc_lldp_restore *)&desc.params.raw;
	int status;

	if (!(hw->flags & I40E_HW_FLAG_FW_LLDP_PERSISTENT)) {
		i40e_debug(hw, I40E_DEBUG_ALL,
			   "Restore LLDP not supported by current FW version.\n");
		return -ENODEV;
	}

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_lldp_restore);

	if (restore)
		cmd->command |= I40E_AQ_LLDP_AGENT_RESTORE;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (setting)
		*setting = cmd->command & 1;

	return status;
}

 
int i40e_aq_stop_lldp(struct i40e_hw *hw, bool shutdown_agent,
		      bool persist,
		      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_lldp_stop *cmd =
		(struct i40e_aqc_lldp_stop *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_lldp_stop);

	if (shutdown_agent)
		cmd->command |= I40E_AQ_LLDP_AGENT_SHUTDOWN;

	if (persist) {
		if (hw->flags & I40E_HW_FLAG_FW_LLDP_PERSISTENT)
			cmd->command |= I40E_AQ_LLDP_AGENT_STOP_PERSIST;
		else
			i40e_debug(hw, I40E_DEBUG_ALL,
				   "Persistent Stop LLDP not supported by current FW version.\n");
	}

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_start_lldp(struct i40e_hw *hw, bool persist,
		       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_lldp_start *cmd =
		(struct i40e_aqc_lldp_start *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_lldp_start);

	cmd->command = I40E_AQ_LLDP_AGENT_START;

	if (persist) {
		if (hw->flags & I40E_HW_FLAG_FW_LLDP_PERSISTENT)
			cmd->command |= I40E_AQ_LLDP_AGENT_START_PERSIST;
		else
			i40e_debug(hw, I40E_DEBUG_ALL,
				   "Persistent Start LLDP not supported by current FW version.\n");
	}

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int
i40e_aq_set_dcb_parameters(struct i40e_hw *hw, bool dcb_enable,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_set_dcb_parameters *cmd =
		(struct i40e_aqc_set_dcb_parameters *)&desc.params.raw;
	int status;

	if (!(hw->flags & I40E_HW_FLAG_FW_LLDP_STOPPABLE))
		return -ENODEV;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_dcb_parameters);

	if (dcb_enable) {
		cmd->valid_flags = I40E_DCB_VALID;
		cmd->command = I40E_AQ_DCB_SET_AGENT;
	}
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_get_cee_dcb_config(struct i40e_hw *hw,
			       void *buff, u16 buff_size,
			       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	int status;

	if (buff_size == 0 || !buff)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_get_cee_dcb_cfg);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	status = i40e_asq_send_command(hw, &desc, (void *)buff, buff_size,
				       cmd_details);

	return status;
}

 
int i40e_aq_add_udp_tunnel(struct i40e_hw *hw,
			   u16 udp_port, u8 protocol_index,
			   u8 *filter_index,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_udp_tunnel *cmd =
		(struct i40e_aqc_add_udp_tunnel *)&desc.params.raw;
	struct i40e_aqc_del_udp_tunnel_completion *resp =
		(struct i40e_aqc_del_udp_tunnel_completion *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_add_udp_tunnel);

	cmd->udp_port = cpu_to_le16(udp_port);
	cmd->protocol_type = protocol_index;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (!status && filter_index)
		*filter_index = resp->index;

	return status;
}

 
int i40e_aq_del_udp_tunnel(struct i40e_hw *hw, u8 index,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_remove_udp_tunnel *cmd =
		(struct i40e_aqc_remove_udp_tunnel *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_del_udp_tunnel);

	cmd->index = index;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_delete_element(struct i40e_hw *hw, u16 seid,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_switch_seid *cmd =
		(struct i40e_aqc_switch_seid *)&desc.params.raw;
	int status;

	if (seid == 0)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_delete_element);

	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command_atomic(hw, &desc, NULL, 0,
					      cmd_details, true);

	return status;
}

 
int i40e_aq_dcb_updated(struct i40e_hw *hw,
			struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_dcb_updated);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
static int i40e_aq_tx_sched_cmd(struct i40e_hw *hw, u16 seid,
				void *buff, u16 buff_size,
				enum i40e_admin_queue_opc opcode,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_tx_sched_ind *cmd =
		(struct i40e_aqc_tx_sched_ind *)&desc.params.raw;
	int status;
	bool cmd_param_flag = false;

	switch (opcode) {
	case i40e_aqc_opc_configure_vsi_ets_sla_bw_limit:
	case i40e_aqc_opc_configure_vsi_tc_bw:
	case i40e_aqc_opc_enable_switching_comp_ets:
	case i40e_aqc_opc_modify_switching_comp_ets:
	case i40e_aqc_opc_disable_switching_comp_ets:
	case i40e_aqc_opc_configure_switching_comp_ets_bw_limit:
	case i40e_aqc_opc_configure_switching_comp_bw_config:
		cmd_param_flag = true;
		break;
	case i40e_aqc_opc_query_vsi_bw_config:
	case i40e_aqc_opc_query_vsi_ets_sla_config:
	case i40e_aqc_opc_query_switching_comp_ets_config:
	case i40e_aqc_opc_query_port_ets_config:
	case i40e_aqc_opc_query_switching_comp_bw_config:
		cmd_param_flag = false;
		break;
	default:
		return -EINVAL;
	}

	i40e_fill_default_direct_cmd_desc(&desc, opcode);

	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (cmd_param_flag)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_RD);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	desc.datalen = cpu_to_le16(buff_size);

	cmd->vsi_seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);

	return status;
}

 
int i40e_aq_config_vsi_bw_limit(struct i40e_hw *hw,
				u16 seid, u16 credit, u8 max_credit,
				struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_configure_vsi_bw_limit *cmd =
		(struct i40e_aqc_configure_vsi_bw_limit *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_configure_vsi_bw_limit);

	cmd->vsi_seid = cpu_to_le16(seid);
	cmd->credit = cpu_to_le16(credit);
	cmd->max_credit = max_credit;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_config_vsi_tc_bw(struct i40e_hw *hw,
			     u16 seid,
			     struct i40e_aqc_configure_vsi_tc_bw_data *bw_data,
			     struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				    i40e_aqc_opc_configure_vsi_tc_bw,
				    cmd_details);
}

 
int
i40e_aq_config_switch_comp_ets(struct i40e_hw *hw,
			       u16 seid,
			       struct i40e_aqc_configure_switching_comp_ets_data *ets_data,
			       enum i40e_admin_queue_opc opcode,
			       struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)ets_data,
				    sizeof(*ets_data), opcode, cmd_details);
}

 
int
i40e_aq_config_switch_comp_bw_config(struct i40e_hw *hw,
	u16 seid,
	struct i40e_aqc_configure_switching_comp_bw_config_data *bw_data,
	struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
			    i40e_aqc_opc_configure_switching_comp_bw_config,
			    cmd_details);
}

 
int
i40e_aq_query_vsi_bw_config(struct i40e_hw *hw,
			    u16 seid,
			    struct i40e_aqc_query_vsi_bw_config_resp *bw_data,
			    struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				    i40e_aqc_opc_query_vsi_bw_config,
				    cmd_details);
}

 
int
i40e_aq_query_vsi_ets_sla_config(struct i40e_hw *hw,
				 u16 seid,
				 struct i40e_aqc_query_vsi_ets_sla_config_resp *bw_data,
				 struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				    i40e_aqc_opc_query_vsi_ets_sla_config,
				    cmd_details);
}

 
int
i40e_aq_query_switch_comp_ets_config(struct i40e_hw *hw,
				     u16 seid,
				     struct i40e_aqc_query_switching_comp_ets_config_resp *bw_data,
				     struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				   i40e_aqc_opc_query_switching_comp_ets_config,
				   cmd_details);
}

 
int
i40e_aq_query_port_ets_config(struct i40e_hw *hw,
			      u16 seid,
			      struct i40e_aqc_query_port_ets_config_resp *bw_data,
			      struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				    i40e_aqc_opc_query_port_ets_config,
				    cmd_details);
}

 
int
i40e_aq_query_switch_comp_bw_config(struct i40e_hw *hw,
				    u16 seid,
				    struct i40e_aqc_query_switching_comp_bw_config_resp *bw_data,
				    struct i40e_asq_cmd_details *cmd_details)
{
	return i40e_aq_tx_sched_cmd(hw, seid, (void *)bw_data, sizeof(*bw_data),
				    i40e_aqc_opc_query_switching_comp_bw_config,
				    cmd_details);
}

 
static int
i40e_validate_filter_settings(struct i40e_hw *hw,
			      struct i40e_filter_control_settings *settings)
{
	u32 fcoe_cntx_size, fcoe_filt_size;
	u32 fcoe_fmax;
	u32 val;

	 
	switch (settings->fcoe_filt_num) {
	case I40E_HASH_FILTER_SIZE_1K:
	case I40E_HASH_FILTER_SIZE_2K:
	case I40E_HASH_FILTER_SIZE_4K:
	case I40E_HASH_FILTER_SIZE_8K:
	case I40E_HASH_FILTER_SIZE_16K:
	case I40E_HASH_FILTER_SIZE_32K:
		fcoe_filt_size = I40E_HASH_FILTER_BASE_SIZE;
		fcoe_filt_size <<= (u32)settings->fcoe_filt_num;
		break;
	default:
		return -EINVAL;
	}

	switch (settings->fcoe_cntx_num) {
	case I40E_DMA_CNTX_SIZE_512:
	case I40E_DMA_CNTX_SIZE_1K:
	case I40E_DMA_CNTX_SIZE_2K:
	case I40E_DMA_CNTX_SIZE_4K:
		fcoe_cntx_size = I40E_DMA_CNTX_BASE_SIZE;
		fcoe_cntx_size <<= (u32)settings->fcoe_cntx_num;
		break;
	default:
		return -EINVAL;
	}

	 
	switch (settings->pe_filt_num) {
	case I40E_HASH_FILTER_SIZE_1K:
	case I40E_HASH_FILTER_SIZE_2K:
	case I40E_HASH_FILTER_SIZE_4K:
	case I40E_HASH_FILTER_SIZE_8K:
	case I40E_HASH_FILTER_SIZE_16K:
	case I40E_HASH_FILTER_SIZE_32K:
	case I40E_HASH_FILTER_SIZE_64K:
	case I40E_HASH_FILTER_SIZE_128K:
	case I40E_HASH_FILTER_SIZE_256K:
	case I40E_HASH_FILTER_SIZE_512K:
	case I40E_HASH_FILTER_SIZE_1M:
		break;
	default:
		return -EINVAL;
	}

	switch (settings->pe_cntx_num) {
	case I40E_DMA_CNTX_SIZE_512:
	case I40E_DMA_CNTX_SIZE_1K:
	case I40E_DMA_CNTX_SIZE_2K:
	case I40E_DMA_CNTX_SIZE_4K:
	case I40E_DMA_CNTX_SIZE_8K:
	case I40E_DMA_CNTX_SIZE_16K:
	case I40E_DMA_CNTX_SIZE_32K:
	case I40E_DMA_CNTX_SIZE_64K:
	case I40E_DMA_CNTX_SIZE_128K:
	case I40E_DMA_CNTX_SIZE_256K:
		break;
	default:
		return -EINVAL;
	}

	 
	val = rd32(hw, I40E_GLHMC_FCOEFMAX);
	fcoe_fmax = (val & I40E_GLHMC_FCOEFMAX_PMFCOEFMAX_MASK)
		     >> I40E_GLHMC_FCOEFMAX_PMFCOEFMAX_SHIFT;
	if (fcoe_filt_size + fcoe_cntx_size >  fcoe_fmax)
		return -EINVAL;

	return 0;
}

 
int i40e_set_filter_control(struct i40e_hw *hw,
			    struct i40e_filter_control_settings *settings)
{
	u32 hash_lut_size = 0;
	int ret = 0;
	u32 val;

	if (!settings)
		return -EINVAL;

	 
	ret = i40e_validate_filter_settings(hw, settings);
	if (ret)
		return ret;

	 
	val = i40e_read_rx_ctl(hw, I40E_PFQF_CTL_0);

	 
	val &= ~I40E_PFQF_CTL_0_PEHSIZE_MASK;
	val |= ((u32)settings->pe_filt_num << I40E_PFQF_CTL_0_PEHSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PEHSIZE_MASK;
	 
	val &= ~I40E_PFQF_CTL_0_PEDSIZE_MASK;
	val |= ((u32)settings->pe_cntx_num << I40E_PFQF_CTL_0_PEDSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PEDSIZE_MASK;

	 
	val &= ~I40E_PFQF_CTL_0_PFFCHSIZE_MASK;
	val |= ((u32)settings->fcoe_filt_num <<
			I40E_PFQF_CTL_0_PFFCHSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PFFCHSIZE_MASK;
	 
	val &= ~I40E_PFQF_CTL_0_PFFCDSIZE_MASK;
	val |= ((u32)settings->fcoe_cntx_num <<
			I40E_PFQF_CTL_0_PFFCDSIZE_SHIFT) &
		I40E_PFQF_CTL_0_PFFCDSIZE_MASK;

	 
	val &= ~I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;
	if (settings->hash_lut_size == I40E_HASH_LUT_SIZE_512)
		hash_lut_size = 1;
	val |= (hash_lut_size << I40E_PFQF_CTL_0_HASHLUTSIZE_SHIFT) &
		I40E_PFQF_CTL_0_HASHLUTSIZE_MASK;

	 
	if (settings->enable_fdir)
		val |= I40E_PFQF_CTL_0_FD_ENA_MASK;
	if (settings->enable_ethtype)
		val |= I40E_PFQF_CTL_0_ETYPE_ENA_MASK;
	if (settings->enable_macvlan)
		val |= I40E_PFQF_CTL_0_MACVLAN_ENA_MASK;

	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, val);

	return 0;
}

 
int i40e_aq_add_rem_control_packet_filter(struct i40e_hw *hw,
					  u8 *mac_addr, u16 ethtype, u16 flags,
					  u16 vsi_seid, u16 queue, bool is_add,
					  struct i40e_control_filter_stats *stats,
					  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_remove_control_packet_filter *cmd =
		(struct i40e_aqc_add_remove_control_packet_filter *)
		&desc.params.raw;
	struct i40e_aqc_add_remove_control_packet_filter_completion *resp =
		(struct i40e_aqc_add_remove_control_packet_filter_completion *)
		&desc.params.raw;
	int status;

	if (vsi_seid == 0)
		return -EINVAL;

	if (is_add) {
		i40e_fill_default_direct_cmd_desc(&desc,
				i40e_aqc_opc_add_control_packet_filter);
		cmd->queue = cpu_to_le16(queue);
	} else {
		i40e_fill_default_direct_cmd_desc(&desc,
				i40e_aqc_opc_remove_control_packet_filter);
	}

	if (mac_addr)
		ether_addr_copy(cmd->mac, mac_addr);

	cmd->etype = cpu_to_le16(ethtype);
	cmd->flags = cpu_to_le16(flags);
	cmd->seid = cpu_to_le16(vsi_seid);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (!status && stats) {
		stats->mac_etype_used = le16_to_cpu(resp->mac_etype_used);
		stats->etype_used = le16_to_cpu(resp->etype_used);
		stats->mac_etype_free = le16_to_cpu(resp->mac_etype_free);
		stats->etype_free = le16_to_cpu(resp->etype_free);
	}

	return status;
}

 
void i40e_add_filter_to_drop_tx_flow_control_frames(struct i40e_hw *hw,
						    u16 seid)
{
#define I40E_FLOW_CONTROL_ETHTYPE 0x8808
	u16 flag = I40E_AQC_ADD_CONTROL_PACKET_FLAGS_IGNORE_MAC |
		   I40E_AQC_ADD_CONTROL_PACKET_FLAGS_DROP |
		   I40E_AQC_ADD_CONTROL_PACKET_FLAGS_TX;
	u16 ethtype = I40E_FLOW_CONTROL_ETHTYPE;
	int status;

	status = i40e_aq_add_rem_control_packet_filter(hw, NULL, ethtype, flag,
						       seid, 0, true, NULL,
						       NULL);
	if (status)
		hw_dbg(hw, "Ethtype Filter Add failed: Error pruning Tx flow control frames\n");
}

 
static int i40e_aq_alternate_read(struct i40e_hw *hw,
				  u32 reg_addr0, u32 *reg_val0,
				  u32 reg_addr1, u32 *reg_val1)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_alternate_write *cmd_resp =
		(struct i40e_aqc_alternate_write *)&desc.params.raw;
	int status;

	if (!reg_val0)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_alternate_read);
	cmd_resp->address0 = cpu_to_le32(reg_addr0);
	cmd_resp->address1 = cpu_to_le32(reg_addr1);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, NULL);

	if (!status) {
		*reg_val0 = le32_to_cpu(cmd_resp->data0);

		if (reg_val1)
			*reg_val1 = le32_to_cpu(cmd_resp->data1);
	}

	return status;
}

 
int i40e_aq_suspend_port_tx(struct i40e_hw *hw, u16 seid,
			    struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aqc_tx_sched_ind *cmd;
	struct i40e_aq_desc desc;
	int status;

	cmd = (struct i40e_aqc_tx_sched_ind *)&desc.params.raw;
	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_suspend_port_tx);
	cmd->vsi_seid = cpu_to_le16(seid);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_resume_port_tx(struct i40e_hw *hw,
			   struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_resume_port_tx);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
void i40e_set_pci_config_data(struct i40e_hw *hw, u16 link_status)
{
	hw->bus.type = i40e_bus_type_pci_express;

	switch (link_status & PCI_EXP_LNKSTA_NLW) {
	case PCI_EXP_LNKSTA_NLW_X1:
		hw->bus.width = i40e_bus_width_pcie_x1;
		break;
	case PCI_EXP_LNKSTA_NLW_X2:
		hw->bus.width = i40e_bus_width_pcie_x2;
		break;
	case PCI_EXP_LNKSTA_NLW_X4:
		hw->bus.width = i40e_bus_width_pcie_x4;
		break;
	case PCI_EXP_LNKSTA_NLW_X8:
		hw->bus.width = i40e_bus_width_pcie_x8;
		break;
	default:
		hw->bus.width = i40e_bus_width_unknown;
		break;
	}

	switch (link_status & PCI_EXP_LNKSTA_CLS) {
	case PCI_EXP_LNKSTA_CLS_2_5GB:
		hw->bus.speed = i40e_bus_speed_2500;
		break;
	case PCI_EXP_LNKSTA_CLS_5_0GB:
		hw->bus.speed = i40e_bus_speed_5000;
		break;
	case PCI_EXP_LNKSTA_CLS_8_0GB:
		hw->bus.speed = i40e_bus_speed_8000;
		break;
	default:
		hw->bus.speed = i40e_bus_speed_unknown;
		break;
	}
}

 
int i40e_aq_debug_dump(struct i40e_hw *hw, u8 cluster_id,
		       u8 table_id, u32 start_index, u16 buff_size,
		       void *buff, u16 *ret_buff_size,
		       u8 *ret_next_table, u32 *ret_next_index,
		       struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_debug_dump_internals *cmd =
		(struct i40e_aqc_debug_dump_internals *)&desc.params.raw;
	struct i40e_aqc_debug_dump_internals *resp =
		(struct i40e_aqc_debug_dump_internals *)&desc.params.raw;
	int status;

	if (buff_size == 0 || !buff)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_debug_dump_internals);
	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	cmd->cluster_id = cluster_id;
	cmd->table_id = table_id;
	cmd->idx = cpu_to_le32(start_index);

	desc.datalen = cpu_to_le16(buff_size);

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	if (!status) {
		if (ret_buff_size)
			*ret_buff_size = le16_to_cpu(desc.datalen);
		if (ret_next_table)
			*ret_next_table = resp->table_id;
		if (ret_next_index)
			*ret_next_index = le32_to_cpu(resp->idx);
	}

	return status;
}

 
int i40e_read_bw_from_alt_ram(struct i40e_hw *hw,
			      u32 *max_bw, u32 *min_bw,
			      bool *min_valid, bool *max_valid)
{
	u32 max_bw_addr, min_bw_addr;
	int status;

	 
	max_bw_addr = I40E_ALT_STRUCT_FIRST_PF_OFFSET +
		      I40E_ALT_STRUCT_MAX_BW_OFFSET +
		      (I40E_ALT_STRUCT_DWORDS_PER_PF * hw->pf_id);
	min_bw_addr = I40E_ALT_STRUCT_FIRST_PF_OFFSET +
		      I40E_ALT_STRUCT_MIN_BW_OFFSET +
		      (I40E_ALT_STRUCT_DWORDS_PER_PF * hw->pf_id);

	 
	status = i40e_aq_alternate_read(hw, max_bw_addr, max_bw,
					min_bw_addr, min_bw);

	if (*min_bw & I40E_ALT_BW_VALID_MASK)
		*min_valid = true;
	else
		*min_valid = false;

	if (*max_bw & I40E_ALT_BW_VALID_MASK)
		*max_valid = true;
	else
		*max_valid = false;

	return status;
}

 
int
i40e_aq_configure_partition_bw(struct i40e_hw *hw,
			       struct i40e_aqc_configure_partition_bw_data *bw_data,
			       struct i40e_asq_cmd_details *cmd_details)
{
	u16 bwd_size = sizeof(*bw_data);
	struct i40e_aq_desc desc;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_configure_partition_bw);

	 
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_RD);

	if (bwd_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	desc.datalen = cpu_to_le16(bwd_size);

	status = i40e_asq_send_command(hw, &desc, bw_data, bwd_size,
				       cmd_details);

	return status;
}

 
int i40e_read_phy_register_clause22(struct i40e_hw *hw,
				    u16 reg, u8 phy_addr, u16 *value)
{
	u8 port_num = (u8)hw->func_caps.mdio_port_num;
	int status = -EIO;
	u32 command = 0;
	u16 retry = 1000;

	command = (reg << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE22_OPCODE_READ_MASK) |
		  (I40E_MDIO_CLAUSE22_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK);
	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		udelay(10);
		retry--;
	} while (retry);

	if (status) {
		i40e_debug(hw, I40E_DEBUG_PHY,
			   "PHY: Can't write command to external PHY.\n");
	} else {
		command = rd32(hw, I40E_GLGEN_MSRWD(port_num));
		*value = (command & I40E_GLGEN_MSRWD_MDIRDDATA_MASK) >>
			 I40E_GLGEN_MSRWD_MDIRDDATA_SHIFT;
	}

	return status;
}

 
int i40e_write_phy_register_clause22(struct i40e_hw *hw,
				     u16 reg, u8 phy_addr, u16 value)
{
	u8 port_num = (u8)hw->func_caps.mdio_port_num;
	int status = -EIO;
	u32 command  = 0;
	u16 retry = 1000;

	command = value << I40E_GLGEN_MSRWD_MDIWRDATA_SHIFT;
	wr32(hw, I40E_GLGEN_MSRWD(port_num), command);

	command = (reg << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE22_OPCODE_WRITE_MASK) |
		  (I40E_MDIO_CLAUSE22_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK);

	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		udelay(10);
		retry--;
	} while (retry);

	return status;
}

 
int i40e_read_phy_register_clause45(struct i40e_hw *hw,
				    u8 page, u16 reg, u8 phy_addr, u16 *value)
{
	u8 port_num = hw->func_caps.mdio_port_num;
	int status = -EIO;
	u32 command = 0;
	u16 retry = 1000;

	command = (reg << I40E_GLGEN_MSCA_MDIADD_SHIFT) |
		  (page << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE45_OPCODE_ADDRESS_MASK) |
		  (I40E_MDIO_CLAUSE45_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK) |
		  (I40E_GLGEN_MSCA_MDIINPROGEN_MASK);
	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		usleep_range(10, 20);
		retry--;
	} while (retry);

	if (status) {
		i40e_debug(hw, I40E_DEBUG_PHY,
			   "PHY: Can't write command to external PHY.\n");
		goto phy_read_end;
	}

	command = (page << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE45_OPCODE_READ_MASK) |
		  (I40E_MDIO_CLAUSE45_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK) |
		  (I40E_GLGEN_MSCA_MDIINPROGEN_MASK);
	status = -EIO;
	retry = 1000;
	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		usleep_range(10, 20);
		retry--;
	} while (retry);

	if (!status) {
		command = rd32(hw, I40E_GLGEN_MSRWD(port_num));
		*value = (command & I40E_GLGEN_MSRWD_MDIRDDATA_MASK) >>
			 I40E_GLGEN_MSRWD_MDIRDDATA_SHIFT;
	} else {
		i40e_debug(hw, I40E_DEBUG_PHY,
			   "PHY: Can't read register value from external PHY.\n");
	}

phy_read_end:
	return status;
}

 
int i40e_write_phy_register_clause45(struct i40e_hw *hw,
				     u8 page, u16 reg, u8 phy_addr, u16 value)
{
	u8 port_num = hw->func_caps.mdio_port_num;
	int status = -EIO;
	u16 retry = 1000;
	u32 command = 0;

	command = (reg << I40E_GLGEN_MSCA_MDIADD_SHIFT) |
		  (page << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE45_OPCODE_ADDRESS_MASK) |
		  (I40E_MDIO_CLAUSE45_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK) |
		  (I40E_GLGEN_MSCA_MDIINPROGEN_MASK);
	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		usleep_range(10, 20);
		retry--;
	} while (retry);
	if (status) {
		i40e_debug(hw, I40E_DEBUG_PHY,
			   "PHY: Can't write command to external PHY.\n");
		goto phy_write_end;
	}

	command = value << I40E_GLGEN_MSRWD_MDIWRDATA_SHIFT;
	wr32(hw, I40E_GLGEN_MSRWD(port_num), command);

	command = (page << I40E_GLGEN_MSCA_DEVADD_SHIFT) |
		  (phy_addr << I40E_GLGEN_MSCA_PHYADD_SHIFT) |
		  (I40E_MDIO_CLAUSE45_OPCODE_WRITE_MASK) |
		  (I40E_MDIO_CLAUSE45_STCODE_MASK) |
		  (I40E_GLGEN_MSCA_MDICMD_MASK) |
		  (I40E_GLGEN_MSCA_MDIINPROGEN_MASK);
	status = -EIO;
	retry = 1000;
	wr32(hw, I40E_GLGEN_MSCA(port_num), command);
	do {
		command = rd32(hw, I40E_GLGEN_MSCA(port_num));
		if (!(command & I40E_GLGEN_MSCA_MDICMD_MASK)) {
			status = 0;
			break;
		}
		usleep_range(10, 20);
		retry--;
	} while (retry);

phy_write_end:
	return status;
}

 
int i40e_write_phy_register(struct i40e_hw *hw,
			    u8 page, u16 reg, u8 phy_addr, u16 value)
{
	int status;

	switch (hw->device_id) {
	case I40E_DEV_ID_1G_BASE_T_X722:
		status = i40e_write_phy_register_clause22(hw, reg, phy_addr,
							  value);
		break;
	case I40E_DEV_ID_1G_BASE_T_BC:
	case I40E_DEV_ID_5G_BASE_T_BC:
	case I40E_DEV_ID_10G_BASE_T:
	case I40E_DEV_ID_10G_BASE_T4:
	case I40E_DEV_ID_10G_BASE_T_BC:
	case I40E_DEV_ID_10G_BASE_T_X722:
	case I40E_DEV_ID_25G_B:
	case I40E_DEV_ID_25G_SFP28:
		status = i40e_write_phy_register_clause45(hw, page, reg,
							  phy_addr, value);
		break;
	default:
		status = -EIO;
		break;
	}

	return status;
}

 
int i40e_read_phy_register(struct i40e_hw *hw,
			   u8 page, u16 reg, u8 phy_addr, u16 *value)
{
	int status;

	switch (hw->device_id) {
	case I40E_DEV_ID_1G_BASE_T_X722:
		status = i40e_read_phy_register_clause22(hw, reg, phy_addr,
							 value);
		break;
	case I40E_DEV_ID_1G_BASE_T_BC:
	case I40E_DEV_ID_5G_BASE_T_BC:
	case I40E_DEV_ID_10G_BASE_T:
	case I40E_DEV_ID_10G_BASE_T4:
	case I40E_DEV_ID_10G_BASE_T_BC:
	case I40E_DEV_ID_10G_BASE_T_X722:
	case I40E_DEV_ID_25G_B:
	case I40E_DEV_ID_25G_SFP28:
		status = i40e_read_phy_register_clause45(hw, page, reg,
							 phy_addr, value);
		break;
	default:
		status = -EIO;
		break;
	}

	return status;
}

 
u8 i40e_get_phy_address(struct i40e_hw *hw, u8 dev_num)
{
	u8 port_num = hw->func_caps.mdio_port_num;
	u32 reg_val = rd32(hw, I40E_GLGEN_MDIO_I2C_SEL(port_num));

	return (u8)(reg_val >> ((dev_num + 1) * 5)) & 0x1f;
}

 
int i40e_blink_phy_link_led(struct i40e_hw *hw,
			    u32 time, u32 interval)
{
	u16 led_addr = I40E_PHY_LED_PROV_REG_1;
	u16 gpio_led_port;
	u8 phy_addr = 0;
	int status = 0;
	u16 led_ctl;
	u8 port_num;
	u16 led_reg;
	u32 i;

	i = rd32(hw, I40E_PFGEN_PORTNUM);
	port_num = (u8)(i & I40E_PFGEN_PORTNUM_PORT_NUM_MASK);
	phy_addr = i40e_get_phy_address(hw, port_num);

	for (gpio_led_port = 0; gpio_led_port < 3; gpio_led_port++,
	     led_addr++) {
		status = i40e_read_phy_register_clause45(hw,
							 I40E_PHY_COM_REG_PAGE,
							 led_addr, phy_addr,
							 &led_reg);
		if (status)
			goto phy_blinking_end;
		led_ctl = led_reg;
		if (led_reg & I40E_PHY_LED_LINK_MODE_MASK) {
			led_reg = 0;
			status = i40e_write_phy_register_clause45(hw,
							 I40E_PHY_COM_REG_PAGE,
							 led_addr, phy_addr,
							 led_reg);
			if (status)
				goto phy_blinking_end;
			break;
		}
	}

	if (time > 0 && interval > 0) {
		for (i = 0; i < time * 1000; i += interval) {
			status = i40e_read_phy_register_clause45(hw,
						I40E_PHY_COM_REG_PAGE,
						led_addr, phy_addr, &led_reg);
			if (status)
				goto restore_config;
			if (led_reg & I40E_PHY_LED_MANUAL_ON)
				led_reg = 0;
			else
				led_reg = I40E_PHY_LED_MANUAL_ON;
			status = i40e_write_phy_register_clause45(hw,
						I40E_PHY_COM_REG_PAGE,
						led_addr, phy_addr, led_reg);
			if (status)
				goto restore_config;
			msleep(interval);
		}
	}

restore_config:
	status = i40e_write_phy_register_clause45(hw,
						  I40E_PHY_COM_REG_PAGE,
						  led_addr, phy_addr, led_ctl);

phy_blinking_end:
	return status;
}

 
static int i40e_led_get_reg(struct i40e_hw *hw, u16 led_addr,
			    u32 *reg_val)
{
	u8 phy_addr = 0;
	u8 port_num;
	int status;
	u32 i;

	*reg_val = 0;
	if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE) {
		status =
		       i40e_aq_get_phy_register(hw,
						I40E_AQ_PHY_REG_ACCESS_EXTERNAL,
						I40E_PHY_COM_REG_PAGE, true,
						I40E_PHY_LED_PROV_REG_1,
						reg_val, NULL);
	} else {
		i = rd32(hw, I40E_PFGEN_PORTNUM);
		port_num = (u8)(i & I40E_PFGEN_PORTNUM_PORT_NUM_MASK);
		phy_addr = i40e_get_phy_address(hw, port_num);
		status = i40e_read_phy_register_clause45(hw,
							 I40E_PHY_COM_REG_PAGE,
							 led_addr, phy_addr,
							 (u16 *)reg_val);
	}
	return status;
}

 
static int i40e_led_set_reg(struct i40e_hw *hw, u16 led_addr,
			    u32 reg_val)
{
	u8 phy_addr = 0;
	u8 port_num;
	int status;
	u32 i;

	if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE) {
		status =
		       i40e_aq_set_phy_register(hw,
						I40E_AQ_PHY_REG_ACCESS_EXTERNAL,
						I40E_PHY_COM_REG_PAGE, true,
						I40E_PHY_LED_PROV_REG_1,
						reg_val, NULL);
	} else {
		i = rd32(hw, I40E_PFGEN_PORTNUM);
		port_num = (u8)(i & I40E_PFGEN_PORTNUM_PORT_NUM_MASK);
		phy_addr = i40e_get_phy_address(hw, port_num);
		status = i40e_write_phy_register_clause45(hw,
							  I40E_PHY_COM_REG_PAGE,
							  led_addr, phy_addr,
							  (u16)reg_val);
	}

	return status;
}

 
int i40e_led_get_phy(struct i40e_hw *hw, u16 *led_addr,
		     u16 *val)
{
	u16 gpio_led_port;
	u8 phy_addr = 0;
	u32 reg_val_aq;
	int status = 0;
	u16 temp_addr;
	u16 reg_val;
	u8 port_num;
	u32 i;

	if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE) {
		status =
		      i40e_aq_get_phy_register(hw,
					       I40E_AQ_PHY_REG_ACCESS_EXTERNAL,
					       I40E_PHY_COM_REG_PAGE, true,
					       I40E_PHY_LED_PROV_REG_1,
					       &reg_val_aq, NULL);
		if (status == 0)
			*val = (u16)reg_val_aq;
		return status;
	}
	temp_addr = I40E_PHY_LED_PROV_REG_1;
	i = rd32(hw, I40E_PFGEN_PORTNUM);
	port_num = (u8)(i & I40E_PFGEN_PORTNUM_PORT_NUM_MASK);
	phy_addr = i40e_get_phy_address(hw, port_num);

	for (gpio_led_port = 0; gpio_led_port < 3; gpio_led_port++,
	     temp_addr++) {
		status = i40e_read_phy_register_clause45(hw,
							 I40E_PHY_COM_REG_PAGE,
							 temp_addr, phy_addr,
							 &reg_val);
		if (status)
			return status;
		*val = reg_val;
		if (reg_val & I40E_PHY_LED_LINK_MODE_MASK) {
			*led_addr = temp_addr;
			break;
		}
	}
	return status;
}

 
int i40e_led_set_phy(struct i40e_hw *hw, bool on,
		     u16 led_addr, u32 mode)
{
	u32 led_ctl = 0;
	u32 led_reg = 0;
	int status = 0;

	status = i40e_led_get_reg(hw, led_addr, &led_reg);
	if (status)
		return status;
	led_ctl = led_reg;
	if (led_reg & I40E_PHY_LED_LINK_MODE_MASK) {
		led_reg = 0;
		status = i40e_led_set_reg(hw, led_addr, led_reg);
		if (status)
			return status;
	}
	status = i40e_led_get_reg(hw, led_addr, &led_reg);
	if (status)
		goto restore_config;
	if (on)
		led_reg = I40E_PHY_LED_MANUAL_ON;
	else
		led_reg = 0;

	status = i40e_led_set_reg(hw, led_addr, led_reg);
	if (status)
		goto restore_config;
	if (mode & I40E_PHY_LED_MODE_ORIG) {
		led_ctl = (mode & I40E_PHY_LED_MODE_MASK);
		status = i40e_led_set_reg(hw, led_addr, led_ctl);
	}
	return status;

restore_config:
	status = i40e_led_set_reg(hw, led_addr, led_ctl);
	return status;
}

 
int i40e_aq_rx_ctl_read_register(struct i40e_hw *hw,
				 u32 reg_addr, u32 *reg_val,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_rx_ctl_reg_read_write *cmd_resp =
		(struct i40e_aqc_rx_ctl_reg_read_write *)&desc.params.raw;
	int status;

	if (!reg_val)
		return -EINVAL;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_rx_ctl_reg_read);

	cmd_resp->address = cpu_to_le32(reg_addr);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	if (status == 0)
		*reg_val = le32_to_cpu(cmd_resp->value);

	return status;
}

 
u32 i40e_read_rx_ctl(struct i40e_hw *hw, u32 reg_addr)
{
	bool use_register;
	int status = 0;
	int retry = 5;
	u32 val = 0;

	use_register = (((hw->aq.api_maj_ver == 1) &&
			(hw->aq.api_min_ver < 5)) ||
			(hw->mac.type == I40E_MAC_X722));
	if (!use_register) {
do_retry:
		status = i40e_aq_rx_ctl_read_register(hw, reg_addr, &val, NULL);
		if (hw->aq.asq_last_status == I40E_AQ_RC_EAGAIN && retry) {
			usleep_range(1000, 2000);
			retry--;
			goto do_retry;
		}
	}

	 
	if (status || use_register)
		val = rd32(hw, reg_addr);

	return val;
}

 
int i40e_aq_rx_ctl_write_register(struct i40e_hw *hw,
				  u32 reg_addr, u32 reg_val,
				  struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_rx_ctl_reg_read_write *cmd =
		(struct i40e_aqc_rx_ctl_reg_read_write *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_rx_ctl_reg_write);

	cmd->address = cpu_to_le32(reg_addr);
	cmd->value = cpu_to_le32(reg_val);

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
void i40e_write_rx_ctl(struct i40e_hw *hw, u32 reg_addr, u32 reg_val)
{
	bool use_register;
	int status = 0;
	int retry = 5;

	use_register = (((hw->aq.api_maj_ver == 1) &&
			(hw->aq.api_min_ver < 5)) ||
			(hw->mac.type == I40E_MAC_X722));
	if (!use_register) {
do_retry:
		status = i40e_aq_rx_ctl_write_register(hw, reg_addr,
						       reg_val, NULL);
		if (hw->aq.asq_last_status == I40E_AQ_RC_EAGAIN && retry) {
			usleep_range(1000, 2000);
			retry--;
			goto do_retry;
		}
	}

	 
	if (status || use_register)
		wr32(hw, reg_addr, reg_val);
}

 
static void i40e_mdio_if_number_selection(struct i40e_hw *hw, bool set_mdio,
					  u8 mdio_num,
					  struct i40e_aqc_phy_register_access *cmd)
{
	if (set_mdio && cmd->phy_interface == I40E_AQ_PHY_REG_ACCESS_EXTERNAL) {
		if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_EXTENDED)
			cmd->cmd_flags |=
				I40E_AQ_PHY_REG_ACCESS_SET_MDIO_IF_NUMBER |
				((mdio_num <<
				I40E_AQ_PHY_REG_ACCESS_MDIO_IF_NUMBER_SHIFT) &
				I40E_AQ_PHY_REG_ACCESS_MDIO_IF_NUMBER_MASK);
		else
			i40e_debug(hw, I40E_DEBUG_PHY,
				   "MDIO I/F number selection not supported by current FW version.\n");
	}
}

 
int i40e_aq_set_phy_register_ext(struct i40e_hw *hw,
				 u8 phy_select, u8 dev_addr, bool page_change,
				 bool set_mdio, u8 mdio_num,
				 u32 reg_addr, u32 reg_val,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_phy_register_access *cmd =
		(struct i40e_aqc_phy_register_access *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_set_phy_register);

	cmd->phy_interface = phy_select;
	cmd->dev_address = dev_addr;
	cmd->reg_address = cpu_to_le32(reg_addr);
	cmd->reg_value = cpu_to_le32(reg_val);

	i40e_mdio_if_number_selection(hw, set_mdio, mdio_num, cmd);

	if (!page_change)
		cmd->cmd_flags = I40E_AQ_PHY_REG_ACCESS_DONT_CHANGE_QSFP_PAGE;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);

	return status;
}

 
int i40e_aq_get_phy_register_ext(struct i40e_hw *hw,
				 u8 phy_select, u8 dev_addr, bool page_change,
				 bool set_mdio, u8 mdio_num,
				 u32 reg_addr, u32 *reg_val,
				 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_phy_register_access *cmd =
		(struct i40e_aqc_phy_register_access *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_get_phy_register);

	cmd->phy_interface = phy_select;
	cmd->dev_address = dev_addr;
	cmd->reg_address = cpu_to_le32(reg_addr);

	i40e_mdio_if_number_selection(hw, set_mdio, mdio_num, cmd);

	if (!page_change)
		cmd->cmd_flags = I40E_AQ_PHY_REG_ACCESS_DONT_CHANGE_QSFP_PAGE;

	status = i40e_asq_send_command(hw, &desc, NULL, 0, cmd_details);
	if (!status)
		*reg_val = le32_to_cpu(cmd->reg_value);

	return status;
}

 
int i40e_aq_write_ddp(struct i40e_hw *hw, void *buff,
		      u16 buff_size, u32 track_id,
		      u32 *error_offset, u32 *error_info,
		      struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_write_personalization_profile *cmd =
		(struct i40e_aqc_write_personalization_profile *)
		&desc.params.raw;
	struct i40e_aqc_write_ddp_resp *resp;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_write_personalization_profile);

	desc.flags |= cpu_to_le16(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);

	desc.datalen = cpu_to_le16(buff_size);

	cmd->profile_track_id = cpu_to_le32(track_id);

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);
	if (!status) {
		resp = (struct i40e_aqc_write_ddp_resp *)&desc.params.raw;
		if (error_offset)
			*error_offset = le32_to_cpu(resp->error_offset);
		if (error_info)
			*error_info = le32_to_cpu(resp->error_info);
	}

	return status;
}

 
int i40e_aq_get_ddp_list(struct i40e_hw *hw, void *buff,
			 u16 buff_size, u8 flags,
			 struct i40e_asq_cmd_details *cmd_details)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_get_applied_profiles *cmd =
		(struct i40e_aqc_get_applied_profiles *)&desc.params.raw;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_get_personalization_profile_list);

	desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_BUF);
	if (buff_size > I40E_AQ_LARGE_BUF)
		desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
	desc.datalen = cpu_to_le16(buff_size);

	cmd->flags = flags;

	status = i40e_asq_send_command(hw, &desc, buff, buff_size, cmd_details);

	return status;
}

 
struct i40e_generic_seg_header *
i40e_find_segment_in_package(u32 segment_type,
			     struct i40e_package_header *pkg_hdr)
{
	struct i40e_generic_seg_header *segment;
	u32 i;

	 
	for (i = 0; i < pkg_hdr->segment_count; i++) {
		segment =
			(struct i40e_generic_seg_header *)((u8 *)pkg_hdr +
			 pkg_hdr->segment_offset[i]);

		if (segment->type == segment_type)
			return segment;
	}

	return NULL;
}

 
#define I40E_SECTION_TABLE(profile, sec_tbl)				\
	do {								\
		struct i40e_profile_segment *p = (profile);		\
		u32 count;						\
		u32 *nvm;						\
		count = p->device_table_count;				\
		nvm = (u32 *)&p->device_table[count];			\
		sec_tbl = (struct i40e_section_table *)&nvm[nvm[0] + 1]; \
	} while (0)

 
#define I40E_SECTION_HEADER(profile, offset)				\
	(struct i40e_profile_section_header *)((u8 *)(profile) + (offset))

 
struct i40e_profile_section_header *
i40e_find_section_in_profile(u32 section_type,
			     struct i40e_profile_segment *profile)
{
	struct i40e_profile_section_header *sec;
	struct i40e_section_table *sec_tbl;
	u32 sec_off;
	u32 i;

	if (profile->header.type != SEGMENT_TYPE_I40E)
		return NULL;

	I40E_SECTION_TABLE(profile, sec_tbl);

	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = I40E_SECTION_HEADER(profile, sec_off);
		if (sec->section.type == section_type)
			return sec;
	}

	return NULL;
}

 
static int i40e_ddp_exec_aq_section(struct i40e_hw *hw,
				    struct i40e_profile_aq_section *aq)
{
	struct i40e_aq_desc desc;
	u8 *msg = NULL;
	u16 msglen;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc, aq->opcode);
	desc.flags |= cpu_to_le16(aq->flags);
	memcpy(desc.params.raw, aq->param, sizeof(desc.params.raw));

	msglen = aq->datalen;
	if (msglen) {
		desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF |
						I40E_AQ_FLAG_RD));
		if (msglen > I40E_AQ_LARGE_BUF)
			desc.flags |= cpu_to_le16((u16)I40E_AQ_FLAG_LB);
		desc.datalen = cpu_to_le16(msglen);
		msg = &aq->data[0];
	}

	status = i40e_asq_send_command(hw, &desc, msg, msglen, NULL);

	if (status) {
		i40e_debug(hw, I40E_DEBUG_PACKAGE,
			   "unable to exec DDP AQ opcode %u, error %d\n",
			   aq->opcode, status);
		return status;
	}

	 
	memcpy(aq->param, desc.params.raw, sizeof(desc.params.raw));

	return 0;
}

 
static int
i40e_validate_profile(struct i40e_hw *hw, struct i40e_profile_segment *profile,
		      u32 track_id, bool rollback)
{
	struct i40e_profile_section_header *sec = NULL;
	struct i40e_section_table *sec_tbl;
	u32 vendor_dev_id;
	int status = 0;
	u32 dev_cnt;
	u32 sec_off;
	u32 i;

	if (track_id == I40E_DDP_TRACKID_INVALID) {
		i40e_debug(hw, I40E_DEBUG_PACKAGE, "Invalid track_id\n");
		return -EOPNOTSUPP;
	}

	dev_cnt = profile->device_table_count;
	for (i = 0; i < dev_cnt; i++) {
		vendor_dev_id = profile->device_table[i].vendor_dev_id;
		if ((vendor_dev_id >> 16) == PCI_VENDOR_ID_INTEL &&
		    hw->device_id == (vendor_dev_id & 0xFFFF))
			break;
	}
	if (dev_cnt && i == dev_cnt) {
		i40e_debug(hw, I40E_DEBUG_PACKAGE,
			   "Device doesn't support DDP\n");
		return -ENODEV;
	}

	I40E_SECTION_TABLE(profile, sec_tbl);

	 
	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = I40E_SECTION_HEADER(profile, sec_off);
		if (rollback) {
			if (sec->section.type == SECTION_TYPE_MMIO ||
			    sec->section.type == SECTION_TYPE_AQ ||
			    sec->section.type == SECTION_TYPE_RB_AQ) {
				i40e_debug(hw, I40E_DEBUG_PACKAGE,
					   "Not a roll-back package\n");
				return -EOPNOTSUPP;
			}
		} else {
			if (sec->section.type == SECTION_TYPE_RB_AQ ||
			    sec->section.type == SECTION_TYPE_RB_MMIO) {
				i40e_debug(hw, I40E_DEBUG_PACKAGE,
					   "Not an original package\n");
				return -EOPNOTSUPP;
			}
		}
	}

	return status;
}

 
int
i40e_write_profile(struct i40e_hw *hw, struct i40e_profile_segment *profile,
		   u32 track_id)
{
	struct i40e_profile_section_header *sec = NULL;
	struct i40e_profile_aq_section *ddp_aq;
	struct i40e_section_table *sec_tbl;
	u32 offset = 0, info = 0;
	u32 section_size = 0;
	int status = 0;
	u32 sec_off;
	u32 i;

	status = i40e_validate_profile(hw, profile, track_id, false);
	if (status)
		return status;

	I40E_SECTION_TABLE(profile, sec_tbl);

	for (i = 0; i < sec_tbl->section_count; i++) {
		sec_off = sec_tbl->section_offset[i];
		sec = I40E_SECTION_HEADER(profile, sec_off);
		 
		if (sec->section.type == SECTION_TYPE_AQ) {
			ddp_aq = (struct i40e_profile_aq_section *)&sec[1];
			status = i40e_ddp_exec_aq_section(hw, ddp_aq);
			if (status) {
				i40e_debug(hw, I40E_DEBUG_PACKAGE,
					   "Failed to execute aq: section %d, opcode %u\n",
					   i, ddp_aq->opcode);
				break;
			}
			sec->section.type = SECTION_TYPE_RB_AQ;
		}

		 
		if (sec->section.type != SECTION_TYPE_MMIO)
			continue;

		section_size = sec->section.size +
			sizeof(struct i40e_profile_section_header);

		 
		status = i40e_aq_write_ddp(hw, (void *)sec, (u16)section_size,
					   track_id, &offset, &info, NULL);
		if (status) {
			i40e_debug(hw, I40E_DEBUG_PACKAGE,
				   "Failed to write profile: section %d, offset %d, info %d\n",
				   i, offset, info);
			break;
		}
	}
	return status;
}

 
int
i40e_rollback_profile(struct i40e_hw *hw, struct i40e_profile_segment *profile,
		      u32 track_id)
{
	struct i40e_profile_section_header *sec = NULL;
	struct i40e_section_table *sec_tbl;
	u32 offset = 0, info = 0;
	u32 section_size = 0;
	int status = 0;
	u32 sec_off;
	int i;

	status = i40e_validate_profile(hw, profile, track_id, true);
	if (status)
		return status;

	I40E_SECTION_TABLE(profile, sec_tbl);

	 
	for (i = sec_tbl->section_count - 1; i >= 0; i--) {
		sec_off = sec_tbl->section_offset[i];
		sec = I40E_SECTION_HEADER(profile, sec_off);

		 
		if (sec->section.type != SECTION_TYPE_RB_MMIO)
			continue;

		section_size = sec->section.size +
			sizeof(struct i40e_profile_section_header);

		 
		status = i40e_aq_write_ddp(hw, (void *)sec, (u16)section_size,
					   track_id, &offset, &info, NULL);
		if (status) {
			i40e_debug(hw, I40E_DEBUG_PACKAGE,
				   "Failed to write profile: section %d, offset %d, info %d\n",
				   i, offset, info);
			break;
		}
	}
	return status;
}

 
int
i40e_add_pinfo_to_list(struct i40e_hw *hw,
		       struct i40e_profile_segment *profile,
		       u8 *profile_info_sec, u32 track_id)
{
	struct i40e_profile_section_header *sec = NULL;
	struct i40e_profile_info *pinfo;
	u32 offset = 0, info = 0;
	int status = 0;

	sec = (struct i40e_profile_section_header *)profile_info_sec;
	sec->tbl_size = 1;
	sec->data_end = sizeof(struct i40e_profile_section_header) +
			sizeof(struct i40e_profile_info);
	sec->section.type = SECTION_TYPE_INFO;
	sec->section.offset = sizeof(struct i40e_profile_section_header);
	sec->section.size = sizeof(struct i40e_profile_info);
	pinfo = (struct i40e_profile_info *)(profile_info_sec +
					     sec->section.offset);
	pinfo->track_id = track_id;
	pinfo->version = profile->version;
	pinfo->op = I40E_DDP_ADD_TRACKID;
	memcpy(pinfo->name, profile->name, I40E_DDP_NAME_SIZE);

	status = i40e_aq_write_ddp(hw, (void *)sec, sec->data_end,
				   track_id, &offset, &info, NULL);

	return status;
}

 
int
i40e_aq_add_cloud_filters(struct i40e_hw *hw, u16 seid,
			  struct i40e_aqc_cloud_filters_element_data *filters,
			  u8 filter_count)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_remove_cloud_filters *cmd =
	(struct i40e_aqc_add_remove_cloud_filters *)&desc.params.raw;
	u16 buff_len;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_add_cloud_filters);

	buff_len = filter_count * sizeof(*filters);
	desc.datalen = cpu_to_le16(buff_len);
	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd->num_filters = filter_count;
	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, filters, buff_len, NULL);

	return status;
}

 
int
i40e_aq_add_cloud_filters_bb(struct i40e_hw *hw, u16 seid,
			     struct i40e_aqc_cloud_filters_element_bb *filters,
			     u8 filter_count)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_remove_cloud_filters *cmd =
	(struct i40e_aqc_add_remove_cloud_filters *)&desc.params.raw;
	u16 buff_len;
	int status;
	int i;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_add_cloud_filters);

	buff_len = filter_count * sizeof(*filters);
	desc.datalen = cpu_to_le16(buff_len);
	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd->num_filters = filter_count;
	cmd->seid = cpu_to_le16(seid);
	cmd->big_buffer_flag = I40E_AQC_ADD_CLOUD_CMD_BB;

	for (i = 0; i < filter_count; i++) {
		u16 tnl_type;
		u32 ti;

		tnl_type = (le16_to_cpu(filters[i].element.flags) &
			   I40E_AQC_ADD_CLOUD_TNL_TYPE_MASK) >>
			   I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT;

		 
		if (tnl_type == I40E_AQC_ADD_CLOUD_TNL_TYPE_GENEVE) {
			ti = le32_to_cpu(filters[i].element.tenant_id);
			filters[i].element.tenant_id = cpu_to_le32(ti << 8);
		}
	}

	status = i40e_asq_send_command(hw, &desc, filters, buff_len, NULL);

	return status;
}

 
int
i40e_aq_rem_cloud_filters(struct i40e_hw *hw, u16 seid,
			  struct i40e_aqc_cloud_filters_element_data *filters,
			  u8 filter_count)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_remove_cloud_filters *cmd =
	(struct i40e_aqc_add_remove_cloud_filters *)&desc.params.raw;
	u16 buff_len;
	int status;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_remove_cloud_filters);

	buff_len = filter_count * sizeof(*filters);
	desc.datalen = cpu_to_le16(buff_len);
	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd->num_filters = filter_count;
	cmd->seid = cpu_to_le16(seid);

	status = i40e_asq_send_command(hw, &desc, filters, buff_len, NULL);

	return status;
}

 
int
i40e_aq_rem_cloud_filters_bb(struct i40e_hw *hw, u16 seid,
			     struct i40e_aqc_cloud_filters_element_bb *filters,
			     u8 filter_count)
{
	struct i40e_aq_desc desc;
	struct i40e_aqc_add_remove_cloud_filters *cmd =
	(struct i40e_aqc_add_remove_cloud_filters *)&desc.params.raw;
	u16 buff_len;
	int status;
	int i;

	i40e_fill_default_direct_cmd_desc(&desc,
					  i40e_aqc_opc_remove_cloud_filters);

	buff_len = filter_count * sizeof(*filters);
	desc.datalen = cpu_to_le16(buff_len);
	desc.flags |= cpu_to_le16((u16)(I40E_AQ_FLAG_BUF | I40E_AQ_FLAG_RD));
	cmd->num_filters = filter_count;
	cmd->seid = cpu_to_le16(seid);
	cmd->big_buffer_flag = I40E_AQC_ADD_CLOUD_CMD_BB;

	for (i = 0; i < filter_count; i++) {
		u16 tnl_type;
		u32 ti;

		tnl_type = (le16_to_cpu(filters[i].element.flags) &
			   I40E_AQC_ADD_CLOUD_TNL_TYPE_MASK) >>
			   I40E_AQC_ADD_CLOUD_TNL_TYPE_SHIFT;

		 
		if (tnl_type == I40E_AQC_ADD_CLOUD_TNL_TYPE_GENEVE) {
			ti = le32_to_cpu(filters[i].element.tenant_id);
			filters[i].element.tenant_id = cpu_to_le32(ti << 8);
		}
	}

	status = i40e_asq_send_command(hw, &desc, filters, buff_len, NULL);

	return status;
}
