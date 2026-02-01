 

#include <linux/circ_buf.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <drm/drm_crtc.h>
#include <drm/drm_debugfs_crc.h>
#include <drm/drm_drv.h>
#include <drm/drm_print.h>

#include "drm_internal.h"

 

static int crc_control_show(struct seq_file *m, void *data)
{
	struct drm_crtc *crtc = m->private;

	if (crtc->funcs->get_crc_sources) {
		size_t count;
		const char *const *sources = crtc->funcs->get_crc_sources(crtc,
									&count);
		size_t values_cnt;
		int i;

		if (count == 0 || !sources)
			goto out;

		for (i = 0; i < count; i++)
			if (!crtc->funcs->verify_crc_source(crtc, sources[i],
							    &values_cnt)) {
				if (strcmp(sources[i], crtc->crc.source))
					seq_printf(m, "%s\n", sources[i]);
				else
					seq_printf(m, "%s*\n", sources[i]);
			}
	}
	return 0;

out:
	seq_printf(m, "%s*\n", crtc->crc.source);
	return 0;
}

static int crc_control_open(struct inode *inode, struct file *file)
{
	struct drm_crtc *crtc = inode->i_private;

	return single_open(file, crc_control_show, crtc);
}

static ssize_t crc_control_write(struct file *file, const char __user *ubuf,
				 size_t len, loff_t *offp)
{
	struct seq_file *m = file->private_data;
	struct drm_crtc *crtc = m->private;
	struct drm_crtc_crc *crc = &crtc->crc;
	char *source;
	size_t values_cnt;
	int ret;

	if (len == 0)
		return 0;

	if (len > PAGE_SIZE - 1) {
		DRM_DEBUG_KMS("Expected < %lu bytes into crtc crc control\n",
			      PAGE_SIZE);
		return -E2BIG;
	}

	source = memdup_user_nul(ubuf, len);
	if (IS_ERR(source))
		return PTR_ERR(source);

	if (source[len - 1] == '\n')
		source[len - 1] = '\0';

	ret = crtc->funcs->verify_crc_source(crtc, source, &values_cnt);
	if (ret) {
		kfree(source);
		return ret;
	}

	spin_lock_irq(&crc->lock);

	if (crc->opened) {
		spin_unlock_irq(&crc->lock);
		kfree(source);
		return -EBUSY;
	}

	kfree(crc->source);
	crc->source = source;

	spin_unlock_irq(&crc->lock);

	*offp += len;
	return len;
}

static const struct file_operations drm_crtc_crc_control_fops = {
	.owner = THIS_MODULE,
	.open = crc_control_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = crc_control_write
};

static int crtc_crc_data_count(struct drm_crtc_crc *crc)
{
	assert_spin_locked(&crc->lock);
	return CIRC_CNT(crc->head, crc->tail, DRM_CRC_ENTRIES_NR);
}

static void crtc_crc_cleanup(struct drm_crtc_crc *crc)
{
	kfree(crc->entries);
	crc->overflow = false;
	crc->entries = NULL;
	crc->head = 0;
	crc->tail = 0;
	crc->values_cnt = 0;
	crc->opened = false;
}

static int crtc_crc_open(struct inode *inode, struct file *filep)
{
	struct drm_crtc *crtc = inode->i_private;
	struct drm_crtc_crc *crc = &crtc->crc;
	struct drm_crtc_crc_entry *entries = NULL;
	size_t values_cnt;
	int ret = 0;

	if (drm_drv_uses_atomic_modeset(crtc->dev)) {
		ret = drm_modeset_lock_single_interruptible(&crtc->mutex);
		if (ret)
			return ret;

		if (!crtc->state->active)
			ret = -EIO;
		drm_modeset_unlock(&crtc->mutex);

		if (ret)
			return ret;
	}

	ret = crtc->funcs->verify_crc_source(crtc, crc->source, &values_cnt);
	if (ret)
		return ret;

	if (WARN_ON(values_cnt > DRM_MAX_CRC_NR))
		return -EINVAL;

	if (WARN_ON(values_cnt == 0))
		return -EINVAL;

	entries = kcalloc(DRM_CRC_ENTRIES_NR, sizeof(*entries), GFP_KERNEL);
	if (!entries)
		return -ENOMEM;

	spin_lock_irq(&crc->lock);
	if (!crc->opened) {
		crc->opened = true;
		crc->entries = entries;
		crc->values_cnt = values_cnt;
	} else {
		ret = -EBUSY;
	}
	spin_unlock_irq(&crc->lock);

	if (ret) {
		kfree(entries);
		return ret;
	}

	ret = crtc->funcs->set_crc_source(crtc, crc->source);
	if (ret)
		goto err;

	return 0;

err:
	spin_lock_irq(&crc->lock);
	crtc_crc_cleanup(crc);
	spin_unlock_irq(&crc->lock);
	return ret;
}

