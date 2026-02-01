
 

#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "compat.h"
#include "ctrl.h"
#include "regs.h"
#include "jr.h"
#include "desc.h"
#include "intern.h"

struct jr_driver_data {
	 
	struct list_head	jr_list;
	spinlock_t		jr_alloc_lock;	 
} ____cacheline_aligned;

static struct jr_driver_data driver_data;
static DEFINE_MUTEX(algs_lock);
static unsigned int active_devs;

static void register_algs(struct caam_drv_private_jr *jrpriv,
			  struct device *dev)
{
	mutex_lock(&algs_lock);

	if (++active_devs != 1)
		goto algs_unlock;

	caam_algapi_init(dev);
	caam_algapi_hash_init(dev);
	caam_pkc_init(dev);
	jrpriv->hwrng = !caam_rng_init(dev);
	caam_prng_register(dev);
	caam_qi_algapi_init(dev);

algs_unlock:
	mutex_unlock(&algs_lock);
}

static void unregister_algs(void)
{
	mutex_lock(&algs_lock);

	if (--active_devs != 0)
		goto algs_unlock;

	caam_qi_algapi_exit();
	caam_prng_unregister(NULL);
	caam_pkc_exit();
	caam_algapi_hash_exit();
	caam_algapi_exit();

algs_unlock:
	mutex_unlock(&algs_lock);
}

static void caam_jr_crypto_engine_exit(void *data)
{
	struct device *jrdev = data;
	struct caam_drv_private_jr *jrpriv = dev_get_drvdata(jrdev);

	 
	crypto_engine_exit(jrpriv->engine);
}

 
static int caam_jr_stop_processing(struct device *dev, u32 jrcr_bits)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	unsigned int timeout = 100000;

	 
	if (rd_reg32(&jrp->rregs->jrintstatus) & JRINT_ERR_HALT_INPROGRESS)
		goto wait_quiesce_completion;

	 
	clrsetbits_32(&jrp->rregs->jrintstatus, JRINT_ERR_HALT_MASK, 0);

	 
	wr_reg32(&jrp->rregs->jrcommand, jrcr_bits);

wait_quiesce_completion:
	while (((rd_reg32(&jrp->rregs->jrintstatus) & JRINT_ERR_HALT_MASK) ==
		JRINT_ERR_HALT_INPROGRESS) && --timeout)
		cpu_relax();

	if ((rd_reg32(&jrp->rregs->jrintstatus) & JRINT_ERR_HALT_MASK) !=
	    JRINT_ERR_HALT_COMPLETE || timeout == 0) {
		dev_err(dev, "failed to flush job ring %d\n", jrp->ridx);
		return -EIO;
	}

	return 0;
}

 
static int caam_jr_flush(struct device *dev)
{
	return caam_jr_stop_processing(dev, JRCR_RESET);
}

 
static int caam_jr_restart_processing(struct device *dev)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	u32 halt_status = rd_reg32(&jrp->rregs->jrintstatus) &
			  JRINT_ERR_HALT_MASK;

	 
	if (halt_status != JRINT_ERR_HALT_COMPLETE)
		return -1;

	 
	clrsetbits_32(&jrp->rregs->jrintstatus, 0, JRINT_ERR_HALT_COMPLETE);

	return 0;
}

static int caam_reset_hw_jr(struct device *dev)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	unsigned int timeout = 100000;
	int err;
	 
	clrsetbits_32(&jrp->rregs->rconfig_lo, 0, JRCFG_IMSK);
	err = caam_jr_flush(dev);
	if (err)
		return err;

	 
	wr_reg32(&jrp->rregs->jrcommand, JRCR_RESET);
	while ((rd_reg32(&jrp->rregs->jrcommand) & JRCR_RESET) && --timeout)
		cpu_relax();

	if (timeout == 0) {
		dev_err(dev, "failed to reset job ring %d\n", jrp->ridx);
		return -EIO;
	}

	 
	clrsetbits_32(&jrp->rregs->rconfig_lo, JRCFG_IMSK, 0);

	return 0;
}

 
static int caam_jr_shutdown(struct device *dev)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	int ret;

	ret = caam_reset_hw_jr(dev);

	tasklet_kill(&jrp->irqtask);

	return ret;
}

