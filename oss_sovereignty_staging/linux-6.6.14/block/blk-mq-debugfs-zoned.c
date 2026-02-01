
 

#include <linux/blkdev.h>
#include "blk-mq-debugfs.h"

int queue_zone_wlock_show(void *data, struct seq_file *m)
{
	struct request_queue *q = data;
	unsigned int i;

	if (!q->disk->seq_zones_wlock)
		return 0;

	for (i = 0; i < q->disk->nr_zones; i++)
		if (test_bit(i, q->disk->seq_zones_wlock))
			seq_printf(m, "%u\n", i);

	return 0;
}
