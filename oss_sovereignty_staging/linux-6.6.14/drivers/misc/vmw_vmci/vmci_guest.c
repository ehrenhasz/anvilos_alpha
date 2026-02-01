
 

#include <linux/vmw_vmci_defs.h>
#include <linux/vmw_vmci_api.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/processor.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/vmalloc.h>

#include "vmci_datagram.h"
#include "vmci_doorbell.h"
#include "vmci_context.h"
#include "vmci_driver.h"
#include "vmci_event.h"

#define PCI_DEVICE_ID_VMWARE_VMCI	0x0740

#define VMCI_UTIL_NUM_RESOURCES 1

 
#define VMCI_DMA_DG_BUFFER_SIZE (VMCI_MAX_DG_SIZE + PAGE_SIZE)

static bool vmci_disable_msi;
module_param_named(disable_msi, vmci_disable_msi, bool, 0);
MODULE_PARM_DESC(disable_msi, "Disable MSI use in driver - (default=0)");

static bool vmci_disable_msix;
module_param_named(disable_msix, vmci_disable_msix, bool, 0);
MODULE_PARM_DESC(disable_msix, "Disable MSI-X use in driver - (default=0)");

static u32 ctx_update_sub_id = VMCI_INVALID_ID;
static u32 vm_context_id = VMCI_INVALID_ID;

struct vmci_guest_device {
	struct device *dev;	 
	void __iomem *iobase;
	void __iomem *mmio_base;

	bool exclusive_vectors;

	struct wait_queue_head inout_wq;

	void *data_buffer;
	dma_addr_t data_buffer_base;
	void *tx_buffer;
	dma_addr_t tx_buffer_base;
	void *notification_bitmap;
	dma_addr_t notification_base;
};

static bool use_ppn64;

bool vmci_use_ppn64(void)
{
	return use_ppn64;
}

 
struct pci_dev *vmci_pdev;
static struct vmci_guest_device *vmci_dev_g;
static DEFINE_SPINLOCK(vmci_dev_spinlock);

static atomic_t vmci_num_guest_devices = ATOMIC_INIT(0);

bool vmci_guest_code_active(void)
{
	return atomic_read(&vmci_num_guest_devices) != 0;
}

u32 vmci_get_vm_context_id(void)
{
	if (vm_context_id == VMCI_INVALID_ID) {
		struct vmci_datagram get_cid_msg;
		get_cid_msg.dst =
		    vmci_make_handle(VMCI_HYPERVISOR_CONTEXT_ID,
				     VMCI_GET_CONTEXT_ID);
		get_cid_msg.src = VMCI_ANON_SRC_HANDLE;
		get_cid_msg.payload_size = 0;
		vm_context_id = vmci_send_datagram(&get_cid_msg);
	}
	return vm_context_id;
}

static unsigned int vmci_read_reg(struct vmci_guest_device *dev, u32 reg)
{
	if (dev->mmio_base != NULL)
		return readl(dev->mmio_base + reg);
	return ioread32(dev->iobase + reg);
}

static void vmci_write_reg(struct vmci_guest_device *dev, u32 val, u32 reg)
{
	if (dev->mmio_base != NULL)
		writel(val, dev->mmio_base + reg);
	else
		iowrite32(val, dev->iobase + reg);
}

static void vmci_read_data(struct vmci_guest_device *vmci_dev,
			   void *dest, size_t size)
{
	if (vmci_dev->mmio_base == NULL)
		ioread8_rep(vmci_dev->iobase + VMCI_DATA_IN_ADDR,
			    dest, size);
	else {
		 
		struct vmci_data_in_out_header *buffer_header = vmci_dev->data_buffer;
		struct vmci_sg_elem *sg_array = (struct vmci_sg_elem *)(buffer_header + 1);
		size_t buffer_offset = dest - vmci_dev->data_buffer;

		buffer_header->opcode = 1;
		buffer_header->size = 1;
		buffer_header->busy = 0;
		sg_array[0].addr = vmci_dev->data_buffer_base + buffer_offset;
		sg_array[0].size = size;

		vmci_write_reg(vmci_dev, lower_32_bits(vmci_dev->data_buffer_base),
			       VMCI_DATA_IN_LOW_ADDR);

		wait_event(vmci_dev->inout_wq, buffer_header->busy == 1);
	}
}

