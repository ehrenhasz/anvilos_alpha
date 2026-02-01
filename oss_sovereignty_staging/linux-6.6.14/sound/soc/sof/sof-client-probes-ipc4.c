






#include <sound/soc.h>
#include <sound/sof/ipc4/header.h>
#include <uapi/sound/sof/header.h>
#include "sof-priv.h"
#include "ipc4-priv.h"
#include "sof-client.h"
#include "sof-client-probes.h"

enum sof_ipc4_dma_type {
	SOF_IPC4_DMA_HDA_HOST_OUTPUT = 0,
	SOF_IPC4_DMA_HDA_HOST_INPUT = 1,
	SOF_IPC4_DMA_HDA_LINK_OUTPUT = 8,
	SOF_IPC4_DMA_HDA_LINK_INPUT = 9,
	SOF_IPC4_DMA_DMIC_LINK_INPUT = 11,
	SOF_IPC4_DMA_I2S_LINK_OUTPUT = 12,
	SOF_IPC4_DMA_I2S_LINK_INPUT = 13,
};

enum sof_ipc4_probe_runtime_param {
	SOF_IPC4_PROBE_INJECTION_DMA = 1,
	SOF_IPC4_PROBE_INJECTION_DMA_DETACH,
	SOF_IPC4_PROBE_POINTS,
	SOF_IPC4_PROBE_POINTS_DISCONNECT,
};

struct sof_ipc4_probe_gtw_cfg {
	u32 node_id;
	u32 dma_buffer_size;
} __packed __aligned(4);

#define SOF_IPC4_PROBE_NODE_ID_INDEX(x)		((x) & GENMASK(7, 0))
#define SOF_IPC4_PROBE_NODE_ID_TYPE(x)		(((x) << 8) & GENMASK(12, 8))

struct sof_ipc4_probe_cfg {
	struct sof_ipc4_base_module_cfg base;
	struct sof_ipc4_probe_gtw_cfg gtw_cfg;
} __packed __aligned(4);

enum sof_ipc4_probe_type {
	SOF_IPC4_PROBE_TYPE_INPUT = 0,
	SOF_IPC4_PROBE_TYPE_OUTPUT,
	SOF_IPC4_PROBE_TYPE_INTERNAL
};

struct sof_ipc4_probe_point {
	u32 point_id;
	u32 purpose;
	u32 stream_tag;
} __packed __aligned(4);

#define INVALID_PIPELINE_ID      0xFF

 
static struct sof_man4_module *sof_ipc4_probe_get_module_info(struct sof_client_dev *cdev)
{
	struct sof_probes_priv *priv = cdev->data;
	struct device *dev = &cdev->auxdev.dev;
	static const guid_t probe_uuid =
		GUID_INIT(0x7CAD0808, 0xAB10, 0xCD23,
			  0xEF, 0x45, 0x12, 0xAB, 0x34, 0xCD, 0x56, 0xEF);

	if (!priv->ipc_priv) {
		struct sof_ipc4_fw_module *fw_module =
			sof_client_ipc4_find_module(cdev, &probe_uuid);

		if (!fw_module) {
			dev_err(dev, "%s: no matching uuid found", __func__);
			return NULL;
		}

		priv->ipc_priv = &fw_module->man4_module_entry;
	}

