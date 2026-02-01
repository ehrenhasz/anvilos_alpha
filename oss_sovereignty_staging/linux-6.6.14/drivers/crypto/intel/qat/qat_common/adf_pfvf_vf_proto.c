
 
#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/minmax.h>
#include <linux/types.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_pfvf_msg.h"
#include "adf_pfvf_utils.h"
#include "adf_pfvf_vf_msg.h"
#include "adf_pfvf_vf_proto.h"

#define ADF_PFVF_MSG_COLLISION_DETECT_DELAY	10
#define ADF_PFVF_MSG_ACK_DELAY			2
#define ADF_PFVF_MSG_ACK_MAX_RETRY		100

 
#define ADF_PFVF_MSG_RESP_RETRIES	5
#define ADF_PFVF_MSG_RESP_TIMEOUT	(ADF_PFVF_MSG_ACK_DELAY * \
					 ADF_PFVF_MSG_ACK_MAX_RETRY + \
					 ADF_PFVF_MSG_COLLISION_DETECT_DELAY)

 
int adf_send_vf2pf_msg(struct adf_accel_dev *accel_dev, struct pfvf_message msg)
{
	struct adf_pfvf_ops *pfvf_ops = GET_PFVF_OPS(accel_dev);
	u32 pfvf_offset = pfvf_ops->get_vf2pf_offset(0);

	return pfvf_ops->send_msg(accel_dev, msg, pfvf_offset,
				  &accel_dev->vf.vf2pf_lock);
}

 
static struct pfvf_message adf_recv_pf2vf_msg(struct adf_accel_dev *accel_dev)
{
	struct adf_pfvf_ops *pfvf_ops = GET_PFVF_OPS(accel_dev);
	u32 pfvf_offset = pfvf_ops->get_pf2vf_offset(0);

	return pfvf_ops->recv_msg(accel_dev, pfvf_offset, accel_dev->vf.pf_compat_ver);
}

 
int adf_send_vf2pf_req(struct adf_accel_dev *accel_dev, struct pfvf_message msg,
		       struct pfvf_message *resp)
{
	unsigned long timeout = msecs_to_jiffies(ADF_PFVF_MSG_RESP_TIMEOUT);
	unsigned int retries = ADF_PFVF_MSG_RESP_RETRIES;
	int ret;

	reinit_completion(&accel_dev->vf.msg_received);

	 
	do {
		ret = adf_send_vf2pf_msg(accel_dev, msg);
		if (ret) {
			dev_err(&GET_DEV(accel_dev),
				"Failed to send request msg to PF\n");
			return ret;
		}

		 
		ret = wait_for_completion_timeout(&accel_dev->vf.msg_received,
						  timeout);
		if (ret) {
			if (likely(resp))
				*resp = accel_dev->vf.response;

			 
			accel_dev->vf.response.type = 0;

			return 0;
		}

		dev_err(&GET_DEV(accel_dev), "PFVF response message timeout\n");
	} while (--retries);

	return -EIO;
}

