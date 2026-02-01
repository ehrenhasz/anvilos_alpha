




#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include "cros_ec_trace.h"

#define EC_COMMAND_RETRIES	50

static const int cros_ec_error_map[] = {
	[EC_RES_INVALID_COMMAND] = -EOPNOTSUPP,
	[EC_RES_ERROR] = -EIO,
	[EC_RES_INVALID_PARAM] = -EINVAL,
	[EC_RES_ACCESS_DENIED] = -EACCES,
	[EC_RES_INVALID_RESPONSE] = -EPROTO,
	[EC_RES_INVALID_VERSION] = -ENOPROTOOPT,
	[EC_RES_INVALID_CHECKSUM] = -EBADMSG,
	[EC_RES_IN_PROGRESS] = -EINPROGRESS,
	[EC_RES_UNAVAILABLE] = -ENODATA,
	[EC_RES_TIMEOUT] = -ETIMEDOUT,
	[EC_RES_OVERFLOW] = -EOVERFLOW,
	[EC_RES_INVALID_HEADER] = -EBADR,
	[EC_RES_REQUEST_TRUNCATED] = -EBADR,
	[EC_RES_RESPONSE_TOO_BIG] = -EFBIG,
	[EC_RES_BUS_ERROR] = -EFAULT,
	[EC_RES_BUSY] = -EBUSY,
	[EC_RES_INVALID_HEADER_VERSION] = -EBADMSG,
	[EC_RES_INVALID_HEADER_CRC] = -EBADMSG,
	[EC_RES_INVALID_DATA_CRC] = -EBADMSG,
	[EC_RES_DUP_UNAVAILABLE] = -ENODATA,
};

static int cros_ec_map_error(uint32_t result)
{
	int ret = 0;

	if (result != EC_RES_SUCCESS) {
		if (result < ARRAY_SIZE(cros_ec_error_map) && cros_ec_error_map[result])
			ret = cros_ec_error_map[result];
		else
			ret = -EPROTO;
	}

	return ret;
}

static int prepare_tx(struct cros_ec_device *ec_dev,
		      struct cros_ec_command *msg)
{
	struct ec_host_request *request;
	u8 *out;
	int i;
	u8 csum = 0;

	if (msg->outsize + sizeof(*request) > ec_dev->dout_size)
		return -EINVAL;

	out = ec_dev->dout;
	request = (struct ec_host_request *)out;
	request->struct_version = EC_HOST_REQUEST_VERSION;
	request->checksum = 0;
	request->command = msg->command;
	request->command_version = msg->version;
	request->reserved = 0;
	request->data_len = msg->outsize;

	for (i = 0; i < sizeof(*request); i++)
		csum += out[i];

	 
	memcpy(out + sizeof(*request), msg->data, msg->outsize);
	for (i = 0; i < msg->outsize; i++)
		csum += msg->data[i];

	request->checksum = -csum;

	return sizeof(*request) + msg->outsize;
}

static int prepare_tx_legacy(struct cros_ec_device *ec_dev,
			     struct cros_ec_command *msg)
{
	u8 *out;
	u8 csum;
	int i;

	if (msg->outsize > EC_PROTO2_MAX_PARAM_SIZE)
		return -EINVAL;

	out = ec_dev->dout;
	out[0] = EC_CMD_VERSION0 + msg->version;
	out[1] = msg->command;
	out[2] = msg->outsize;
	csum = out[0] + out[1] + out[2];
	for (i = 0; i < msg->outsize; i++)
		csum += out[EC_MSG_TX_HEADER_BYTES + i] = msg->data[i];
	out[EC_MSG_TX_HEADER_BYTES + msg->outsize] = csum;

	return EC_MSG_TX_PROTO_BYTES + msg->outsize;
}

static int cros_ec_xfer_command(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret;
	int (*xfer_fxn)(struct cros_ec_device *ec, struct cros_ec_command *msg);

	if (ec_dev->proto_version > 2)
		xfer_fxn = ec_dev->pkt_xfer;
	else
		xfer_fxn = ec_dev->cmd_xfer;

	if (!xfer_fxn) {
		 
		dev_err_once(ec_dev->dev, "missing EC transfer API, cannot send command\n");
		return -EIO;
	}

	trace_cros_ec_request_start(msg);
	ret = (*xfer_fxn)(ec_dev, msg);
	trace_cros_ec_request_done(msg, ret);

	return ret;
}

