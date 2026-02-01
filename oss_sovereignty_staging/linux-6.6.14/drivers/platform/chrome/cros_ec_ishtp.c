








#include <linux/delay.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/intel-ish-client-if.h>

#include "cros_ec.h"

 
#define CROS_ISH_CL_TX_RING_SIZE		8
#define CROS_ISH_CL_RX_RING_SIZE		8

 
enum cros_ec_ish_channel {
	CROS_EC_COMMAND = 1,			 
	CROS_MKBP_EVENT = 2,			 
};

 
#define ISHTP_SEND_TIMEOUT			(3 * HZ)

 
static const struct ishtp_device_id cros_ec_ishtp_id_table[] = {
	{ .guid = GUID_INIT(0x7b7154d0, 0x56f4, 0x4bdc,
		  0xb0, 0xd8, 0x9e, 0x7c, 0xda,	0xe0, 0xd6, 0xa0), },
	{ }
};
MODULE_DEVICE_TABLE(ishtp, cros_ec_ishtp_id_table);

struct header {
	u8 channel;
	u8 status;
	u8 token;
	u8 reserved;
} __packed;

struct cros_ish_out_msg {
	struct header hdr;
	struct ec_host_request ec_request;
} __packed;

struct cros_ish_in_msg {
	struct header hdr;
	struct ec_host_response ec_response;
} __packed;

#define IN_MSG_EC_RESPONSE_PREAMBLE					\
	offsetof(struct cros_ish_in_msg, ec_response)

#define OUT_MSG_EC_REQUEST_PREAMBLE					\
	offsetof(struct cros_ish_out_msg, ec_request)

#define cl_data_to_dev(client_data) ishtp_device((client_data)->cl_device)

 
static DECLARE_RWSEM(init_lock);

 
struct response_info {
	void *data;
	size_t max_size;
	size_t size;
	int error;
	u8 token;
	bool received;
	wait_queue_head_t wait_queue;
};

 
struct ishtp_cl_data {
	struct ishtp_cl *cros_ish_cl;
	struct ishtp_cl_device *cl_device;

	 
	struct response_info response;

	struct work_struct work_ishtp_reset;
	struct work_struct work_ec_evt;
	struct cros_ec_device *ec_dev;
};

 
static void ish_evt_handler(struct work_struct *work)
{
	struct ishtp_cl_data *client_data =
		container_of(work, struct ishtp_cl_data, work_ec_evt);

	cros_ec_irq_thread(0, client_data->ec_dev);
}

 
static int ish_send(struct ishtp_cl_data *client_data,
		    u8 *out_msg, size_t out_size,
		    u8 *in_msg, size_t in_size)
{
	static u8 next_token;
	int rv;
	struct header *out_hdr = (struct header *)out_msg;
	struct ishtp_cl *cros_ish_cl = client_data->cros_ish_cl;

	dev_dbg(cl_data_to_dev(client_data),
		"%s: channel=%02u status=%02u\n",
		__func__, out_hdr->channel, out_hdr->status);

	 
	client_data->response.data = in_msg;
	client_data->response.max_size = in_size;
	client_data->response.error = 0;
	client_data->response.token = next_token++;
	client_data->response.received = false;

	out_hdr->token = client_data->response.token;

	rv = ishtp_cl_send(cros_ish_cl, out_msg, out_size);
	if (rv) {
		dev_err(cl_data_to_dev(client_data),
			"ishtp_cl_send error %d\n", rv);
		return rv;
	}

	wait_event_interruptible_timeout(client_data->response.wait_queue,
					 client_data->response.received,
					 ISHTP_SEND_TIMEOUT);
	if (!client_data->response.received) {
		dev_err(cl_data_to_dev(client_data),
			"Timed out for response to host message\n");
		return -ETIMEDOUT;
	}

	if (client_data->response.error < 0)
		return client_data->response.error;

	return client_data->response.size;
}

 
static void process_recv(struct ishtp_cl *cros_ish_cl,
			 struct ishtp_cl_rb *rb_in_proc, ktime_t timestamp)
{
	size_t data_len = rb_in_proc->buf_idx;
	struct ishtp_cl_data *client_data =
		ishtp_get_client_data(cros_ish_cl);
	struct device *dev = cl_data_to_dev(client_data);
	struct cros_ish_in_msg *in_msg =
		(struct cros_ish_in_msg *)rb_in_proc->buffer.data;

	 
	if (!down_read_trylock(&init_lock)) {
		 
		ishtp_cl_io_rb_recycle(rb_in_proc);
		dev_warn(dev,
			 "Host is not ready to receive incoming messages\n");
		return;
	}

	 
	if (!rb_in_proc->buffer.data) {
		dev_warn(dev, "rb_in_proc->buffer.data returned null");
		client_data->response.error = -EBADMSG;
		goto end_error;
	}