static int adf_vf2pf_blkmsg_data_req(struct adf_accel_dev *accel_dev, bool crc,
				     u8 *type, u8 *data)
{
	struct pfvf_message req = { 0 };
	struct pfvf_message resp = { 0 };
	u8 blk_type;
	u8 blk_byte;
	u8 msg_type;
	u8 max_data;
	int err;

	 
	if (*type <= ADF_VF2PF_SMALL_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_SMALL_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_SMALL_BLOCK_TYPE_MASK, *type);
		blk_byte = FIELD_PREP(ADF_VF2PF_SMALL_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_SMALL_BLOCK_BYTE_MAX;
	} else if (*type <= ADF_VF2PF_MEDIUM_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_MEDIUM_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_MEDIUM_BLOCK_TYPE_MASK,
				      *type - ADF_VF2PF_SMALL_BLOCK_TYPE_MAX);
		blk_byte = FIELD_PREP(ADF_VF2PF_MEDIUM_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_MEDIUM_BLOCK_BYTE_MAX;
	} else if (*type <= ADF_VF2PF_LARGE_BLOCK_TYPE_MAX) {
		msg_type = ADF_VF2PF_MSGTYPE_LARGE_BLOCK_REQ;
		blk_type = FIELD_PREP(ADF_VF2PF_LARGE_BLOCK_TYPE_MASK,
				      *type - ADF_VF2PF_MEDIUM_BLOCK_TYPE_MAX);
		blk_byte = FIELD_PREP(ADF_VF2PF_LARGE_BLOCK_BYTE_MASK, *data);
		max_data = ADF_VF2PF_LARGE_BLOCK_BYTE_MAX;
	} else {
		dev_err(&GET_DEV(accel_dev), "Invalid message type %u\n", *type);
		return -EINVAL;
	}

	 
	if (*data > max_data) {
		dev_err(&GET_DEV(accel_dev),
			"Invalid byte %s %u for message type %u\n",
			crc ? "count" : "index", *data, *type);
		return -EINVAL;
	}

	 
	req.type = msg_type;
	req.data = blk_type | blk_byte | FIELD_PREP(ADF_VF2PF_BLOCK_CRC_REQ_MASK, crc);

	err = adf_send_vf2pf_req(accel_dev, req, &resp);
	if (err)
		return err;

	*type = FIELD_GET(ADF_PF2VF_BLKMSG_RESP_TYPE_MASK, resp.data);
	*data = FIELD_GET(ADF_PF2VF_BLKMSG_RESP_DATA_MASK, resp.data);

	return 0;
}

static int adf_vf2pf_blkmsg_get_byte(struct adf_accel_dev *accel_dev, u8 type,
				     u8 index, u8 *data)
{
	int ret;

	ret = adf_vf2pf_blkmsg_data_req(accel_dev, false, &type, &index);
	if (ret < 0)
		return ret;

	if (unlikely(type != ADF_PF2VF_BLKMSG_RESP_TYPE_DATA)) {
		dev_err(&GET_DEV(accel_dev),
			"Unexpected BLKMSG response type %u, byte 0x%x\n",
			type, index);
		return -EFAULT;
	}

	*data = index;
	return 0;
}

