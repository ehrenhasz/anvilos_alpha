
 

 

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kfifo.h>
#include "kfd_priv.h"

#define KFD_IH_NUM_ENTRIES 8192

static void interrupt_wq(struct work_struct *);

int kfd_interrupt_init(struct kfd_node *node)
{
	int r;

	r = kfifo_alloc(&node->ih_fifo,
		KFD_IH_NUM_ENTRIES * node->kfd->device_info.ih_ring_entry_size,
		GFP_KERNEL);
	if (r) {
		dev_err(node->adev->dev, "Failed to allocate IH fifo\n");
		return r;
	}

	node->ih_wq = alloc_workqueue("KFD IH", WQ_HIGHPRI, 1);
	if (unlikely(!node->ih_wq)) {
		kfifo_free(&node->ih_fifo);
		dev_err(node->adev->dev, "Failed to allocate KFD IH workqueue\n");
		return -ENOMEM;
	}
	spin_lock_init(&node->interrupt_lock);

	INIT_WORK(&node->interrupt_work, interrupt_wq);

	node->interrupts_active = true;

	 
	smp_wmb();

	return 0;
}

void kfd_interrupt_exit(struct kfd_node *node)
{
	 
	unsigned long flags;

	spin_lock_irqsave(&node->interrupt_lock, flags);
	node->interrupts_active = false;
	spin_unlock_irqrestore(&node->interrupt_lock, flags);

	 
	flush_workqueue(node->ih_wq);

	kfifo_free(&node->ih_fifo);
}

 
bool enqueue_ih_ring_entry(struct kfd_node *node, const void *ih_ring_entry)
{
	int count;

	count = kfifo_in(&node->ih_fifo, ih_ring_entry,
				node->kfd->device_info.ih_ring_entry_size);
	if (count != node->kfd->device_info.ih_ring_entry_size) {
		dev_dbg_ratelimited(node->adev->dev,
			"Interrupt ring overflow, dropping interrupt %d\n",
			count);
		return false;
	}

	return true;
}

 
static bool dequeue_ih_ring_entry(struct kfd_node *node, void *ih_ring_entry)
{
	int count;

	count = kfifo_out(&node->ih_fifo, ih_ring_entry,
				node->kfd->device_info.ih_ring_entry_size);

	WARN_ON(count && count != node->kfd->device_info.ih_ring_entry_size);

	return count == node->kfd->device_info.ih_ring_entry_size;
}

static void interrupt_wq(struct work_struct *work)
{
	struct kfd_node *dev = container_of(work, struct kfd_node,
						interrupt_work);
	uint32_t ih_ring_entry[KFD_MAX_RING_ENTRY_SIZE];
	unsigned long start_jiffies = jiffies;

	if (dev->kfd->device_info.ih_ring_entry_size > sizeof(ih_ring_entry)) {
		dev_err_once(dev->adev->dev, "Ring entry too small\n");
		return;
	}

	while (dequeue_ih_ring_entry(dev, ih_ring_entry)) {
		dev->kfd->device_info.event_interrupt_class->interrupt_wq(dev,
								ih_ring_entry);
		if (time_is_before_jiffies(start_jiffies + HZ)) {
			 
			queue_work(dev->ih_wq, &dev->interrupt_work);
			break;
		}
	}
}

bool interrupt_is_wanted(struct kfd_node *dev,
			const uint32_t *ih_ring_entry,
			uint32_t *patched_ihre, bool *flag)
{
	 
	unsigned int wanted = 0;

	wanted |= dev->kfd->device_info.event_interrupt_class->interrupt_isr(dev,
					 ih_ring_entry, patched_ihre, flag);

	return wanted != 0;
}
