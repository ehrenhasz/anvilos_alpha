
 

#include <linux/debugfs.h>
#include <linux/module.h>

#include "ixgbe.h"

static struct dentry *ixgbe_dbg_root;

static char ixgbe_dbg_reg_ops_buf[256] = "";

static ssize_t ixgbe_dbg_common_ops_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *ppos,
					 char *dbg_buf)
{
	struct ixgbe_adapter *adapter = filp->private_data;
	char *buf;
	int len;

	 
	if (*ppos != 0)
		return 0;

	buf = kasprintf(GFP_KERNEL, "%s: %s\n",
			adapter->netdev->name, dbg_buf);
	if (!buf)
		return -ENOMEM;

	if (count < strlen(buf)) {
		kfree(buf);
		return -ENOSPC;
	}

	len = simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));

	kfree(buf);
	return len;
}

 
static ssize_t ixgbe_dbg_reg_ops_read(struct file *filp, char __user *buffer,
				      size_t count, loff_t *ppos)
{
	return ixgbe_dbg_common_ops_read(filp, buffer, count, ppos,
					 ixgbe_dbg_reg_ops_buf);
}

 
static ssize_t ixgbe_dbg_reg_ops_write(struct file *filp,
				     const char __user *buffer,
				     size_t count, loff_t *ppos)
{
	struct ixgbe_adapter *adapter = filp->private_data;
	int len;

	 
	if (*ppos != 0)
		return 0;
	if (count >= sizeof(ixgbe_dbg_reg_ops_buf))
		return -ENOSPC;

	len = simple_write_to_buffer(ixgbe_dbg_reg_ops_buf,
				     sizeof(ixgbe_dbg_reg_ops_buf)-1,
				     ppos,
				     buffer,
				     count);
	if (len < 0)
		return len;

	ixgbe_dbg_reg_ops_buf[len] = '\0';

	if (strncmp(ixgbe_dbg_reg_ops_buf, "write", 5) == 0) {
		u32 reg, value;
		int cnt;
		cnt = sscanf(&ixgbe_dbg_reg_ops_buf[5], "%x %x", &reg, &value);
		if (cnt == 2) {
			IXGBE_WRITE_REG(&adapter->hw, reg, value);
			value = IXGBE_READ_REG(&adapter->hw, reg);
			e_dev_info("write: 0x%08x = 0x%08x\n", reg, value);
		} else {
			e_dev_info("write <reg> <value>\n");
		}
	} else if (strncmp(ixgbe_dbg_reg_ops_buf, "read", 4) == 0) {
		u32 reg, value;
		int cnt;
		cnt = sscanf(&ixgbe_dbg_reg_ops_buf[4], "%x", &reg);
		if (cnt == 1) {
			value = IXGBE_READ_REG(&adapter->hw, reg);
			e_dev_info("read 0x%08x = 0x%08x\n", reg, value);
		} else {
			e_dev_info("read <reg>\n");
		}
	} else {
		e_dev_info("Unknown command %s\n", ixgbe_dbg_reg_ops_buf);
		e_dev_info("Available commands:\n");
		e_dev_info("   read <reg>\n");
		e_dev_info("   write <reg> <value>\n");
	}
	return count;
}

static const struct file_operations ixgbe_dbg_reg_ops_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read =  ixgbe_dbg_reg_ops_read,
	.write = ixgbe_dbg_reg_ops_write,
};

static char ixgbe_dbg_netdev_ops_buf[256] = "";

 
static ssize_t ixgbe_dbg_netdev_ops_read(struct file *filp, char __user *buffer,
					 size_t count, loff_t *ppos)
{
	return ixgbe_dbg_common_ops_read(filp, buffer, count, ppos,
					 ixgbe_dbg_netdev_ops_buf);
}

 
static ssize_t ixgbe_dbg_netdev_ops_write(struct file *filp,
					  const char __user *buffer,
					  size_t count, loff_t *ppos)
{
	struct ixgbe_adapter *adapter = filp->private_data;
	int len;

	 
	if (*ppos != 0)
		return 0;
	if (count >= sizeof(ixgbe_dbg_netdev_ops_buf))
		return -ENOSPC;

	len = simple_write_to_buffer(ixgbe_dbg_netdev_ops_buf,
				     sizeof(ixgbe_dbg_netdev_ops_buf)-1,
				     ppos,
				     buffer,
				     count);
	if (len < 0)
		return len;

	ixgbe_dbg_netdev_ops_buf[len] = '\0';

	if (strncmp(ixgbe_dbg_netdev_ops_buf, "tx_timeout", 10) == 0) {
		 
		adapter->netdev->netdev_ops->ndo_tx_timeout(adapter->netdev,
							    UINT_MAX);
		e_dev_info("tx_timeout called\n");
	} else {
		e_dev_info("Unknown command: %s\n", ixgbe_dbg_netdev_ops_buf);
		e_dev_info("Available commands:\n");
		e_dev_info("    tx_timeout\n");
	}
	return count;
}

static const struct file_operations ixgbe_dbg_netdev_ops_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ixgbe_dbg_netdev_ops_read,
	.write = ixgbe_dbg_netdev_ops_write,
};

 
void ixgbe_dbg_adapter_init(struct ixgbe_adapter *adapter)
{
	const char *name = pci_name(adapter->pdev);

	adapter->ixgbe_dbg_adapter = debugfs_create_dir(name, ixgbe_dbg_root);
	debugfs_create_file("reg_ops", 0600, adapter->ixgbe_dbg_adapter,
			    adapter, &ixgbe_dbg_reg_ops_fops);
	debugfs_create_file("netdev_ops", 0600, adapter->ixgbe_dbg_adapter,
			    adapter, &ixgbe_dbg_netdev_ops_fops);
}

 
void ixgbe_dbg_adapter_exit(struct ixgbe_adapter *adapter)
{
	debugfs_remove_recursive(adapter->ixgbe_dbg_adapter);
	adapter->ixgbe_dbg_adapter = NULL;
}

 
void ixgbe_dbg_init(void)
{
	ixgbe_dbg_root = debugfs_create_dir(ixgbe_driver_name, NULL);
}

 
void ixgbe_dbg_exit(void)
{
	debugfs_remove_recursive(ixgbe_dbg_root);
}
