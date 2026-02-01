
 

 

#include <linux/circ_buf.h>
#include <linux/debugfs.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <drm/drm_file.h>

#include "msm_drv.h"
#include "msm_gpu.h"
#include "msm_gem.h"

bool rd_full = false;
MODULE_PARM_DESC(rd_full, "If true, $debugfs/.../rd will snapshot all buffer contents");
module_param_named(rd_full, rd_full, bool, 0600);

#ifdef CONFIG_DEBUG_FS

enum rd_sect_type {
	RD_NONE,
	RD_TEST,        
	RD_CMD,         
	RD_GPUADDR,     
	RD_CONTEXT,     
	RD_CMDSTREAM,   
	RD_CMDSTREAM_ADDR,  
	RD_PARAM,       
	RD_FLUSH,       
	RD_PROGRAM,     
	RD_VERT_SHADER,
	RD_FRAG_SHADER,
	RD_BUFFER_CONTENTS,
	RD_GPU_ID,
	RD_CHIP_ID,
};

#define BUF_SZ 512   

 
#define circ_count(circ) \
	(CIRC_CNT((circ)->head, (circ)->tail, BUF_SZ))
#define circ_count_to_end(circ) \
	(CIRC_CNT_TO_END((circ)->head, (circ)->tail, BUF_SZ))
 
#define circ_space(circ) \
	(CIRC_SPACE((circ)->head, (circ)->tail, BUF_SZ))
#define circ_space_to_end(circ) \
	(CIRC_SPACE_TO_END((circ)->head, (circ)->tail, BUF_SZ))

struct msm_rd_state {
	struct drm_device *dev;

	bool open;

	 
	struct mutex read_lock, write_lock;

	wait_queue_head_t fifo_event;
	struct circ_buf fifo;

	char buf[BUF_SZ];
};

static void rd_write(struct msm_rd_state *rd, const void *buf, int sz)
{
	struct circ_buf *fifo = &rd->fifo;
	const char *ptr = buf;

	while (sz > 0) {
		char *fptr = &fifo->buf[fifo->head];
		int n;

		wait_event(rd->fifo_event, circ_space(&rd->fifo) > 0 || !rd->open);
		if (!rd->open)
			return;

		 
		n = min(sz, circ_space_to_end(&rd->fifo));
		memcpy(fptr, ptr, n);

		smp_store_release(&fifo->head, (fifo->head + n) & (BUF_SZ - 1));
		sz  -= n;
		ptr += n;

		wake_up_all(&rd->fifo_event);
	}
}

static void rd_write_section(struct msm_rd_state *rd,
		enum rd_sect_type type, const void *buf, int sz)
{
	rd_write(rd, &type, 4);
	rd_write(rd, &sz, 4);
	rd_write(rd, buf, sz);
}

static ssize_t rd_read(struct file *file, char __user *buf,
		size_t sz, loff_t *ppos)
{
	struct msm_rd_state *rd = file->private_data;
	struct circ_buf *fifo = &rd->fifo;
	const char *fptr = &fifo->buf[fifo->tail];
	int n = 0, ret = 0;

	mutex_lock(&rd->read_lock);

	ret = wait_event_interruptible(rd->fifo_event,
			circ_count(&rd->fifo) > 0);
	if (ret)
		goto out;

	 
	n = min_t(int, sz, circ_count_to_end(&rd->fifo));
	if (copy_to_user(buf, fptr, n)) {
		ret = -EFAULT;
		goto out;
	}

	smp_store_release(&fifo->tail, (fifo->tail + n) & (BUF_SZ - 1));
	*ppos += n;

	wake_up_all(&rd->fifo_event);

out:
	mutex_unlock(&rd->read_lock);
	if (ret)
		return ret;
	return n;
}

static int rd_open(struct inode *inode, struct file *file)
{
	struct msm_rd_state *rd = inode->i_private;
	struct drm_device *dev = rd->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_gpu *gpu = priv->gpu;
	uint64_t val;
	uint32_t gpu_id;
	uint32_t zero = 0;
	int ret = 0;

	if (!gpu)
		return -ENODEV;

	mutex_lock(&gpu->lock);

	if (rd->open) {
		ret = -EBUSY;
		goto out;
	}

	file->private_data = rd;
	rd->open = true;

	 
	rd->fifo.head = rd->fifo.tail = 0;

	 
	gpu->funcs->get_param(gpu, NULL, MSM_PARAM_GPU_ID, &val, &zero);
	gpu_id = val;

	rd_write_section(rd, RD_GPU_ID, &gpu_id, sizeof(gpu_id));

	gpu->funcs->get_param(gpu, NULL, MSM_PARAM_CHIP_ID, &val, &zero);
	rd_write_section(rd, RD_CHIP_ID, &val, sizeof(val));

out:
	mutex_unlock(&gpu->lock);
	return ret;
}

static int rd_release(struct inode *inode, struct file *file)
{
	struct msm_rd_state *rd = inode->i_private;

	rd->open = false;
	wake_up_all(&rd->fifo_event);

	return 0;
}