static int vmci_write_data(struct vmci_guest_device *dev,
			   struct vmci_datagram *dg)
{
	int result;

	if (dev->mmio_base != NULL) {
		struct vmci_data_in_out_header *buffer_header = dev->tx_buffer;
		u8 *dg_out_buffer = (u8 *)(buffer_header + 1);

		if (VMCI_DG_SIZE(dg) > VMCI_MAX_DG_SIZE)
			return VMCI_ERROR_INVALID_ARGS;

		 
		memcpy(dg_out_buffer, dg, VMCI_DG_SIZE(dg));
		buffer_header->opcode = 0;
		buffer_header->size = VMCI_DG_SIZE(dg);
		buffer_header->busy = 1;

		vmci_write_reg(dev, lower_32_bits(dev->tx_buffer_base),
			       VMCI_DATA_OUT_LOW_ADDR);

		 
		spin_until_cond(buffer_header->busy == 0);

		result = vmci_read_reg(vmci_dev_g, VMCI_RESULT_LOW_ADDR);
		if (result == VMCI_SUCCESS)
			result = (int)buffer_header->result;
	} else {
		iowrite8_rep(dev->iobase + VMCI_DATA_OUT_ADDR,
			     dg, VMCI_DG_SIZE(dg));
		result = vmci_read_reg(vmci_dev_g, VMCI_RESULT_LOW_ADDR);
	}

