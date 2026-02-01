
 

#include "iavf_type.h"
#include "iavf_adminq.h"
#include "iavf_prototype.h"
#include <linux/avf/virtchnl.h>

 
enum iavf_status iavf_set_mac_type(struct iavf_hw *hw)
{
	enum iavf_status status = 0;

	if (hw->vendor_id == PCI_VENDOR_ID_INTEL) {
		switch (hw->device_id) {
		case IAVF_DEV_ID_X722_VF:
			hw->mac.type = IAVF_MAC_X722_VF;
			break;
		case IAVF_DEV_ID_VF:
		case IAVF_DEV_ID_VF_HV:
		case IAVF_DEV_ID_ADAPTIVE_VF:
			hw->mac.type = IAVF_MAC_VF;
			break;
		default:
			hw->mac.type = IAVF_MAC_GENERIC;
			break;
		}
	} else {
		status = IAVF_ERR_DEVICE_NOT_SUPPORTED;
	}

	return status;
}

 
const char *iavf_aq_str(struct iavf_hw *hw, enum iavf_admin_queue_err aq_err)
{
	switch (aq_err) {
	case IAVF_AQ_RC_OK:
		return "OK";
	case IAVF_AQ_RC_EPERM:
		return "IAVF_AQ_RC_EPERM";
	case IAVF_AQ_RC_ENOENT:
		return "IAVF_AQ_RC_ENOENT";
	case IAVF_AQ_RC_ESRCH:
		return "IAVF_AQ_RC_ESRCH";
	case IAVF_AQ_RC_EINTR:
		return "IAVF_AQ_RC_EINTR";
	case IAVF_AQ_RC_EIO:
		return "IAVF_AQ_RC_EIO";
	case IAVF_AQ_RC_ENXIO:
		return "IAVF_AQ_RC_ENXIO";
	case IAVF_AQ_RC_E2BIG:
		return "IAVF_AQ_RC_E2BIG";
	case IAVF_AQ_RC_EAGAIN:
		return "IAVF_AQ_RC_EAGAIN";
	case IAVF_AQ_RC_ENOMEM:
		return "IAVF_AQ_RC_ENOMEM";
	case IAVF_AQ_RC_EACCES:
		return "IAVF_AQ_RC_EACCES";
	case IAVF_AQ_RC_EFAULT:
		return "IAVF_AQ_RC_EFAULT";
	case IAVF_AQ_RC_EBUSY:
		return "IAVF_AQ_RC_EBUSY";
	case IAVF_AQ_RC_EEXIST:
		return "IAVF_AQ_RC_EEXIST";
	case IAVF_AQ_RC_EINVAL:
		return "IAVF_AQ_RC_EINVAL";
	case IAVF_AQ_RC_ENOTTY:
		return "IAVF_AQ_RC_ENOTTY";
	case IAVF_AQ_RC_ENOSPC:
		return "IAVF_AQ_RC_ENOSPC";
	case IAVF_AQ_RC_ENOSYS:
		return "IAVF_AQ_RC_ENOSYS";
	case IAVF_AQ_RC_ERANGE:
		return "IAVF_AQ_RC_ERANGE";
	case IAVF_AQ_RC_EFLUSHED:
		return "IAVF_AQ_RC_EFLUSHED";
	case IAVF_AQ_RC_BAD_ADDR:
		return "IAVF_AQ_RC_BAD_ADDR";
	case IAVF_AQ_RC_EMODE:
		return "IAVF_AQ_RC_EMODE";
	case IAVF_AQ_RC_EFBIG:
		return "IAVF_AQ_RC_EFBIG";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", aq_err);
	return hw->err_str;
}

 
const char *iavf_stat_str(struct iavf_hw *hw, enum iavf_status stat_err)
{
	switch (stat_err) {
	case 0:
		return "OK";
	case IAVF_ERR_NVM:
		return "IAVF_ERR_NVM";
	case IAVF_ERR_NVM_CHECKSUM:
		return "IAVF_ERR_NVM_CHECKSUM";
	case IAVF_ERR_PHY:
		return "IAVF_ERR_PHY";
	case IAVF_ERR_CONFIG:
		return "IAVF_ERR_CONFIG";
	case IAVF_ERR_PARAM:
		return "IAVF_ERR_PARAM";
	case IAVF_ERR_MAC_TYPE:
		return "IAVF_ERR_MAC_TYPE";
	case IAVF_ERR_UNKNOWN_PHY:
		return "IAVF_ERR_UNKNOWN_PHY";
	case IAVF_ERR_LINK_SETUP:
		return "IAVF_ERR_LINK_SETUP";
	case IAVF_ERR_ADAPTER_STOPPED:
		return "IAVF_ERR_ADAPTER_STOPPED";
	case IAVF_ERR_INVALID_MAC_ADDR:
		return "IAVF_ERR_INVALID_MAC_ADDR";
	case IAVF_ERR_DEVICE_NOT_SUPPORTED:
		return "IAVF_ERR_DEVICE_NOT_SUPPORTED";
	case IAVF_ERR_PRIMARY_REQUESTS_PENDING:
		return "IAVF_ERR_PRIMARY_REQUESTS_PENDING";
	case IAVF_ERR_INVALID_LINK_SETTINGS:
		return "IAVF_ERR_INVALID_LINK_SETTINGS";
	case IAVF_ERR_AUTONEG_NOT_COMPLETE:
		return "IAVF_ERR_AUTONEG_NOT_COMPLETE";
	case IAVF_ERR_RESET_FAILED:
		return "IAVF_ERR_RESET_FAILED";
	case IAVF_ERR_SWFW_SYNC:
		return "IAVF_ERR_SWFW_SYNC";
	case IAVF_ERR_NO_AVAILABLE_VSI:
		return "IAVF_ERR_NO_AVAILABLE_VSI";
	case IAVF_ERR_NO_MEMORY:
		return "IAVF_ERR_NO_MEMORY";
	case IAVF_ERR_BAD_PTR:
		return "IAVF_ERR_BAD_PTR";
	case IAVF_ERR_RING_FULL:
		return "IAVF_ERR_RING_FULL";
	case IAVF_ERR_INVALID_PD_ID:
		return "IAVF_ERR_INVALID_PD_ID";
	case IAVF_ERR_INVALID_QP_ID:
		return "IAVF_ERR_INVALID_QP_ID";
	case IAVF_ERR_INVALID_CQ_ID:
		return "IAVF_ERR_INVALID_CQ_ID";
	case IAVF_ERR_INVALID_CEQ_ID:
		return "IAVF_ERR_INVALID_CEQ_ID";
	case IAVF_ERR_INVALID_AEQ_ID:
		return "IAVF_ERR_INVALID_AEQ_ID";
	case IAVF_ERR_INVALID_SIZE:
		return "IAVF_ERR_INVALID_SIZE";
	case IAVF_ERR_INVALID_ARP_INDEX:
		return "IAVF_ERR_INVALID_ARP_INDEX";
	case IAVF_ERR_INVALID_FPM_FUNC_ID:
		return "IAVF_ERR_INVALID_FPM_FUNC_ID";
	case IAVF_ERR_QP_INVALID_MSG_SIZE:
		return "IAVF_ERR_QP_INVALID_MSG_SIZE";
	case IAVF_ERR_QP_TOOMANY_WRS_POSTED:
		return "IAVF_ERR_QP_TOOMANY_WRS_POSTED";
	case IAVF_ERR_INVALID_FRAG_COUNT:
		return "IAVF_ERR_INVALID_FRAG_COUNT";
	case IAVF_ERR_QUEUE_EMPTY:
		return "IAVF_ERR_QUEUE_EMPTY";
	case IAVF_ERR_INVALID_ALIGNMENT:
		return "IAVF_ERR_INVALID_ALIGNMENT";
	case IAVF_ERR_FLUSHED_QUEUE:
		return "IAVF_ERR_FLUSHED_QUEUE";
	case IAVF_ERR_INVALID_PUSH_PAGE_INDEX:
		return "IAVF_ERR_INVALID_PUSH_PAGE_INDEX";
	case IAVF_ERR_INVALID_IMM_DATA_SIZE:
		return "IAVF_ERR_INVALID_IMM_DATA_SIZE";
	case IAVF_ERR_TIMEOUT:
		return "IAVF_ERR_TIMEOUT";
	case IAVF_ERR_OPCODE_MISMATCH:
		return "IAVF_ERR_OPCODE_MISMATCH";
	case IAVF_ERR_CQP_COMPL_ERROR:
		return "IAVF_ERR_CQP_COMPL_ERROR";
	case IAVF_ERR_INVALID_VF_ID:
		return "IAVF_ERR_INVALID_VF_ID";
	case IAVF_ERR_INVALID_HMCFN_ID:
		return "IAVF_ERR_INVALID_HMCFN_ID";
	case IAVF_ERR_BACKING_PAGE_ERROR:
		return "IAVF_ERR_BACKING_PAGE_ERROR";
	case IAVF_ERR_NO_PBLCHUNKS_AVAILABLE:
		return "IAVF_ERR_NO_PBLCHUNKS_AVAILABLE";
	case IAVF_ERR_INVALID_PBLE_INDEX:
		return "IAVF_ERR_INVALID_PBLE_INDEX";
	case IAVF_ERR_INVALID_SD_INDEX:
		return "IAVF_ERR_INVALID_SD_INDEX";
	case IAVF_ERR_INVALID_PAGE_DESC_INDEX:
		return "IAVF_ERR_INVALID_PAGE_DESC_INDEX";
	case IAVF_ERR_INVALID_SD_TYPE:
		return "IAVF_ERR_INVALID_SD_TYPE";
	case IAVF_ERR_MEMCPY_FAILED:
		return "IAVF_ERR_MEMCPY_FAILED";
	case IAVF_ERR_INVALID_HMC_OBJ_INDEX:
		return "IAVF_ERR_INVALID_HMC_OBJ_INDEX";
	case IAVF_ERR_INVALID_HMC_OBJ_COUNT:
		return "IAVF_ERR_INVALID_HMC_OBJ_COUNT";
	case IAVF_ERR_INVALID_SRQ_ARM_LIMIT:
		return "IAVF_ERR_INVALID_SRQ_ARM_LIMIT";
	case IAVF_ERR_SRQ_ENABLED:
		return "IAVF_ERR_SRQ_ENABLED";
	case IAVF_ERR_ADMIN_QUEUE_ERROR:
		return "IAVF_ERR_ADMIN_QUEUE_ERROR";
	case IAVF_ERR_ADMIN_QUEUE_TIMEOUT:
		return "IAVF_ERR_ADMIN_QUEUE_TIMEOUT";
	case IAVF_ERR_BUF_TOO_SHORT:
		return "IAVF_ERR_BUF_TOO_SHORT";
	case IAVF_ERR_ADMIN_QUEUE_FULL:
		return "IAVF_ERR_ADMIN_QUEUE_FULL";
	case IAVF_ERR_ADMIN_QUEUE_NO_WORK:
		return "IAVF_ERR_ADMIN_QUEUE_NO_WORK";
	case IAVF_ERR_BAD_RDMA_CQE:
		return "IAVF_ERR_BAD_RDMA_CQE";
	case IAVF_ERR_NVM_BLANK_MODE:
		return "IAVF_ERR_NVM_BLANK_MODE";
	case IAVF_ERR_NOT_IMPLEMENTED:
		return "IAVF_ERR_NOT_IMPLEMENTED";
	case IAVF_ERR_PE_DOORBELL_NOT_ENABLED:
		return "IAVF_ERR_PE_DOORBELL_NOT_ENABLED";
	case IAVF_ERR_DIAG_TEST_FAILED:
		return "IAVF_ERR_DIAG_TEST_FAILED";
	case IAVF_ERR_NOT_READY:
		return "IAVF_ERR_NOT_READY";
	case IAVF_NOT_SUPPORTED:
		return "IAVF_NOT_SUPPORTED";
	case IAVF_ERR_FIRMWARE_API_VERSION:
		return "IAVF_ERR_FIRMWARE_API_VERSION";
	case IAVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR:
		return "IAVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", stat_err);
	return hw->err_str;
}

 
void iavf_debug_aq(struct iavf_hw *hw, enum iavf_debug_mask mask, void *desc,
		   void *buffer, u16 buf_len)
{
	struct iavf_aq_desc *aq_desc = (struct iavf_aq_desc *)desc;
	u8 *buf = (u8 *)buffer;

	if ((!(mask & hw->debug_mask)) || !desc)
		return;

	iavf_debug(hw, mask,
		   "AQ CMD: opcode 0x%04X, flags 0x%04X, datalen 0x%04X, retval 0x%04X\n",
		   le16_to_cpu(aq_desc->opcode),
		   le16_to_cpu(aq_desc->flags),
		   le16_to_cpu(aq_desc->datalen),
		   le16_to_cpu(aq_desc->retval));
	iavf_debug(hw, mask, "\tcookie (h,l) 0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->cookie_high),
		   le32_to_cpu(aq_desc->cookie_low));
	iavf_debug(hw, mask, "\tparam (0,1)  0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->params.internal.param0),
		   le32_to_cpu(aq_desc->params.internal.param1));
	iavf_debug(hw, mask, "\taddr (h,l)   0x%08X 0x%08X\n",
		   le32_to_cpu(aq_desc->params.external.addr_high),
		   le32_to_cpu(aq_desc->params.external.addr_low));

	if (buffer && aq_desc->datalen) {
		u16 len = le16_to_cpu(aq_desc->datalen);

		iavf_debug(hw, mask, "AQ CMD Buffer:\n");
		if (buf_len < len)
			len = buf_len;
		 
		if (hw->debug_mask & mask) {
			char prefix[27];

			snprintf(prefix, sizeof(prefix),
				 "iavf %02x:%02x.%x: \t0x",
				 hw->bus.bus_id,
				 hw->bus.device,
				 hw->bus.func);

			print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET,
				       16, 1, buf, len, false);
		}
	}
}

 
bool iavf_check_asq_alive(struct iavf_hw *hw)
{
	if (hw->aq.asq.len)
		return !!(rd32(hw, hw->aq.asq.len) &
			  IAVF_VF_ATQLEN1_ATQENABLE_MASK);
	else
		return false;
}

 
enum iavf_status iavf_aq_queue_shutdown(struct iavf_hw *hw, bool unloading)
{
	struct iavf_aq_desc desc;
	struct iavf_aqc_queue_shutdown *cmd =
		(struct iavf_aqc_queue_shutdown *)&desc.params.raw;
	enum iavf_status status;

