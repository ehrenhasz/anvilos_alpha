
 

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include <linux/uuid.h>
#include <uapi/linux/tee.h>

#include "common.h"

#define SCMI_OPTEE_MAX_MSG_SIZE		128

enum scmi_optee_pta_cmd {
	 
	PTA_SCMI_CMD_CAPABILITIES = 0,

	 
	PTA_SCMI_CMD_PROCESS_SMT_CHANNEL = 1,

	 
	PTA_SCMI_CMD_PROCESS_SMT_CHANNEL_MESSAGE = 2,

	 
	PTA_SCMI_CMD_GET_CHANNEL = 3,

	 
	PTA_SCMI_CMD_PROCESS_MSG_CHANNEL = 4,
};

 
#define PTA_SCMI_CAPS_NONE		0
#define PTA_SCMI_CAPS_SMT_HEADER	BIT(0)
#define PTA_SCMI_CAPS_MSG_HEADER	BIT(1)
#define PTA_SCMI_CAPS_MASK		(PTA_SCMI_CAPS_SMT_HEADER | \
					 PTA_SCMI_CAPS_MSG_HEADER)

 
struct scmi_optee_channel {
	u32 channel_id;
	u32 tee_session;
	u32 caps;
	u32 rx_len;
	struct mutex mu;
	struct scmi_chan_info *cinfo;
	union {
		struct scmi_shared_mem __iomem *shmem;
		struct scmi_msg_payld *msg;
	} req;
	struct tee_shm *tee_shm;
	struct list_head link;
};

 
struct scmi_optee_agent {
	struct device *dev;
	struct tee_context *tee_ctx;
	u32 caps;
	struct mutex mu;
	struct list_head channel_list;
};

 
static struct scmi_optee_agent *scmi_optee_private;

 
static int scmi_optee_init(void);

 
static int open_session(struct scmi_optee_agent *agent, u32 *tee_session)
{
	struct device *dev = agent->dev;
	struct tee_client_device *scmi_pta = to_tee_client_device(dev);
	struct tee_ioctl_open_session_arg arg = { };
	int ret;

	memcpy(arg.uuid, scmi_pta->id.uuid.b, TEE_IOCTL_UUID_LEN);
	arg.clnt_login = TEE_IOCTL_LOGIN_REE_KERNEL;

	ret = tee_client_open_session(agent->tee_ctx, &arg, NULL);
	if (ret < 0 || arg.ret) {
		dev_err(dev, "Can't open tee session: %d / %#x\n", ret, arg.ret);
		return -EOPNOTSUPP;
	}

	*tee_session = arg.session;

	return 0;
}

static void close_session(struct scmi_optee_agent *agent, u32 tee_session)
{
	tee_client_close_session(agent->tee_ctx, tee_session);
}

static int get_capabilities(struct scmi_optee_agent *agent)
{
	struct tee_ioctl_invoke_arg arg = { };
	struct tee_param param[1] = { };
	u32 caps;
	u32 tee_session;
	int ret;

	ret = open_session(agent, &tee_session);
	if (ret)
		return ret;

	arg.func = PTA_SCMI_CMD_CAPABILITIES;
	arg.session = tee_session;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT;

	ret = tee_client_invoke_func(agent->tee_ctx, &arg, param);

	close_session(agent, tee_session);

	if (ret < 0 || arg.ret) {
		dev_err(agent->dev, "Can't get capabilities: %d / %#x\n", ret, arg.ret);
		return -EOPNOTSUPP;
	}

	caps = param[0].u.value.a;

	if (!(caps & (PTA_SCMI_CAPS_SMT_HEADER | PTA_SCMI_CAPS_MSG_HEADER))) {
		dev_err(agent->dev, "OP-TEE SCMI PTA doesn't support SMT and MSG\n");
		return -EOPNOTSUPP;
	}

	agent->caps = caps;

	return 0;
}