static int cros_ec_wait_until_complete(struct cros_ec_device *ec_dev, uint32_t *result)
{
	struct {
		struct cros_ec_command msg;
		struct ec_response_get_comms_status status;
	} __packed buf;
	struct cros_ec_command *msg = &buf.msg;
	struct ec_response_get_comms_status *status = &buf.status;
	int ret = 0, i;

	msg->version = 0;
	msg->command = EC_CMD_GET_COMMS_STATUS;
	msg->insize = sizeof(*status);
	msg->outsize = 0;

	 
	for (i = 0; i < EC_COMMAND_RETRIES; ++i) {
		usleep_range(10000, 11000);

		ret = cros_ec_xfer_command(ec_dev, msg);
		if (ret == -EAGAIN)
			continue;
		if (ret < 0)
			return ret;

		*result = msg->result;
		if (msg->result != EC_RES_SUCCESS)
			return ret;

		if (ret == 0) {
			ret = -EPROTO;
			break;
		}

		if (!(status->flags & EC_COMMS_STATUS_PROCESSING))
			return ret;
	}

	if (i >= EC_COMMAND_RETRIES)
		ret = -EAGAIN;

	return ret;
}

static int cros_ec_send_command(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret = cros_ec_xfer_command(ec_dev, msg);

	if (msg->result == EC_RES_IN_PROGRESS)
		ret = cros_ec_wait_until_complete(ec_dev, &msg->result);

	return ret;
}

 
int cros_ec_prepare_tx(struct cros_ec_device *ec_dev,
		       struct cros_ec_command *msg)
{
	if (ec_dev->proto_version > 2)
		return prepare_tx(ec_dev, msg);