	return result;
}

 
int vmci_send_datagram(struct vmci_datagram *dg)
{
	unsigned long flags;
	int result;

	 
	if (dg == NULL)
		return VMCI_ERROR_INVALID_ARGS;

	 
	spin_lock_irqsave(&vmci_dev_spinlock, flags);

	if (vmci_dev_g) {
		vmci_write_data(vmci_dev_g, dg);
		result = vmci_read_reg(vmci_dev_g, VMCI_RESULT_LOW_ADDR);
	} else {
		result = VMCI_ERROR_UNAVAILABLE;
	}

	spin_unlock_irqrestore(&vmci_dev_spinlock, flags);

	return result;
}
EXPORT_SYMBOL_GPL(vmci_send_datagram);

 
static void vmci_guest_cid_update(u32 sub_id,
				  const struct vmci_event_data *event_data,
				  void *client_data)
{
	const struct vmci_event_payld_ctx *ev_payload =
				vmci_event_data_const_payload(event_data);

	if (sub_id != ctx_update_sub_id) {
		pr_devel("Invalid subscriber (ID=0x%x)\n", sub_id);
		return;
	}

	if (!event_data || ev_payload->context_id == VMCI_INVALID_ID) {
		pr_devel("Invalid event data\n");
		return;
	}

	pr_devel("Updating context from (ID=0x%x) to (ID=0x%x) on event (type=%d)\n",
		 vm_context_id, ev_payload->context_id, event_data->event);

	vm_context_id = ev_payload->context_id;
}

 
static int vmci_check_host_caps(struct pci_dev *pdev)
{
	bool result;
	struct vmci_resource_query_msg *msg;
	u32 msg_size = sizeof(struct vmci_resource_query_hdr) +
				VMCI_UTIL_NUM_RESOURCES * sizeof(u32);
	struct vmci_datagram *check_msg;

	check_msg = kzalloc(msg_size, GFP_KERNEL);
	if (!check_msg) {
		dev_err(&pdev->dev, "%s: Insufficient memory\n", __func__);
		return -ENOMEM;
	}

	check_msg->dst = vmci_make_handle(VMCI_HYPERVISOR_CONTEXT_ID,
					  VMCI_RESOURCES_QUERY);
	check_msg->src = VMCI_ANON_SRC_HANDLE;
	check_msg->payload_size = msg_size - VMCI_DG_HEADERSIZE;
	msg = (struct vmci_resource_query_msg *)VMCI_DG_PAYLOAD(check_msg);

	msg->num_resources = VMCI_UTIL_NUM_RESOURCES;
	msg->resources[0] = VMCI_GET_CONTEXT_ID;

	 
	result = vmci_send_datagram(check_msg) == 0x01;
	kfree(check_msg);

	dev_dbg(&pdev->dev, "%s: Host capability check: %s\n",
		__func__, result ? "PASSED" : "FAILED");

	 
	return result ? 0 : -ENXIO;
}

 
static void vmci_dispatch_dgs(struct vmci_guest_device *vmci_dev)
{
	u8 *dg_in_buffer = vmci_dev->data_buffer;
	struct vmci_datagram *dg;
	size_t dg_in_buffer_size = VMCI_MAX_DG_SIZE;
	size_t current_dg_in_buffer_size;
	size_t remaining_bytes;
	bool is_io_port = vmci_dev->mmio_base == NULL;

	BUILD_BUG_ON(VMCI_MAX_DG_SIZE < PAGE_SIZE);

	if (!is_io_port) {
		 
		dg_in_buffer += PAGE_SIZE;

		 
		current_dg_in_buffer_size = VMCI_MAX_DG_SIZE;
	} else {
		current_dg_in_buffer_size = PAGE_SIZE;
	}
	vmci_read_data(vmci_dev, dg_in_buffer, current_dg_in_buffer_size);
	dg = (struct vmci_datagram *)dg_in_buffer;
	remaining_bytes = current_dg_in_buffer_size;

	 
	while (dg->dst.resource != VMCI_INVALID_ID ||
	       (is_io_port && remaining_bytes > PAGE_SIZE)) {
		unsigned dg_in_size;

		 
		if (dg->dst.resource == VMCI_INVALID_ID) {
			dg = (struct vmci_datagram *)roundup(
				(uintptr_t)dg + 1, PAGE_SIZE);
			remaining_bytes =
				(size_t)(dg_in_buffer +
					 current_dg_in_buffer_size -
					 (u8 *)dg);
			continue;
		}

		dg_in_size = VMCI_DG_SIZE_ALIGNED(dg);

		if (dg_in_size <= dg_in_buffer_size) {
			int result;

			 
			if (dg_in_size > remaining_bytes) {
				if (remaining_bytes !=
				    current_dg_in_buffer_size) {

					 
					memmove(dg_in_buffer, dg_in_buffer +
						current_dg_in_buffer_size -
						remaining_bytes,
						remaining_bytes);
					dg = (struct vmci_datagram *)
					    dg_in_buffer;
				}

				if (current_dg_in_buffer_size !=
				    dg_in_buffer_size)
					current_dg_in_buffer_size =
					    dg_in_buffer_size;

				vmci_read_data(vmci_dev,
					       dg_in_buffer +
						remaining_bytes,
					       current_dg_in_buffer_size -
						remaining_bytes);
			}

			 
			if (dg->src.context == VMCI_HYPERVISOR_CONTEXT_ID &&
			    dg->dst.resource == VMCI_EVENT_HANDLER) {
				result = vmci_event_dispatch(dg);
			} else {
				result = vmci_datagram_invoke_guest_handler(dg);
			}
			if (result < VMCI_SUCCESS)
				dev_dbg(vmci_dev->dev,
					"Datagram with resource (ID=0x%x) failed (err=%d)\n",
					 dg->dst.resource, result);

			 
			dg = (struct vmci_datagram *)((u8 *)dg +
						      dg_in_size);
		} else {
			size_t bytes_to_skip;

			 
			dev_dbg(vmci_dev->dev,
				"Failed to receive datagram (size=%u bytes)\n",
				 dg_in_size);

			bytes_to_skip = dg_in_size - remaining_bytes;
			if (current_dg_in_buffer_size != dg_in_buffer_size)
				current_dg_in_buffer_size = dg_in_buffer_size;

			for (;;) {
				vmci_read_data(vmci_dev, dg_in_buffer,
					       current_dg_in_buffer_size);
				if (bytes_to_skip <= current_dg_in_buffer_size)
					break;

				bytes_to_skip -= current_dg_in_buffer_size;
			}
			dg = (struct vmci_datagram *)(dg_in_buffer +
						      bytes_to_skip);
		}

		remaining_bytes =
		    (size_t) (dg_in_buffer + current_dg_in_buffer_size -
			      (u8 *)dg);

		if (remaining_bytes < VMCI_DG_HEADERSIZE) {
			 

			vmci_read_data(vmci_dev, dg_in_buffer,
				    current_dg_in_buffer_size);
			dg = (struct vmci_datagram *)dg_in_buffer;
			remaining_bytes = current_dg_in_buffer_size;
		}
	}
}

 
static void vmci_process_bitmap(struct vmci_guest_device *dev)
{
	if (!dev->notification_bitmap) {
		dev_dbg(dev->dev, "No bitmap present in %s\n", __func__);
		return;
	}

	vmci_dbell_scan_notification_entries(dev->notification_bitmap);
}

 
static irqreturn_t vmci_interrupt(int irq, void *_dev)
{
	struct vmci_guest_device *dev = _dev;

	 

	if (dev->exclusive_vectors) {
		vmci_dispatch_dgs(dev);
	} else {
		unsigned int icr;

		 
		icr = vmci_read_reg(dev, VMCI_ICR_ADDR);
		if (icr == 0 || icr == ~0)
			return IRQ_NONE;

		if (icr & VMCI_ICR_DATAGRAM) {
			vmci_dispatch_dgs(dev);
			icr &= ~VMCI_ICR_DATAGRAM;
		}

		if (icr & VMCI_ICR_NOTIFICATION) {
			vmci_process_bitmap(dev);
			icr &= ~VMCI_ICR_NOTIFICATION;
		}


		if (icr & VMCI_ICR_DMA_DATAGRAM) {
			wake_up_all(&dev->inout_wq);
			icr &= ~VMCI_ICR_DMA_DATAGRAM;
		}

		if (icr != 0)
			dev_warn(dev->dev,
				 "Ignoring unknown interrupt cause (%d)\n",
				 icr);
	}

	return IRQ_HANDLED;
}

 
static irqreturn_t vmci_interrupt_bm(int irq, void *_dev)
{
	struct vmci_guest_device *dev = _dev;

	 
	vmci_process_bitmap(dev);

	return IRQ_HANDLED;
}

 
static irqreturn_t vmci_interrupt_dma_datagram(int irq, void *_dev)
{
	struct vmci_guest_device *dev = _dev;

	wake_up_all(&dev->inout_wq);

	return IRQ_HANDLED;
}

