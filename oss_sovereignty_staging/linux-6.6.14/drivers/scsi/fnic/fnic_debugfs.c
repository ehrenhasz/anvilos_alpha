


#include <linux/module.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include "fnic.h"

static struct dentry *fnic_trace_debugfs_root;
static struct dentry *fnic_trace_debugfs_file;
static struct dentry *fnic_trace_enable;
static struct dentry *fnic_stats_debugfs_root;

static struct dentry *fnic_fc_trace_debugfs_file;
static struct dentry *fnic_fc_rdata_trace_debugfs_file;
static struct dentry *fnic_fc_trace_enable;
static struct dentry *fnic_fc_trace_clear;

struct fc_trace_flag_type {
	u8 fc_row_file;
	u8 fc_normal_file;
	u8 fnic_trace;
	u8 fc_trace;
	u8 fc_clear;
};

static struct fc_trace_flag_type *fc_trc_flag;

 
int fnic_debugfs_init(void)
{
	fnic_trace_debugfs_root = debugfs_create_dir("fnic", NULL);

	fnic_stats_debugfs_root = debugfs_create_dir("statistics",
						fnic_trace_debugfs_root);

	 
	fc_trc_flag = vmalloc(sizeof(struct fc_trace_flag_type));

	if (fc_trc_flag) {
		fc_trc_flag->fc_row_file = 0;
		fc_trc_flag->fc_normal_file = 1;
		fc_trc_flag->fnic_trace = 2;
		fc_trc_flag->fc_trace = 3;
		fc_trc_flag->fc_clear = 4;
		return 0;
	}

	return -ENOMEM;
}

 
void fnic_debugfs_terminate(void)
{
	debugfs_remove(fnic_stats_debugfs_root);
	fnic_stats_debugfs_root = NULL;

	debugfs_remove(fnic_trace_debugfs_root);
	fnic_trace_debugfs_root = NULL;

	vfree(fc_trc_flag);
}

 
static ssize_t fnic_trace_ctrl_read(struct file *filp,
				  char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	char buf[64];
	int len;
	u8 *trace_type;
	len = 0;
	trace_type = (u8 *)filp->private_data;
	if (*trace_type == fc_trc_flag->fnic_trace)
		len = sprintf(buf, "%d\n", fnic_tracing_enabled);
	else if (*trace_type == fc_trc_flag->fc_trace)
		len = sprintf(buf, "%d\n", fnic_fc_tracing_enabled);
	else if (*trace_type == fc_trc_flag->fc_clear)
		len = sprintf(buf, "%d\n", fnic_fc_trace_cleared);
	else
		pr_err("fnic: Cannot read to any debugfs file\n");

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
}

 
static ssize_t fnic_trace_ctrl_write(struct file *filp,
				  const char __user *ubuf,
				  size_t cnt, loff_t *ppos)
{
	char buf[64];
	unsigned long val;
	int ret;
	u8 *trace_type;
	trace_type = (u8 *)filp->private_data;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (*trace_type == fc_trc_flag->fnic_trace)
		fnic_tracing_enabled = val;
	else if (*trace_type == fc_trc_flag->fc_trace)
		fnic_fc_tracing_enabled = val;
	else if (*trace_type == fc_trc_flag->fc_clear)
		fnic_fc_trace_cleared = val;
	else
		pr_err("fnic: cannot write to any debugfs file\n");

	(*ppos)++;

	return cnt;
}

static const struct file_operations fnic_trace_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = fnic_trace_ctrl_read,
	.write = fnic_trace_ctrl_write,
};

 
static int fnic_trace_debugfs_open(struct inode *inode,
				  struct file *file)
{
	fnic_dbgfs_t *fnic_dbg_prt;
	u8 *rdata_ptr;
	rdata_ptr = (u8 *)inode->i_private;
	fnic_dbg_prt = kzalloc(sizeof(fnic_dbgfs_t), GFP_KERNEL);
	if (!fnic_dbg_prt)
		return -ENOMEM;

	if (*rdata_ptr == fc_trc_flag->fnic_trace) {
		fnic_dbg_prt->buffer = vzalloc(array3_size(3, trace_max_pages,
							   PAGE_SIZE));
		if (!fnic_dbg_prt->buffer) {
			kfree(fnic_dbg_prt);
			return -ENOMEM;
		}
		fnic_dbg_prt->buffer_len = fnic_get_trace_data(fnic_dbg_prt);
	} else {
		fnic_dbg_prt->buffer =
			vzalloc(array3_size(3, fnic_fc_trace_max_pages,
					    PAGE_SIZE));
		if (!fnic_dbg_prt->buffer) {
			kfree(fnic_dbg_prt);
			return -ENOMEM;
		}
		fnic_dbg_prt->buffer_len =
			fnic_fc_trace_get_data(fnic_dbg_prt, *rdata_ptr);
	}
	file->private_data = fnic_dbg_prt;