	return (struct sof_man4_module *)priv->ipc_priv;
}

 
static int ipc4_probes_init(struct sof_client_dev *cdev, u32 stream_tag,
			    size_t buffer_size)
{
	struct sof_man4_module *mentry = sof_ipc4_probe_get_module_info(cdev);
	struct sof_ipc4_msg msg;
	struct sof_ipc4_probe_cfg cfg;

	if (!mentry)
		return -ENODEV;

	memset(&cfg, '\0', sizeof(cfg));
	cfg.gtw_cfg.node_id = SOF_IPC4_PROBE_NODE_ID_INDEX(stream_tag - 1) |
		SOF_IPC4_PROBE_NODE_ID_TYPE(SOF_IPC4_DMA_HDA_HOST_INPUT);

	cfg.gtw_cfg.dma_buffer_size = buffer_size;

	msg.primary = mentry->id;
	msg.primary |= SOF_IPC4_MSG_TYPE_SET(SOF_IPC4_MOD_INIT_INSTANCE);
	msg.primary |= SOF_IPC4_MSG_DIR(SOF_IPC4_MSG_REQUEST);
	msg.primary |= SOF_IPC4_MSG_TARGET(SOF_IPC4_MODULE_MSG);
	msg.extension = SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE(INVALID_PIPELINE_ID);
	msg.extension |= SOF_IPC4_MOD_EXT_CORE_ID(0);

	msg.data_size = sizeof(cfg);
	msg.data_ptr = &cfg;

	return sof_client_ipc_tx_message_no_reply(cdev, &msg);
}

 
static int ipc4_probes_deinit(struct sof_client_dev *cdev)
{
	struct sof_man4_module *mentry = sof_ipc4_probe_get_module_info(cdev);
	struct sof_ipc4_msg msg;

	if (!mentry)
		return -ENODEV;

	msg.primary = mentry->id;
	msg.primary |= SOF_IPC4_MSG_TYPE_SET(SOF_IPC4_MOD_DELETE_INSTANCE);
	msg.primary |= SOF_IPC4_MSG_DIR(SOF_IPC4_MSG_REQUEST);
	msg.primary |= SOF_IPC4_MSG_TARGET(SOF_IPC4_MODULE_MSG);
	msg.extension = SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE(INVALID_PIPELINE_ID);
	msg.extension |= SOF_IPC4_MOD_EXT_CORE_ID(0);

	msg.data_size = 0;
	msg.data_ptr = NULL;

	return sof_client_ipc_tx_message_no_reply(cdev, &msg);
}

 
static int ipc4_probes_points_info(struct sof_client_dev *cdev,
				   struct sof_probe_point_desc **desc,
				   size_t *num_desc)
{
	 
	*desc = NULL;
	*num_desc = 0;
	return 0;
}

 
static int ipc4_probes_points_add(struct sof_client_dev *cdev,
				  struct sof_probe_point_desc *desc,
				  size_t num_desc)
{
	struct sof_man4_module *mentry = sof_ipc4_probe_get_module_info(cdev);
	struct sof_ipc4_probe_point *points;
	struct sof_ipc4_msg msg;
	int i, ret;

	if (!mentry)
		return -ENODEV;

	 
	points = kcalloc(num_desc, sizeof(*points), GFP_KERNEL);
	if (!points)
		return -ENOMEM;

	for (i = 0; i < num_desc; i++) {
		points[i].point_id = desc[i].buffer_id;
		points[i].purpose = desc[i].purpose;
		points[i].stream_tag = desc[i].stream_tag;
	}

	msg.primary = mentry->id;
	msg.primary |= SOF_IPC4_MSG_DIR(SOF_IPC4_MSG_REQUEST);
	msg.primary |= SOF_IPC4_MSG_TARGET(SOF_IPC4_MODULE_MSG);

	msg.extension = SOF_IPC4_MOD_EXT_MSG_PARAM_ID(SOF_IPC4_PROBE_POINTS);

	msg.data_size = sizeof(*points) * num_desc;
	msg.data_ptr = points;

	ret = sof_client_ipc_set_get_data(cdev, &msg, true);

	kfree(points);

	return ret;
}

 
static int ipc4_probes_points_remove(struct sof_client_dev *cdev,
				     unsigned int *buffer_id, size_t num_buffer_id)
{
	struct sof_man4_module *mentry = sof_ipc4_probe_get_module_info(cdev);
	struct sof_ipc4_msg msg;
	u32 *probe_point_ids;
	int i, ret;

	if (!mentry)
		return -ENODEV;

	probe_point_ids = kcalloc(num_buffer_id, sizeof(*probe_point_ids),
				  GFP_KERNEL);
	if (!probe_point_ids)
		return -ENOMEM;

	for (i = 0; i < num_buffer_id; i++)
		probe_point_ids[i] = buffer_id[i];

	msg.primary = mentry->id;
	msg.primary |= SOF_IPC4_MSG_DIR(SOF_IPC4_MSG_REQUEST);
	msg.primary |= SOF_IPC4_MSG_TARGET(SOF_IPC4_MODULE_MSG);

	msg.extension =
		SOF_IPC4_MOD_EXT_MSG_PARAM_ID(SOF_IPC4_PROBE_POINTS_DISCONNECT);

	msg.data_size = num_buffer_id * sizeof(*probe_point_ids);
	msg.data_ptr = probe_point_ids;

	ret = sof_client_ipc_set_get_data(cdev, &msg, true);

	kfree(probe_point_ids);

	return ret;
}

const struct sof_probes_ipc_ops ipc4_probe_ops =  {
	.init = ipc4_probes_init,
	.deinit = ipc4_probes_deinit,
	.points_info = ipc4_probes_points_info,
	.points_add = ipc4_probes_points_add,
	.points_remove = ipc4_probes_points_remove,
};