static void vmci_free_dg_buffers(struct vmci_guest_device *vmci_dev)
{
	if (vmci_dev->mmio_base != NULL) {
		if (vmci_dev->tx_buffer != NULL)
			dma_free_coherent(vmci_dev->dev,
					  VMCI_DMA_DG_BUFFER_SIZE,
					  vmci_dev->tx_buffer,
					  vmci_dev->tx_buffer_base);
		if (vmci_dev->data_buffer != NULL)
			dma_free_coherent(vmci_dev->dev,
					  VMCI_DMA_DG_BUFFER_SIZE,
					  vmci_dev->data_buffer,
					  vmci_dev->data_buffer_base);
	} else {
		vfree(vmci_dev->data_buffer);
	}
}

 
static int vmci_guest_probe_device(struct pci_dev *pdev,
				   const struct pci_device_id *id)
{
	struct vmci_guest_device *vmci_dev;
	void __iomem *iobase = NULL;
	void __iomem *mmio_base = NULL;
	unsigned int num_irq_vectors;
	unsigned int capabilities;
	unsigned int caps_in_use;
	unsigned long cmd;
	int vmci_err;
	int error;

	dev_dbg(&pdev->dev, "Probing for vmci/PCI guest device\n");

	error = pcim_enable_device(pdev);
	if (error) {
		dev_err(&pdev->dev,
			"Failed to enable VMCI device: %d\n", error);
		return error;
	}

	 

	if (pci_resource_len(pdev, 1) == VMCI_WITH_MMIO_ACCESS_BAR_SIZE) {
		dev_info(&pdev->dev, "MMIO register access is available\n");
		mmio_base = pci_iomap_range(pdev, 1, VMCI_MMIO_ACCESS_OFFSET,
					    VMCI_MMIO_ACCESS_SIZE);
		 
		if (!mmio_base)
			dev_warn(&pdev->dev, "Failed to map MMIO register access\n");
	}

	if (!mmio_base) {
		if (IS_ENABLED(CONFIG_ARM64)) {
			dev_err(&pdev->dev, "MMIO base is invalid\n");
			return -ENXIO;
		}
		error = pcim_iomap_regions(pdev, BIT(0), KBUILD_MODNAME);
		if (error) {
			dev_err(&pdev->dev, "Failed to reserve/map IO regions\n");
			return error;
		}
		iobase = pcim_iomap_table(pdev)[0];
	}

	vmci_dev = devm_kzalloc(&pdev->dev, sizeof(*vmci_dev), GFP_KERNEL);
	if (!vmci_dev) {
		dev_err(&pdev->dev,
			"Can't allocate memory for VMCI device\n");
		return -ENOMEM;
	}

	vmci_dev->dev = &pdev->dev;
	vmci_dev->exclusive_vectors = false;
	vmci_dev->iobase = iobase;
	vmci_dev->mmio_base = mmio_base;

	init_waitqueue_head(&vmci_dev->inout_wq);

	if (mmio_base != NULL) {
		vmci_dev->tx_buffer = dma_alloc_coherent(&pdev->dev, VMCI_DMA_DG_BUFFER_SIZE,
							 &vmci_dev->tx_buffer_base,
							 GFP_KERNEL);
		if (!vmci_dev->tx_buffer) {
			dev_err(&pdev->dev,
				"Can't allocate memory for datagram tx buffer\n");
			return -ENOMEM;
		}

		vmci_dev->data_buffer = dma_alloc_coherent(&pdev->dev, VMCI_DMA_DG_BUFFER_SIZE,
							   &vmci_dev->data_buffer_base,
							   GFP_KERNEL);
	} else {
		vmci_dev->data_buffer = vmalloc(VMCI_MAX_DG_SIZE);
	}
	if (!vmci_dev->data_buffer) {
		dev_err(&pdev->dev,
			"Can't allocate memory for datagram buffer\n");
		error = -ENOMEM;
		goto err_free_data_buffers;
	}

	pci_set_master(pdev);	 

	 
	capabilities = vmci_read_reg(vmci_dev, VMCI_CAPS_ADDR);
	if (!(capabilities & VMCI_CAPS_DATAGRAM)) {
		dev_err(&pdev->dev, "Device does not support datagrams\n");
		error = -ENXIO;
		goto err_free_data_buffers;
	}
	caps_in_use = VMCI_CAPS_DATAGRAM;

	 
	if (capabilities & VMCI_CAPS_PPN64) {
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
		use_ppn64 = true;
		caps_in_use |= VMCI_CAPS_PPN64;
	} else {
		dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(44));
		use_ppn64 = false;
	}

	 
	if (capabilities & VMCI_CAPS_NOTIFICATIONS) {
		vmci_dev->notification_bitmap = dma_alloc_coherent(
			&pdev->dev, PAGE_SIZE, &vmci_dev->notification_base,
			GFP_KERNEL);
		if (!vmci_dev->notification_bitmap)
			dev_warn(&pdev->dev,
				 "Unable to allocate notification bitmap\n");
		else
			caps_in_use |= VMCI_CAPS_NOTIFICATIONS;
	}

	if (mmio_base != NULL) {
		if (capabilities & VMCI_CAPS_DMA_DATAGRAM) {
			caps_in_use |= VMCI_CAPS_DMA_DATAGRAM;
		} else {
			dev_err(&pdev->dev,
				"Missing capability: VMCI_CAPS_DMA_DATAGRAM\n");
			error = -ENXIO;
			goto err_free_notification_bitmap;
		}
	}

	dev_info(&pdev->dev, "Using capabilities 0x%x\n", caps_in_use);

	 
	vmci_write_reg(vmci_dev, caps_in_use, VMCI_CAPS_ADDR);

	if (caps_in_use & VMCI_CAPS_DMA_DATAGRAM) {
		 
		vmci_write_reg(vmci_dev, PAGE_SHIFT, VMCI_GUEST_PAGE_SHIFT);

		 
		vmci_write_reg(vmci_dev, upper_32_bits(vmci_dev->data_buffer_base),
			       VMCI_DATA_IN_HIGH_ADDR);
		vmci_write_reg(vmci_dev, upper_32_bits(vmci_dev->tx_buffer_base),
			       VMCI_DATA_OUT_HIGH_ADDR);
	}

	 
	spin_lock_irq(&vmci_dev_spinlock);
	vmci_dev_g = vmci_dev;
	vmci_pdev = pdev;
	spin_unlock_irq(&vmci_dev_spinlock);

	 
	if (caps_in_use & VMCI_CAPS_NOTIFICATIONS) {
		unsigned long bitmap_ppn =
			vmci_dev->notification_base >> PAGE_SHIFT;
		if (!vmci_dbell_register_notification_bitmap(bitmap_ppn)) {
			dev_warn(&pdev->dev,
				 "VMCI device unable to register notification bitmap with PPN 0x%lx\n",
				 bitmap_ppn);
			error = -ENXIO;
			goto err_remove_vmci_dev_g;
		}
	}

	 
	error = vmci_check_host_caps(pdev);
	if (error)
		goto err_remove_vmci_dev_g;

	 

	 
	vmci_err = vmci_event_subscribe(VMCI_EVENT_CTX_ID_UPDATE,
					vmci_guest_cid_update, NULL,
					&ctx_update_sub_id);
	if (vmci_err < VMCI_SUCCESS)
		dev_warn(&pdev->dev,
			 "Failed to subscribe to event (type=%d): %d\n",
			 VMCI_EVENT_CTX_ID_UPDATE, vmci_err);

	 
	if (vmci_dev->mmio_base != NULL)
		num_irq_vectors = VMCI_MAX_INTRS;
	else
		num_irq_vectors = VMCI_MAX_INTRS_NOTIFICATION;
	error = pci_alloc_irq_vectors(pdev, num_irq_vectors, num_irq_vectors,
				      PCI_IRQ_MSIX);
	if (error < 0) {
		error = pci_alloc_irq_vectors(pdev, 1, 1,
				PCI_IRQ_MSIX | PCI_IRQ_MSI | PCI_IRQ_LEGACY);
		if (error < 0)
			goto err_unsubscribe_event;
	} else {
		vmci_dev->exclusive_vectors = true;
	}

	 
	error = request_threaded_irq(pci_irq_vector(pdev, 0), NULL,
				     vmci_interrupt, IRQF_SHARED,
				     KBUILD_MODNAME, vmci_dev);
	if (error) {
		dev_err(&pdev->dev, "Irq %u in use: %d\n",
			pci_irq_vector(pdev, 0), error);
		goto err_disable_msi;
	}

	 
	if (vmci_dev->exclusive_vectors) {
		error = request_threaded_irq(pci_irq_vector(pdev, 1), NULL,
					     vmci_interrupt_bm, 0,
					     KBUILD_MODNAME, vmci_dev);
		if (error) {
			dev_err(&pdev->dev,
				"Failed to allocate irq %u: %d\n",
				pci_irq_vector(pdev, 1), error);
			goto err_free_irq;
		}
		if (caps_in_use & VMCI_CAPS_DMA_DATAGRAM) {
			error = request_threaded_irq(pci_irq_vector(pdev, 2),
						     NULL,
						    vmci_interrupt_dma_datagram,
						     0, KBUILD_MODNAME,
						     vmci_dev);
			if (error) {
				dev_err(&pdev->dev,
					"Failed to allocate irq %u: %d\n",
					pci_irq_vector(pdev, 2), error);
				goto err_free_bm_irq;
			}
		}
	}

	dev_dbg(&pdev->dev, "Registered device\n");

	atomic_inc(&vmci_num_guest_devices);

	 
	cmd = VMCI_IMR_DATAGRAM;
	if (caps_in_use & VMCI_CAPS_NOTIFICATIONS)
		cmd |= VMCI_IMR_NOTIFICATION;
	if (caps_in_use & VMCI_CAPS_DMA_DATAGRAM)
		cmd |= VMCI_IMR_DMA_DATAGRAM;
	vmci_write_reg(vmci_dev, cmd, VMCI_IMR_ADDR);

	 
	vmci_write_reg(vmci_dev, VMCI_CONTROL_INT_ENABLE, VMCI_CONTROL_ADDR);

	pci_set_drvdata(pdev, vmci_dev);

	vmci_call_vsock_callback(false);
	return 0;

