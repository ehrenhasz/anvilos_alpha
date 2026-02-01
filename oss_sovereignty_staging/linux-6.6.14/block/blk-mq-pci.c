
 
#include <linux/kobject.h>
#include <linux/blkdev.h>
#include <linux/blk-mq-pci.h>
#include <linux/pci.h>
#include <linux/module.h>

#include "blk-mq.h"

 
void blk_mq_pci_map_queues(struct blk_mq_queue_map *qmap, struct pci_dev *pdev,
			   int offset)
{
	const struct cpumask *mask;
	unsigned int queue, cpu;

	for (queue = 0; queue < qmap->nr_queues; queue++) {
		mask = pci_irq_get_affinity(pdev, queue + offset);
		if (!mask)
			goto fallback;

		for_each_cpu(cpu, mask)
			qmap->mq_map[cpu] = qmap->queue_offset + queue;
	}

	return;

fallback:
	WARN_ON_ONCE(qmap->nr_queues > 1);
	blk_mq_clear_mq_map(qmap);
}
EXPORT_SYMBOL_GPL(blk_mq_pci_map_queues);