static int adf_vf2pf_blkmsg_get_crc(struct adf_accel_dev *accel_dev, u8 type,
				    u8 bytes, u8 *crc)
{
	int ret;

	 
	--bytes;

	ret = adf_vf2pf_blkmsg_data_req(accel_dev, true, &type, &bytes);
	if (ret < 0)
		return ret;

	if (unlikely(type != ADF_PF2VF_BLKMSG_RESP_TYPE_CRC)) {
		dev_err(&GET_DEV(accel_dev),
			"Unexpected CRC BLKMSG response type %u, crc 0x%x\n",
			type, bytes);
		return  -EFAULT;
	}

	*crc = bytes;
	return 0;
}

 
int adf_send_vf2pf_blkmsg_req(struct adf_accel_dev *accel_dev, u8 type,
			      u8 *buffer, unsigned int *buffer_len)
{
	unsigned int index;
	unsigned int msg_len;
	int ret;
	u8 remote_crc;
	u8 local_crc;

	if (unlikely(type > ADF_VF2PF_LARGE_BLOCK_TYPE_MAX)) {
		dev_err(&GET_DEV(accel_dev), "Invalid block message type %d\n",
			type);
		return -EINVAL;
	}

	if (unlikely(*buffer_len < ADF_PFVF_BLKMSG_HEADER_SIZE)) {
		dev_err(&GET_DEV(accel_dev),
			"Buffer size too small for a block message\n");
		return -EINVAL;
	}

	ret = adf_vf2pf_blkmsg_get_byte(accel_dev, type,
					ADF_PFVF_BLKMSG_VER_BYTE,
					&buffer[ADF_PFVF_BLKMSG_VER_BYTE]);
	if (unlikely(ret))
		return ret;

	if (unlikely(!buffer[ADF_PFVF_BLKMSG_VER_BYTE])) {
		dev_err(&GET_DEV(accel_dev),
			"Invalid version 0 received for block request %u", type);
		return -EFAULT;
	}

	ret = adf_vf2pf_blkmsg_get_byte(accel_dev, type,
					ADF_PFVF_BLKMSG_LEN_BYTE,
					&buffer[ADF_PFVF_BLKMSG_LEN_BYTE]);
	if (unlikely(ret))
		return ret;

	if (unlikely(!buffer[ADF_PFVF_BLKMSG_LEN_BYTE])) {
		dev_err(&GET_DEV(accel_dev),
			"Invalid size 0 received for block request %u", type);
		return -EFAULT;
	}

	 
	msg_len = ADF_PFVF_BLKMSG_HEADER_SIZE + buffer[ADF_PFVF_BLKMSG_LEN_BYTE];
	msg_len = min(*buffer_len, msg_len);

	 
	for (index = ADF_PFVF_BLKMSG_HEADER_SIZE; index < msg_len; index++) {
		ret = adf_vf2pf_blkmsg_get_byte(accel_dev, type, index,
						&buffer[index]);
		if (unlikely(ret))
			return ret;
	}

	ret = adf_vf2pf_blkmsg_get_crc(accel_dev, type, msg_len, &remote_crc);
	if (unlikely(ret))
		return ret;

	local_crc = adf_pfvf_calc_blkmsg_crc(buffer, msg_len);
	if (unlikely(local_crc != remote_crc)) {
		dev_err(&GET_DEV(accel_dev),
			"CRC error on msg type %d. Local %02X, remote %02X\n",
			type, local_crc, remote_crc);
		return -EIO;
	}

	*buffer_len = msg_len;
	return 0;
}

static bool adf_handle_pf2vf_msg(struct adf_accel_dev *accel_dev,
				 struct pfvf_message msg)
{
	switch (msg.type) {
	case ADF_PF2VF_MSGTYPE_RESTARTING:
		dev_dbg(&GET_DEV(accel_dev), "Restarting message received from PF\n");

		adf_pf2vf_handle_pf_restarting(accel_dev);
		return false;
	case ADF_PF2VF_MSGTYPE_VERSION_RESP:
	case ADF_PF2VF_MSGTYPE_BLKMSG_RESP:
	case ADF_PF2VF_MSGTYPE_RP_RESET_RESP:
		dev_dbg(&GET_DEV(accel_dev),
			"Response Message received from PF (type 0x%.4x, data 0x%.4x)\n",
			msg.type, msg.data);
		accel_dev->vf.response = msg;
		complete(&accel_dev->vf.msg_received);
		return true;
	default:
		dev_err(&GET_DEV(accel_dev),
			"Unknown message from PF (type 0x%.4x, data: 0x%.4x)\n",
			msg.type, msg.data);
	}

	return false;
}

bool adf_recv_and_handle_pf2vf_msg(struct adf_accel_dev *accel_dev)
{
	struct pfvf_message msg;

	msg = adf_recv_pf2vf_msg(accel_dev);
	if (msg.type)   
		return adf_handle_pf2vf_msg(accel_dev, msg);

	 

	return true;
}

 
int adf_enable_vf2pf_comms(struct adf_accel_dev *accel_dev)
{
	int ret;

	adf_pfvf_crc_init();
	adf_enable_pf2vf_interrupts(accel_dev);

	ret = adf_vf2pf_request_version(accel_dev);
	if (ret)
		return ret;

	ret = adf_vf2pf_get_capabilities(accel_dev);
	if (ret)
		return ret;

	ret = adf_vf2pf_get_ring_to_svc(accel_dev);

	return ret;
}
EXPORT_SYMBOL_GPL(adf_enable_vf2pf_comms);