err_free_bm_irq:
	if (vmci_dev->exclusive_vectors)
		free_irq(pci_irq_vector(pdev, 1), vmci_dev);

err_free_irq:
	free_irq(pci_irq_vector(pdev, 0), vmci_dev);

err_disable_msi:
	pci_free_irq_vectors(pdev);

err_unsubscribe_event:
	vmci_err = vmci_event_unsubscribe(ctx_update_sub_id);
	if (vmci_err < VMCI_SUCCESS)
		dev_warn(&pdev->dev,
			 "Failed to unsubscribe from event (type=%d) with subscriber (ID=0x%x): %d\n",
			 VMCI_EVENT_CTX_ID_UPDATE, ctx_update_sub_id, vmci_err);

err_remove_vmci_dev_g:
	spin_lock_irq(&vmci_dev_spinlock);
	vmci_pdev = NULL;
	vmci_dev_g = NULL;
	spin_unlock_irq(&vmci_dev_spinlock);

err_free_notification_bitmap:
	if (vmci_dev->notification_bitmap) {
		vmci_write_reg(vmci_dev, VMCI_CONTROL_RESET, VMCI_CONTROL_ADDR);
		dma_free_coherent(&pdev->dev, PAGE_SIZE,
				  vmci_dev->notification_bitmap,
				  vmci_dev->notification_base);
	}

