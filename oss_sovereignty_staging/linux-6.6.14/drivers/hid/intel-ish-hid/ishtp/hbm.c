
 

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include "ishtp-dev.h"
#include "hbm.h"
#include "client.h"

 
static void ishtp_hbm_fw_cl_allocate(struct ishtp_device *dev)
{
	struct ishtp_fw_client *clients;
	int b;

	 
	for_each_set_bit(b, dev->fw_clients_map, ISHTP_CLIENTS_MAX)
		dev->fw_clients_num++;

	if (dev->fw_clients_num <= 0)
		return;

	 
	clients = kcalloc(dev->fw_clients_num, sizeof(struct ishtp_fw_client),
			  GFP_KERNEL);
	if (!clients) {
		dev->dev_state = ISHTP_DEV_RESETTING;
		ish_hw_reset(dev);
		return;
	}
	dev->fw_clients = clients;
}

 
static inline void ishtp_hbm_cl_hdr(struct ishtp_cl *cl, uint8_t hbm_cmd,
	void *buf, size_t len)
{
	struct ishtp_hbm_cl_cmd *cmd = buf;

	memset(cmd, 0, len);

	cmd->hbm_cmd = hbm_cmd;
	cmd->host_addr = cl->host_client_id;
	cmd->fw_addr = cl->fw_client_id;
}

 
static inline bool ishtp_hbm_cl_addr_equal(struct ishtp_cl *cl, void *buf)
{
	struct ishtp_hbm_cl_cmd *cmd = buf;

	return cl->host_client_id == cmd->host_addr &&
		cl->fw_client_id == cmd->fw_addr;
}

 
int ishtp_hbm_start_wait(struct ishtp_device *dev)
{
	int ret;

	if (dev->hbm_state > ISHTP_HBM_START)
		return 0;

	dev_dbg(dev->devc, "Going to wait for ishtp start. hbm_state=%08X\n",
		dev->hbm_state);
	ret = wait_event_interruptible_timeout(dev->wait_hbm_recvd_msg,
					dev->hbm_state >= ISHTP_HBM_STARTED,
					(ISHTP_INTEROP_TIMEOUT * HZ));

	dev_dbg(dev->devc,
		"Woke up from waiting for ishtp start. hbm_state=%08X\n",
		dev->hbm_state);

	if (ret <= 0 && (dev->hbm_state <= ISHTP_HBM_START)) {
		dev->hbm_state = ISHTP_HBM_IDLE;
		dev_err(dev->devc,
		"waiting for ishtp start failed. ret=%d hbm_state=%08X\n",
			ret, dev->hbm_state);
		return -ETIMEDOUT;
	}
	return 0;
}

 
int ishtp_hbm_start_req(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_host_version_request start_req = { 0 };

	ishtp_hbm_hdr(&hdr, sizeof(start_req));

	 
	start_req.hbm_cmd = HOST_START_REQ_CMD;
	start_req.host_version.major_version = HBM_MAJOR_VERSION;
	start_req.host_version.minor_version = HBM_MINOR_VERSION;

	 
	dev->hbm_state = ISHTP_HBM_START;
	if (ishtp_write_message(dev, &hdr, &start_req)) {
		dev_err(dev->devc, "version message send failed\n");
		dev->dev_state = ISHTP_DEV_RESETTING;
		dev->hbm_state = ISHTP_HBM_IDLE;
		ish_hw_reset(dev);
		return -ENODEV;
	}

	return 0;
}

 
void ishtp_hbm_enum_clients_req(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_host_enum_request enum_req = { 0 };

	 
	ishtp_hbm_hdr(&hdr, sizeof(enum_req));
	enum_req.hbm_cmd = HOST_ENUM_REQ_CMD;

	if (ishtp_write_message(dev, &hdr, &enum_req)) {
		dev->dev_state = ISHTP_DEV_RESETTING;
		dev_err(dev->devc, "enumeration request send failed\n");
		ish_hw_reset(dev);
	}
	dev->hbm_state = ISHTP_HBM_ENUM_CLIENTS;
}

 
static int ishtp_hbm_prop_req(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_props_request prop_req = { 0 };
	unsigned long next_client_index;
	uint8_t client_num;

	client_num = dev->fw_client_presentation_num;

	next_client_index = find_next_bit(dev->fw_clients_map,
		ISHTP_CLIENTS_MAX, dev->fw_client_index);

	 
	if (next_client_index == ISHTP_CLIENTS_MAX) {
		dev->hbm_state = ISHTP_HBM_WORKING;
		dev->dev_state = ISHTP_DEV_ENABLED;

		for (dev->fw_client_presentation_num = 1;
			dev->fw_client_presentation_num < client_num + 1;
				++dev->fw_client_presentation_num)
			 
			ishtp_bus_new_client(dev);
		return 0;
	}

	dev->fw_clients[client_num].client_id = next_client_index;

	ishtp_hbm_hdr(&hdr, sizeof(prop_req));

	prop_req.hbm_cmd = HOST_CLIENT_PROPERTIES_REQ_CMD;
	prop_req.address = next_client_index;

	if (ishtp_write_message(dev, &hdr, &prop_req)) {
		dev->dev_state = ISHTP_DEV_RESETTING;
		dev_err(dev->devc, "properties request send failed\n");
		ish_hw_reset(dev);
		return -EIO;
	}

	dev->fw_client_index = next_client_index;

	return 0;
}

 
static void ishtp_hbm_stop_req(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_host_stop_request stop_req = { 0 } ;

	ishtp_hbm_hdr(&hdr, sizeof(stop_req));

	stop_req.hbm_cmd = HOST_STOP_REQ_CMD;
	stop_req.reason = DRIVER_STOP_REQUEST;

	ishtp_write_message(dev, &hdr, &stop_req);
}

 
int ishtp_hbm_cl_flow_control_req(struct ishtp_device *dev,
				  struct ishtp_cl *cl)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_flow_control flow_ctrl;
	const size_t len = sizeof(flow_ctrl);
	int	rv;
	unsigned long	flags;

	spin_lock_irqsave(&cl->fc_spinlock, flags);

	ishtp_hbm_hdr(&hdr, len);
	ishtp_hbm_cl_hdr(cl, ISHTP_FLOW_CONTROL_CMD, &flow_ctrl, len);

	 
	if (cl->out_flow_ctrl_creds) {
		spin_unlock_irqrestore(&cl->fc_spinlock, flags);
		return	0;
	}

	cl->recv_msg_num_frags = 0;

	rv = ishtp_write_message(dev, &hdr, &flow_ctrl);
	if (!rv) {
		++cl->out_flow_ctrl_creds;
		++cl->out_flow_ctrl_cnt;
		cl->ts_out_fc = ktime_get();
		if (cl->ts_rx) {
			ktime_t ts_diff = ktime_sub(cl->ts_out_fc, cl->ts_rx);
			if (ktime_after(ts_diff, cl->ts_max_fc_delay))
				cl->ts_max_fc_delay = ts_diff;
		}
	} else {
		++cl->err_send_fc;
	}

	spin_unlock_irqrestore(&cl->fc_spinlock, flags);
	return	rv;
}

 
int ishtp_hbm_cl_disconnect_req(struct ishtp_device *dev, struct ishtp_cl *cl)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_client_connect_request disconn_req;
	const size_t len = sizeof(disconn_req);

	ishtp_hbm_hdr(&hdr, len);
	ishtp_hbm_cl_hdr(cl, CLIENT_DISCONNECT_REQ_CMD, &disconn_req, len);

	return ishtp_write_message(dev, &hdr, &disconn_req);
}

 
static void ishtp_hbm_cl_disconnect_res(struct ishtp_device *dev,
	struct hbm_client_connect_response *rs)
{
	struct ishtp_cl *cl = NULL;
	unsigned long	flags;