	if (data_len < sizeof(struct header)) {
		dev_err(dev, "data size %zu is less than header %zu\n",
			data_len, sizeof(struct header));
		client_data->response.error = -EMSGSIZE;
		goto end_error;
	}

	dev_dbg(dev, "channel=%02u status=%02u\n",
		in_msg->hdr.channel, in_msg->hdr.status);

	switch (in_msg->hdr.channel) {
	case CROS_EC_COMMAND:
		if (client_data->response.received) {
			dev_err(dev,
				"Previous firmware message not yet processed\n");
			goto end_error;
		}

		if (client_data->response.token != in_msg->hdr.token) {
			dev_err_ratelimited(dev,
					    "Dropping old response token %d\n",
					    in_msg->hdr.token);
			goto end_error;
		}

		 
		if (!client_data->response.data) {
			dev_err(dev,
				"Receiving buffer is null. Should be allocated by calling function\n");
			client_data->response.error = -EINVAL;
			goto error_wake_up;
		}

		if (data_len > client_data->response.max_size) {
			dev_err(dev,
				"Received buffer size %zu is larger than allocated buffer %zu\n",
				data_len, client_data->response.max_size);
			client_data->response.error = -EMSGSIZE;
			goto error_wake_up;
		}

		if (in_msg->hdr.status) {
			dev_err(dev, "firmware returned status %d\n",
				in_msg->hdr.status);
			client_data->response.error = -EIO;
			goto error_wake_up;
		}

		 
		client_data->response.size = data_len;

		 
		memcpy(client_data->response.data,
		       rb_in_proc->buffer.data, data_len);

error_wake_up:
		 
		ishtp_cl_io_rb_recycle(rb_in_proc);
		rb_in_proc = NULL;

		 
		client_data->response.received = true;

		 
		wake_up_interruptible(&client_data->response.wait_queue);

		break;

	case CROS_MKBP_EVENT:
		 
		ishtp_cl_io_rb_recycle(rb_in_proc);
		rb_in_proc = NULL;
		 
		client_data->ec_dev->last_event_time = timestamp;
		schedule_work(&client_data->work_ec_evt);

		break;

	default:
		dev_err(dev, "Invalid channel=%02d\n", in_msg->hdr.channel);
	}

end_error:
	 
	if (rb_in_proc)
		ishtp_cl_io_rb_recycle(rb_in_proc);

	up_read(&init_lock);
}

 
static void ish_event_cb(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl_rb *rb_in_proc;
	struct ishtp_cl	*cros_ish_cl = ishtp_get_drvdata(cl_device);
	ktime_t timestamp;

	 
	timestamp = cros_ec_get_time_ns();

	while ((rb_in_proc = ishtp_cl_rx_get_rb(cros_ish_cl)) != NULL) {
		 
		process_recv(cros_ish_cl, rb_in_proc, timestamp);
	}
}

 
static int cros_ish_init(struct ishtp_cl *cros_ish_cl)
{
	int rv;
	struct ishtp_device *dev;
	struct ishtp_fw_client *fw_client;
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);

	rv = ishtp_cl_link(cros_ish_cl);
	if (rv) {
		dev_err(cl_data_to_dev(client_data),
			"ishtp_cl_link failed\n");
		return rv;
	}

	dev = ishtp_get_ishtp_device(cros_ish_cl);

	 
	ishtp_set_tx_ring_size(cros_ish_cl, CROS_ISH_CL_TX_RING_SIZE);
	ishtp_set_rx_ring_size(cros_ish_cl, CROS_ISH_CL_RX_RING_SIZE);

	fw_client = ishtp_fw_cl_get_client(dev, &cros_ec_ishtp_id_table[0].guid);
	if (!fw_client) {
		dev_err(cl_data_to_dev(client_data),
			"ish client uuid not found\n");
		rv = -ENOENT;
		goto err_cl_unlink;
	}

	ishtp_cl_set_fw_client_id(cros_ish_cl,
				  ishtp_get_fw_client_id(fw_client));
	ishtp_set_connection_state(cros_ish_cl, ISHTP_CL_CONNECTING);

	rv = ishtp_cl_connect(cros_ish_cl);
	if (rv) {
		dev_err(cl_data_to_dev(client_data),
			"client connect fail\n");
		goto err_cl_unlink;
	}

	ishtp_register_event_cb(client_data->cl_device, ish_event_cb);
	return 0;

