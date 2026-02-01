












#include <linux/mutex.h>
#include <linux/types.h>

#include "sof-priv.h"
#include "sof-audio.h"
#include "ops.h"

 
int sof_ipc_send_msg(struct snd_sof_dev *sdev, void *msg_data, size_t msg_bytes,
		     size_t reply_bytes)
{
	struct snd_sof_ipc *ipc = sdev->ipc;
	struct snd_sof_ipc_msg *msg;
	int ret;

	if (ipc->disable_ipc_tx || sdev->fw_state != SOF_FW_BOOT_COMPLETE)
		return -ENODEV;

	 
	spin_lock_irq(&sdev->ipc_lock);

	 
	msg = &ipc->msg;

	 
	msg->msg_data = msg_data;
	msg->msg_size = msg_bytes;

	msg->reply_size = reply_bytes;
	msg->reply_error = 0;

	sdev->msg = msg;

	ret = snd_sof_dsp_send_msg(sdev, msg);
	 
	if (!ret)
		msg->ipc_complete = false;

	spin_unlock_irq(&sdev->ipc_lock);

	return ret;
}

 
int sof_ipc_tx_message(struct snd_sof_ipc *ipc, void *msg_data, size_t msg_bytes,
		       void *reply_data, size_t reply_bytes)
{
	if (msg_bytes > ipc->max_payload_size ||
	    reply_bytes > ipc->max_payload_size)
		return -ENOBUFS;

	return ipc->ops->tx_msg(ipc->sdev, msg_data, msg_bytes, reply_data,
				reply_bytes, false);
}
EXPORT_SYMBOL(sof_ipc_tx_message);

 
int sof_ipc_set_get_data(struct snd_sof_ipc *ipc, void *msg_data,
			 size_t msg_bytes, bool set)
{
	return ipc->ops->set_get_data(ipc->sdev, msg_data, msg_bytes, set);
}
EXPORT_SYMBOL(sof_ipc_set_get_data);

 
int sof_ipc_tx_message_no_pm(struct snd_sof_ipc *ipc, void *msg_data, size_t msg_bytes,
			     void *reply_data, size_t reply_bytes)
{
	if (msg_bytes > ipc->max_payload_size ||
	    reply_bytes > ipc->max_payload_size)
		return -ENOBUFS;

	return ipc->ops->tx_msg(ipc->sdev, msg_data, msg_bytes, reply_data,
				reply_bytes, true);
}
EXPORT_SYMBOL(sof_ipc_tx_message_no_pm);

 
void snd_sof_ipc_get_reply(struct snd_sof_dev *sdev)
{
	 
	if (!sdev->msg) {
		dev_warn(sdev->dev, "unexpected ipc interrupt raised!\n");
		return;
	}

	sdev->msg->reply_error = sdev->ipc->ops->get_reply(sdev);
}
EXPORT_SYMBOL(snd_sof_ipc_get_reply);

 
void snd_sof_ipc_reply(struct snd_sof_dev *sdev, u32 msg_id)
{
	struct snd_sof_ipc_msg *msg = &sdev->ipc->msg;

	if (msg->ipc_complete) {
		dev_dbg(sdev->dev,
			"no reply expected, received 0x%x, will be ignored",
			msg_id);
		return;
	}

	 
	msg->ipc_complete = true;
	wake_up(&msg->waitq);
}
EXPORT_SYMBOL(snd_sof_ipc_reply);

struct snd_sof_ipc *snd_sof_ipc_init(struct snd_sof_dev *sdev)
{
	struct snd_sof_ipc *ipc;
	struct snd_sof_ipc_msg *msg;
	const struct sof_ipc_ops *ops;

	ipc = devm_kzalloc(sdev->dev, sizeof(*ipc), GFP_KERNEL);
	if (!ipc)
		return NULL;

	mutex_init(&ipc->tx_mutex);
	ipc->sdev = sdev;
	msg = &ipc->msg;

	 
	msg->ipc_complete = true;

	init_waitqueue_head(&msg->waitq);

	switch (sdev->pdata->ipc_type) {
#if defined(CONFIG_SND_SOC_SOF_IPC3)
	case SOF_IPC:
		ops = &ipc3_ops;
		break;
#endif
#if defined(CONFIG_SND_SOC_SOF_INTEL_IPC4)
	case SOF_INTEL_IPC4:
		ops = &ipc4_ops;
		break;
#endif
	default:
		dev_err(sdev->dev, "Not supported IPC version: %d\n",
			sdev->pdata->ipc_type);
		return NULL;
	}

	 
	if (!ops->tx_msg || !ops->rx_msg || !ops->set_get_data || !ops->get_reply) {
		dev_err(sdev->dev, "Missing IPC message handling ops\n");
		return NULL;
	}

	if (!ops->fw_loader || !ops->fw_loader->validate ||
	    !ops->fw_loader->parse_ext_manifest) {
		dev_err(sdev->dev, "Missing IPC firmware loading ops\n");
		return NULL;
	}

	if (!ops->pcm) {
		dev_err(sdev->dev, "Missing IPC PCM ops\n");
		return NULL;
	}

	if (!ops->tplg || !ops->tplg->widget || !ops->tplg->control) {
		dev_err(sdev->dev, "Missing IPC topology ops\n");
		return NULL;
	}

	if (ops->fw_tracing && (!ops->fw_tracing->init || !ops->fw_tracing->suspend ||
				!ops->fw_tracing->resume)) {
		dev_err(sdev->dev, "Missing firmware tracing ops\n");
		return NULL;
	}

	if (ops->init && ops->init(sdev))
		return NULL;

	ipc->ops = ops;

	return ipc;
}
EXPORT_SYMBOL(snd_sof_ipc_init);

void snd_sof_ipc_free(struct snd_sof_dev *sdev)
{
	struct snd_sof_ipc *ipc = sdev->ipc;

	if (!ipc)
		return;

	 
	mutex_lock(&ipc->tx_mutex);
	ipc->disable_ipc_tx = true;
	mutex_unlock(&ipc->tx_mutex);

	if (ipc->ops->exit)
		ipc->ops->exit(sdev);
}
EXPORT_SYMBOL(snd_sof_ipc_free);