	spin_lock_irqsave(&dev->cl_list_lock, flags);
	list_for_each_entry(cl, &dev->cl_list, link) {
		if (!rs->status && ishtp_hbm_cl_addr_equal(cl, rs)) {
			cl->state = ISHTP_CL_DISCONNECTED;
			wake_up_interruptible(&cl->wait_ctrl_res);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->cl_list_lock, flags);
}

 
int ishtp_hbm_cl_connect_req(struct ishtp_device *dev, struct ishtp_cl *cl)
{
	struct ishtp_msg_hdr hdr;
	struct hbm_client_connect_request conn_req;
	const size_t len = sizeof(conn_req);

	ishtp_hbm_hdr(&hdr, len);
	ishtp_hbm_cl_hdr(cl, CLIENT_CONNECT_REQ_CMD, &conn_req, len);

	return ishtp_write_message(dev, &hdr, &conn_req);
}

 
static void ishtp_hbm_cl_connect_res(struct ishtp_device *dev,
	struct hbm_client_connect_response *rs)
{
	struct ishtp_cl *cl = NULL;
	unsigned long	flags;

	spin_lock_irqsave(&dev->cl_list_lock, flags);
	list_for_each_entry(cl, &dev->cl_list, link) {
		if (ishtp_hbm_cl_addr_equal(cl, rs)) {
			if (!rs->status) {
				cl->state = ISHTP_CL_CONNECTED;
				cl->status = 0;
			} else {
				cl->state = ISHTP_CL_DISCONNECTED;
				cl->status = -ENODEV;
			}
			wake_up_interruptible(&cl->wait_ctrl_res);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->cl_list_lock, flags);
}

 
static void ishtp_hbm_fw_disconnect_req(struct ishtp_device *dev,
	struct hbm_client_connect_request *disconnect_req)
{
	struct ishtp_cl *cl;
	const size_t len = sizeof(struct hbm_client_connect_response);
	unsigned long	flags;
	struct ishtp_msg_hdr hdr;
	unsigned char data[4];	 