	return 0;
}

 
static loff_t fnic_trace_debugfs_lseek(struct file *file,
					loff_t offset,
					int howto)
{
	fnic_dbgfs_t *fnic_dbg_prt = file->private_data;
	return fixed_size_llseek(file, offset, howto,
				fnic_dbg_prt->buffer_len);
}

 
static ssize_t fnic_trace_debugfs_read(struct file *file,
					char __user *ubuf,
					size_t nbytes,
					loff_t *pos)
{
	fnic_dbgfs_t *fnic_dbg_prt = file->private_data;
	int rc = 0;
	rc = simple_read_from_buffer(ubuf, nbytes, pos,
				  fnic_dbg_prt->buffer,
				  fnic_dbg_prt->buffer_len);
	return rc;
}

 
static int fnic_trace_debugfs_release(struct inode *inode,
					  struct file *file)
{
	fnic_dbgfs_t *fnic_dbg_prt = file->private_data;

	vfree(fnic_dbg_prt->buffer);
	kfree(fnic_dbg_prt);
	return 0;
}

static const struct file_operations fnic_trace_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = fnic_trace_debugfs_open,
	.llseek = fnic_trace_debugfs_lseek,
	.read = fnic_trace_debugfs_read,
	.release = fnic_trace_debugfs_release,
};

 
void fnic_trace_debugfs_init(void)
{
	fnic_trace_enable = debugfs_create_file("tracing_enable",
					S_IFREG|S_IRUGO|S_IWUSR,
					fnic_trace_debugfs_root,
					&(fc_trc_flag->fnic_trace),
					&fnic_trace_ctrl_fops);

	fnic_trace_debugfs_file = debugfs_create_file("trace",
					S_IFREG|S_IRUGO|S_IWUSR,
					fnic_trace_debugfs_root,
					&(fc_trc_flag->fnic_trace),
					&fnic_trace_debugfs_fops);
}

 
void fnic_trace_debugfs_terminate(void)
{
	debugfs_remove(fnic_trace_debugfs_file);
	fnic_trace_debugfs_file = NULL;

	debugfs_remove(fnic_trace_enable);
	fnic_trace_enable = NULL;
}

 

void fnic_fc_trace_debugfs_init(void)
{
	fnic_fc_trace_enable = debugfs_create_file("fc_trace_enable",
					S_IFREG|S_IRUGO|S_IWUSR,
					fnic_trace_debugfs_root,
					&(fc_trc_flag->fc_trace),
					&fnic_trace_ctrl_fops);

	fnic_fc_trace_clear = debugfs_create_file("fc_trace_clear",
					S_IFREG|S_IRUGO|S_IWUSR,
					fnic_trace_debugfs_root,
					&(fc_trc_flag->fc_clear),
					&fnic_trace_ctrl_fops);

	fnic_fc_rdata_trace_debugfs_file =
		debugfs_create_file("fc_trace_rdata",
				    S_IFREG|S_IRUGO|S_IWUSR,
				    fnic_trace_debugfs_root,
				    &(fc_trc_flag->fc_normal_file),
				    &fnic_trace_debugfs_fops);

	fnic_fc_trace_debugfs_file =
		debugfs_create_file("fc_trace",
				    S_IFREG|S_IRUGO|S_IWUSR,
				    fnic_trace_debugfs_root,
				    &(fc_trc_flag->fc_row_file),
				    &fnic_trace_debugfs_fops);
}

 