	iavf_fill_default_direct_cmd_desc(&desc, iavf_aqc_opc_queue_shutdown);

	if (unloading)
		cmd->driver_unloading = cpu_to_le32(IAVF_AQ_DRIVER_UNLOADING);
	status = iavf_asq_send_command(hw, &desc, NULL, 0, NULL);

	return status;
}

 
static enum iavf_status iavf_aq_get_set_rss_lut(struct iavf_hw *hw,
						u16 vsi_id, bool pf_lut,
						u8 *lut, u16 lut_size,
						bool set)
{
	enum iavf_status status;
	struct iavf_aq_desc desc;
	struct iavf_aqc_get_set_rss_lut *cmd_resp =
		   (struct iavf_aqc_get_set_rss_lut *)&desc.params.raw;

	if (set)
		iavf_fill_default_direct_cmd_desc(&desc,
						  iavf_aqc_opc_set_rss_lut);
	else
		iavf_fill_default_direct_cmd_desc(&desc,
						  iavf_aqc_opc_get_rss_lut);

	 
	desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_BUF);
	desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			cpu_to_le16((u16)((vsi_id <<
					  IAVF_AQC_SET_RSS_LUT_VSI_ID_SHIFT) &
					  IAVF_AQC_SET_RSS_LUT_VSI_ID_MASK));
	cmd_resp->vsi_id |= cpu_to_le16((u16)IAVF_AQC_SET_RSS_LUT_VSI_VALID);

	if (pf_lut)
		cmd_resp->flags |= cpu_to_le16((u16)
					((IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_PF <<
					IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));
	else
		cmd_resp->flags |= cpu_to_le16((u16)
					((IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_VSI <<
					IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT) &
					IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_MASK));

	status = iavf_asq_send_command(hw, &desc, lut, lut_size, NULL);

	return status;
}

 
enum iavf_status iavf_aq_set_rss_lut(struct iavf_hw *hw, u16 vsi_id,
				     bool pf_lut, u8 *lut, u16 lut_size)
{
	return iavf_aq_get_set_rss_lut(hw, vsi_id, pf_lut, lut, lut_size, true);
}

 
static enum
iavf_status iavf_aq_get_set_rss_key(struct iavf_hw *hw, u16 vsi_id,
				    struct iavf_aqc_get_set_rss_key_data *key,
				    bool set)
{
	enum iavf_status status;
	struct iavf_aq_desc desc;
	struct iavf_aqc_get_set_rss_key *cmd_resp =
			(struct iavf_aqc_get_set_rss_key *)&desc.params.raw;
	u16 key_size = sizeof(struct iavf_aqc_get_set_rss_key_data);

	if (set)
		iavf_fill_default_direct_cmd_desc(&desc,
						  iavf_aqc_opc_set_rss_key);
	else
		iavf_fill_default_direct_cmd_desc(&desc,
						  iavf_aqc_opc_get_rss_key);

	 
	desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_BUF);
	desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_RD);

	cmd_resp->vsi_id =
			cpu_to_le16((u16)((vsi_id <<
					  IAVF_AQC_SET_RSS_KEY_VSI_ID_SHIFT) &
					  IAVF_AQC_SET_RSS_KEY_VSI_ID_MASK));
	cmd_resp->vsi_id |= cpu_to_le16((u16)IAVF_AQC_SET_RSS_KEY_VSI_VALID);

	status = iavf_asq_send_command(hw, &desc, key, key_size, NULL);

	return status;
}

 
enum iavf_status iavf_aq_set_rss_key(struct iavf_hw *hw, u16 vsi_id,
				     struct iavf_aqc_get_set_rss_key_data *key)
{
	return iavf_aq_get_set_rss_key(hw, vsi_id, key, true);
}

 

 
#define IAVF_PTT(PTYPE, OUTER_IP, OUTER_IP_VER, OUTER_FRAG, T, TE, TEF, I, PL)\
	[PTYPE] = { \
		1, \
		IAVF_RX_PTYPE_OUTER_##OUTER_IP, \
		IAVF_RX_PTYPE_OUTER_##OUTER_IP_VER, \
		IAVF_RX_PTYPE_##OUTER_FRAG, \
		IAVF_RX_PTYPE_TUNNEL_##T, \
		IAVF_RX_PTYPE_TUNNEL_END_##TE, \
		IAVF_RX_PTYPE_##TEF, \
		IAVF_RX_PTYPE_INNER_PROT_##I, \
		IAVF_RX_PTYPE_PAYLOAD_LAYER_##PL }

#define IAVF_PTT_UNUSED_ENTRY(PTYPE) [PTYPE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

 
#define IAVF_RX_PTYPE_NOF		IAVF_RX_PTYPE_NOT_FRAG
#define IAVF_RX_PTYPE_FRG		IAVF_RX_PTYPE_FRAG
#define IAVF_RX_PTYPE_INNER_PROT_TS	IAVF_RX_PTYPE_INNER_PROT_TIMESYNC

 
struct iavf_rx_ptype_decoded iavf_ptype_lookup[BIT(8)] = {
	 
	IAVF_PTT_UNUSED_ENTRY(0),
	IAVF_PTT(1,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	IAVF_PTT(2,  L2, NONE, NOF, NONE, NONE, NOF, TS,   PAY2),
	IAVF_PTT(3,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	IAVF_PTT_UNUSED_ENTRY(4),
	IAVF_PTT_UNUSED_ENTRY(5),
	IAVF_PTT(6,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	IAVF_PTT(7,  L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	IAVF_PTT_UNUSED_ENTRY(8),
	IAVF_PTT_UNUSED_ENTRY(9),
	IAVF_PTT(10, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY2),
	IAVF_PTT(11, L2, NONE, NOF, NONE, NONE, NOF, NONE, NONE),
	IAVF_PTT(12, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(13, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(14, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(15, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(16, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(17, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(18, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(19, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(20, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(21, L2, NONE, NOF, NONE, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(22, IP, IPV4, FRG, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(23, IP, IPV4, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(24, IP, IPV4, NOF, NONE, NONE, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(25),
	IAVF_PTT(26, IP, IPV4, NOF, NONE, NONE, NOF, TCP,  PAY4),
	IAVF_PTT(27, IP, IPV4, NOF, NONE, NONE, NOF, SCTP, PAY4),
	IAVF_PTT(28, IP, IPV4, NOF, NONE, NONE, NOF, ICMP, PAY4),

	 
	IAVF_PTT(29, IP, IPV4, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(30, IP, IPV4, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(31, IP, IPV4, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(32),
	IAVF_PTT(33, IP, IPV4, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(34, IP, IPV4, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(35, IP, IPV4, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(36, IP, IPV4, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(37, IP, IPV4, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(38, IP, IPV4, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(39),
	IAVF_PTT(40, IP, IPV4, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(41, IP, IPV4, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(42, IP, IPV4, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(43, IP, IPV4, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(44, IP, IPV4, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(45, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(46, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(47),
	IAVF_PTT(48, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(49, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(50, IP, IPV4, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(51, IP, IPV4, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(52, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(53, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(54),
	IAVF_PTT(55, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(56, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(57, IP, IPV4, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(58, IP, IPV4, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(59, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(60, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(61, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(62),
	IAVF_PTT(63, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(64, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(65, IP, IPV4, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(66, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(67, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(68, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(69),
	IAVF_PTT(70, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(71, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(72, IP, IPV4, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(73, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(74, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(75, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(76, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(77),
	IAVF_PTT(78, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(79, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(80, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(81, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(82, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(83, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(84),
	IAVF_PTT(85, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(86, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(87, IP, IPV4, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(88, IP, IPV6, FRG, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(89, IP, IPV6, NOF, NONE, NONE, NOF, NONE, PAY3),
	IAVF_PTT(90, IP, IPV6, NOF, NONE, NONE, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(91),
	IAVF_PTT(92, IP, IPV6, NOF, NONE, NONE, NOF, TCP,  PAY4),
	IAVF_PTT(93, IP, IPV6, NOF, NONE, NONE, NOF, SCTP, PAY4),
	IAVF_PTT(94, IP, IPV6, NOF, NONE, NONE, NOF, ICMP, PAY4),

	 
	IAVF_PTT(95,  IP, IPV6, NOF, IP_IP, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(96,  IP, IPV6, NOF, IP_IP, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(97,  IP, IPV6, NOF, IP_IP, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(98),
	IAVF_PTT(99,  IP, IPV6, NOF, IP_IP, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(100, IP, IPV6, NOF, IP_IP, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(101, IP, IPV6, NOF, IP_IP, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(102, IP, IPV6, NOF, IP_IP, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(103, IP, IPV6, NOF, IP_IP, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(104, IP, IPV6, NOF, IP_IP, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(105),
	IAVF_PTT(106, IP, IPV6, NOF, IP_IP, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(107, IP, IPV6, NOF, IP_IP, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(108, IP, IPV6, NOF, IP_IP, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(109, IP, IPV6, NOF, IP_GRENAT, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(110, IP, IPV6, NOF, IP_GRENAT, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(111, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(112, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(113),
	IAVF_PTT(114, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(115, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(116, IP, IPV6, NOF, IP_GRENAT, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(117, IP, IPV6, NOF, IP_GRENAT, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(118, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(119, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(120),
	IAVF_PTT(121, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(122, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(123, IP, IPV6, NOF, IP_GRENAT, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(124, IP, IPV6, NOF, IP_GRENAT_MAC, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(125, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(126, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(127, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(128),
	IAVF_PTT(129, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(130, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(131, IP, IPV6, NOF, IP_GRENAT_MAC, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(132, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(133, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(134, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(135),
	IAVF_PTT(136, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(137, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(138, IP, IPV6, NOF, IP_GRENAT_MAC, IPV6, NOF, ICMP, PAY4),

	 
	IAVF_PTT(139, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, NONE, NOF, NONE, PAY3),

	 
	IAVF_PTT(140, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, FRG, NONE, PAY3),
	IAVF_PTT(141, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, NONE, PAY3),
	IAVF_PTT(142, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(143),
	IAVF_PTT(144, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, TCP,  PAY4),
	IAVF_PTT(145, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, SCTP, PAY4),
	IAVF_PTT(146, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV4, NOF, ICMP, PAY4),

	 
	IAVF_PTT(147, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, FRG, NONE, PAY3),
	IAVF_PTT(148, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, NONE, PAY3),
	IAVF_PTT(149, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, UDP,  PAY4),
	IAVF_PTT_UNUSED_ENTRY(150),
	IAVF_PTT(151, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, TCP,  PAY4),
	IAVF_PTT(152, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, SCTP, PAY4),
	IAVF_PTT(153, IP, IPV6, NOF, IP_GRENAT_MAC_VLAN, IPV6, NOF, ICMP, PAY4),

	 
	[154 ... 255] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

 
enum iavf_status iavf_aq_send_msg_to_pf(struct iavf_hw *hw,
					enum virtchnl_ops v_opcode,
					enum iavf_status v_retval,
					u8 *msg, u16 msglen,
					struct iavf_asq_cmd_details *cmd_details)
{
	struct iavf_asq_cmd_details details;
	struct iavf_aq_desc desc;
	enum iavf_status status;

	iavf_fill_default_direct_cmd_desc(&desc, iavf_aqc_opc_send_msg_to_pf);
	desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_SI);
	desc.cookie_high = cpu_to_le32(v_opcode);
	desc.cookie_low = cpu_to_le32(v_retval);
	if (msglen) {
		desc.flags |= cpu_to_le16((u16)(IAVF_AQ_FLAG_BUF
						| IAVF_AQ_FLAG_RD));
		if (msglen > IAVF_AQ_LARGE_BUF)
			desc.flags |= cpu_to_le16((u16)IAVF_AQ_FLAG_LB);
		desc.datalen = cpu_to_le16(msglen);
	}
	if (!cmd_details) {
		memset(&details, 0, sizeof(details));
		details.async = true;
		cmd_details = &details;
	}
	status = iavf_asq_send_command(hw, &desc, msg, msglen, cmd_details);
	return status;
}

 
void iavf_vf_parse_hw_config(struct iavf_hw *hw,
			     struct virtchnl_vf_resource *msg)
{
	struct virtchnl_vsi_resource *vsi_res;
	int i;

	vsi_res = &msg->vsi_res[0];

	hw->dev_caps.num_vsis = msg->num_vsis;
	hw->dev_caps.num_rx_qp = msg->num_queue_pairs;
	hw->dev_caps.num_tx_qp = msg->num_queue_pairs;
	hw->dev_caps.num_msix_vectors_vf = msg->max_vectors;
	hw->dev_caps.dcb = msg->vf_cap_flags &
			   VIRTCHNL_VF_OFFLOAD_L2;
	hw->dev_caps.fcoe = 0;
	for (i = 0; i < msg->num_vsis; i++) {
		if (vsi_res->vsi_type == VIRTCHNL_VSI_SRIOV) {
			ether_addr_copy(hw->mac.perm_addr,
					vsi_res->default_mac_addr);
			ether_addr_copy(hw->mac.addr,
					vsi_res->default_mac_addr);
		}
		vsi_res++;
	}
}