	return prepare_tx_legacy(ec_dev, msg);
}
EXPORT_SYMBOL(cros_ec_prepare_tx);

 
int cros_ec_check_result(struct cros_ec_device *ec_dev,
			 struct cros_ec_command *msg)
{
	switch (msg->result) {
	case EC_RES_SUCCESS:
		return 0;
	case EC_RES_IN_PROGRESS:
		dev_dbg(ec_dev->dev, "command 0x%02x in progress\n",
			msg->command);
		return -EAGAIN;
	default:
		dev_dbg(ec_dev->dev, "command 0x%02x returned %d\n",
			msg->command, msg->result);
		return 0;
	}
}
EXPORT_SYMBOL(cros_ec_check_result);

 
static int cros_ec_get_host_event_wake_mask(struct cros_ec_device *ec_dev, uint32_t *mask)
{
	struct cros_ec_command *msg;
	struct ec_response_host_event_mask *r;
	int ret, mapped;

	msg = kzalloc(sizeof(*msg) + sizeof(*r), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_HOST_EVENT_GET_WAKE_MASK;
	msg->insize = sizeof(*r);

	ret = cros_ec_send_command(ec_dev, msg);
	if (ret < 0)
		goto exit;

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	r = (struct ec_response_host_event_mask *)msg->data;
	*mask = r->mask;
	ret = 0;
exit:
	kfree(msg);
	return ret;
}

static int cros_ec_get_proto_info(struct cros_ec_device *ec_dev, int devidx)
{
	struct cros_ec_command *msg;
	struct ec_response_get_protocol_info *info;
	int ret, mapped;

	ec_dev->proto_version = 3;
	if (devidx > 0)
		ec_dev->max_passthru = 0;

	msg = kzalloc(sizeof(*msg) + sizeof(*info), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_PASSTHRU_OFFSET(devidx) | EC_CMD_GET_PROTOCOL_INFO;
	msg->insize = sizeof(*info);

	ret = cros_ec_send_command(ec_dev, msg);
	 
	if (ret == -ETIMEDOUT)
		ret = cros_ec_send_command(ec_dev, msg);

	if (ret < 0) {
		dev_dbg(ec_dev->dev,
			"failed to check for EC[%d] protocol version: %d\n",
			devidx, ret);
		goto exit;
	}

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	info = (struct ec_response_get_protocol_info *)msg->data;

	switch (devidx) {
	case CROS_EC_DEV_EC_INDEX:
		ec_dev->max_request = info->max_request_packet_size -
						sizeof(struct ec_host_request);
		ec_dev->max_response = info->max_response_packet_size -
						sizeof(struct ec_host_response);
		ec_dev->proto_version = min(EC_HOST_REQUEST_VERSION,
					    fls(info->protocol_versions) - 1);
		ec_dev->din_size = info->max_response_packet_size + EC_MAX_RESPONSE_OVERHEAD;
		ec_dev->dout_size = info->max_request_packet_size + EC_MAX_REQUEST_OVERHEAD;

		dev_dbg(ec_dev->dev, "using proto v%u\n", ec_dev->proto_version);
		break;
	case CROS_EC_DEV_PD_INDEX:
		ec_dev->max_passthru = info->max_request_packet_size -
						sizeof(struct ec_host_request);

		dev_dbg(ec_dev->dev, "found PD chip\n");
		break;
	default:
		dev_dbg(ec_dev->dev, "unknown passthru index: %d\n", devidx);
		break;
	}

	ret = 0;
exit:
	kfree(msg);
	return ret;
}

static int cros_ec_get_proto_info_legacy(struct cros_ec_device *ec_dev)
{
	struct cros_ec_command *msg;
	struct ec_params_hello *params;
	struct ec_response_hello *response;
	int ret, mapped;

	ec_dev->proto_version = 2;

	msg = kzalloc(sizeof(*msg) + max(sizeof(*params), sizeof(*response)), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->command = EC_CMD_HELLO;
	msg->insize = sizeof(*response);
	msg->outsize = sizeof(*params);

	params = (struct ec_params_hello *)msg->data;
	params->in_data = 0xa0b0c0d0;

	ret = cros_ec_send_command(ec_dev, msg);
	if (ret < 0) {
		dev_dbg(ec_dev->dev, "EC failed to respond to v2 hello: %d\n", ret);
		goto exit;
	}

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		dev_err(ec_dev->dev, "EC responded to v2 hello with error: %d\n", msg->result);
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	response = (struct ec_response_hello *)msg->data;
	if (response->out_data != 0xa1b2c3d4) {
		dev_err(ec_dev->dev,
			"EC responded to v2 hello with bad result: %u\n",
			response->out_data);
		ret = -EBADMSG;
		goto exit;
	}

	ec_dev->max_request = EC_PROTO2_MAX_PARAM_SIZE;
	ec_dev->max_response = EC_PROTO2_MAX_PARAM_SIZE;
	ec_dev->max_passthru = 0;
	ec_dev->pkt_xfer = NULL;
	ec_dev->din_size = EC_PROTO2_MSG_BYTES;
	ec_dev->dout_size = EC_PROTO2_MSG_BYTES;

	dev_dbg(ec_dev->dev, "falling back to proto v2\n");
	ret = 0;
exit:
	kfree(msg);
	return ret;
}

 
static int cros_ec_get_host_command_version_mask(struct cros_ec_device *ec_dev, u16 cmd, u32 *mask)
{
	struct ec_params_get_cmd_versions *pver;
	struct ec_response_get_cmd_versions *rver;
	struct cros_ec_command *msg;
	int ret, mapped;

	msg = kmalloc(sizeof(*msg) + max(sizeof(*rver), sizeof(*pver)),
		      GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = 0;
	msg->command = EC_CMD_GET_CMD_VERSIONS;
	msg->insize = sizeof(*rver);
	msg->outsize = sizeof(*pver);

	pver = (struct ec_params_get_cmd_versions *)msg->data;
	pver->cmd = cmd;

	ret = cros_ec_send_command(ec_dev, msg);
	if (ret < 0)
		goto exit;

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		ret = mapped;
		goto exit;
	}

	if (ret == 0) {
		ret = -EPROTO;
		goto exit;
	}

	rver = (struct ec_response_get_cmd_versions *)msg->data;
	*mask = rver->version_mask;
	ret = 0;
exit:
	kfree(msg);
	return ret;
}

 
int cros_ec_query_all(struct cros_ec_device *ec_dev)
{
	struct device *dev = ec_dev->dev;
	u32 ver_mask;
	int ret;

	 
	if (!cros_ec_get_proto_info(ec_dev, CROS_EC_DEV_EC_INDEX)) {
		 
		cros_ec_get_proto_info(ec_dev, CROS_EC_DEV_PD_INDEX);
	} else {
		 
		ret = cros_ec_get_proto_info_legacy(ec_dev);
		if (ret) {
			 
			ec_dev->proto_version = EC_PROTO_VERSION_UNKNOWN;
			dev_dbg(ec_dev->dev, "EC query failed: %d\n", ret);
			return ret;
		}
	}

	devm_kfree(dev, ec_dev->din);
	devm_kfree(dev, ec_dev->dout);

	ec_dev->din = devm_kzalloc(dev, ec_dev->din_size, GFP_KERNEL);
	if (!ec_dev->din) {
		ret = -ENOMEM;
		goto exit;
	}

	ec_dev->dout = devm_kzalloc(dev, ec_dev->dout_size, GFP_KERNEL);
	if (!ec_dev->dout) {
		devm_kfree(dev, ec_dev->din);
		ret = -ENOMEM;
		goto exit;
	}

	 
	ret = cros_ec_get_host_command_version_mask(ec_dev, EC_CMD_GET_NEXT_EVENT, &ver_mask);
	if (ret < 0 || ver_mask == 0) {
		ec_dev->mkbp_event_supported = 0;
	} else {
		ec_dev->mkbp_event_supported = fls(ver_mask);

		dev_dbg(ec_dev->dev, "MKBP support version %u\n", ec_dev->mkbp_event_supported - 1);
	}

	 
	ret = cros_ec_get_host_command_version_mask(ec_dev, EC_CMD_HOST_SLEEP_EVENT, &ver_mask);
	ec_dev->host_sleep_v1 = (ret == 0 && (ver_mask & EC_VER_MASK(1)));

	 
	ret = cros_ec_get_host_event_wake_mask(ec_dev, &ec_dev->host_event_wake_mask);
	if (ret < 0) {
		 
		ec_dev->host_event_wake_mask = U32_MAX &
			~(EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_AC_DISCONNECTED) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_BATTERY_LOW) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_BATTERY_CRITICAL) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_BATTERY) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_PD_MCU) |
			  EC_HOST_EVENT_MASK(EC_HOST_EVENT_BATTERY_STATUS));
		 
		if (ret != -EOPNOTSUPP)
			dev_err(ec_dev->dev,
				"failed to retrieve wake mask: %d\n", ret);
	}

	ret = 0;