static int get_channel(struct scmi_optee_channel *channel)
{
	struct device *dev = scmi_optee_private->dev;
	struct tee_ioctl_invoke_arg arg = { };
	struct tee_param param[1] = { };
	unsigned int caps = 0;
	int ret;

	if (channel->tee_shm)
		caps = PTA_SCMI_CAPS_MSG_HEADER;
	else
		caps = PTA_SCMI_CAPS_SMT_HEADER;

	arg.func = PTA_SCMI_CMD_GET_CHANNEL;
	arg.session = channel->tee_session;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param[0].u.value.a = channel->channel_id;
	param[0].u.value.b = caps;

	ret = tee_client_invoke_func(scmi_optee_private->tee_ctx, &arg, param);

	if (ret || arg.ret) {
		dev_err(dev, "Can't get channel with caps %#x: %d / %#x\n", caps, ret, arg.ret);
		return -EOPNOTSUPP;
	}

	 
	channel->channel_id = param[0].u.value.a;
	channel->caps = caps;

	return 0;
}

static int invoke_process_smt_channel(struct scmi_optee_channel *channel)
{
	struct tee_ioctl_invoke_arg arg = {
		.func = PTA_SCMI_CMD_PROCESS_SMT_CHANNEL,
		.session = channel->tee_session,
		.num_params = 1,
	};
	struct tee_param param[1] = { };
	int ret;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = channel->channel_id;

	ret = tee_client_invoke_func(scmi_optee_private->tee_ctx, &arg, param);
	if (ret < 0 || arg.ret) {
		dev_err(scmi_optee_private->dev, "Can't invoke channel %u: %d / %#x\n",
			channel->channel_id, ret, arg.ret);
		return -EIO;
	}

	return 0;
}

static int invoke_process_msg_channel(struct scmi_optee_channel *channel, size_t msg_size)
{
	struct tee_ioctl_invoke_arg arg = {
		.func = PTA_SCMI_CMD_PROCESS_MSG_CHANNEL,
		.session = channel->tee_session,
		.num_params = 3,
	};
	struct tee_param param[3] = { };
	int ret;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = channel->channel_id;

	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[1].u.memref.shm = channel->tee_shm;
	param[1].u.memref.size = msg_size;

	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[2].u.memref.shm = channel->tee_shm;
	param[2].u.memref.size = SCMI_OPTEE_MAX_MSG_SIZE;

	ret = tee_client_invoke_func(scmi_optee_private->tee_ctx, &arg, param);
	if (ret < 0 || arg.ret) {
		dev_err(scmi_optee_private->dev, "Can't invoke channel %u: %d / %#x\n",
			channel->channel_id, ret, arg.ret);
		return -EIO;
	}

	 
	channel->rx_len = param[2].u.memref.size;

	return 0;
}

static int scmi_optee_link_supplier(struct device *dev)
{
	if (!scmi_optee_private) {
		if (scmi_optee_init())
			dev_dbg(dev, "Optee bus not yet ready\n");

		 
		return -EPROBE_DEFER;
	}

	if (!device_link_add(dev, scmi_optee_private->dev, DL_FLAG_AUTOREMOVE_CONSUMER)) {
		dev_err(dev, "Adding link to supplier optee device failed\n");
		return -ECANCELED;
	}

	return 0;
}

static bool scmi_optee_chan_available(struct device_node *of_node, int idx)
{
	u32 channel_id;

	return !of_property_read_u32_index(of_node, "linaro,optee-channel-id",
					   idx, &channel_id);
}

static void scmi_optee_clear_channel(struct scmi_chan_info *cinfo)
{
	struct scmi_optee_channel *channel = cinfo->transport_info;

	if (!channel->tee_shm)
		shmem_clear_channel(channel->req.shmem);
}

static int setup_dynamic_shmem(struct device *dev, struct scmi_optee_channel *channel)
{
	const size_t msg_size = SCMI_OPTEE_MAX_MSG_SIZE;
	void *shbuf;

	channel->tee_shm = tee_shm_alloc_kernel_buf(scmi_optee_private->tee_ctx, msg_size);
	if (IS_ERR(channel->tee_shm)) {
		dev_err(channel->cinfo->dev, "shmem allocation failed\n");
		return -ENOMEM;
	}

	shbuf = tee_shm_get_va(channel->tee_shm, 0);
	memset(shbuf, 0, msg_size);
	channel->req.msg = shbuf;
	channel->rx_len = msg_size;

	return 0;
}