	spin_lock_irqsave(&dev->cl_list_lock, flags);
	list_for_each_entry(cl, &dev->cl_list, link) {
		if (ishtp_hbm_cl_addr_equal(cl, disconnect_req)) {
			cl->state = ISHTP_CL_DISCONNECTED;

			 
			ishtp_hbm_hdr(&hdr, len);
			ishtp_hbm_cl_hdr(cl, CLIENT_DISCONNECT_RES_CMD, data,
				len);
			ishtp_write_message(dev, &hdr, data);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->cl_list_lock, flags);
}

 
static void ishtp_hbm_dma_xfer_ack(struct ishtp_device *dev,
				   struct dma_xfer_hbm *dma_xfer)
{
	void	*msg;
	uint64_t	offs;
	struct ishtp_msg_hdr	*ishtp_hdr =
		(struct ishtp_msg_hdr *)&dev->ishtp_msg_hdr;
	unsigned int	msg_offs;
	struct ishtp_cl *cl;

	for (msg_offs = 0; msg_offs < ishtp_hdr->length;
		msg_offs += sizeof(struct dma_xfer_hbm)) {
		offs = dma_xfer->msg_addr - dev->ishtp_host_dma_tx_buf_phys;
		if (offs > dev->ishtp_host_dma_tx_buf_size) {
			dev_err(dev->devc, "Bad DMA Tx ack message address\n");
			return;
		}
		if (dma_xfer->msg_length >
				dev->ishtp_host_dma_tx_buf_size - offs) {
			dev_err(dev->devc, "Bad DMA Tx ack message size\n");
			return;
		}

		 
		msg = (unsigned char *)dev->ishtp_host_dma_tx_buf + offs;
		ishtp_cl_release_dma_acked_mem(dev, msg, dma_xfer->msg_length);

		list_for_each_entry(cl, &dev->cl_list, link) {
			if (cl->fw_client_id == dma_xfer->fw_client_id &&
			    cl->host_client_id == dma_xfer->host_client_id)
				 
				if (cl->last_dma_addr >=
							(unsigned char *)msg &&
						cl->last_dma_addr <
						(unsigned char *)msg +
						dma_xfer->msg_length) {
					cl->last_dma_acked = 1;

					if (!list_empty(&cl->tx_list.list) &&
						cl->ishtp_flow_ctrl_creds) {
						 
						ishtp_cl_send_msg(dev, cl);
					}
				}
		}
		++dma_xfer;
	}
}

 
static void ishtp_hbm_dma_xfer(struct ishtp_device *dev,
			       struct dma_xfer_hbm *dma_xfer)
{
	void	*msg;
	uint64_t	offs;
	struct ishtp_msg_hdr	hdr;
	struct ishtp_msg_hdr	*ishtp_hdr =
		(struct ishtp_msg_hdr *) &dev->ishtp_msg_hdr;
	struct dma_xfer_hbm	*prm = dma_xfer;
	unsigned int	msg_offs;