exit:
	return ret;
}
EXPORT_SYMBOL(cros_ec_query_all);

 
int cros_ec_cmd_xfer(struct cros_ec_device *ec_dev, struct cros_ec_command *msg)
{
	int ret;

	mutex_lock(&ec_dev->lock);
	if (ec_dev->proto_version == EC_PROTO_VERSION_UNKNOWN) {
		ret = cros_ec_query_all(ec_dev);
		if (ret) {
			dev_err(ec_dev->dev,
				"EC version unknown and query failed; aborting command\n");
			mutex_unlock(&ec_dev->lock);
			return ret;
		}
	}

	if (msg->insize > ec_dev->max_response) {
		dev_dbg(ec_dev->dev, "clamping message receive buffer\n");
		msg->insize = ec_dev->max_response;
	}

	if (msg->command < EC_CMD_PASSTHRU_OFFSET(CROS_EC_DEV_PD_INDEX)) {
		if (msg->outsize > ec_dev->max_request) {
			dev_err(ec_dev->dev,
				"request of size %u is too big (max: %u)\n",
				msg->outsize,
				ec_dev->max_request);
			mutex_unlock(&ec_dev->lock);
			return -EMSGSIZE;
		}
	} else {
		if (msg->outsize > ec_dev->max_passthru) {
			dev_err(ec_dev->dev,
				"passthru rq of size %u is too big (max: %u)\n",
				msg->outsize,
				ec_dev->max_passthru);
			mutex_unlock(&ec_dev->lock);
			return -EMSGSIZE;
		}
	}

	ret = cros_ec_send_command(ec_dev, msg);
	mutex_unlock(&ec_dev->lock);

	return ret;
}
EXPORT_SYMBOL(cros_ec_cmd_xfer);

 
int cros_ec_cmd_xfer_status(struct cros_ec_device *ec_dev,
			    struct cros_ec_command *msg)
{
	int ret, mapped;