static int setup_static_shmem(struct device *dev, struct scmi_chan_info *cinfo,
			      struct scmi_optee_channel *channel)
{
	struct device_node *np;
	resource_size_t size;
	struct resource res;
	int ret;

	np = of_parse_phandle(cinfo->dev->of_node, "shmem", 0);
	if (!of_device_is_compatible(np, "arm,scmi-shmem")) {
		ret = -ENXIO;
		goto out;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		dev_err(dev, "Failed to get SCMI Tx shared memory\n");
		goto out;
	}

	size = resource_size(&res);

	channel->req.shmem = devm_ioremap(dev, res.start, size);
	if (!channel->req.shmem) {
		dev_err(dev, "Failed to ioremap SCMI Tx shared memory\n");
		ret = -EADDRNOTAVAIL;
		goto out;
	}

	ret = 0;

out:
	of_node_put(np);

	return ret;
}

static int setup_shmem(struct device *dev, struct scmi_chan_info *cinfo,
		       struct scmi_optee_channel *channel)
{
	if (of_property_present(cinfo->dev->of_node, "shmem"))
		return setup_static_shmem(dev, cinfo, channel);
	else
		return setup_dynamic_shmem(dev, channel);
}

static int scmi_optee_chan_setup(struct scmi_chan_info *cinfo, struct device *dev, bool tx)
{
	struct scmi_optee_channel *channel;
	uint32_t channel_id;
	int ret;

	if (!tx)
		return -ENODEV;

	channel = devm_kzalloc(dev, sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return -ENOMEM;

	ret = of_property_read_u32_index(cinfo->dev->of_node, "linaro,optee-channel-id",
					 0, &channel_id);
	if (ret)
		return ret;

	cinfo->transport_info = channel;
	channel->cinfo = cinfo;
	channel->channel_id = channel_id;
	mutex_init(&channel->mu);

	ret = setup_shmem(dev, cinfo, channel);
	if (ret)
		return ret;

	ret = open_session(scmi_optee_private, &channel->tee_session);
	if (ret)
		goto err_free_shm;

	ret = get_channel(channel);
	if (ret)
		goto err_close_sess;

	 
	cinfo->no_completion_irq = true;

	mutex_lock(&scmi_optee_private->mu);
	list_add(&channel->link, &scmi_optee_private->channel_list);
	mutex_unlock(&scmi_optee_private->mu);

	return 0;

err_close_sess:
	close_session(scmi_optee_private, channel->tee_session);
err_free_shm:
	if (channel->tee_shm)
		tee_shm_free(channel->tee_shm);

	return ret;
}

static int scmi_optee_chan_free(int id, void *p, void *data)
{
	struct scmi_chan_info *cinfo = p;
	struct scmi_optee_channel *channel = cinfo->transport_info;

	mutex_lock(&scmi_optee_private->mu);
	list_del(&channel->link);
	mutex_unlock(&scmi_optee_private->mu);

	close_session(scmi_optee_private, channel->tee_session);

	if (channel->tee_shm) {
		tee_shm_free(channel->tee_shm);
		channel->tee_shm = NULL;
	}

	cinfo->transport_info = NULL;
	channel->cinfo = NULL;

	return 0;
}

static int scmi_optee_send_message(struct scmi_chan_info *cinfo,
				   struct scmi_xfer *xfer)
{
	struct scmi_optee_channel *channel = cinfo->transport_info;
	int ret;

	mutex_lock(&channel->mu);

	if (channel->tee_shm) {
		msg_tx_prepare(channel->req.msg, xfer);
		ret = invoke_process_msg_channel(channel, msg_command_size(xfer));
	} else {
		shmem_tx_prepare(channel->req.shmem, xfer, cinfo);
		ret = invoke_process_smt_channel(channel);
	}

	if (ret)
		mutex_unlock(&channel->mu);

	return ret;
}

static void scmi_optee_fetch_response(struct scmi_chan_info *cinfo,
				      struct scmi_xfer *xfer)
{
	struct scmi_optee_channel *channel = cinfo->transport_info;

	if (channel->tee_shm)
		msg_fetch_response(channel->req.msg, channel->rx_len, xfer);
	else
		shmem_fetch_response(channel->req.shmem, xfer);
}

static void scmi_optee_mark_txdone(struct scmi_chan_info *cinfo, int ret,
				   struct scmi_xfer *__unused)
{
	struct scmi_optee_channel *channel = cinfo->transport_info;

	mutex_unlock(&channel->mu);
}

static struct scmi_transport_ops scmi_optee_ops = {
	.link_supplier = scmi_optee_link_supplier,
	.chan_available = scmi_optee_chan_available,
	.chan_setup = scmi_optee_chan_setup,
	.chan_free = scmi_optee_chan_free,
	.send_message = scmi_optee_send_message,
	.mark_txdone = scmi_optee_mark_txdone,
	.fetch_response = scmi_optee_fetch_response,
	.clear_channel = scmi_optee_clear_channel,
};

static int scmi_optee_ctx_match(struct tee_ioctl_version_data *ver, const void *data)
{
	return ver->impl_id == TEE_IMPL_ID_OPTEE;
}

static int scmi_optee_service_probe(struct device *dev)
{
	struct scmi_optee_agent *agent;
	struct tee_context *tee_ctx;
	int ret;

	 
	if (scmi_optee_private) {
		dev_err(dev, "An SCMI OP-TEE device was already initialized: only one allowed\n");
		return -EBUSY;
	}

	tee_ctx = tee_client_open_context(NULL, scmi_optee_ctx_match, NULL, NULL);
	if (IS_ERR(tee_ctx))
		return -ENODEV;

	agent = devm_kzalloc(dev, sizeof(*agent), GFP_KERNEL);
	if (!agent) {
		ret = -ENOMEM;
		goto err;
	}

	agent->dev = dev;
	agent->tee_ctx = tee_ctx;
	INIT_LIST_HEAD(&agent->channel_list);
	mutex_init(&agent->mu);

	ret = get_capabilities(agent);
	if (ret)
		goto err;

	 
	smp_mb();
	scmi_optee_private = agent;

	return 0;

err:
	tee_client_close_context(tee_ctx);

	return ret;
}

static int scmi_optee_service_remove(struct device *dev)
{
	struct scmi_optee_agent *agent = scmi_optee_private;

	if (!scmi_optee_private)
		return -EINVAL;

	if (!list_empty(&scmi_optee_private->channel_list))
		return -EBUSY;

	 
	smp_store_mb(scmi_optee_private, NULL);

	tee_client_close_context(agent->tee_ctx);

	return 0;
}

static const struct tee_client_device_id scmi_optee_service_id[] = {
	{
		UUID_INIT(0xa8cfe406, 0xd4f5, 0x4a2e,
			  0x9f, 0x8d, 0xa2, 0x5d, 0xc7, 0x54, 0xc0, 0x99)
	},
	{ }
};

MODULE_DEVICE_TABLE(tee, scmi_optee_service_id);

static struct tee_client_driver scmi_optee_driver = {
	.id_table	= scmi_optee_service_id,
	.driver		= {
		.name = "scmi-optee",
		.bus = &tee_bus_type,
		.probe = scmi_optee_service_probe,
		.remove = scmi_optee_service_remove,
	},
};

static int scmi_optee_init(void)
{
	return driver_register(&scmi_optee_driver.driver);
}

static void scmi_optee_exit(void)
{
	if (scmi_optee_private)
		driver_unregister(&scmi_optee_driver.driver);
}

const struct scmi_desc scmi_optee_desc = {
	.transport_exit = scmi_optee_exit,
	.ops = &scmi_optee_ops,
	.max_rx_timeout_ms = 30,
	.max_msg = 20,
	.max_msg_size = SCMI_OPTEE_MAX_MSG_SIZE,
	.sync_cmds_completed_on_ret = true,
};
