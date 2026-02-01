



#include <asm/barrier.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/time64.h>
#include <linux/remoteproc/mtk_scp.h>

#include "mtk_common.h"

#define SCP_TIMEOUT_US		(2000 * USEC_PER_MSEC)

 
int scp_ipi_register(struct mtk_scp *scp,
		     u32 id,
		     scp_ipi_handler_t handler,
		     void *priv)
{
	if (!scp)
		return -EPROBE_DEFER;

	if (WARN_ON(id >= SCP_IPI_MAX) || WARN_ON(handler == NULL))
		return -EINVAL;

	scp_ipi_lock(scp, id);
	scp->ipi_desc[id].handler = handler;
	scp->ipi_desc[id].priv = priv;
	scp_ipi_unlock(scp, id);

	return 0;
}
EXPORT_SYMBOL_GPL(scp_ipi_register);

 
void scp_ipi_unregister(struct mtk_scp *scp, u32 id)
{
	if (!scp)
		return;

	if (WARN_ON(id >= SCP_IPI_MAX))
		return;

	scp_ipi_lock(scp, id);
	scp->ipi_desc[id].handler = NULL;
	scp->ipi_desc[id].priv = NULL;
	scp_ipi_unlock(scp, id);
}
EXPORT_SYMBOL_GPL(scp_ipi_unregister);

 
void scp_memcpy_aligned(void __iomem *dst, const void *src, unsigned int len)
{
	void __iomem *ptr;
	u32 val;
	unsigned int i = 0, remain;

	if (!IS_ALIGNED((unsigned long)dst, 4)) {
		ptr = (void __iomem *)ALIGN_DOWN((unsigned long)dst, 4);
		i = 4 - (dst - ptr);
		val = readl_relaxed(ptr);
		memcpy((u8 *)&val + (4 - i), src, i);
		writel_relaxed(val, ptr);
	}

	__iowrite32_copy(dst + i, src + i, (len - i) / 4);
	remain = (len - i) % 4;

	if (remain > 0) {
		val = readl_relaxed(dst + len - remain);
		memcpy(&val, src + len - remain, remain);
		writel_relaxed(val, dst + len - remain);
	}
}
EXPORT_SYMBOL_GPL(scp_memcpy_aligned);

 
void scp_ipi_lock(struct mtk_scp *scp, u32 id)
{
	if (WARN_ON(id >= SCP_IPI_MAX))
		return;
	mutex_lock(&scp->ipi_desc[id].lock);
}
EXPORT_SYMBOL_GPL(scp_ipi_lock);

 
void scp_ipi_unlock(struct mtk_scp *scp, u32 id)
{
	if (WARN_ON(id >= SCP_IPI_MAX))
		return;
	mutex_unlock(&scp->ipi_desc[id].lock);
}
EXPORT_SYMBOL_GPL(scp_ipi_unlock);

 
int scp_ipi_send(struct mtk_scp *scp, u32 id, void *buf, unsigned int len,
		 unsigned int wait)
{
	struct mtk_share_obj __iomem *send_obj = scp->send_buf;
	u32 val;
	int ret;

	if (WARN_ON(id <= SCP_IPI_INIT) || WARN_ON(id >= SCP_IPI_MAX) ||
	    WARN_ON(id == SCP_IPI_NS_SERVICE) ||
	    WARN_ON(len > sizeof(send_obj->share_buf)) || WARN_ON(!buf))
		return -EINVAL;

	ret = clk_prepare_enable(scp->clk);
	if (ret) {
		dev_err(scp->dev, "failed to enable clock\n");
		return ret;
	}

	mutex_lock(&scp->send_lock);

	  
	ret = readl_poll_timeout_atomic(scp->reg_base + scp->data->host_to_scp_reg,
					val, !val, 0, SCP_TIMEOUT_US);
	if (ret) {
		dev_err(scp->dev, "%s: IPI timeout!\n", __func__);
		goto unlock_mutex;
	}

	scp_memcpy_aligned(send_obj->share_buf, buf, len);

	writel(len, &send_obj->len);
	writel(id, &send_obj->id);

	scp->ipi_id_ack[id] = false;
	 
	writel(scp->data->host_to_scp_int_bit,
	       scp->reg_base + scp->data->host_to_scp_reg);

	if (wait) {
		 
		ret = wait_event_timeout(scp->ack_wq,
					 scp->ipi_id_ack[id],
					 msecs_to_jiffies(wait));
		scp->ipi_id_ack[id] = false;
		if (WARN(!ret, "scp ipi %d ack time out !", id))
			ret = -EIO;
		else
			ret = 0;
	}

unlock_mutex:
	mutex_unlock(&scp->send_lock);
	clk_disable_unprepare(scp->clk);

	return ret;
}
EXPORT_SYMBOL_GPL(scp_ipi_send);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek scp IPI interface");