void fnic_fc_trace_debugfs_terminate(void)
{
	debugfs_remove(fnic_fc_trace_debugfs_file);
	fnic_fc_trace_debugfs_file = NULL;

	debugfs_remove(fnic_fc_rdata_trace_debugfs_file);
	fnic_fc_rdata_trace_debugfs_file = NULL;

	debugfs_remove(fnic_fc_trace_enable);
	fnic_fc_trace_enable = NULL;

	debugfs_remove(fnic_fc_trace_clear);
	fnic_fc_trace_clear = NULL;
}

 
static int fnic_reset_stats_open(struct inode *inode, struct file *file)
{
	struct stats_debug_info *debug;

	debug = kzalloc(sizeof(struct stats_debug_info), GFP_KERNEL);
	if (!debug)
		return -ENOMEM;

	debug->i_private = inode->i_private;

	file->private_data = debug;

	return 0;
}

 
static ssize_t fnic_reset_stats_read(struct file *file,
					char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	struct stats_debug_info *debug = file->private_data;
	struct fnic *fnic = (struct fnic *)debug->i_private;
	char buf[64];
	int len;

	len = sprintf(buf, "%u\n", fnic->reset_stats);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, len);
}

 
static ssize_t fnic_reset_stats_write(struct file *file,
					const char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	struct stats_debug_info *debug = file->private_data;
	struct fnic *fnic = (struct fnic *)debug->i_private;
	struct fnic_stats *stats = &fnic->fnic_stats;
	u64 *io_stats_p = (u64 *)&stats->io_stats;
	u64 *fw_stats_p = (u64 *)&stats->fw_stats;
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	fnic->reset_stats = val;

	if (fnic->reset_stats) {
		 
		atomic64_set(&fnic->io_cmpl_skip,
			atomic64_read(&stats->io_stats.active_ios));
		memset(&stats->abts_stats, 0, sizeof(struct abort_stats));
		memset(&stats->term_stats, 0,
			sizeof(struct terminate_stats));
		memset(&stats->reset_stats, 0, sizeof(struct reset_stats));
		memset(&stats->misc_stats, 0, sizeof(struct misc_stats));
		memset(&stats->vlan_stats, 0, sizeof(struct vlan_stats));
		memset(io_stats_p+1, 0,
			sizeof(struct io_path_stats) - sizeof(u64));
		memset(fw_stats_p+1, 0,
			sizeof(struct fw_stats) - sizeof(u64));
		ktime_get_real_ts64(&stats->stats_timestamps.last_reset_time);
	}

	(*ppos)++;
	return cnt;
}

 
static int fnic_reset_stats_release(struct inode *inode,
					struct file *file)
{
	struct stats_debug_info *debug = file->private_data;
	kfree(debug);
	return 0;
}

 
static int fnic_stats_debugfs_open(struct inode *inode,
					struct file *file)
{
	struct fnic *fnic = inode->i_private;
	struct fnic_stats *fnic_stats = &fnic->fnic_stats;
	struct stats_debug_info *debug;
	int buf_size = 2 * PAGE_SIZE;

	debug = kzalloc(sizeof(struct stats_debug_info), GFP_KERNEL);
	if (!debug)
		return -ENOMEM;

	debug->debug_buffer = vmalloc(buf_size);
	if (!debug->debug_buffer) {
		kfree(debug);
		return -ENOMEM;
	}

	debug->buf_size = buf_size;
	memset((void *)debug->debug_buffer, 0, buf_size);
	debug->buffer_len = fnic_get_stats_data(debug, fnic_stats);

	file->private_data = debug;

	return 0;
}

 
static ssize_t fnic_stats_debugfs_read(struct file *file,
					char __user *ubuf,
					size_t nbytes,
					loff_t *pos)
{
	struct stats_debug_info *debug = file->private_data;
	int rc = 0;
	rc = simple_read_from_buffer(ubuf, nbytes, pos,
					debug->debug_buffer,
					debug->buffer_len);
	return rc;
}

 
static int fnic_stats_debugfs_release(struct inode *inode,
					struct file *file)
{
	struct stats_debug_info *debug = file->private_data;
	vfree(debug->debug_buffer);
	kfree(debug);
	return 0;
}

static const struct file_operations fnic_stats_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = fnic_stats_debugfs_open,
	.read = fnic_stats_debugfs_read,
	.release = fnic_stats_debugfs_release,
};

static const struct file_operations fnic_reset_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = fnic_reset_stats_open,
	.read = fnic_reset_stats_read,
	.write = fnic_reset_stats_write,
	.release = fnic_reset_stats_release,
};

 
void fnic_stats_debugfs_init(struct fnic *fnic)
{
	char name[16];

	snprintf(name, sizeof(name), "host%d", fnic->lport->host->host_no);

	fnic->fnic_stats_debugfs_host = debugfs_create_dir(name,
						fnic_stats_debugfs_root);

	fnic->fnic_stats_debugfs_file = debugfs_create_file("stats",
						S_IFREG|S_IRUGO|S_IWUSR,
						fnic->fnic_stats_debugfs_host,
						fnic,
						&fnic_stats_debugfs_fops);

	fnic->fnic_reset_debugfs_file = debugfs_create_file("reset_stats",
						S_IFREG|S_IRUGO|S_IWUSR,
						fnic->fnic_stats_debugfs_host,
						fnic,
						&fnic_reset_debugfs_fops);
}

 
void fnic_stats_debugfs_remove(struct fnic *fnic)
{
	if (!fnic)
		return;

	debugfs_remove(fnic->fnic_stats_debugfs_file);
	fnic->fnic_stats_debugfs_file = NULL;

	debugfs_remove(fnic->fnic_reset_debugfs_file);
	fnic->fnic_reset_debugfs_file = NULL;

	debugfs_remove(fnic->fnic_stats_debugfs_host);
	fnic->fnic_stats_debugfs_host = NULL;
}