	for (msg_offs = 0; msg_offs < ishtp_hdr->length;
		msg_offs += sizeof(struct dma_xfer_hbm)) {

		offs = dma_xfer->msg_addr - dev->ishtp_host_dma_rx_buf_phys;
		if (offs > dev->ishtp_host_dma_rx_buf_size) {
			dev_err(dev->devc, "Bad DMA Rx message address\n");
			return;
		}
		if (dma_xfer->msg_length >
				dev->ishtp_host_dma_rx_buf_size - offs) {
			dev_err(dev->devc, "Bad DMA Rx message size\n");
			return;
		}
		msg = dev->ishtp_host_dma_rx_buf + offs;
		recv_ishtp_cl_msg_dma(dev, msg, dma_xfer);
		dma_xfer->hbm = DMA_XFER_ACK;	 
		++dma_xfer;
	}

	 
	ishtp_hbm_hdr(&hdr, ishtp_hdr->length);
	ishtp_write_message(dev, &hdr, (unsigned char *)prm);
}

 
void ishtp_hbm_dispatch(struct ishtp_device *dev,
			struct ishtp_bus_message *hdr)
{
	struct ishtp_bus_message *ishtp_msg;
	struct ishtp_fw_client *fw_client;
	struct hbm_host_version_response *version_res;
	struct hbm_client_connect_response *connect_res;
	struct hbm_client_connect_response *disconnect_res;
	struct hbm_client_connect_request *disconnect_req;
	struct hbm_props_response *props_res;
	struct hbm_host_enum_response *enum_res;
	struct ishtp_msg_hdr ishtp_hdr;
	struct dma_alloc_notify	dma_alloc_notify;
	struct dma_xfer_hbm	*dma_xfer;

	ishtp_msg = hdr;