	ret = cros_ec_cmd_xfer(ec_dev, msg);
	if (ret < 0)
		return ret;

	mapped = cros_ec_map_error(msg->result);
	if (mapped) {
		dev_dbg(ec_dev->dev, "Command result (err: %d [%d])\n",
			msg->result, mapped);
		ret = mapped;
	}

	return ret;
}
EXPORT_SYMBOL(cros_ec_cmd_xfer_status);

static int get_next_event_xfer(struct cros_ec_device *ec_dev,
			       struct cros_ec_command *msg,
			       struct ec_response_get_next_event_v1 *event,
			       int version, uint32_t size)
{
	int ret;

	msg->version = version;
	msg->command = EC_CMD_GET_NEXT_EVENT;
	msg->insize = size;
	msg->outsize = 0;

	ret = cros_ec_cmd_xfer_status(ec_dev, msg);
	if (ret > 0) {
		ec_dev->event_size = ret - 1;
		ec_dev->event_data = *event;
	}

	return ret;
}

static int get_next_event(struct cros_ec_device *ec_dev)
{
	struct {
		struct cros_ec_command msg;
		struct ec_response_get_next_event_v1 event;
	} __packed buf;
	struct cros_ec_command *msg = &buf.msg;
	struct ec_response_get_next_event_v1 *event = &buf.event;
	const int cmd_version = ec_dev->mkbp_event_supported - 1;

	memset(msg, 0, sizeof(*msg));
	if (ec_dev->suspended) {
		dev_dbg(ec_dev->dev, "Device suspended.\n");
		return -EHOSTDOWN;
	}

	if (cmd_version == 0)
		return get_next_event_xfer(ec_dev, msg, event, 0,
				  sizeof(struct ec_response_get_next_event));

	return get_next_event_xfer(ec_dev, msg, event, cmd_version,
				sizeof(struct ec_response_get_next_event_v1));
}