static int crtc_crc_release(struct inode *inode, struct file *filep)
{
	struct drm_crtc *crtc = filep->f_inode->i_private;
	struct drm_crtc_crc *crc = &crtc->crc;

	 
	spin_lock_irq(&crc->lock);
	crc->opened = false;
	spin_unlock_irq(&crc->lock);

	crtc->funcs->set_crc_source(crtc, NULL);

	spin_lock_irq(&crc->lock);
	crtc_crc_cleanup(crc);
	spin_unlock_irq(&crc->lock);

	return 0;
}

 
#define LINE_LEN(values_cnt)	(10 + 11 * values_cnt + 1 + 1)
#define MAX_LINE_LEN		(LINE_LEN(DRM_MAX_CRC_NR))

static ssize_t crtc_crc_read(struct file *filep, char __user *user_buf,
			     size_t count, loff_t *pos)
{
	struct drm_crtc *crtc = filep->f_inode->i_private;
	struct drm_crtc_crc *crc = &crtc->crc;
	struct drm_crtc_crc_entry *entry;
	char buf[MAX_LINE_LEN];
	int ret, i;

	spin_lock_irq(&crc->lock);

	if (!crc->source) {
		spin_unlock_irq(&crc->lock);
		return 0;
	}

	 
	while (crtc_crc_data_count(crc) == 0) {
		if (filep->f_flags & O_NONBLOCK) {
			spin_unlock_irq(&crc->lock);
			return -EAGAIN;
		}

		ret = wait_event_interruptible_lock_irq(crc->wq,
							crtc_crc_data_count(crc),
							crc->lock);
		if (ret) {
			spin_unlock_irq(&crc->lock);
			return ret;
		}
	}

	 
	entry = &crc->entries[crc->tail];

	if (count < LINE_LEN(crc->values_cnt)) {
		spin_unlock_irq(&crc->lock);
		return -EINVAL;
	}

	BUILD_BUG_ON_NOT_POWER_OF_2(DRM_CRC_ENTRIES_NR);
	crc->tail = (crc->tail + 1) & (DRM_CRC_ENTRIES_NR - 1);

	spin_unlock_irq(&crc->lock);

	if (entry->has_frame_counter)
		sprintf(buf, "0x%08x", entry->frame);
	else
		sprintf(buf, "XXXXXXXXXX");

	for (i = 0; i < crc->values_cnt; i++)
		sprintf(buf + 10 + i * 11, " 0x%08x", entry->crcs[i]);
	sprintf(buf + 10 + crc->values_cnt * 11, "\n");

	if (copy_to_user(user_buf, buf, LINE_LEN(crc->values_cnt)))
		return -EFAULT;

	return LINE_LEN(crc->values_cnt);
}

static __poll_t crtc_crc_poll(struct file *file, poll_table *wait)
{
	struct drm_crtc *crtc = file->f_inode->i_private;
	struct drm_crtc_crc *crc = &crtc->crc;
	__poll_t ret = 0;

	poll_wait(file, &crc->wq, wait);

	spin_lock_irq(&crc->lock);
	if (crc->source && crtc_crc_data_count(crc))
		ret |= EPOLLIN | EPOLLRDNORM;
	spin_unlock_irq(&crc->lock);

	return ret;
}

static const struct file_operations drm_crtc_crc_data_fops = {
	.owner = THIS_MODULE,
	.open = crtc_crc_open,
	.read = crtc_crc_read,
	.poll = crtc_crc_poll,
	.release = crtc_crc_release,
};

void drm_debugfs_crtc_crc_add(struct drm_crtc *crtc)
{
	struct dentry *crc_ent;

	if (!crtc->funcs->set_crc_source || !crtc->funcs->verify_crc_source)
		return;

	crc_ent = debugfs_create_dir("crc", crtc->debugfs_entry);

	debugfs_create_file("control", S_IRUGO | S_IWUSR, crc_ent, crtc,
			    &drm_crtc_crc_control_fops);
	debugfs_create_file("data", S_IRUGO, crc_ent, crtc,
			    &drm_crtc_crc_data_fops);
}

 
int drm_crtc_add_crc_entry(struct drm_crtc *crtc, bool has_frame,
			   uint32_t frame, uint32_t *crcs)
{
	struct drm_crtc_crc *crc = &crtc->crc;
	struct drm_crtc_crc_entry *entry;
	int head, tail;
	unsigned long flags;

	spin_lock_irqsave(&crc->lock, flags);

	 
	if (!crc->entries) {
		spin_unlock_irqrestore(&crc->lock, flags);
		return -EINVAL;
	}

	head = crc->head;
	tail = crc->tail;

	if (CIRC_SPACE(head, tail, DRM_CRC_ENTRIES_NR) < 1) {
		bool was_overflow = crc->overflow;

		crc->overflow = true;
		spin_unlock_irqrestore(&crc->lock, flags);

		if (!was_overflow)
			DRM_ERROR("Overflow of CRC buffer, userspace reads too slow.\n");

		return -ENOBUFS;
	}

	entry = &crc->entries[head];
	entry->frame = frame;
	entry->has_frame_counter = has_frame;
	memcpy(&entry->crcs, crcs, sizeof(*crcs) * crc->values_cnt);

	head = (head + 1) & (DRM_CRC_ENTRIES_NR - 1);
	crc->head = head;

	spin_unlock_irqrestore(&crc->lock, flags);

	wake_up_interruptible(&crc->wq);

	return 0;
}
EXPORT_SYMBOL_GPL(drm_crtc_add_crc_entry);
