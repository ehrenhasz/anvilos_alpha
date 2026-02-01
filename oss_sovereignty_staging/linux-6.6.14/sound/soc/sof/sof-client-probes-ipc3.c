









#include <linux/stddef.h>
#include <sound/soc.h>
#include <sound/sof/header.h>
#include "sof-client.h"
#include "sof-client-probes.h"

struct sof_probe_dma {
	unsigned int stream_tag;
	unsigned int dma_buffer_size;
} __packed;

struct sof_ipc_probe_dma_add_params {
	struct sof_ipc_cmd_hdr hdr;
	unsigned int num_elems;
	struct sof_probe_dma dma[];
} __packed;

struct sof_ipc_probe_info_params {
	struct sof_ipc_reply rhdr;
	unsigned int num_elems;
	union {
		DECLARE_FLEX_ARRAY(struct sof_probe_dma, dma);
		DECLARE_FLEX_ARRAY(struct sof_probe_point_desc, desc);
	};
} __packed;

struct sof_ipc_probe_point_add_params {
	struct sof_ipc_cmd_hdr hdr;
	unsigned int num_elems;
	struct sof_probe_point_desc desc[];
} __packed;

struct sof_ipc_probe_point_remove_params {
	struct sof_ipc_cmd_hdr hdr;
	unsigned int num_elems;
	unsigned int buffer_id[];
} __packed;

 
static int ipc3_probes_init(struct sof_client_dev *cdev, u32 stream_tag,
			    size_t buffer_size)
{
	struct sof_ipc_probe_dma_add_params *msg;
	size_t size = struct_size(msg, dma, 1);
	int ret;

	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	msg->hdr.size = size;
	msg->hdr.cmd = SOF_IPC_GLB_PROBE | SOF_IPC_PROBE_INIT;
	msg->num_elems = 1;
	msg->dma[0].stream_tag = stream_tag;
	msg->dma[0].dma_buffer_size = buffer_size;

	ret = sof_client_ipc_tx_message_no_reply(cdev, msg);
	kfree(msg);
	return ret;
}

 
static int ipc3_probes_deinit(struct sof_client_dev *cdev)
{
	struct sof_ipc_cmd_hdr msg;

	msg.size = sizeof(msg);
	msg.cmd = SOF_IPC_GLB_PROBE | SOF_IPC_PROBE_DEINIT;

	return sof_client_ipc_tx_message_no_reply(cdev, &msg);
}

static int ipc3_probes_info(struct sof_client_dev *cdev, unsigned int cmd,
			    void **params, size_t *num_params)
{
	size_t max_msg_size = sof_client_get_ipc_max_payload_size(cdev);
	struct sof_ipc_probe_info_params msg = {{{0}}};
	struct sof_ipc_probe_info_params *reply;
	size_t bytes;
	int ret;

	*params = NULL;
	*num_params = 0;

	reply = kzalloc(max_msg_size, GFP_KERNEL);
	if (!reply)
		return -ENOMEM;
	msg.rhdr.hdr.size = sizeof(msg);
	msg.rhdr.hdr.cmd = SOF_IPC_GLB_PROBE | cmd;

	ret = sof_client_ipc_tx_message(cdev, &msg, reply, max_msg_size);
	if (ret < 0 || reply->rhdr.error < 0)
		goto exit;

	if (!reply->num_elems)
		goto exit;

	if (cmd == SOF_IPC_PROBE_DMA_INFO)
		bytes = sizeof(reply->dma[0]);
	else
		bytes = sizeof(reply->desc[0]);
	bytes *= reply->num_elems;
	*params = kmemdup(&reply->dma[0], bytes, GFP_KERNEL);
	if (!*params) {
		ret = -ENOMEM;
		goto exit;
	}
	*num_params = reply->num_elems;

exit:
	kfree(reply);
	return ret;
}

 
static int ipc3_probes_points_info(struct sof_client_dev *cdev,
				   struct sof_probe_point_desc **desc,
				   size_t *num_desc)
{
	return ipc3_probes_info(cdev, SOF_IPC_PROBE_POINT_INFO,
			       (void **)desc, num_desc);
}

 
static int ipc3_probes_points_add(struct sof_client_dev *cdev,
				  struct sof_probe_point_desc *desc,
				  size_t num_desc)
{
	struct sof_ipc_probe_point_add_params *msg;
	size_t size = struct_size(msg, desc, num_desc);
	int ret;

	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	msg->hdr.size = size;
	msg->num_elems = num_desc;
	msg->hdr.cmd = SOF_IPC_GLB_PROBE | SOF_IPC_PROBE_POINT_ADD;
	memcpy(&msg->desc[0], desc, size - sizeof(*msg));

	ret = sof_client_ipc_tx_message_no_reply(cdev, msg);
	kfree(msg);
	return ret;
}

 
static int ipc3_probes_points_remove(struct sof_client_dev *cdev,
				     unsigned int *buffer_id,
				     size_t num_buffer_id)
{
	struct sof_ipc_probe_point_remove_params *msg;
	size_t size = struct_size(msg, buffer_id, num_buffer_id);
	int ret;

	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;
	msg->hdr.size = size;
	msg->num_elems = num_buffer_id;
	msg->hdr.cmd = SOF_IPC_GLB_PROBE | SOF_IPC_PROBE_POINT_REMOVE;
	memcpy(&msg->buffer_id[0], buffer_id, size - sizeof(*msg));

	ret = sof_client_ipc_tx_message_no_reply(cdev, msg);
	kfree(msg);
	return ret;
}

const struct sof_probes_ipc_ops ipc3_probe_ops =  {
	.init = ipc3_probes_init,
	.deinit = ipc3_probes_deinit,
	.points_info = ipc3_probes_points_info,
	.points_add = ipc3_probes_points_add,
	.points_remove = ipc3_probes_points_remove,
};
