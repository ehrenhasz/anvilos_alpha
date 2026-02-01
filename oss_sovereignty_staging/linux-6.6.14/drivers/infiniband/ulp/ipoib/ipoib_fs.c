 

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

struct file_operations;

#include <linux/debugfs.h>
#include <linux/export.h>

#include "ipoib.h"

static struct dentry *ipoib_root;

static void format_gid(union ib_gid *gid, char *buf)
{
	int i, n;

	for (n = 0, i = 0; i < 8; ++i) {
		n += sprintf(buf + n, "%x",
			     be16_to_cpu(((__be16 *) gid->raw)[i]));
		if (i < 7)
			buf[n++] = ':';
	}
}

static void *ipoib_mcg_seq_start(struct seq_file *file, loff_t *pos)
{
	struct ipoib_mcast_iter *iter;
	loff_t n = *pos;

	iter = ipoib_mcast_iter_init(file->private);
	if (!iter)
		return NULL;

	while (n--) {
		if (ipoib_mcast_iter_next(iter)) {
			kfree(iter);
			return NULL;
		}
	}

	return iter;
}

static void *ipoib_mcg_seq_next(struct seq_file *file, void *iter_ptr,
				   loff_t *pos)
{
	struct ipoib_mcast_iter *iter = iter_ptr;

	(*pos)++;

	if (ipoib_mcast_iter_next(iter)) {
		kfree(iter);
		return NULL;
	}

	return iter;
}

static void ipoib_mcg_seq_stop(struct seq_file *file, void *iter_ptr)
{
	 
}

static int ipoib_mcg_seq_show(struct seq_file *file, void *iter_ptr)
{
	struct ipoib_mcast_iter *iter = iter_ptr;
	char gid_buf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"];
	union ib_gid mgid;
	unsigned long created;
	unsigned int queuelen, complete, send_only;

	if (!iter)
		return 0;

	ipoib_mcast_iter_read(iter, &mgid, &created, &queuelen,
			      &complete, &send_only);

	format_gid(&mgid, gid_buf);

	seq_printf(file,
		   "GID: %s\n"
		   "  created: %10ld\n"
		   "  queuelen: %9d\n"
		   "  complete: %9s\n"
		   "  send_only: %8s\n"
		   "\n",
		   gid_buf, created, queuelen,
		   complete ? "yes" : "no",
		   send_only ? "yes" : "no");

	return 0;
}

static const struct seq_operations ipoib_mcg_sops = {
	.start = ipoib_mcg_seq_start,
	.next  = ipoib_mcg_seq_next,
	.stop  = ipoib_mcg_seq_stop,
	.show  = ipoib_mcg_seq_show,
};

DEFINE_SEQ_ATTRIBUTE(ipoib_mcg);

static void *ipoib_path_seq_start(struct seq_file *file, loff_t *pos)
{
	struct ipoib_path_iter *iter;
	loff_t n = *pos;

	iter = ipoib_path_iter_init(file->private);
	if (!iter)
		return NULL;

	while (n--) {
		if (ipoib_path_iter_next(iter)) {
			kfree(iter);
			return NULL;
		}
	}

	return iter;
}

static void *ipoib_path_seq_next(struct seq_file *file, void *iter_ptr,
				   loff_t *pos)
{
	struct ipoib_path_iter *iter = iter_ptr;

	(*pos)++;

	if (ipoib_path_iter_next(iter)) {
		kfree(iter);
		return NULL;
	}

	return iter;
}

static void ipoib_path_seq_stop(struct seq_file *file, void *iter_ptr)
{
	 
}

static int ipoib_path_seq_show(struct seq_file *file, void *iter_ptr)
{
	struct ipoib_path_iter *iter = iter_ptr;
	char gid_buf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"];
	struct ipoib_path path;
	int rate;

	if (!iter)
		return 0;

	ipoib_path_iter_read(iter, &path);

	format_gid(&path.pathrec.dgid, gid_buf);

	seq_printf(file,
		   "GID: %s\n"
		   "  complete: %6s\n",
		   gid_buf, sa_path_get_dlid(&path.pathrec) ? "yes" : "no");

	if (sa_path_get_dlid(&path.pathrec)) {
		rate = ib_rate_to_mbps(path.pathrec.rate);

		seq_printf(file,
			   "  DLID:     0x%04x\n"
			   "  SL: %12d\n"
			   "  rate: %8d.%d Gb/sec\n",
			   be32_to_cpu(sa_path_get_dlid(&path.pathrec)),
			   path.pathrec.sl,
			   rate / 1000, rate % 1000);
	}

	seq_putc(file, '\n');

	return 0;
}

static const struct seq_operations ipoib_path_sops = {
	.start = ipoib_path_seq_start,
	.next  = ipoib_path_seq_next,
	.stop  = ipoib_path_seq_stop,
	.show  = ipoib_path_seq_show,
};

DEFINE_SEQ_ATTRIBUTE(ipoib_path);

void ipoib_create_debug_files(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = ipoib_priv(dev);
	char name[IFNAMSIZ + sizeof("_path")];

	snprintf(name, sizeof(name), "%s_mcg", dev->name);
	priv->mcg_dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
					       ipoib_root, dev, &ipoib_mcg_fops);

	snprintf(name, sizeof(name), "%s_path", dev->name);
	priv->path_dentry = debugfs_create_file(name, S_IFREG | S_IRUGO,
						ipoib_root, dev, &ipoib_path_fops);
}

void ipoib_delete_debug_files(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = ipoib_priv(dev);

	debugfs_remove(priv->mcg_dentry);
	debugfs_remove(priv->path_dentry);
	priv->mcg_dentry = priv->path_dentry = NULL;
}

void ipoib_register_debugfs(void)
{
	ipoib_root = debugfs_create_dir("ipoib", NULL);
}

void ipoib_unregister_debugfs(void)
{
	debugfs_remove(ipoib_root);
}