err_free_data_buffers:
	vmci_free_dg_buffers(vmci_dev);

	 
	return error;
}

static void vmci_guest_remove_device(struct pci_dev *pdev)
{
	struct vmci_guest_device *vmci_dev = pci_get_drvdata(pdev);
	int vmci_err;

	dev_dbg(&pdev->dev, "Removing device\n");

	atomic_dec(&vmci_num_guest_devices);

	vmci_qp_guest_endpoints_exit();

	vmci_err = vmci_event_unsubscribe(ctx_update_sub_id);
	if (vmci_err < VMCI_SUCCESS)
		dev_warn(&pdev->dev,
			 "Failed to unsubscribe from event (type=%d) with subscriber (ID=0x%x): %d\n",
			 VMCI_EVENT_CTX_ID_UPDATE, ctx_update_sub_id, vmci_err);

	spin_lock_irq(&vmci_dev_spinlock);
	vmci_dev_g = NULL;
	vmci_pdev = NULL;
	spin_unlock_irq(&vmci_dev_spinlock);

	dev_dbg(&pdev->dev, "Resetting vmci device\n");
	vmci_write_reg(vmci_dev, VMCI_CONTROL_RESET, VMCI_CONTROL_ADDR);

	 
	if (vmci_dev->exclusive_vectors) {
		free_irq(pci_irq_vector(pdev, 1), vmci_dev);
		if (vmci_dev->mmio_base != NULL)
			free_irq(pci_irq_vector(pdev, 2), vmci_dev);
	}
	free_irq(pci_irq_vector(pdev, 0), vmci_dev);
	pci_free_irq_vectors(pdev);

	if (vmci_dev->notification_bitmap) {
		 

		dma_free_coherent(&pdev->dev, PAGE_SIZE,
				  vmci_dev->notification_bitmap,
				  vmci_dev->notification_base);
	}

	vmci_free_dg_buffers(vmci_dev);

	if (vmci_dev->mmio_base != NULL)
		pci_iounmap(pdev, vmci_dev->mmio_base);

	 
}

static const struct pci_device_id vmci_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_VMCI), },
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, vmci_ids);

static struct pci_driver vmci_guest_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= vmci_ids,
	.probe		= vmci_guest_probe_device,
	.remove		= vmci_guest_remove_device,
};

int __init vmci_guest_init(void)
{
	return pci_register_driver(&vmci_guest_driver);
}

void __exit vmci_guest_exit(void)
{
	pci_unregister_driver(&vmci_guest_driver);
}