static const struct file_operations rd_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = rd_open,
	.read = rd_read,
	.llseek = no_llseek,
	.release = rd_release,
};


static void rd_cleanup(struct msm_rd_state *rd)
{
	if (!rd)
		return;

	mutex_destroy(&rd->read_lock);
	mutex_destroy(&rd->write_lock);
	kfree(rd);
}

static struct msm_rd_state *rd_init(struct drm_minor *minor, const char *name)
{
	struct msm_rd_state *rd;

	rd = kzalloc(sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return ERR_PTR(-ENOMEM);

	rd->dev = minor->dev;
	rd->fifo.buf = rd->buf;

	mutex_init(&rd->read_lock);
	mutex_init(&rd->write_lock);

	init_waitqueue_head(&rd->fifo_event);

	debugfs_create_file(name, S_IFREG | S_IRUGO, minor->debugfs_root, rd,
			    &rd_debugfs_fops);

	return rd;
}

int msm_rd_debugfs_init(struct drm_minor *minor)
{
	struct msm_drm_private *priv = minor->dev->dev_private;
	struct msm_rd_state *rd;
	int ret;

	 
	if (priv->rd)
		return 0;

	rd = rd_init(minor, "rd");
	if (IS_ERR(rd)) {
		ret = PTR_ERR(rd);
		goto fail;
	}

	priv->rd = rd;

	rd = rd_init(minor, "hangrd");
	if (IS_ERR(rd)) {
		ret = PTR_ERR(rd);
		goto fail;
	}

	priv->hangrd = rd;

	return 0;

fail:
	msm_rd_debugfs_cleanup(priv);
	return ret;
}

void msm_rd_debugfs_cleanup(struct msm_drm_private *priv)
{
	rd_cleanup(priv->rd);
	priv->rd = NULL;

	rd_cleanup(priv->hangrd);
	priv->hangrd = NULL;
}

static void snapshot_buf(struct msm_rd_state *rd,
		struct msm_gem_submit *submit, int idx,
		uint64_t iova, uint32_t size, bool full)
{
	struct drm_gem_object *obj = submit->bos[idx].obj;
	unsigned offset = 0;
	const char *buf;

	if (iova) {
		offset = iova - submit->bos[idx].iova;
	} else {
		iova = submit->bos[idx].iova;
		size = obj->size;
	}

	 
	rd_write_section(rd, RD_GPUADDR,
			(uint32_t[3]){ iova, size, iova >> 32 }, 12);

	if (!full)
		return;

	 
	if (!(submit->bos[idx].flags & MSM_SUBMIT_BO_READ))
		return;

	buf = msm_gem_get_vaddr_active(obj);
	if (IS_ERR(buf))
		return;

	buf += offset;

	rd_write_section(rd, RD_BUFFER_CONTENTS, buf, size);

	msm_gem_put_vaddr_locked(obj);
}

 
void msm_rd_dump_submit(struct msm_rd_state *rd, struct msm_gem_submit *submit,
		const char *fmt, ...)
{
	struct task_struct *task;
	char msg[256];
	int i, n;

	if (!rd->open)
		return;

	mutex_lock(&rd->write_lock);

	if (fmt) {
		va_list args;

		va_start(args, fmt);
		n = vscnprintf(msg, sizeof(msg), fmt, args);
		va_end(args);

		rd_write_section(rd, RD_CMD, msg, ALIGN(n, 4));
	}

	rcu_read_lock();
	task = pid_task(submit->pid, PIDTYPE_PID);
	if (task) {
		n = scnprintf(msg, sizeof(msg), "%.*s/%d: fence=%u",
				TASK_COMM_LEN, task->comm,
				pid_nr(submit->pid), submit->seqno);
	} else {
		n = scnprintf(msg, sizeof(msg), "???/%d: fence=%u",
				pid_nr(submit->pid), submit->seqno);
	}
	rcu_read_unlock();

	rd_write_section(rd, RD_CMD, msg, ALIGN(n, 4));

	for (i = 0; i < submit->nr_bos; i++)
		snapshot_buf(rd, submit, i, 0, 0, should_dump(submit, i));

	for (i = 0; i < submit->nr_cmds; i++) {
		uint32_t szd  = submit->cmd[i].size;  

		 
		if (!should_dump(submit, i)) {
			snapshot_buf(rd, submit, submit->cmd[i].idx,
					submit->cmd[i].iova, szd * 4, true);
		}
	}

	for (i = 0; i < submit->nr_cmds; i++) {
		uint64_t iova = submit->cmd[i].iova;
		uint32_t szd  = submit->cmd[i].size;  

		switch (submit->cmd[i].type) {
		case MSM_SUBMIT_CMD_IB_TARGET_BUF:
			 
			break;
		case MSM_SUBMIT_CMD_CTX_RESTORE_BUF:
		case MSM_SUBMIT_CMD_BUF:
			rd_write_section(rd, RD_CMDSTREAM_ADDR,
				(uint32_t[3]){ iova, szd, iova >> 32 }, 12);
			break;
		}
	}

	mutex_unlock(&rd->write_lock);
}
#endif
