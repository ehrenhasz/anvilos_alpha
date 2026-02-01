
 

#include "hfi.h"
#include "affinity.h"
#include "sdma.h"
#include "netdev.h"

 
int msix_initialize(struct hfi1_devdata *dd)
{
	u32 total;
	int ret;
	struct hfi1_msix_entry *entries;

	 
	total = 1 + dd->num_sdma + dd->n_krcv_queues + dd->num_netdev_contexts;

	if (total >= CCE_NUM_MSIX_VECTORS)
		return -EINVAL;

	ret = pci_alloc_irq_vectors(dd->pcidev, total, total, PCI_IRQ_MSIX);
	if (ret < 0) {
		dd_dev_err(dd, "pci_alloc_irq_vectors() failed: %d\n", ret);
		return ret;
	}

	entries = kcalloc(total, sizeof(*dd->msix_info.msix_entries),
			  GFP_KERNEL);
	if (!entries) {
		pci_free_irq_vectors(dd->pcidev);
		return -ENOMEM;
	}

	dd->msix_info.msix_entries = entries;
	spin_lock_init(&dd->msix_info.msix_lock);
	bitmap_zero(dd->msix_info.in_use_msix, total);
	dd->msix_info.max_requested = total;
	dd_dev_info(dd, "%u MSI-X interrupts allocated\n", total);

	return 0;
}

 
static int msix_request_irq(struct hfi1_devdata *dd, void *arg,
			    irq_handler_t handler, irq_handler_t thread,
			    enum irq_type type, const char *name)
{
	unsigned long nr;
	int irq;
	int ret;
	struct hfi1_msix_entry *me;

	 
	spin_lock(&dd->msix_info.msix_lock);
	nr = find_first_zero_bit(dd->msix_info.in_use_msix,
				 dd->msix_info.max_requested);
	if (nr < dd->msix_info.max_requested)
		__set_bit(nr, dd->msix_info.in_use_msix);
	spin_unlock(&dd->msix_info.msix_lock);

	if (nr == dd->msix_info.max_requested)
		return -ENOSPC;

	if (type < IRQ_SDMA || type >= IRQ_OTHER)
		return -EINVAL;

	irq = pci_irq_vector(dd->pcidev, nr);
	ret = pci_request_irq(dd->pcidev, nr, handler, thread, arg, name);
	if (ret) {
		dd_dev_err(dd,
			   "%s: request for IRQ %d failed, MSIx %lx, err %d\n",
			   name, irq, nr, ret);
		spin_lock(&dd->msix_info.msix_lock);
		__clear_bit(nr, dd->msix_info.in_use_msix);
		spin_unlock(&dd->msix_info.msix_lock);
		return ret;
	}

	 
	me = &dd->msix_info.msix_entries[nr];
	me->irq = irq;
	me->arg = arg;
	me->type = type;

	 
	ret = hfi1_get_irq_affinity(dd, me);
	if (ret)
		dd_dev_err(dd, "%s: unable to pin IRQ %d\n", name, ret);

	return nr;
}