err_cl_unlink:
	ishtp_cl_unlink(cros_ish_cl);
	return rv;
}

 
static void cros_ish_deinit(struct ishtp_cl *cros_ish_cl)
{
	ishtp_set_connection_state(cros_ish_cl, ISHTP_CL_DISCONNECTING);
	ishtp_cl_disconnect(cros_ish_cl);
	ishtp_cl_unlink(cros_ish_cl);
	ishtp_cl_flush_queues(cros_ish_cl);

	 
	ishtp_cl_free(cros_ish_cl);
}

 
static int prepare_cros_ec_rx(struct cros_ec_device *ec_dev,
			      const struct cros_ish_in_msg *in_msg,
			      struct cros_ec_command *msg)
{
	u8 sum = 0;
	int i, rv, offset;

	 
	msg->result = in_msg->ec_response.result;
	rv = cros_ec_check_result(ec_dev, msg);
	if (rv < 0)
		return rv;

	if (in_msg->ec_response.data_len > msg->insize) {
		dev_err(ec_dev->dev, "Packet too long (%d bytes, expected %d)",
			in_msg->ec_response.data_len, msg->insize);
		return -ENOSPC;
	}

	 
	for (i = 0; i < sizeof(struct ec_host_response); i++)
		sum += ((u8 *)in_msg)[IN_MSG_EC_RESPONSE_PREAMBLE + i];

	offset = sizeof(struct cros_ish_in_msg);
	for (i = 0; i < in_msg->ec_response.data_len; i++)
		sum += msg->data[i] = ((u8 *)in_msg)[offset + i];

	if (sum) {
		dev_dbg(ec_dev->dev, "Bad received packet checksum %d\n", sum);
		return -EBADMSG;
	}

	return 0;
}

static int cros_ec_pkt_xfer_ish(struct cros_ec_device *ec_dev,
				struct cros_ec_command *msg)
{
	int rv;
	struct ishtp_cl *cros_ish_cl = ec_dev->priv;
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);
	struct device *dev = cl_data_to_dev(client_data);
	struct cros_ish_in_msg *in_msg = (struct cros_ish_in_msg *)ec_dev->din;
	struct cros_ish_out_msg *out_msg =
		(struct cros_ish_out_msg *)ec_dev->dout;
	size_t in_size = sizeof(struct cros_ish_in_msg) + msg->insize;
	size_t out_size = sizeof(struct cros_ish_out_msg) + msg->outsize;

	 
	if (in_size > ec_dev->din_size) {
		dev_err(dev,
			"Incoming payload size %zu is too large for ec_dev->din_size %d\n",
			in_size, ec_dev->din_size);
		return -EMSGSIZE;
	}

	if (out_size > ec_dev->dout_size) {
		dev_err(dev,
			"Outgoing payload size %zu is too large for ec_dev->dout_size %d\n",
			out_size, ec_dev->dout_size);
		return -EMSGSIZE;
	}

	 
	if (!down_read_trylock(&init_lock)) {
		dev_warn(dev,
			 "Host is not ready to send messages to ISH. Try again\n");
		return -EAGAIN;
	}

	 
	out_msg->hdr.channel = CROS_EC_COMMAND;
	out_msg->hdr.status = 0;

	ec_dev->dout += OUT_MSG_EC_REQUEST_PREAMBLE;
	rv = cros_ec_prepare_tx(ec_dev, msg);
	if (rv < 0)
		goto end_error;
	ec_dev->dout -= OUT_MSG_EC_REQUEST_PREAMBLE;

	dev_dbg(dev,
		"out_msg: struct_ver=0x%x checksum=0x%x command=0x%x command_ver=0x%x data_len=0x%x\n",
		out_msg->ec_request.struct_version,
		out_msg->ec_request.checksum,
		out_msg->ec_request.command,
		out_msg->ec_request.command_version,
		out_msg->ec_request.data_len);

	 
	rv = ish_send(client_data,
		      (u8 *)out_msg, out_size,
		      (u8 *)in_msg, in_size);
	if (rv < 0)
		goto end_error;

	rv = prepare_cros_ec_rx(ec_dev, in_msg, msg);
	if (rv)
		goto end_error;

	rv = in_msg->ec_response.data_len;

	dev_dbg(dev,
		"in_msg: struct_ver=0x%x checksum=0x%x result=0x%x data_len=0x%x\n",
		in_msg->ec_response.struct_version,
		in_msg->ec_response.checksum,
		in_msg->ec_response.result,
		in_msg->ec_response.data_len);