static int get_keyboard_state_event(struct cros_ec_device *ec_dev)
{
	u8 buffer[sizeof(struct cros_ec_command) +
		  sizeof(ec_dev->event_data.data)];
	struct cros_ec_command *msg = (struct cros_ec_command *)&buffer;

	msg->version = 0;
	msg->command = EC_CMD_MKBP_STATE;
	msg->insize = sizeof(ec_dev->event_data.data);
	msg->outsize = 0;

	ec_dev->event_size = cros_ec_cmd_xfer_status(ec_dev, msg);
	ec_dev->event_data.event_type = EC_MKBP_EVENT_KEY_MATRIX;
	memcpy(&ec_dev->event_data.data, msg->data,
	       sizeof(ec_dev->event_data.data));

	return ec_dev->event_size;
}

 
int cros_ec_get_next_event(struct cros_ec_device *ec_dev,
			   bool *wake_event,
			   bool *has_more_events)
{
	u8 event_type;
	u32 host_event;
	int ret;
	u32 ver_mask;

	 
	if (wake_event)
		*wake_event = true;

	 
	if (has_more_events)
		*has_more_events = false;

	if (!ec_dev->mkbp_event_supported)
		return get_keyboard_state_event(ec_dev);

	ret = get_next_event(ec_dev);
	 
	if (ret == -ENOPROTOOPT) {
		dev_dbg(ec_dev->dev,
			"GET_NEXT_EVENT returned invalid version error.\n");
		ret = cros_ec_get_host_command_version_mask(ec_dev,
							EC_CMD_GET_NEXT_EVENT,
							&ver_mask);
		if (ret < 0 || ver_mask == 0)
			 
			return -ENOPROTOOPT;

		ec_dev->mkbp_event_supported = fls(ver_mask);
		dev_dbg(ec_dev->dev, "MKBP support version changed to %u\n",
			ec_dev->mkbp_event_supported - 1);

		 
		ret = get_next_event(ec_dev);
	}

	if (ret <= 0)
		return ret;

	if (has_more_events)
		*has_more_events = ec_dev->event_data.event_type &
			EC_MKBP_HAS_MORE_EVENTS;
	ec_dev->event_data.event_type &= EC_MKBP_EVENT_TYPE_MASK;

	if (wake_event) {
		event_type = ec_dev->event_data.event_type;
		host_event = cros_ec_get_host_event(ec_dev);

		 
		if (event_type == EC_MKBP_EVENT_SENSOR_FIFO) {
			*wake_event = false;
		} else if (host_event) {
			 
			if (host_event & EC_HOST_EVENT_MASK(EC_HOST_EVENT_RTC))
				*wake_event = false;
			 
			if (!(host_event & ec_dev->host_event_wake_mask))
				*wake_event = false;
		}
	}

	return ret;
}
EXPORT_SYMBOL(cros_ec_get_next_event);

 
u32 cros_ec_get_host_event(struct cros_ec_device *ec_dev)
{
	u32 host_event;

	if (!ec_dev->mkbp_event_supported)
		return 0;

	if (ec_dev->event_data.event_type != EC_MKBP_EVENT_HOST_EVENT)
		return 0;

	if (ec_dev->event_size != sizeof(host_event)) {
		dev_warn(ec_dev->dev, "Invalid host event size\n");
		return 0;
	}

	host_event = get_unaligned_le32(&ec_dev->event_data.data.host_event);

	return host_event;
}
EXPORT_SYMBOL(cros_ec_get_host_event);

 
bool cros_ec_check_features(struct cros_ec_dev *ec, int feature)
{
	struct ec_response_get_features *features = &ec->features;
	int ret;

	if (features->flags[0] == -1U && features->flags[1] == -1U) {
		 
		ret = cros_ec_cmd(ec->ec_dev, 0, EC_CMD_GET_FEATURES + ec->cmd_offset,
				  NULL, 0, features, sizeof(*features));
		if (ret < 0) {
			dev_warn(ec->dev, "cannot get EC features: %d\n", ret);
			memset(features, 0, sizeof(*features));
		}

		dev_dbg(ec->dev, "EC features %08x %08x\n",
			features->flags[0], features->flags[1]);
	}

	return !!(features->flags[feature / 32] & EC_FEATURE_MASK_0(feature));
}
EXPORT_SYMBOL_GPL(cros_ec_check_features);

 
int cros_ec_get_sensor_count(struct cros_ec_dev *ec)
{
	 
	int ret, sensor_count;
	struct ec_params_motion_sense *params;
	struct ec_response_motion_sense *resp;
	struct cros_ec_command *msg;
	struct cros_ec_device *ec_dev = ec->ec_dev;
	u8 status;

	msg = kzalloc(sizeof(*msg) + max(sizeof(*params), sizeof(*resp)),
		      GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = 1;
	msg->command = EC_CMD_MOTION_SENSE_CMD + ec->cmd_offset;
	msg->outsize = sizeof(*params);
	msg->insize = sizeof(*resp);

	params = (struct ec_params_motion_sense *)msg->data;
	params->cmd = MOTIONSENSE_CMD_DUMP;

	ret = cros_ec_cmd_xfer_status(ec->ec_dev, msg);
	if (ret < 0) {
		sensor_count = ret;
	} else {
		resp = (struct ec_response_motion_sense *)msg->data;
		sensor_count = resp->dump.sensor_count;
	}
	kfree(msg);

	 
	if (sensor_count < 0 && ec->cmd_offset == 0 && ec_dev->cmd_readmem) {
		ret = ec_dev->cmd_readmem(ec_dev, EC_MEMMAP_ACC_STATUS,
				1, &status);
		if (ret >= 0 &&
		    (status & EC_MEMMAP_ACC_STATUS_PRESENCE_BIT)) {
			 
			sensor_count = 2;
		} else {
			 
			sensor_count = 0;
		}
	}
	return sensor_count;
}
EXPORT_SYMBOL_GPL(cros_ec_get_sensor_count);

 
int cros_ec_cmd(struct cros_ec_device *ec_dev,
		unsigned int version,
		int command,
		void *outdata,
		size_t outsize,
		void *indata,
		size_t insize)
{
	struct cros_ec_command *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + max(insize, outsize), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->version = version;
	msg->command = command;
	msg->outsize = outsize;
	msg->insize = insize;

	if (outsize)
		memcpy(msg->data, outdata, outsize);

	ret = cros_ec_cmd_xfer_status(ec_dev, msg);
	if (ret < 0)
		goto error;

	if (insize)
		memcpy(indata, msg->data, insize);
error:
	kfree(msg);
	return ret;
}
EXPORT_SYMBOL_GPL(cros_ec_cmd);