static int caam_jr_remove(struct platform_device *pdev)
{
	int ret;
	struct device *jrdev;
	struct caam_drv_private_jr *jrpriv;

	jrdev = &pdev->dev;
	jrpriv = dev_get_drvdata(jrdev);

	if (jrpriv->hwrng)
		caam_rng_exit(jrdev->parent);

	 
	if (atomic_read(&jrpriv->tfm_count)) {
		dev_err(jrdev, "Device is busy\n");
		return -EBUSY;
	}

	 
	unregister_algs();

	 
	spin_lock(&driver_data.jr_alloc_lock);
	list_del(&jrpriv->list_node);
	spin_unlock(&driver_data.jr_alloc_lock);

	 
	ret = caam_jr_shutdown(jrdev);
	if (ret)
		dev_err(jrdev, "Failed to shut down job ring\n");

	return ret;
}

static void caam_jr_platform_shutdown(struct platform_device *pdev)
{
	caam_jr_remove(pdev);
}

 
static irqreturn_t caam_jr_interrupt(int irq, void *st_dev)
{
	struct device *dev = st_dev;
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	u32 irqstate;

	 
	irqstate = rd_reg32(&jrp->rregs->jrintstatus);
	if (!(irqstate & JRINT_JR_INT))
		return IRQ_NONE;

	 
	if (irqstate & JRINT_JR_ERROR) {
		dev_err(dev, "job ring error: irqstate: %08x\n", irqstate);
		BUG();
	}

	 
	clrsetbits_32(&jrp->rregs->rconfig_lo, 0, JRCFG_IMSK);

	 
	wr_reg32(&jrp->rregs->jrintstatus, irqstate);

	preempt_disable();
	tasklet_schedule(&jrp->irqtask);
	preempt_enable();

	return IRQ_HANDLED;
}

 
static void caam_jr_dequeue(unsigned long devarg)
{
	int hw_idx, sw_idx, i, head, tail;
	struct caam_jr_dequeue_params *params = (void *)devarg;
	struct device *dev = params->dev;
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	void (*usercall)(struct device *dev, u32 *desc, u32 status, void *arg);
	u32 *userdesc, userstatus;
	void *userarg;
	u32 outring_used = 0;

	while (outring_used ||
	       (outring_used = rd_reg32(&jrp->rregs->outring_used))) {

		head = READ_ONCE(jrp->head);

		sw_idx = tail = jrp->tail;
		hw_idx = jrp->out_ring_read_index;

		for (i = 0; CIRC_CNT(head, tail + i, JOBR_DEPTH) >= 1; i++) {
			sw_idx = (tail + i) & (JOBR_DEPTH - 1);

			if (jr_outentry_desc(jrp->outring, hw_idx) ==
			    caam_dma_to_cpu(jrp->entinfo[sw_idx].desc_addr_dma))
				break;  
		}
		 
		BUG_ON(CIRC_CNT(head, tail + i, JOBR_DEPTH) <= 0);

		 
		dma_unmap_single(dev,
				 caam_dma_to_cpu(jr_outentry_desc(jrp->outring,
								  hw_idx)),
				 jrp->entinfo[sw_idx].desc_size,
				 DMA_TO_DEVICE);

		 
		jrp->entinfo[sw_idx].desc_addr_dma = 0;

		 
		usercall = jrp->entinfo[sw_idx].callbk;
		userarg = jrp->entinfo[sw_idx].cbkarg;
		userdesc = jrp->entinfo[sw_idx].desc_addr_virt;
		userstatus = caam32_to_cpu(jr_outentry_jrstatus(jrp->outring,
								hw_idx));

		 
		mb();

		 
		wr_reg32(&jrp->rregs->outring_rmvd, 1);

		jrp->out_ring_read_index = (jrp->out_ring_read_index + 1) &
					   (JOBR_DEPTH - 1);

		 
		if (sw_idx == tail) {
			do {
				tail = (tail + 1) & (JOBR_DEPTH - 1);
			} while (CIRC_CNT(head, tail, JOBR_DEPTH) >= 1 &&
				 jrp->entinfo[tail].desc_addr_dma == 0);

			jrp->tail = tail;
		}

		 
		usercall(dev, userdesc, userstatus, userarg);
		outring_used--;
	}

	if (params->enable_itr)
		 
		clrsetbits_32(&jrp->rregs->rconfig_lo, JRCFG_IMSK, 0);
}

 
struct device *caam_jr_alloc(void)
{
	struct caam_drv_private_jr *jrpriv, *min_jrpriv = NULL;
	struct device *dev = ERR_PTR(-ENODEV);
	int min_tfm_cnt	= INT_MAX;
	int tfm_cnt;

