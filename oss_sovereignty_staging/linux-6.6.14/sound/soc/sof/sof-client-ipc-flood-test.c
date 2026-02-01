







#include <linux/auxiliary_bus.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <sound/sof/header.h>

#include "sof-client.h"

#define MAX_IPC_FLOOD_DURATION_MS	1000
#define MAX_IPC_FLOOD_COUNT		10000
#define IPC_FLOOD_TEST_RESULT_LEN	512
#define SOF_IPC_CLIENT_SUSPEND_DELAY_MS	3000

#define DEBUGFS_IPC_FLOOD_COUNT		"ipc_flood_count"
#define DEBUGFS_IPC_FLOOD_DURATION	"ipc_flood_duration_ms"

struct sof_ipc_flood_priv {
	struct dentry *dfs_root;
	struct dentry *dfs_link[2];
	char *buf;
};

static int sof_ipc_flood_dfs_open(struct inode *inode, struct file *file)
{
	struct sof_client_dev *cdev = inode->i_private;
	int ret;

	if (sof_client_get_fw_state(cdev) == SOF_FW_CRASHED)
		return -ENODEV;

	ret = debugfs_file_get(file->f_path.dentry);
	if (unlikely(ret))
		return ret;

	ret = simple_open(inode, file);
	if (ret)
		debugfs_file_put(file->f_path.dentry);

	return ret;
}

 
static int sof_debug_ipc_flood_test(struct sof_client_dev *cdev,
				    bool flood_duration_test,
				    unsigned long ipc_duration_ms,
				    unsigned long ipc_count)
{
	struct sof_ipc_flood_priv *priv = cdev->data;
	struct device *dev = &cdev->auxdev.dev;
	struct sof_ipc_cmd_hdr hdr;
	u64 min_response_time = U64_MAX;
	ktime_t start, end, test_end;
	u64 avg_response_time = 0;
	u64 max_response_time = 0;
	u64 ipc_response_time;
	int i = 0;
	int ret;

	 
	hdr.cmd = SOF_IPC_GLB_TEST_MSG | SOF_IPC_TEST_IPC_FLOOD;
	hdr.size = sizeof(hdr);

	 
	if (flood_duration_test)
		test_end = ktime_get_ns() + ipc_duration_ms * NSEC_PER_MSEC;

	 
	while (1) {
		start = ktime_get();
		ret = sof_client_ipc_tx_message_no_reply(cdev, &hdr);
		end = ktime_get();

		if (ret < 0)
			break;

		 
		ipc_response_time = ktime_to_ns(ktime_sub(end, start));
		min_response_time = min(min_response_time, ipc_response_time);
		max_response_time = max(max_response_time, ipc_response_time);

		 
		avg_response_time += ipc_response_time;
		i++;

		 
		if (flood_duration_test) {
			if (ktime_to_ns(end) >= test_end)
				break;
		} else {
			if (i == ipc_count)
				break;
		}
	}

	if (ret < 0)
		dev_err(dev, "ipc flood test failed at %d iterations\n", i);

	 
	if (!i)
		return ret;

	 
	do_div(avg_response_time, i);

	 
	memset(priv->buf, 0, IPC_FLOOD_TEST_RESULT_LEN);

	if (!ipc_count) {
		dev_dbg(dev, "IPC Flood test duration: %lums\n", ipc_duration_ms);
		snprintf(priv->buf, IPC_FLOOD_TEST_RESULT_LEN,
			 "IPC Flood test duration: %lums\n", ipc_duration_ms);
	}

	dev_dbg(dev, "IPC Flood count: %d, Avg response time: %lluns\n",
		i, avg_response_time);
	dev_dbg(dev, "Max response time: %lluns\n", max_response_time);
	dev_dbg(dev, "Min response time: %lluns\n", min_response_time);

	 
	snprintf(priv->buf + strlen(priv->buf),
		 IPC_FLOOD_TEST_RESULT_LEN - strlen(priv->buf),
		 "IPC Flood count: %d\nAvg response time: %lluns\n",
		 i, avg_response_time);

	snprintf(priv->buf + strlen(priv->buf),
		 IPC_FLOOD_TEST_RESULT_LEN - strlen(priv->buf),
		 "Max response time: %lluns\nMin response time: %lluns\n",
		 max_response_time, min_response_time);

	return ret;
}

 
static ssize_t sof_ipc_flood_dfs_write(struct file *file, const char __user *buffer,
				       size_t count, loff_t *ppos)
{
	struct sof_client_dev *cdev = file->private_data;
	struct device *dev = &cdev->auxdev.dev;
	unsigned long ipc_duration_ms = 0;
	bool flood_duration_test = false;
	unsigned long ipc_count = 0;
	struct dentry *dentry;
	int err;
	size_t size;
	char *string;
	int ret;

	string = kzalloc(count + 1, GFP_KERNEL);
	if (!string)
		return -ENOMEM;

	size = simple_write_to_buffer(string, count, ppos, buffer, count);

	 
	dentry = file->f_path.dentry;
	if (strcmp(dentry->d_name.name, DEBUGFS_IPC_FLOOD_COUNT) &&
	    strcmp(dentry->d_name.name, DEBUGFS_IPC_FLOOD_DURATION)) {
		ret = -EINVAL;
		goto out;
	}

	if (!strcmp(dentry->d_name.name, DEBUGFS_IPC_FLOOD_DURATION))
		flood_duration_test = true;

	 
	if (flood_duration_test)
		ret = kstrtoul(string, 0, &ipc_duration_ms);
	else
		ret = kstrtoul(string, 0, &ipc_count);
	if (ret < 0)
		goto out;

	 
	if (flood_duration_test) {
		if (!ipc_duration_ms) {
			ret = size;
			goto out;
		}

		 
		if (ipc_duration_ms > MAX_IPC_FLOOD_DURATION_MS)
			ipc_duration_ms = MAX_IPC_FLOOD_DURATION_MS;
	} else {
		if (!ipc_count) {
			ret = size;
			goto out;
		}

		 
		if (ipc_count > MAX_IPC_FLOOD_COUNT)
			ipc_count = MAX_IPC_FLOOD_COUNT;
	}

	ret = pm_runtime_resume_and_get(dev);
	if (ret < 0 && ret != -EACCES) {
		dev_err_ratelimited(dev, "debugfs write failed to resume %d\n", ret);
		goto out;
	}

	 
	ret = sof_debug_ipc_flood_test(cdev, flood_duration_test,
				       ipc_duration_ms, ipc_count);

	pm_runtime_mark_last_busy(dev);
	err = pm_runtime_put_autosuspend(dev);
	if (err < 0)
		dev_err_ratelimited(dev, "debugfs write failed to idle %d\n", err);

	 
	if (ret >= 0)
		ret = size;
out:
	kfree(string);
	return ret;
}

 
static ssize_t sof_ipc_flood_dfs_read(struct file *file, char __user *buffer,
				      size_t count, loff_t *ppos)
{
	struct sof_client_dev *cdev = file->private_data;
	struct sof_ipc_flood_priv *priv = cdev->data;
	size_t size_ret;

	struct dentry *dentry;

	dentry = file->f_path.dentry;
	if (!strcmp(dentry->d_name.name, DEBUGFS_IPC_FLOOD_COUNT) ||
	    !strcmp(dentry->d_name.name, DEBUGFS_IPC_FLOOD_DURATION)) {
		if (*ppos)
			return 0;

		count = min_t(size_t, count, strlen(priv->buf));
		size_ret = copy_to_user(buffer, priv->buf, count);
		if (size_ret)
			return -EFAULT;

		*ppos += count;
		return count;
	}
	return count;
}

