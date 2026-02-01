
 

#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "kfd_priv.h"

static struct dentry *debugfs_root;

static int kfd_debugfs_open(struct inode *inode, struct file *file)
{
	int (*show)(struct seq_file *, void *) = inode->i_private;

	return single_open(file, show, NULL);
}
static int kfd_debugfs_hang_hws_read(struct seq_file *m, void *data)
{
	seq_puts(m, "echo gpu_id > hang_hws\n");
	return 0;
}

static ssize_t kfd_debugfs_hang_hws_write(struct file *file,
	const char __user *user_buf, size_t size, loff_t *ppos)
{
	struct kfd_node *dev;
	char tmp[16];
	uint32_t gpu_id;
	int ret = -EINVAL;

	memset(tmp, 0, 16);
	if (size >= 16) {
		pr_err("Invalid input for gpu id.\n");
		goto out;
	}
	if (copy_from_user(tmp, user_buf, size)) {
		ret = -EFAULT;
		goto out;
	}
	if (kstrtoint(tmp, 10, &gpu_id)) {
		pr_err("Invalid input for gpu id.\n");
		goto out;
	}
	dev = kfd_device_by_id(gpu_id);
	if (dev) {
		kfd_debugfs_hang_hws(dev);
		ret = size;
	} else
		pr_err("Cannot find device %d.\n", gpu_id);

out:
	return ret;
}

static const struct file_operations kfd_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = kfd_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations kfd_debugfs_hang_hws_fops = {
	.owner = THIS_MODULE,
	.open = kfd_debugfs_open,
	.read = seq_read,
	.write = kfd_debugfs_hang_hws_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void kfd_debugfs_init(void)
{
	debugfs_root = debugfs_create_dir("kfd", NULL);

	debugfs_create_file("mqds", S_IFREG | 0444, debugfs_root,
			    kfd_debugfs_mqds_by_process, &kfd_debugfs_fops);
	debugfs_create_file("hqds", S_IFREG | 0444, debugfs_root,
			    kfd_debugfs_hqds_by_device, &kfd_debugfs_fops);
	debugfs_create_file("rls", S_IFREG | 0444, debugfs_root,
			    kfd_debugfs_rls_by_device, &kfd_debugfs_fops);
	debugfs_create_file("hang_hws", S_IFREG | 0200, debugfs_root,
			    kfd_debugfs_hang_hws_read, &kfd_debugfs_hang_hws_fops);
	debugfs_create_file("mem_limit", S_IFREG | 0200, debugfs_root,
			    kfd_debugfs_kfd_mem_limits, &kfd_debugfs_fops);
}

void kfd_debugfs_fini(void)
{
	debugfs_remove_recursive(debugfs_root);
}