	switch (ishtp_msg->hbm_cmd) {
	case HOST_START_RES_CMD:
		version_res = (struct hbm_host_version_response *)ishtp_msg;
		if (!version_res->host_version_supported) {
			dev->version = version_res->fw_max_version;

			dev->hbm_state = ISHTP_HBM_STOPPED;
			ishtp_hbm_stop_req(dev);
			return;
		}

		dev->version.major_version = HBM_MAJOR_VERSION;
		dev->version.minor_version = HBM_MINOR_VERSION;
		if (dev->dev_state == ISHTP_DEV_INIT_CLIENTS &&
				dev->hbm_state == ISHTP_HBM_START) {
			dev->hbm_state = ISHTP_HBM_STARTED;
			ishtp_hbm_enum_clients_req(dev);
		} else {
			dev_err(dev->devc,
				"reset: wrong host start response\n");
			 
			ish_hw_reset(dev);
			return;
		}

		wake_up_interruptible(&dev->wait_hbm_recvd_msg);
		break;

	case CLIENT_CONNECT_RES_CMD:
		connect_res = (struct hbm_client_connect_response *)ishtp_msg;
		ishtp_hbm_cl_connect_res(dev, connect_res);
		break;

	case CLIENT_DISCONNECT_RES_CMD:
		disconnect_res =
			(struct hbm_client_connect_response *)ishtp_msg;
		ishtp_hbm_cl_disconnect_res(dev, disconnect_res);
		break;

	case HOST_CLIENT_PROPERTIES_RES_CMD:
		props_res = (struct hbm_props_response *)ishtp_msg;
		fw_client = &dev->fw_clients[dev->fw_client_presentation_num];

		if (props_res->status || !dev->fw_clients) {
			dev_err(dev->devc,
			"reset: properties response hbm wrong status\n");
			ish_hw_reset(dev);
			return;
		}

		if (fw_client->client_id != props_res->address) {
			dev_err(dev->devc,
				"reset: host properties response address mismatch [%02X %02X]\n",
				fw_client->client_id, props_res->address);
			ish_hw_reset(dev);
			return;
		}

		if (dev->dev_state != ISHTP_DEV_INIT_CLIENTS ||
			dev->hbm_state != ISHTP_HBM_CLIENT_PROPERTIES) {
			dev_err(dev->devc,
				"reset: unexpected properties response\n");
			ish_hw_reset(dev);
			return;
		}

		fw_client->props = props_res->client_properties;
		dev->fw_client_index++;
		dev->fw_client_presentation_num++;

		 
		ishtp_hbm_prop_req(dev);

		if (dev->dev_state != ISHTP_DEV_ENABLED)
			break;

		if (!ishtp_use_dma_transfer())
			break;

		dev_dbg(dev->devc, "Requesting to use DMA\n");
		ishtp_cl_alloc_dma_buf(dev);
		if (dev->ishtp_host_dma_rx_buf) {
			const size_t len = sizeof(dma_alloc_notify);

			memset(&dma_alloc_notify, 0, sizeof(dma_alloc_notify));
			dma_alloc_notify.hbm = DMA_BUFFER_ALLOC_NOTIFY;
			dma_alloc_notify.buf_size =
					dev->ishtp_host_dma_rx_buf_size;
			dma_alloc_notify.buf_address =
					dev->ishtp_host_dma_rx_buf_phys;
			ishtp_hbm_hdr(&ishtp_hdr, len);
			ishtp_write_message(dev, &ishtp_hdr,
				(unsigned char *)&dma_alloc_notify);
		}

		break;

	case HOST_ENUM_RES_CMD:
		enum_res = (struct hbm_host_enum_response *) ishtp_msg;
		memcpy(dev->fw_clients_map, enum_res->valid_addresses, 32);
		if (dev->dev_state == ISHTP_DEV_INIT_CLIENTS &&
			dev->hbm_state == ISHTP_HBM_ENUM_CLIENTS) {
			dev->fw_client_presentation_num = 0;
			dev->fw_client_index = 0;

			ishtp_hbm_fw_cl_allocate(dev);
			dev->hbm_state = ISHTP_HBM_CLIENT_PROPERTIES;

			 
			ishtp_hbm_prop_req(dev);
		} else {
			dev_err(dev->devc,
			      "reset: unexpected enumeration response hbm\n");
			ish_hw_reset(dev);
			return;
		}
		break;

	case HOST_STOP_RES_CMD:
		if (dev->hbm_state != ISHTP_HBM_STOPPED)
			dev_err(dev->devc, "unexpected stop response\n");

		dev->dev_state = ISHTP_DEV_DISABLED;
		dev_info(dev->devc, "reset: FW stop response\n");
		ish_hw_reset(dev);
		break;

	case CLIENT_DISCONNECT_REQ_CMD:
		 
		disconnect_req =
			(struct hbm_client_connect_request *)ishtp_msg;
		ishtp_hbm_fw_disconnect_req(dev, disconnect_req);
		break;

	case FW_STOP_REQ_CMD:
		dev->hbm_state = ISHTP_HBM_STOPPED;
		break;

	case DMA_BUFFER_ALLOC_RESPONSE:
		dev->ishtp_host_dma_enabled = 1;
		break;

	case DMA_XFER:
		dma_xfer = (struct dma_xfer_hbm *)ishtp_msg;
		if (!dev->ishtp_host_dma_enabled) {
			dev_err(dev->devc,
				"DMA XFER requested but DMA is not enabled\n");
			break;
		}
		ishtp_hbm_dma_xfer(dev, dma_xfer);
		break;

	case DMA_XFER_ACK:
		dma_xfer = (struct dma_xfer_hbm *)ishtp_msg;
		if (!dev->ishtp_host_dma_enabled ||
		    !dev->ishtp_host_dma_tx_buf) {
			dev_err(dev->devc,
				"DMA XFER acked but DMA Tx is not enabled\n");
			break;
		}
		ishtp_hbm_dma_xfer_ack(dev, dma_xfer);
		break;

	default:
		dev_err(dev->devc, "unknown HBM: %u\n",
			(unsigned int)ishtp_msg->hbm_cmd);

		break;
	}
}

 
void	bh_hbm_work_fn(struct work_struct *work)
{
	unsigned long	flags;
	struct ishtp_device	*dev;
	unsigned char	hbm[IPC_PAYLOAD_SIZE];

	dev = container_of(work, struct ishtp_device, bh_hbm_work);
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	if (dev->rd_msg_fifo_head != dev->rd_msg_fifo_tail) {
		memcpy(hbm, dev->rd_msg_fifo + dev->rd_msg_fifo_head,
			IPC_PAYLOAD_SIZE);
		dev->rd_msg_fifo_head =
			(dev->rd_msg_fifo_head + IPC_PAYLOAD_SIZE) %
			(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE);
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
		ishtp_hbm_dispatch(dev, (struct ishtp_bus_message *)hbm);
	} else {
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
	}
}

 
void	recv_hbm(struct ishtp_device *dev, struct ishtp_msg_hdr *ishtp_hdr)
{
	uint8_t	rd_msg_buf[ISHTP_RD_MSG_BUF_SIZE];
	struct ishtp_bus_message	*ishtp_msg =
		(struct ishtp_bus_message *)rd_msg_buf;
	unsigned long	flags;

	dev->ops->ishtp_read(dev, rd_msg_buf, ishtp_hdr->length);

	 
	if (ishtp_msg->hbm_cmd == ISHTP_FLOW_CONTROL_CMD) {
		struct hbm_flow_control *flow_control =
			(struct hbm_flow_control *)ishtp_msg;
		struct ishtp_cl *cl = NULL;
		unsigned long	flags, tx_flags;

		spin_lock_irqsave(&dev->cl_list_lock, flags);
		list_for_each_entry(cl, &dev->cl_list, link) {
			if (cl->host_client_id == flow_control->host_addr &&
					cl->fw_client_id ==
					flow_control->fw_addr) {
				 
				if (cl->ishtp_flow_ctrl_creds)
					dev_err(dev->devc,
					 "recv extra FC from FW client %u (host client %u) (FC count was %d)\n",
					 (unsigned int)cl->fw_client_id,
					 (unsigned int)cl->host_client_id,
					 cl->ishtp_flow_ctrl_creds);
				else {
					++cl->ishtp_flow_ctrl_creds;
					++cl->ishtp_flow_ctrl_cnt;
					cl->last_ipc_acked = 1;
					spin_lock_irqsave(
							&cl->tx_list_spinlock,
							tx_flags);
					if (!list_empty(&cl->tx_list.list)) {
						 
						spin_unlock_irqrestore(
							&cl->tx_list_spinlock,
							tx_flags);
						ishtp_cl_send_msg(dev, cl);
					} else {
						spin_unlock_irqrestore(
							&cl->tx_list_spinlock,
							tx_flags);
					}
				}
				break;
			}
		}
		spin_unlock_irqrestore(&dev->cl_list_lock, flags);
		goto	eoi;
	}

	 
	if (ishtp_msg->hbm_cmd == CLIENT_CONNECT_RES_CMD ||
			ishtp_msg->hbm_cmd == CLIENT_DISCONNECT_RES_CMD ||
			ishtp_msg->hbm_cmd == CLIENT_DISCONNECT_REQ_CMD ||
			ishtp_msg->hbm_cmd == DMA_XFER) {
		ishtp_hbm_dispatch(dev, ishtp_msg);
		goto	eoi;
	}

	 
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	if ((dev->rd_msg_fifo_tail + IPC_PAYLOAD_SIZE) %
			(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE) ==
			dev->rd_msg_fifo_head) {
		spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
		dev_err(dev->devc, "BH buffer overflow, dropping HBM %u\n",
			(unsigned int)ishtp_msg->hbm_cmd);
		goto	eoi;
	}
	memcpy(dev->rd_msg_fifo + dev->rd_msg_fifo_tail, ishtp_msg,
		ishtp_hdr->length);
	dev->rd_msg_fifo_tail = (dev->rd_msg_fifo_tail + IPC_PAYLOAD_SIZE) %
		(RD_INT_FIFO_SIZE * IPC_PAYLOAD_SIZE);
	spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);
	schedule_work(&dev->bh_hbm_work);
eoi:
	return;
}

 
void recv_fixed_cl_msg(struct ishtp_device *dev,
	struct ishtp_msg_hdr *ishtp_hdr)
{
	uint8_t rd_msg_buf[ISHTP_RD_MSG_BUF_SIZE];