	spin_lock(&driver_data.jr_alloc_lock);

	if (list_empty(&driver_data.jr_list)) {
		spin_unlock(&driver_data.jr_alloc_lock);
		return ERR_PTR(-ENODEV);
	}

	list_for_each_entry(jrpriv, &driver_data.jr_list, list_node) {
		tfm_cnt = atomic_read(&jrpriv->tfm_count);
		if (tfm_cnt < min_tfm_cnt) {
			min_tfm_cnt = tfm_cnt;
			min_jrpriv = jrpriv;
		}
		if (!min_tfm_cnt)
			break;
	}

	if (min_jrpriv) {
		atomic_inc(&min_jrpriv->tfm_count);
		dev = min_jrpriv->dev;
	}
	spin_unlock(&driver_data.jr_alloc_lock);

	return dev;
}
EXPORT_SYMBOL(caam_jr_alloc);

 
void caam_jr_free(struct device *rdev)
{
	struct caam_drv_private_jr *jrpriv = dev_get_drvdata(rdev);

	atomic_dec(&jrpriv->tfm_count);
}
EXPORT_SYMBOL(caam_jr_free);

 
int caam_jr_enqueue(struct device *dev, u32 *desc,
		    void (*cbk)(struct device *dev, u32 *desc,
				u32 status, void *areq),
		    void *areq)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);
	struct caam_jrentry_info *head_entry;
	int head, tail, desc_size;
	dma_addr_t desc_dma;

	desc_size = (caam32_to_cpu(*desc) & HDR_JD_LENGTH_MASK) * sizeof(u32);
	desc_dma = dma_map_single(dev, desc, desc_size, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, desc_dma)) {
		dev_err(dev, "caam_jr_enqueue(): can't map jobdesc\n");
		return -EIO;
	}

	spin_lock_bh(&jrp->inplock);

	head = jrp->head;
	tail = READ_ONCE(jrp->tail);

	if (!jrp->inpring_avail ||
	    CIRC_SPACE(head, tail, JOBR_DEPTH) <= 0) {
		spin_unlock_bh(&jrp->inplock);
		dma_unmap_single(dev, desc_dma, desc_size, DMA_TO_DEVICE);
		return -ENOSPC;
	}

	head_entry = &jrp->entinfo[head];
	head_entry->desc_addr_virt = desc;
	head_entry->desc_size = desc_size;
	head_entry->callbk = (void *)cbk;
	head_entry->cbkarg = areq;
	head_entry->desc_addr_dma = desc_dma;

	jr_inpentry_set(jrp->inpring, head, cpu_to_caam_dma(desc_dma));

	 
	wmb();

	jrp->head = (head + 1) & (JOBR_DEPTH - 1);

	 

	wr_reg32(&jrp->rregs->inpring_jobadd, 1);

	jrp->inpring_avail--;
	if (!jrp->inpring_avail)
		jrp->inpring_avail = rd_reg32(&jrp->rregs->inpring_avail);

	spin_unlock_bh(&jrp->inplock);

	return -EINPROGRESS;
}
EXPORT_SYMBOL(caam_jr_enqueue);

static void caam_jr_init_hw(struct device *dev, dma_addr_t inpbusaddr,
			    dma_addr_t outbusaddr)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);

	wr_reg64(&jrp->rregs->inpring_base, inpbusaddr);
	wr_reg64(&jrp->rregs->outring_base, outbusaddr);
	wr_reg32(&jrp->rregs->inpring_size, JOBR_DEPTH);
	wr_reg32(&jrp->rregs->outring_size, JOBR_DEPTH);

	 
	clrsetbits_32(&jrp->rregs->rconfig_lo, 0, JOBR_INTC |
		      (JOBR_INTC_COUNT_THLD << JRCFG_ICDCT_SHIFT) |
		      (JOBR_INTC_TIME_THLD << JRCFG_ICTT_SHIFT));
}

