
 

#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>

#define DM_MSG_PREFIX "zero"

 
static int zero_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	if (argc != 0) {
		ti->error = "No arguments required";
		return -EINVAL;
	}

	 
	ti->num_discard_bios = 1;
	ti->discards_supported = true;

	return 0;
}

 
static int zero_map(struct dm_target *ti, struct bio *bio)
{
	switch (bio_op(bio)) {
	case REQ_OP_READ:
		if (bio->bi_opf & REQ_RAHEAD) {
			 
			return DM_MAPIO_KILL;
		}
		zero_fill_bio(bio);
		break;
	case REQ_OP_WRITE:
	case REQ_OP_DISCARD:
		 
		break;
	default:
		return DM_MAPIO_KILL;
	}

	bio_endio(bio);

	 
	return DM_MAPIO_SUBMITTED;
}

static void zero_io_hints(struct dm_target *ti, struct queue_limits *limits)
{
	limits->max_discard_sectors = UINT_MAX;
	limits->max_hw_discard_sectors = UINT_MAX;
	limits->discard_granularity = 512;
}

static struct target_type zero_target = {
	.name   = "zero",
	.version = {1, 2, 0},
	.features = DM_TARGET_NOWAIT,
	.module = THIS_MODULE,
	.ctr    = zero_ctr,
	.map    = zero_map,
	.io_hints = zero_io_hints,
};
module_dm(zero);

MODULE_AUTHOR("Jana Saout <jana@saout.de>");
MODULE_DESCRIPTION(DM_NAME " dummy target returning zeros");
MODULE_LICENSE("GPL");