static int msix_request_rcd_irq_common(struct hfi1_ctxtdata *rcd,
				       irq_handler_t handler,
				       irq_handler_t thread,
				       const char *name)
{
	int nr = msix_request_irq(rcd->dd, rcd, handler, thread,
				  rcd->is_vnic ? IRQ_NETDEVCTXT : IRQ_RCVCTXT,
				  name);
	if (nr < 0)
		return nr;

	 
	rcd->ireg = (IS_RCVAVAIL_START + rcd->ctxt) / 64;
	rcd->imask = ((u64)1) << ((IS_RCVAVAIL_START + rcd->ctxt) % 64);
	rcd->msix_intr = nr;
	remap_intr(rcd->dd, IS_RCVAVAIL_START + rcd->ctxt, nr);

	return 0;
}

 
int msix_request_rcd_irq(struct hfi1_ctxtdata *rcd)
{
	char name[MAX_NAME_SIZE];

	snprintf(name, sizeof(name), DRIVER_NAME "_%d kctxt%d",
		 rcd->dd->unit, rcd->ctxt);

	return msix_request_rcd_irq_common(rcd, receive_context_interrupt,
					   receive_context_thread, name);
}

 
int msix_netdev_request_rcd_irq(struct hfi1_ctxtdata *rcd)
{
	char name[MAX_NAME_SIZE];

	snprintf(name, sizeof(name), DRIVER_NAME "_%d nd kctxt%d",
		 rcd->dd->unit, rcd->ctxt);
	return msix_request_rcd_irq_common(rcd, receive_context_interrupt_napi,
					   NULL, name);
}

 
int msix_request_sdma_irq(struct sdma_engine *sde)
{
	int nr;
	char name[MAX_NAME_SIZE];

	snprintf(name, sizeof(name), DRIVER_NAME "_%d sdma%d",
		 sde->dd->unit, sde->this_idx);
	nr = msix_request_irq(sde->dd, sde, sdma_interrupt, NULL,
			      IRQ_SDMA, name);
	if (nr < 0)
		return nr;
	sde->msix_intr = nr;
	remap_sdma_interrupts(sde->dd, sde->this_idx, nr);

	return 0;
}

 
int msix_request_general_irq(struct hfi1_devdata *dd)
{
	int nr;
	char name[MAX_NAME_SIZE];

	snprintf(name, sizeof(name), DRIVER_NAME "_%d", dd->unit);
	nr = msix_request_irq(dd, dd, general_interrupt, NULL, IRQ_GENERAL,
			      name);
	if (nr < 0)
		return nr;

	 
	if (nr) {
		msix_free_irq(dd, (u8)nr);
		dd_dev_err(dd, "Invalid index %d for GENERAL IRQ\n", nr);
		return -EINVAL;
	}

	return 0;
}

 
static void enable_sdma_srcs(struct hfi1_devdata *dd, int i)
{
	set_intr_bits(dd, IS_SDMA_START + i, IS_SDMA_START + i, true);
	set_intr_bits(dd, IS_SDMA_PROGRESS_START + i,
		      IS_SDMA_PROGRESS_START + i, true);
	set_intr_bits(dd, IS_SDMA_IDLE_START + i, IS_SDMA_IDLE_START + i, true);
	set_intr_bits(dd, IS_SDMAENG_ERR_START + i, IS_SDMAENG_ERR_START + i,
		      true);
}

 
int msix_request_irqs(struct hfi1_devdata *dd)
{
	int i;
	int ret = msix_request_general_irq(dd);

	if (ret)
		return ret;

	for (i = 0; i < dd->num_sdma; i++) {
		struct sdma_engine *sde = &dd->per_sdma[i];

		ret = msix_request_sdma_irq(sde);
		if (ret)
			return ret;
		enable_sdma_srcs(sde->dd, i);
	}

	for (i = 0; i < dd->n_krcv_queues; i++) {
		struct hfi1_ctxtdata *rcd = hfi1_rcd_get_by_index_safe(dd, i);

		if (rcd)
			ret = msix_request_rcd_irq(rcd);
		hfi1_rcd_put(rcd);
		if (ret)
			return ret;
	}

	return 0;
}

 
void msix_free_irq(struct hfi1_devdata *dd, u8 msix_intr)
{
	struct hfi1_msix_entry *me;

	if (msix_intr >= dd->msix_info.max_requested)
		return;

	me = &dd->msix_info.msix_entries[msix_intr];

	if (!me->arg)  
		return;

	hfi1_put_irq_affinity(dd, me);
	pci_free_irq(dd->pcidev, msix_intr, me->arg);

	me->arg = NULL;

	spin_lock(&dd->msix_info.msix_lock);
	__clear_bit(msix_intr, dd->msix_info.in_use_msix);
	spin_unlock(&dd->msix_info.msix_lock);
}

 
void msix_clean_up_interrupts(struct hfi1_devdata *dd)
{
	int i;
	struct hfi1_msix_entry *me = dd->msix_info.msix_entries;

	 
	for (i = 0; i < dd->msix_info.max_requested; i++, me++)
		msix_free_irq(dd, i);

	 
	kfree(dd->msix_info.msix_entries);
	dd->msix_info.msix_entries = NULL;
	dd->msix_info.max_requested = 0;

	pci_free_irq_vectors(dd->pcidev);
}

 
void msix_netdev_synchronize_irq(struct hfi1_devdata *dd)
{
	int i;
	int ctxt_count = hfi1_netdev_ctxt_count(dd);

	for (i = 0; i < ctxt_count; i++) {
		struct hfi1_ctxtdata *rcd = hfi1_netdev_get_ctxt(dd, i);
		struct hfi1_msix_entry *me;

		me = &dd->msix_info.msix_entries[rcd->msix_intr];

		synchronize_irq(me->irq);
	}
}