static void caam_jr_reset_index(struct caam_drv_private_jr *jrp)
{
	jrp->out_ring_read_index = 0;
	jrp->head = 0;
	jrp->tail = 0;
}

 
static int caam_jr_init(struct device *dev)
{
	struct caam_drv_private_jr *jrp;
	dma_addr_t inpbusaddr, outbusaddr;
	int i, error;

	jrp = dev_get_drvdata(dev);

	error = caam_reset_hw_jr(dev);
	if (error)
		return error;

	jrp->inpring = dmam_alloc_coherent(dev, SIZEOF_JR_INPENTRY *
					   JOBR_DEPTH, &inpbusaddr,
					   GFP_KERNEL);
	if (!jrp->inpring)
		return -ENOMEM;

	jrp->outring = dmam_alloc_coherent(dev, SIZEOF_JR_OUTENTRY *
					   JOBR_DEPTH, &outbusaddr,
					   GFP_KERNEL);
	if (!jrp->outring)
		return -ENOMEM;

	jrp->entinfo = devm_kcalloc(dev, JOBR_DEPTH, sizeof(*jrp->entinfo),
				    GFP_KERNEL);
	if (!jrp->entinfo)
		return -ENOMEM;

	for (i = 0; i < JOBR_DEPTH; i++)
		jrp->entinfo[i].desc_addr_dma = !0;

	 
	caam_jr_reset_index(jrp);
	jrp->inpring_avail = JOBR_DEPTH;
	caam_jr_init_hw(dev, inpbusaddr, outbusaddr);

	spin_lock_init(&jrp->inplock);

	jrp->tasklet_params.dev = dev;
	jrp->tasklet_params.enable_itr = 1;
	tasklet_init(&jrp->irqtask, caam_jr_dequeue,
		     (unsigned long)&jrp->tasklet_params);

	 
	error = devm_request_irq(dev, jrp->irq, caam_jr_interrupt, IRQF_SHARED,
				 dev_name(dev), dev);
	if (error) {
		dev_err(dev, "can't connect JobR %d interrupt (%d)\n",
			jrp->ridx, jrp->irq);
		tasklet_kill(&jrp->irqtask);
	}

	return error;
}

static void caam_jr_irq_dispose_mapping(void *data)
{
	irq_dispose_mapping((unsigned long)data);
}

 
static int caam_jr_probe(struct platform_device *pdev)
{
	struct device *jrdev;
	struct device_node *nprop;
	struct caam_job_ring __iomem *ctrl;
	struct caam_drv_private_jr *jrpriv;
	static int total_jobrs;
	struct resource *r;
	int error;

	jrdev = &pdev->dev;
	jrpriv = devm_kzalloc(jrdev, sizeof(*jrpriv), GFP_KERNEL);
	if (!jrpriv)
		return -ENOMEM;

	dev_set_drvdata(jrdev, jrpriv);

	 
	jrpriv->ridx = total_jobrs++;

	nprop = pdev->dev.of_node;
	 
	 
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(jrdev, "platform_get_resource() failed\n");
		return -ENOMEM;
	}

	ctrl = devm_ioremap(jrdev, r->start, resource_size(r));
	if (!ctrl) {
		dev_err(jrdev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	jrpriv->rregs = (struct caam_job_ring __iomem __force *)ctrl;

	error = dma_set_mask_and_coherent(jrdev, caam_get_dma_mask(jrdev));
	if (error) {
		dev_err(jrdev, "dma_set_mask_and_coherent failed (%d)\n",
			error);
		return error;
	}

	 
	jrpriv->engine = crypto_engine_alloc_init_and_set(jrdev, true, NULL,
							  false,
							  CRYPTO_ENGINE_MAX_QLEN);
	if (!jrpriv->engine) {
		dev_err(jrdev, "Could not init crypto-engine\n");
		return -ENOMEM;
	}

	error = devm_add_action_or_reset(jrdev, caam_jr_crypto_engine_exit,
					 jrdev);
	if (error)
		return error;

	 
	error = crypto_engine_start(jrpriv->engine);
	if (error) {
		dev_err(jrdev, "Could not start crypto-engine\n");
		return error;
	}

	 
	jrpriv->irq = irq_of_parse_and_map(nprop, 0);
	if (!jrpriv->irq) {
		dev_err(jrdev, "irq_of_parse_and_map failed\n");
		return -EINVAL;
	}

	error = devm_add_action_or_reset(jrdev, caam_jr_irq_dispose_mapping,
					 (void *)(unsigned long)jrpriv->irq);
	if (error)
		return error;

	 
	error = caam_jr_init(jrdev);  
	if (error)
		return error;

	jrpriv->dev = jrdev;
	spin_lock(&driver_data.jr_alloc_lock);
	list_add_tail(&jrpriv->list_node, &driver_data.jr_list);
	spin_unlock(&driver_data.jr_alloc_lock);

	atomic_set(&jrpriv->tfm_count, 0);

	device_init_wakeup(&pdev->dev, 1);
	device_set_wakeup_enable(&pdev->dev, false);

	register_algs(jrpriv, jrdev->parent);

	return 0;
}

static void caam_jr_get_hw_state(struct device *dev)
{
	struct caam_drv_private_jr *jrp = dev_get_drvdata(dev);

	jrp->state.inpbusaddr = rd_reg64(&jrp->rregs->inpring_base);
	jrp->state.outbusaddr = rd_reg64(&jrp->rregs->outring_base);
}

static int caam_jr_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct caam_drv_private_jr *jrpriv = platform_get_drvdata(pdev);
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev->parent);
	struct caam_jr_dequeue_params suspend_params = {
		.dev = dev,
		.enable_itr = 0,
	};

	 
	spin_lock(&driver_data.jr_alloc_lock);
	list_del(&jrpriv->list_node);
	spin_unlock(&driver_data.jr_alloc_lock);

	if (jrpriv->hwrng)
		caam_rng_exit(dev->parent);

	if (ctrlpriv->caam_off_during_pm) {
		int err;

		tasklet_disable(&jrpriv->irqtask);

		 
		clrsetbits_32(&jrpriv->rregs->rconfig_lo, 0, JRCFG_IMSK);

		 
		err = caam_jr_flush(dev);
		if (err) {
			dev_err(dev, "Failed to flush\n");
			return err;
		}

		 
		caam_jr_dequeue((unsigned long)&suspend_params);

		 
		caam_jr_get_hw_state(dev);
	} else if (device_may_wakeup(&pdev->dev)) {
		enable_irq_wake(jrpriv->irq);
	}

	return 0;
}