static int sof_ipc_flood_dfs_release(struct inode *inode, struct file *file)
{
	debugfs_file_put(file->f_path.dentry);

	return 0;
}

static const struct file_operations sof_ipc_flood_fops = {
	.open = sof_ipc_flood_dfs_open,
	.read = sof_ipc_flood_dfs_read,
	.llseek = default_llseek,
	.write = sof_ipc_flood_dfs_write,
	.release = sof_ipc_flood_dfs_release,

	.owner = THIS_MODULE,
};

 
static int sof_ipc_flood_probe(struct auxiliary_device *auxdev,
			       const struct auxiliary_device_id *id)
{
	struct sof_client_dev *cdev = auxiliary_dev_to_sof_client_dev(auxdev);
	struct dentry *debugfs_root = sof_client_get_debugfs_root(cdev);
	struct device *dev = &auxdev->dev;
	struct sof_ipc_flood_priv *priv;

	 
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->buf = devm_kmalloc(dev, IPC_FLOOD_TEST_RESULT_LEN, GFP_KERNEL);
	if (!priv->buf)
		return -ENOMEM;

	cdev->data = priv;

	 
	priv->dfs_root = debugfs_create_dir(dev_name(dev), debugfs_root);
	if (!IS_ERR_OR_NULL(priv->dfs_root)) {
		 
		debugfs_create_file(DEBUGFS_IPC_FLOOD_COUNT, 0644, priv->dfs_root,
				    cdev, &sof_ipc_flood_fops);

		 
		debugfs_create_file(DEBUGFS_IPC_FLOOD_DURATION, 0644,
				    priv->dfs_root, cdev, &sof_ipc_flood_fops);

		if (auxdev->id == 0) {
			 
			char target[100];

			snprintf(target, 100, "%s/" DEBUGFS_IPC_FLOOD_COUNT,
				 dev_name(dev));
			priv->dfs_link[0] =
				debugfs_create_symlink(DEBUGFS_IPC_FLOOD_COUNT,
						       debugfs_root, target);

			snprintf(target, 100, "%s/" DEBUGFS_IPC_FLOOD_DURATION,
				 dev_name(dev));
			priv->dfs_link[1] =
				debugfs_create_symlink(DEBUGFS_IPC_FLOOD_DURATION,
						       debugfs_root, target);
		}
	}

	 
	pm_runtime_set_autosuspend_delay(dev, SOF_IPC_CLIENT_SUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_idle(dev);

	return 0;
}

static void sof_ipc_flood_remove(struct auxiliary_device *auxdev)
{
	struct sof_client_dev *cdev = auxiliary_dev_to_sof_client_dev(auxdev);
	struct sof_ipc_flood_priv *priv = cdev->data;

	pm_runtime_disable(&auxdev->dev);

	if (auxdev->id == 0) {
		debugfs_remove(priv->dfs_link[0]);
		debugfs_remove(priv->dfs_link[1]);
	}

	debugfs_remove_recursive(priv->dfs_root);
}

static const struct auxiliary_device_id sof_ipc_flood_client_id_table[] = {
	{ .name = "snd_sof.ipc_flood" },
	{},
};
MODULE_DEVICE_TABLE(auxiliary, sof_ipc_flood_client_id_table);

 
static struct auxiliary_driver sof_ipc_flood_client_drv = {
	.probe = sof_ipc_flood_probe,
	.remove = sof_ipc_flood_remove,

	.id_table = sof_ipc_flood_client_id_table,
};

module_auxiliary_driver(sof_ipc_flood_client_drv);

MODULE_DESCRIPTION("SOF IPC Flood Test Client Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(SND_SOC_SOF_CLIENT);