	dev->print_log(dev,
		"%s() got fixed client msg from client #%d\n",
		__func__, ishtp_hdr->fw_addr);
	dev->ops->ishtp_read(dev, rd_msg_buf, ishtp_hdr->length);
	if (ishtp_hdr->fw_addr == ISHTP_SYSTEM_STATE_CLIENT_ADDR) {
		struct ish_system_states_header *msg_hdr =
			(struct ish_system_states_header *)rd_msg_buf;
		if (msg_hdr->cmd == SYSTEM_STATE_SUBSCRIBE)
			ishtp_send_resume(dev);
		 
		else
			dev_err(dev->devc, "unknown fixed client msg [%02X]\n",
				msg_hdr->cmd);
	}
}

 
static inline void fix_cl_hdr(struct ishtp_msg_hdr *hdr, size_t length,
	uint8_t cl_addr)
{
	hdr->host_addr = 0;
	hdr->fw_addr = cl_addr;
	hdr->length = length;
	hdr->msg_complete = 1;
	hdr->reserved = 0;
}

 

static uint32_t current_state;
static uint32_t supported_states = SUSPEND_STATE_BIT | CONNECTED_STANDBY_STATE_BIT;

 
void ishtp_send_suspend(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr	ishtp_hdr;
	struct ish_system_states_status state_status_msg;
	const size_t len = sizeof(struct ish_system_states_status);

	fix_cl_hdr(&ishtp_hdr, len, ISHTP_SYSTEM_STATE_CLIENT_ADDR);

	memset(&state_status_msg, 0, len);
	state_status_msg.hdr.cmd = SYSTEM_STATE_STATUS;
	state_status_msg.supported_states = supported_states;
	current_state |= (SUSPEND_STATE_BIT | CONNECTED_STANDBY_STATE_BIT);
	dev->print_log(dev, "%s() sends SUSPEND notification\n", __func__);
	state_status_msg.states_status = current_state;

	ishtp_write_message(dev, &ishtp_hdr,
		(unsigned char *)&state_status_msg);
}
EXPORT_SYMBOL(ishtp_send_suspend);

 
void ishtp_send_resume(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr	ishtp_hdr;
	struct ish_system_states_status state_status_msg;
	const size_t len = sizeof(struct ish_system_states_status);

	fix_cl_hdr(&ishtp_hdr, len, ISHTP_SYSTEM_STATE_CLIENT_ADDR);

	memset(&state_status_msg, 0, len);
	state_status_msg.hdr.cmd = SYSTEM_STATE_STATUS;
	state_status_msg.supported_states = supported_states;
	current_state &= ~(CONNECTED_STANDBY_STATE_BIT | SUSPEND_STATE_BIT);
	dev->print_log(dev, "%s() sends RESUME notification\n", __func__);
	state_status_msg.states_status = current_state;

	ishtp_write_message(dev, &ishtp_hdr,
		(unsigned char *)&state_status_msg);
}
EXPORT_SYMBOL(ishtp_send_resume);

 
void ishtp_query_subscribers(struct ishtp_device *dev)
{
	struct ishtp_msg_hdr	ishtp_hdr;
	struct ish_system_states_query_subscribers query_subscribers_msg;
	const size_t len = sizeof(struct ish_system_states_query_subscribers);

	fix_cl_hdr(&ishtp_hdr, len, ISHTP_SYSTEM_STATE_CLIENT_ADDR);

	memset(&query_subscribers_msg, 0, len);
	query_subscribers_msg.hdr.cmd = SYSTEM_STATE_QUERY_SUBSCRIBERS;

	ishtp_write_message(dev, &ishtp_hdr,
		(unsigned char *)&query_subscribers_msg);
}