static int caam_jr_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct caam_drv_private_jr *jrpriv = platform_get_drvdata(pdev);
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev->parent);

	if (ctrlpriv->caam_off_during_pm) {
		u64 inp_addr;
		int err;

		 
		inp_addr = rd_reg64(&jrpriv->rregs->inpring_base);
		if (inp_addr != 0) {
			 
			if (inp_addr == jrpriv->state.inpbusaddr) {
				 
				err = caam_jr_restart_processing(dev);
				if (err) {
					dev_err(dev,
						"Restart processing failed\n");
					return err;
				}

				tasklet_enable(&jrpriv->irqtask);

				clrsetbits_32(&jrpriv->rregs->rconfig_lo,
					      JRCFG_IMSK, 0);

				goto add_jr;
			} else if (ctrlpriv->optee_en) {
				 
				err = caam_reset_hw_jr(dev);
				if (err) {
					dev_err(dev, "Failed to reset JR\n");
					return err;
				}
			} else {
				 
				return -EIO;
			}
		}

		caam_jr_reset_index(jrpriv);
		caam_jr_init_hw(dev, jrpriv->state.inpbusaddr,
				jrpriv->state.outbusaddr);

		tasklet_enable(&jrpriv->irqtask);
	} else if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(jrpriv->irq);
	}

add_jr:
	spin_lock(&driver_data.jr_alloc_lock);
	list_add_tail(&jrpriv->list_node, &driver_data.jr_list);
	spin_unlock(&driver_data.jr_alloc_lock);

	if (jrpriv->hwrng)
		jrpriv->hwrng = !caam_rng_init(dev->parent);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(caam_jr_pm_ops, caam_jr_suspend, caam_jr_resume);

static const struct of_device_id caam_jr_match[] = {
	{
		.compatible = "fsl,sec-v4.0-job-ring",
	},
	{
		.compatible = "fsl,sec4.0-job-ring",
	},
	{},
};
MODULE_DEVICE_TABLE(of, caam_jr_match);

static struct platform_driver caam_jr_driver = {
	.driver = {
		.name = "caam_jr",
		.of_match_table = caam_jr_match,
		.pm = pm_ptr(&caam_jr_pm_ops),
	},
	.probe       = caam_jr_probe,
	.remove      = caam_jr_remove,
	.shutdown    = caam_jr_platform_shutdown,
};

static int __init jr_driver_init(void)
{
	spin_lock_init(&driver_data.jr_alloc_lock);
	INIT_LIST_HEAD(&driver_data.jr_list);
	return platform_driver_register(&caam_jr_driver);
}

static void __exit jr_driver_exit(void)
{
	platform_driver_unregister(&caam_jr_driver);
}

module_init(jr_driver_init);
module_exit(jr_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FSL CAAM JR request backend");
MODULE_AUTHOR("Freescale Semiconductor - NMG/STC");