end_error:
	if (msg->command == EC_CMD_REBOOT_EC)
		msleep(EC_REBOOT_DELAY_MS);

	up_read(&init_lock);

	return rv;
}

static int cros_ec_dev_init(struct ishtp_cl_data *client_data)
{
	struct cros_ec_device *ec_dev;
	struct device *dev = cl_data_to_dev(client_data);

	ec_dev = devm_kzalloc(dev, sizeof(*ec_dev), GFP_KERNEL);
	if (!ec_dev)
		return -ENOMEM;

	client_data->ec_dev = ec_dev;
	dev->driver_data = ec_dev;

	ec_dev->dev = dev;
	ec_dev->priv = client_data->cros_ish_cl;
	ec_dev->cmd_xfer = NULL;
	ec_dev->pkt_xfer = cros_ec_pkt_xfer_ish;
	ec_dev->phys_name = dev_name(dev);
	ec_dev->din_size = sizeof(struct cros_ish_in_msg) +
			   sizeof(struct ec_response_get_protocol_info);
	ec_dev->dout_size = sizeof(struct cros_ish_out_msg);

	return cros_ec_register(ec_dev);
}

static void reset_handler(struct work_struct *work)
{
	int rv;
	struct device *dev;
	struct ishtp_cl *cros_ish_cl;
	struct ishtp_cl_device *cl_device;
	struct ishtp_cl_data *client_data =
		container_of(work, struct ishtp_cl_data, work_ishtp_reset);

	 
	down_write(&init_lock);

	cros_ish_cl = client_data->cros_ish_cl;
	cl_device = client_data->cl_device;

	 
	ishtp_cl_unlink(cros_ish_cl);
	ishtp_cl_flush_queues(cros_ish_cl);
	ishtp_cl_free(cros_ish_cl);

	cros_ish_cl = ishtp_cl_allocate(cl_device);
	if (!cros_ish_cl) {
		up_write(&init_lock);
		return;
	}

	ishtp_set_drvdata(cl_device, cros_ish_cl);
	ishtp_set_client_data(cros_ish_cl, client_data);
	client_data->cros_ish_cl = cros_ish_cl;

	rv = cros_ish_init(cros_ish_cl);
	if (rv) {
		ishtp_cl_free(cros_ish_cl);
		dev_err(cl_data_to_dev(client_data), "Reset Failed\n");
		up_write(&init_lock);
		return;
	}

	 
	client_data->ec_dev->priv = client_data->cros_ish_cl;
	dev = cl_data_to_dev(client_data);
	dev->driver_data = client_data->ec_dev;

	dev_info(cl_data_to_dev(client_data), "Chrome EC ISH reset done\n");

	up_write(&init_lock);
}

 
static int cros_ec_ishtp_probe(struct ishtp_cl_device *cl_device)
{
	int rv;
	struct ishtp_cl *cros_ish_cl;
	struct ishtp_cl_data *client_data =
		devm_kzalloc(ishtp_device(cl_device),
			     sizeof(*client_data), GFP_KERNEL);
	if (!client_data)
		return -ENOMEM;

	 
	down_write(&init_lock);

	cros_ish_cl = ishtp_cl_allocate(cl_device);
	if (!cros_ish_cl) {
		rv = -ENOMEM;
		goto end_ishtp_cl_alloc_error;
	}

	ishtp_set_drvdata(cl_device, cros_ish_cl);
	ishtp_set_client_data(cros_ish_cl, client_data);
	client_data->cros_ish_cl = cros_ish_cl;
	client_data->cl_device = cl_device;

	init_waitqueue_head(&client_data->response.wait_queue);

	INIT_WORK(&client_data->work_ishtp_reset,
		  reset_handler);
	INIT_WORK(&client_data->work_ec_evt,
		  ish_evt_handler);

	rv = cros_ish_init(cros_ish_cl);
	if (rv)
		goto end_ishtp_cl_init_error;

	ishtp_get_device(cl_device);

	up_write(&init_lock);

	 
	rv = cros_ec_dev_init(client_data);
	if (rv) {
		down_write(&init_lock);
		goto end_cros_ec_dev_init_error;
	}

	return 0;

end_cros_ec_dev_init_error:
	ishtp_set_connection_state(cros_ish_cl, ISHTP_CL_DISCONNECTING);
	ishtp_cl_disconnect(cros_ish_cl);
	ishtp_cl_unlink(cros_ish_cl);
	ishtp_cl_flush_queues(cros_ish_cl);
	ishtp_put_device(cl_device);
end_ishtp_cl_init_error:
	ishtp_cl_free(cros_ish_cl);
end_ishtp_cl_alloc_error:
	up_write(&init_lock);
	return rv;
}

 
static void cros_ec_ishtp_remove(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl	*cros_ish_cl = ishtp_get_drvdata(cl_device);
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);

	cancel_work_sync(&client_data->work_ishtp_reset);
	cancel_work_sync(&client_data->work_ec_evt);
	cros_ish_deinit(cros_ish_cl);
	ishtp_put_device(cl_device);
}

 
static int cros_ec_ishtp_reset(struct ishtp_cl_device *cl_device)
{
	struct ishtp_cl	*cros_ish_cl = ishtp_get_drvdata(cl_device);
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);

	schedule_work(&client_data->work_ishtp_reset);

	return 0;
}

 
static int __maybe_unused cros_ec_ishtp_suspend(struct device *device)
{
	struct ishtp_cl_device *cl_device = ishtp_dev_to_cl_device(device);
	struct ishtp_cl	*cros_ish_cl = ishtp_get_drvdata(cl_device);
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);

	return cros_ec_suspend(client_data->ec_dev);
}

 
static int __maybe_unused cros_ec_ishtp_resume(struct device *device)
{
	struct ishtp_cl_device *cl_device = ishtp_dev_to_cl_device(device);
	struct ishtp_cl	*cros_ish_cl = ishtp_get_drvdata(cl_device);
	struct ishtp_cl_data *client_data = ishtp_get_client_data(cros_ish_cl);

	return cros_ec_resume(client_data->ec_dev);
}

static SIMPLE_DEV_PM_OPS(cros_ec_ishtp_pm_ops, cros_ec_ishtp_suspend,
			 cros_ec_ishtp_resume);

static struct ishtp_cl_driver	cros_ec_ishtp_driver = {
	.name = "cros_ec_ishtp",
	.id = cros_ec_ishtp_id_table,
	.probe = cros_ec_ishtp_probe,
	.remove = cros_ec_ishtp_remove,
	.reset = cros_ec_ishtp_reset,
	.driver = {
		.pm = &cros_ec_ishtp_pm_ops,
	},
};

static int __init cros_ec_ishtp_mod_init(void)
{
	return ishtp_cl_driver_register(&cros_ec_ishtp_driver, THIS_MODULE);
}

static void __exit cros_ec_ishtp_mod_exit(void)
{
	ishtp_cl_driver_unregister(&cros_ec_ishtp_driver);
}

module_init(cros_ec_ishtp_mod_init);
module_exit(cros_ec_ishtp_mod_exit);

MODULE_DESCRIPTION("ChromeOS EC ISHTP Client Driver");
MODULE_AUTHOR("Rushikesh S Kadam <rushikesh.s.kadam@intel.com>");

MODULE_LICENSE("GPL v2");
