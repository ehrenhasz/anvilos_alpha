
 

#include <linux/slab.h>
#include <linux/module.h>

#include "md.h"
#include "raid1.h"
#include "raid5.h"
#include "raid10.h"
#include "md-bitmap.h"

#include <linux/device-mapper.h>

#define DM_MSG_PREFIX "raid"
#define	MAX_RAID_DEVICES	253  

 
#define	MIN_FREE_RESHAPE_SPACE to_sector(4*4096)

 
#define	MIN_RAID456_JOURNAL_SPACE (4*2048)

static bool devices_handle_discard_safely;

 
#define FirstUse 10		 

struct raid_dev {
	 
	struct dm_dev *meta_dev;
	struct dm_dev *data_dev;
	struct md_rdev rdev;
};

 
#define __CTR_FLAG_SYNC			0     
#define __CTR_FLAG_NOSYNC		1     
#define __CTR_FLAG_REBUILD		2     
#define __CTR_FLAG_DAEMON_SLEEP		3     
#define __CTR_FLAG_MIN_RECOVERY_RATE	4     
#define __CTR_FLAG_MAX_RECOVERY_RATE	5     
#define __CTR_FLAG_MAX_WRITE_BEHIND	6     
#define __CTR_FLAG_WRITE_MOSTLY		7     
#define __CTR_FLAG_STRIPE_CACHE		8     
#define __CTR_FLAG_REGION_SIZE		9     
#define __CTR_FLAG_RAID10_COPIES	10    
#define __CTR_FLAG_RAID10_FORMAT	11    
 
#define __CTR_FLAG_DELTA_DISKS		12    
#define __CTR_FLAG_DATA_OFFSET		13    
#define __CTR_FLAG_RAID10_USE_NEAR_SETS 14    

 
#define __CTR_FLAG_JOURNAL_DEV		15    

 
#define __CTR_FLAG_JOURNAL_MODE		16    

 
#define CTR_FLAG_SYNC			(1 << __CTR_FLAG_SYNC)
#define CTR_FLAG_NOSYNC			(1 << __CTR_FLAG_NOSYNC)
#define CTR_FLAG_REBUILD		(1 << __CTR_FLAG_REBUILD)
#define CTR_FLAG_DAEMON_SLEEP		(1 << __CTR_FLAG_DAEMON_SLEEP)
#define CTR_FLAG_MIN_RECOVERY_RATE	(1 << __CTR_FLAG_MIN_RECOVERY_RATE)
#define CTR_FLAG_MAX_RECOVERY_RATE	(1 << __CTR_FLAG_MAX_RECOVERY_RATE)
#define CTR_FLAG_MAX_WRITE_BEHIND	(1 << __CTR_FLAG_MAX_WRITE_BEHIND)
#define CTR_FLAG_WRITE_MOSTLY		(1 << __CTR_FLAG_WRITE_MOSTLY)
#define CTR_FLAG_STRIPE_CACHE		(1 << __CTR_FLAG_STRIPE_CACHE)
#define CTR_FLAG_REGION_SIZE		(1 << __CTR_FLAG_REGION_SIZE)
#define CTR_FLAG_RAID10_COPIES		(1 << __CTR_FLAG_RAID10_COPIES)
#define CTR_FLAG_RAID10_FORMAT		(1 << __CTR_FLAG_RAID10_FORMAT)
#define CTR_FLAG_DELTA_DISKS		(1 << __CTR_FLAG_DELTA_DISKS)
#define CTR_FLAG_DATA_OFFSET		(1 << __CTR_FLAG_DATA_OFFSET)
#define CTR_FLAG_RAID10_USE_NEAR_SETS	(1 << __CTR_FLAG_RAID10_USE_NEAR_SETS)
#define CTR_FLAG_JOURNAL_DEV		(1 << __CTR_FLAG_JOURNAL_DEV)
#define CTR_FLAG_JOURNAL_MODE		(1 << __CTR_FLAG_JOURNAL_MODE)

 
 
#define	CTR_FLAGS_ANY_SYNC		(CTR_FLAG_SYNC | CTR_FLAG_NOSYNC)

 
#define	CTR_FLAG_OPTIONS_NO_ARGS	(CTR_FLAGS_ANY_SYNC | \
					 CTR_FLAG_RAID10_USE_NEAR_SETS)

 
#define CTR_FLAG_OPTIONS_ONE_ARG (CTR_FLAG_REBUILD | \
				  CTR_FLAG_WRITE_MOSTLY | \
				  CTR_FLAG_DAEMON_SLEEP | \
				  CTR_FLAG_MIN_RECOVERY_RATE | \
				  CTR_FLAG_MAX_RECOVERY_RATE | \
				  CTR_FLAG_MAX_WRITE_BEHIND | \
				  CTR_FLAG_STRIPE_CACHE | \
				  CTR_FLAG_REGION_SIZE | \
				  CTR_FLAG_RAID10_COPIES | \
				  CTR_FLAG_RAID10_FORMAT | \
				  CTR_FLAG_DELTA_DISKS | \
				  CTR_FLAG_DATA_OFFSET | \
				  CTR_FLAG_JOURNAL_DEV | \
				  CTR_FLAG_JOURNAL_MODE)

 

 
#define RAID0_VALID_FLAGS	(CTR_FLAG_DATA_OFFSET)

 
#define RAID1_VALID_FLAGS	(CTR_FLAGS_ANY_SYNC | \
				 CTR_FLAG_REBUILD | \
				 CTR_FLAG_WRITE_MOSTLY | \
				 CTR_FLAG_DAEMON_SLEEP | \
				 CTR_FLAG_MIN_RECOVERY_RATE | \
				 CTR_FLAG_MAX_RECOVERY_RATE | \
				 CTR_FLAG_MAX_WRITE_BEHIND | \
				 CTR_FLAG_REGION_SIZE | \
				 CTR_FLAG_DELTA_DISKS | \
				 CTR_FLAG_DATA_OFFSET)

 
#define RAID10_VALID_FLAGS	(CTR_FLAGS_ANY_SYNC | \
				 CTR_FLAG_REBUILD | \
				 CTR_FLAG_DAEMON_SLEEP | \
				 CTR_FLAG_MIN_RECOVERY_RATE | \
				 CTR_FLAG_MAX_RECOVERY_RATE | \
				 CTR_FLAG_REGION_SIZE | \
				 CTR_FLAG_RAID10_COPIES | \
				 CTR_FLAG_RAID10_FORMAT | \
				 CTR_FLAG_DELTA_DISKS | \
				 CTR_FLAG_DATA_OFFSET | \
				 CTR_FLAG_RAID10_USE_NEAR_SETS)

 
#define RAID45_VALID_FLAGS	(CTR_FLAGS_ANY_SYNC | \
				 CTR_FLAG_REBUILD | \
				 CTR_FLAG_DAEMON_SLEEP | \
				 CTR_FLAG_MIN_RECOVERY_RATE | \
				 CTR_FLAG_MAX_RECOVERY_RATE | \
				 CTR_FLAG_STRIPE_CACHE | \
				 CTR_FLAG_REGION_SIZE | \
				 CTR_FLAG_DELTA_DISKS | \
				 CTR_FLAG_DATA_OFFSET | \
				 CTR_FLAG_JOURNAL_DEV | \
				 CTR_FLAG_JOURNAL_MODE)

#define RAID6_VALID_FLAGS	(CTR_FLAG_SYNC | \
				 CTR_FLAG_REBUILD | \
				 CTR_FLAG_DAEMON_SLEEP | \
				 CTR_FLAG_MIN_RECOVERY_RATE | \
				 CTR_FLAG_MAX_RECOVERY_RATE | \
				 CTR_FLAG_STRIPE_CACHE | \
				 CTR_FLAG_REGION_SIZE | \
				 CTR_FLAG_DELTA_DISKS | \
				 CTR_FLAG_DATA_OFFSET | \
				 CTR_FLAG_JOURNAL_DEV | \
				 CTR_FLAG_JOURNAL_MODE)
 

 
#define RT_FLAG_RS_PRERESUMED		0
#define RT_FLAG_RS_RESUMED		1
#define RT_FLAG_RS_BITMAP_LOADED	2
#define RT_FLAG_UPDATE_SBS		3
#define RT_FLAG_RESHAPE_RS		4
#define RT_FLAG_RS_SUSPENDED		5
#define RT_FLAG_RS_IN_SYNC		6
#define RT_FLAG_RS_RESYNCING		7
#define RT_FLAG_RS_GROW			8

 
#define DISKS_ARRAY_ELEMS ((MAX_RAID_DEVICES + (sizeof(uint64_t) * 8 - 1)) / sizeof(uint64_t) / 8)

 
struct rs_layout {
	int new_level;
	int new_layout;
	int new_chunk_sectors;
};

struct raid_set {
	struct dm_target *ti;

	uint32_t stripe_cache_entries;
	unsigned long ctr_flags;
	unsigned long runtime_flags;

	uint64_t rebuild_disks[DISKS_ARRAY_ELEMS];

	int raid_disks;
	int delta_disks;
	int data_offset;
	int raid10_copies;
	int requested_bitmap_chunk_sectors;

	struct mddev md;
	struct raid_type *raid_type;

	sector_t array_sectors;
	sector_t dev_sectors;

	 
	struct journal_dev {
		struct dm_dev *dev;
		struct md_rdev rdev;
		int mode;
	} journal_dev;

	struct raid_dev dev[];
};

static void rs_config_backup(struct raid_set *rs, struct rs_layout *l)
{
	struct mddev *mddev = &rs->md;

	l->new_level = mddev->new_level;
	l->new_layout = mddev->new_layout;
	l->new_chunk_sectors = mddev->new_chunk_sectors;
}

static void rs_config_restore(struct raid_set *rs, struct rs_layout *l)
{
	struct mddev *mddev = &rs->md;

	mddev->new_level = l->new_level;
	mddev->new_layout = l->new_layout;
	mddev->new_chunk_sectors = l->new_chunk_sectors;
}

 
#define	ALGORITHM_RAID10_DEFAULT	0
#define	ALGORITHM_RAID10_NEAR		1
#define	ALGORITHM_RAID10_OFFSET		2
#define	ALGORITHM_RAID10_FAR		3

 
static struct raid_type {
	const char *name;		 
	const char *descr;		 
	const unsigned int parity_devs;	 
	const unsigned int minimal_devs; 
	const unsigned int level;	 
	const unsigned int algorithm;	 
} raid_types[] = {
	{"raid0",	  "raid0 (striping)",			    0, 2, 0,  0  },
	{"raid1",	  "raid1 (mirroring)",			    0, 2, 1,  0  },
	{"raid10_far",	  "raid10 far (striped mirrors)",	    0, 2, 10, ALGORITHM_RAID10_FAR},
	{"raid10_offset", "raid10 offset (striped mirrors)",	    0, 2, 10, ALGORITHM_RAID10_OFFSET},
	{"raid10_near",	  "raid10 near (striped mirrors)",	    0, 2, 10, ALGORITHM_RAID10_NEAR},
	{"raid10",	  "raid10 (striped mirrors)",		    0, 2, 10, ALGORITHM_RAID10_DEFAULT},
	{"raid4",	  "raid4 (dedicated first parity disk)",    1, 2, 5,  ALGORITHM_PARITY_0},  
	{"raid5_n",	  "raid5 (dedicated last parity disk)",	    1, 2, 5,  ALGORITHM_PARITY_N},
	{"raid5_ls",	  "raid5 (left symmetric)",		    1, 2, 5,  ALGORITHM_LEFT_SYMMETRIC},
	{"raid5_rs",	  "raid5 (right symmetric)",		    1, 2, 5,  ALGORITHM_RIGHT_SYMMETRIC},
	{"raid5_la",	  "raid5 (left asymmetric)",		    1, 2, 5,  ALGORITHM_LEFT_ASYMMETRIC},
	{"raid5_ra",	  "raid5 (right asymmetric)",		    1, 2, 5,  ALGORITHM_RIGHT_ASYMMETRIC},
	{"raid6_zr",	  "raid6 (zero restart)",		    2, 4, 6,  ALGORITHM_ROTATING_ZERO_RESTART},
	{"raid6_nr",	  "raid6 (N restart)",			    2, 4, 6,  ALGORITHM_ROTATING_N_RESTART},
	{"raid6_nc",	  "raid6 (N continue)",			    2, 4, 6,  ALGORITHM_ROTATING_N_CONTINUE},
	{"raid6_n_6",	  "raid6 (dedicated parity/Q n/6)",	    2, 4, 6,  ALGORITHM_PARITY_N_6},
	{"raid6_ls_6",	  "raid6 (left symmetric dedicated Q 6)",   2, 4, 6,  ALGORITHM_LEFT_SYMMETRIC_6},
	{"raid6_rs_6",	  "raid6 (right symmetric dedicated Q 6)",  2, 4, 6,  ALGORITHM_RIGHT_SYMMETRIC_6},
	{"raid6_la_6",	  "raid6 (left asymmetric dedicated Q 6)",  2, 4, 6,  ALGORITHM_LEFT_ASYMMETRIC_6},
	{"raid6_ra_6",	  "raid6 (right asymmetric dedicated Q 6)", 2, 4, 6,  ALGORITHM_RIGHT_ASYMMETRIC_6}
};

 
static bool __within_range(long v, long min, long max)
{
	return v >= min && v <= max;
}

 
static struct arg_name_flag {
	const unsigned long flag;
	const char *name;
} __arg_name_flags[] = {
	{ CTR_FLAG_SYNC, "sync"},
	{ CTR_FLAG_NOSYNC, "nosync"},
	{ CTR_FLAG_REBUILD, "rebuild"},
	{ CTR_FLAG_DAEMON_SLEEP, "daemon_sleep"},
	{ CTR_FLAG_MIN_RECOVERY_RATE, "min_recovery_rate"},
	{ CTR_FLAG_MAX_RECOVERY_RATE, "max_recovery_rate"},
	{ CTR_FLAG_MAX_WRITE_BEHIND, "max_write_behind"},
	{ CTR_FLAG_WRITE_MOSTLY, "write_mostly"},
	{ CTR_FLAG_STRIPE_CACHE, "stripe_cache"},
	{ CTR_FLAG_REGION_SIZE, "region_size"},
	{ CTR_FLAG_RAID10_COPIES, "raid10_copies"},
	{ CTR_FLAG_RAID10_FORMAT, "raid10_format"},
	{ CTR_FLAG_DATA_OFFSET, "data_offset"},
	{ CTR_FLAG_DELTA_DISKS, "delta_disks"},
	{ CTR_FLAG_RAID10_USE_NEAR_SETS, "raid10_use_near_sets"},
	{ CTR_FLAG_JOURNAL_DEV, "journal_dev" },
	{ CTR_FLAG_JOURNAL_MODE, "journal_mode" },
};

 
static const char *dm_raid_arg_name_by_flag(const uint32_t flag)
{
	if (hweight32(flag) == 1) {
		struct arg_name_flag *anf = __arg_name_flags + ARRAY_SIZE(__arg_name_flags);

		while (anf-- > __arg_name_flags)
			if (flag & anf->flag)
				return anf->name;

	} else
		DMERR("%s called with more than one flag!", __func__);

	return NULL;
}

 
static struct {
	const int mode;
	const char *param;
} _raid456_journal_mode[] = {
	{ R5C_JOURNAL_MODE_WRITE_THROUGH, "writethrough" },
	{ R5C_JOURNAL_MODE_WRITE_BACK,    "writeback" }
};

 
static int dm_raid_journal_mode_to_md(const char *mode)
{
	int m = ARRAY_SIZE(_raid456_journal_mode);

	while (m--)
		if (!strcasecmp(mode, _raid456_journal_mode[m].param))
			return _raid456_journal_mode[m].mode;

	return -EINVAL;
}

 
static const char *md_journal_mode_to_dm_raid(const int mode)
{
	int m = ARRAY_SIZE(_raid456_journal_mode);

	while (m--)
		if (mode == _raid456_journal_mode[m].mode)
			return _raid456_journal_mode[m].param;

	return "unknown";
}

 
 
static bool rs_is_raid0(struct raid_set *rs)
{
	return !rs->md.level;
}

 
static bool rs_is_raid1(struct raid_set *rs)
{
	return rs->md.level == 1;
}

 
static bool rs_is_raid10(struct raid_set *rs)
{
	return rs->md.level == 10;
}

 
static bool rs_is_raid6(struct raid_set *rs)
{
	return rs->md.level == 6;
}

 
static bool rs_is_raid456(struct raid_set *rs)
{
	return __within_range(rs->md.level, 4, 6);
}

 
static bool __is_raid10_far(int layout);
static bool rs_is_reshapable(struct raid_set *rs)
{
	return rs_is_raid456(rs) ||
	       (rs_is_raid10(rs) && !__is_raid10_far(rs->md.new_layout));
}

 
static bool rs_is_recovering(struct raid_set *rs)
{
	return rs->md.recovery_cp < rs->md.dev_sectors;
}

 
static bool rs_is_reshaping(struct raid_set *rs)
{
	return rs->md.reshape_position != MaxSector;
}

 

 
static bool rt_is_raid0(struct raid_type *rt)
{
	return !rt->level;
}

 
static bool rt_is_raid1(struct raid_type *rt)
{
	return rt->level == 1;
}

 
static bool rt_is_raid10(struct raid_type *rt)
{
	return rt->level == 10;
}

 
static bool rt_is_raid45(struct raid_type *rt)
{
	return __within_range(rt->level, 4, 5);
}

 
static bool rt_is_raid6(struct raid_type *rt)
{
	return rt->level == 6;
}

 
static bool rt_is_raid456(struct raid_type *rt)
{
	return __within_range(rt->level, 4, 6);
}
 

 
static unsigned long __valid_flags(struct raid_set *rs)
{
	if (rt_is_raid0(rs->raid_type))
		return RAID0_VALID_FLAGS;
	else if (rt_is_raid1(rs->raid_type))
		return RAID1_VALID_FLAGS;
	else if (rt_is_raid10(rs->raid_type))
		return RAID10_VALID_FLAGS;
	else if (rt_is_raid45(rs->raid_type))
		return RAID45_VALID_FLAGS;
	else if (rt_is_raid6(rs->raid_type))
		return RAID6_VALID_FLAGS;

	return 0;
}

 
static int rs_check_for_valid_flags(struct raid_set *rs)
{
	if (rs->ctr_flags & ~__valid_flags(rs)) {
		rs->ti->error = "Invalid flags combination";
		return -EINVAL;
	}

	return 0;
}

 
#define RAID10_OFFSET			(1 << 16)  
#define RAID10_BROCKEN_USE_FAR_SETS	(1 << 17)  
#define RAID10_USE_FAR_SETS		(1 << 18)  
#define RAID10_FAR_COPIES_SHIFT		8	   

 
static unsigned int __raid10_near_copies(int layout)
{
	return layout & 0xFF;
}

 
static unsigned int __raid10_far_copies(int layout)
{
	return __raid10_near_copies(layout >> RAID10_FAR_COPIES_SHIFT);
}

 
static bool __is_raid10_offset(int layout)
{
	return !!(layout & RAID10_OFFSET);
}

 
static bool __is_raid10_near(int layout)
{
	return !__is_raid10_offset(layout) && __raid10_near_copies(layout) > 1;
}

 
static bool __is_raid10_far(int layout)
{
	return !__is_raid10_offset(layout) && __raid10_far_copies(layout) > 1;
}

 
static const char *raid10_md_layout_to_format(int layout)
{
	 
	if (__is_raid10_offset(layout))
		return "offset";

	if (__raid10_near_copies(layout) > 1)
		return "near";

	if (__raid10_far_copies(layout) > 1)
		return "far";

	return "unknown";
}

 
static int raid10_name_to_format(const char *name)
{
	if (!strcasecmp(name, "near"))
		return ALGORITHM_RAID10_NEAR;
	else if (!strcasecmp(name, "offset"))
		return ALGORITHM_RAID10_OFFSET;
	else if (!strcasecmp(name, "far"))
		return ALGORITHM_RAID10_FAR;

	return -EINVAL;
}

 
static unsigned int raid10_md_layout_to_copies(int layout)
{
	return max(__raid10_near_copies(layout), __raid10_far_copies(layout));
}

 
static int raid10_format_to_md_layout(struct raid_set *rs,
				      unsigned int algorithm,
				      unsigned int copies)
{
	unsigned int n = 1, f = 1, r = 0;

	 
	if (algorithm == ALGORITHM_RAID10_DEFAULT ||
	    algorithm == ALGORITHM_RAID10_NEAR)
		n = copies;

	else if (algorithm == ALGORITHM_RAID10_OFFSET) {
		f = copies;
		r = RAID10_OFFSET;
		if (!test_bit(__CTR_FLAG_RAID10_USE_NEAR_SETS, &rs->ctr_flags))
			r |= RAID10_USE_FAR_SETS;

	} else if (algorithm == ALGORITHM_RAID10_FAR) {
		f = copies;
		if (!test_bit(__CTR_FLAG_RAID10_USE_NEAR_SETS, &rs->ctr_flags))
			r |= RAID10_USE_FAR_SETS;

	} else
		return -EINVAL;

	return r | (f << RAID10_FAR_COPIES_SHIFT) | n;
}
 

 
static bool __got_raid10(struct raid_type *rtp, const int layout)
{
	if (rtp->level == 10) {
		switch (rtp->algorithm) {
		case ALGORITHM_RAID10_DEFAULT:
		case ALGORITHM_RAID10_NEAR:
			return __is_raid10_near(layout);
		case ALGORITHM_RAID10_OFFSET:
			return __is_raid10_offset(layout);
		case ALGORITHM_RAID10_FAR:
			return __is_raid10_far(layout);
		default:
			break;
		}
	}

	return false;
}

 
static struct raid_type *get_raid_type(const char *name)
{
	struct raid_type *rtp = raid_types + ARRAY_SIZE(raid_types);

	while (rtp-- > raid_types)
		if (!strcasecmp(rtp->name, name))
			return rtp;

	return NULL;
}

 
static struct raid_type *get_raid_type_by_ll(const int level, const int layout)
{
	struct raid_type *rtp = raid_types + ARRAY_SIZE(raid_types);

	while (rtp-- > raid_types) {
		 
		if (rtp->level == level &&
		    (__got_raid10(rtp, layout) || rtp->algorithm == layout))
			return rtp;
	}

	return NULL;
}

 
static void rs_set_rdev_sectors(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;
	struct md_rdev *rdev;

	 
	rdev_for_each(rdev, mddev)
		if (!test_bit(Journal, &rdev->flags))
			rdev->sectors = mddev->dev_sectors;
}

 
static void rs_set_capacity(struct raid_set *rs)
{
	struct gendisk *gendisk = dm_disk(dm_table_get_md(rs->ti->table));

	set_capacity_and_notify(gendisk, rs->md.array_sectors);
}

 
static void rs_set_cur(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;

	mddev->new_level = mddev->level;
	mddev->new_layout = mddev->layout;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
}

 
static void rs_set_new(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;

	mddev->level = mddev->new_level;
	mddev->layout = mddev->new_layout;
	mddev->chunk_sectors = mddev->new_chunk_sectors;
	mddev->raid_disks = rs->raid_disks;
	mddev->delta_disks = 0;
}

static struct raid_set *raid_set_alloc(struct dm_target *ti, struct raid_type *raid_type,
				       unsigned int raid_devs)
{
	unsigned int i;
	struct raid_set *rs;

	if (raid_devs <= raid_type->parity_devs) {
		ti->error = "Insufficient number of devices";
		return ERR_PTR(-EINVAL);
	}

	rs = kzalloc(struct_size(rs, dev, raid_devs), GFP_KERNEL);
	if (!rs) {
		ti->error = "Cannot allocate raid context";
		return ERR_PTR(-ENOMEM);
	}

	mddev_init(&rs->md);

	rs->raid_disks = raid_devs;
	rs->delta_disks = 0;

	rs->ti = ti;
	rs->raid_type = raid_type;
	rs->stripe_cache_entries = 256;
	rs->md.raid_disks = raid_devs;
	rs->md.level = raid_type->level;
	rs->md.new_level = rs->md.level;
	rs->md.layout = raid_type->algorithm;
	rs->md.new_layout = rs->md.layout;
	rs->md.delta_disks = 0;
	rs->md.recovery_cp = MaxSector;

	for (i = 0; i < raid_devs; i++)
		md_rdev_init(&rs->dev[i].rdev);

	 

	return rs;
}

 
static void raid_set_free(struct raid_set *rs)
{
	int i;

	if (rs->journal_dev.dev) {
		md_rdev_clear(&rs->journal_dev.rdev);
		dm_put_device(rs->ti, rs->journal_dev.dev);
	}

	for (i = 0; i < rs->raid_disks; i++) {
		if (rs->dev[i].meta_dev)
			dm_put_device(rs->ti, rs->dev[i].meta_dev);
		md_rdev_clear(&rs->dev[i].rdev);
		if (rs->dev[i].data_dev)
			dm_put_device(rs->ti, rs->dev[i].data_dev);
	}

	kfree(rs);
}

 
static int parse_dev_params(struct raid_set *rs, struct dm_arg_set *as)
{
	int i;
	int rebuild = 0;
	int metadata_available = 0;
	int r = 0;
	const char *arg;

	 
	arg = dm_shift_arg(as);
	if (!arg)
		return -EINVAL;

	for (i = 0; i < rs->raid_disks; i++) {
		rs->dev[i].rdev.raid_disk = i;

		rs->dev[i].meta_dev = NULL;
		rs->dev[i].data_dev = NULL;

		 
		rs->dev[i].rdev.data_offset = 0;
		rs->dev[i].rdev.new_data_offset = 0;
		rs->dev[i].rdev.mddev = &rs->md;

		arg = dm_shift_arg(as);
		if (!arg)
			return -EINVAL;

		if (strcmp(arg, "-")) {
			r = dm_get_device(rs->ti, arg, dm_table_get_mode(rs->ti->table),
					  &rs->dev[i].meta_dev);
			if (r) {
				rs->ti->error = "RAID metadata device lookup failure";
				return r;
			}

			rs->dev[i].rdev.sb_page = alloc_page(GFP_KERNEL);
			if (!rs->dev[i].rdev.sb_page) {
				rs->ti->error = "Failed to allocate superblock page";
				return -ENOMEM;
			}
		}

		arg = dm_shift_arg(as);
		if (!arg)
			return -EINVAL;

		if (!strcmp(arg, "-")) {
			if (!test_bit(In_sync, &rs->dev[i].rdev.flags) &&
			    (!rs->dev[i].rdev.recovery_offset)) {
				rs->ti->error = "Drive designated for rebuild not specified";
				return -EINVAL;
			}

			if (rs->dev[i].meta_dev) {
				rs->ti->error = "No data device supplied with metadata device";
				return -EINVAL;
			}

			continue;
		}

		r = dm_get_device(rs->ti, arg, dm_table_get_mode(rs->ti->table),
				  &rs->dev[i].data_dev);
		if (r) {
			rs->ti->error = "RAID device lookup failure";
			return r;
		}

		if (rs->dev[i].meta_dev) {
			metadata_available = 1;
			rs->dev[i].rdev.meta_bdev = rs->dev[i].meta_dev->bdev;
		}
		rs->dev[i].rdev.bdev = rs->dev[i].data_dev->bdev;
		list_add_tail(&rs->dev[i].rdev.same_set, &rs->md.disks);
		if (!test_bit(In_sync, &rs->dev[i].rdev.flags))
			rebuild++;
	}

	if (rs->journal_dev.dev)
		list_add_tail(&rs->journal_dev.rdev.same_set, &rs->md.disks);

	if (metadata_available) {
		rs->md.external = 0;
		rs->md.persistent = 1;
		rs->md.major_version = 2;
	} else if (rebuild && !rs->md.recovery_cp) {
		 
		rs->ti->error = "Unable to rebuild drive while array is not in-sync";
		return -EINVAL;
	}

	return 0;
}

 
static int validate_region_size(struct raid_set *rs, unsigned long region_size)
{
	unsigned long min_region_size = rs->ti->len / (1 << 21);

	if (rs_is_raid0(rs))
		return 0;

	if (!region_size) {
		 
		if (min_region_size > (1 << 13)) {
			 
			region_size = roundup_pow_of_two(min_region_size);
			DMINFO("Choosing default region size of %lu sectors",
			       region_size);
		} else {
			DMINFO("Choosing default region size of 4MiB");
			region_size = 1 << 13;  
		}
	} else {
		 
		if (region_size > rs->ti->len) {
			rs->ti->error = "Supplied region size is too large";
			return -EINVAL;
		}

		if (region_size < min_region_size) {
			DMERR("Supplied region_size (%lu sectors) below minimum (%lu)",
			      region_size, min_region_size);
			rs->ti->error = "Supplied region size is too small";
			return -EINVAL;
		}

		if (!is_power_of_2(region_size)) {
			rs->ti->error = "Region size is not a power of 2";
			return -EINVAL;
		}

		if (region_size < rs->md.chunk_sectors) {
			rs->ti->error = "Region size is smaller than the chunk size";
			return -EINVAL;
		}
	}

	 
	rs->md.bitmap_info.chunksize = to_bytes(region_size);

	return 0;
}

 
static int validate_raid_redundancy(struct raid_set *rs)
{
	unsigned int i, rebuild_cnt = 0;
	unsigned int rebuilds_per_group = 0, copies, raid_disks;
	unsigned int group_size, last_group_start;

	for (i = 0; i < rs->raid_disks; i++)
		if (!test_bit(FirstUse, &rs->dev[i].rdev.flags) &&
		    ((!test_bit(In_sync, &rs->dev[i].rdev.flags) ||
		      !rs->dev[i].rdev.sb_page)))
			rebuild_cnt++;

	switch (rs->md.level) {
	case 0:
		break;
	case 1:
		if (rebuild_cnt >= rs->md.raid_disks)
			goto too_many;
		break;
	case 4:
	case 5:
	case 6:
		if (rebuild_cnt > rs->raid_type->parity_devs)
			goto too_many;
		break;
	case 10:
		copies = raid10_md_layout_to_copies(rs->md.new_layout);
		if (copies < 2) {
			DMERR("Bogus raid10 data copies < 2!");
			return -EINVAL;
		}

		if (rebuild_cnt < copies)
			break;

		 
		raid_disks = min(rs->raid_disks, rs->md.raid_disks);
		if (__is_raid10_near(rs->md.new_layout)) {
			for (i = 0; i < raid_disks; i++) {
				if (!(i % copies))
					rebuilds_per_group = 0;
				if ((!rs->dev[i].rdev.sb_page ||
				    !test_bit(In_sync, &rs->dev[i].rdev.flags)) &&
				    (++rebuilds_per_group >= copies))
					goto too_many;
			}
			break;
		}

		 
		group_size = (raid_disks / copies);
		last_group_start = (raid_disks / group_size) - 1;
		last_group_start *= group_size;
		for (i = 0; i < raid_disks; i++) {
			if (!(i % copies) && !(i > last_group_start))
				rebuilds_per_group = 0;
			if ((!rs->dev[i].rdev.sb_page ||
			     !test_bit(In_sync, &rs->dev[i].rdev.flags)) &&
			    (++rebuilds_per_group >= copies))
				goto too_many;
		}
		break;
	default:
		if (rebuild_cnt)
			return -EINVAL;
	}

	return 0;

too_many:
	return -EINVAL;
}

 
static int parse_raid_params(struct raid_set *rs, struct dm_arg_set *as,
			     unsigned int num_raid_params)
{
	int value, raid10_format = ALGORITHM_RAID10_DEFAULT;
	unsigned int raid10_copies = 2;
	unsigned int i, write_mostly = 0;
	unsigned int region_size = 0;
	sector_t max_io_len;
	const char *arg, *key;
	struct raid_dev *rd;
	struct raid_type *rt = rs->raid_type;

	arg = dm_shift_arg(as);
	num_raid_params--;  

	if (kstrtoint(arg, 10, &value) < 0) {
		rs->ti->error = "Bad numerical argument given for chunk_size";
		return -EINVAL;
	}

	 
	if (rt_is_raid1(rt)) {
		if (value)
			DMERR("Ignoring chunk size parameter for RAID 1");
		value = 0;
	} else if (!is_power_of_2(value)) {
		rs->ti->error = "Chunk size must be a power of 2";
		return -EINVAL;
	} else if (value < 8) {
		rs->ti->error = "Chunk size value is too small";
		return -EINVAL;
	}

	rs->md.new_chunk_sectors = rs->md.chunk_sectors = value;

	 
	for (i = 0; i < rs->raid_disks; i++) {
		set_bit(In_sync, &rs->dev[i].rdev.flags);
		rs->dev[i].rdev.recovery_offset = MaxSector;
	}

	 
	for (i = 0; i < num_raid_params; i++) {
		key = dm_shift_arg(as);
		if (!key) {
			rs->ti->error = "Not enough raid parameters given";
			return -EINVAL;
		}

		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_NOSYNC))) {
			if (test_and_set_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags)) {
				rs->ti->error = "Only one 'nosync' argument allowed";
				return -EINVAL;
			}
			continue;
		}
		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_SYNC))) {
			if (test_and_set_bit(__CTR_FLAG_SYNC, &rs->ctr_flags)) {
				rs->ti->error = "Only one 'sync' argument allowed";
				return -EINVAL;
			}
			continue;
		}
		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_RAID10_USE_NEAR_SETS))) {
			if (test_and_set_bit(__CTR_FLAG_RAID10_USE_NEAR_SETS, &rs->ctr_flags)) {
				rs->ti->error = "Only one 'raid10_use_new_sets' argument allowed";
				return -EINVAL;
			}
			continue;
		}

		arg = dm_shift_arg(as);
		i++;  
		if (!arg) {
			rs->ti->error = "Wrong number of raid parameters given";
			return -EINVAL;
		}

		 
		 
		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_RAID10_FORMAT))) {
			if (test_and_set_bit(__CTR_FLAG_RAID10_FORMAT, &rs->ctr_flags)) {
				rs->ti->error = "Only one 'raid10_format' argument pair allowed";
				return -EINVAL;
			}
			if (!rt_is_raid10(rt)) {
				rs->ti->error = "'raid10_format' is an invalid parameter for this RAID type";
				return -EINVAL;
			}
			raid10_format = raid10_name_to_format(arg);
			if (raid10_format < 0) {
				rs->ti->error = "Invalid 'raid10_format' value given";
				return raid10_format;
			}
			continue;
		}

		 
		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_JOURNAL_DEV))) {
			int r;
			struct md_rdev *jdev;

			if (test_and_set_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags)) {
				rs->ti->error = "Only one raid4/5/6 set journaling device allowed";
				return -EINVAL;
			}
			if (!rt_is_raid456(rt)) {
				rs->ti->error = "'journal_dev' is an invalid parameter for this RAID type";
				return -EINVAL;
			}
			r = dm_get_device(rs->ti, arg, dm_table_get_mode(rs->ti->table),
					  &rs->journal_dev.dev);
			if (r) {
				rs->ti->error = "raid4/5/6 journal device lookup failure";
				return r;
			}
			jdev = &rs->journal_dev.rdev;
			md_rdev_init(jdev);
			jdev->mddev = &rs->md;
			jdev->bdev = rs->journal_dev.dev->bdev;
			jdev->sectors = bdev_nr_sectors(jdev->bdev);
			if (jdev->sectors < MIN_RAID456_JOURNAL_SPACE) {
				rs->ti->error = "No space for raid4/5/6 journal";
				return -ENOSPC;
			}
			rs->journal_dev.mode = R5C_JOURNAL_MODE_WRITE_THROUGH;
			set_bit(Journal, &jdev->flags);
			continue;
		}

		 
		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_JOURNAL_MODE))) {
			int r;

			if (!test_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags)) {
				rs->ti->error = "raid4/5/6 'journal_mode' is invalid without 'journal_dev'";
				return -EINVAL;
			}
			if (test_and_set_bit(__CTR_FLAG_JOURNAL_MODE, &rs->ctr_flags)) {
				rs->ti->error = "Only one raid4/5/6 'journal_mode' argument allowed";
				return -EINVAL;
			}
			r = dm_raid_journal_mode_to_md(arg);
			if (r < 0) {
				rs->ti->error = "Invalid 'journal_mode' argument";
				return r;
			}
			rs->journal_dev.mode = r;
			continue;
		}

		 
		if (kstrtoint(arg, 10, &value) < 0) {
			rs->ti->error = "Bad numerical argument given in raid params";
			return -EINVAL;
		}

		if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_REBUILD))) {
			 
			if (!__within_range(value, 0, rs->raid_disks - 1)) {
				rs->ti->error = "Invalid rebuild index given";
				return -EINVAL;
			}

			if (test_and_set_bit(value, (void *) rs->rebuild_disks)) {
				rs->ti->error = "rebuild for this index already given";
				return -EINVAL;
			}

			rd = rs->dev + value;
			clear_bit(In_sync, &rd->rdev.flags);
			clear_bit(Faulty, &rd->rdev.flags);
			rd->rdev.recovery_offset = 0;
			set_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags);
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_WRITE_MOSTLY))) {
			if (!rt_is_raid1(rt)) {
				rs->ti->error = "write_mostly option is only valid for RAID1";
				return -EINVAL;
			}

			if (!__within_range(value, 0, rs->md.raid_disks - 1)) {
				rs->ti->error = "Invalid write_mostly index given";
				return -EINVAL;
			}

			write_mostly++;
			set_bit(WriteMostly, &rs->dev[value].rdev.flags);
			set_bit(__CTR_FLAG_WRITE_MOSTLY, &rs->ctr_flags);
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_MAX_WRITE_BEHIND))) {
			if (!rt_is_raid1(rt)) {
				rs->ti->error = "max_write_behind option is only valid for RAID1";
				return -EINVAL;
			}

			if (test_and_set_bit(__CTR_FLAG_MAX_WRITE_BEHIND, &rs->ctr_flags)) {
				rs->ti->error = "Only one max_write_behind argument pair allowed";
				return -EINVAL;
			}

			 
			if (value < 0 || value / 2 > COUNTER_MAX) {
				rs->ti->error = "Max write-behind limit out of range";
				return -EINVAL;
			}

			rs->md.bitmap_info.max_write_behind = value / 2;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_DAEMON_SLEEP))) {
			if (test_and_set_bit(__CTR_FLAG_DAEMON_SLEEP, &rs->ctr_flags)) {
				rs->ti->error = "Only one daemon_sleep argument pair allowed";
				return -EINVAL;
			}
			if (value < 0) {
				rs->ti->error = "daemon sleep period out of range";
				return -EINVAL;
			}
			rs->md.bitmap_info.daemon_sleep = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_DATA_OFFSET))) {
			 
			if (test_and_set_bit(__CTR_FLAG_DATA_OFFSET, &rs->ctr_flags)) {
				rs->ti->error = "Only one data_offset argument pair allowed";
				return -EINVAL;
			}
			 
			if (value < 0 ||
			    (value && (value < MIN_FREE_RESHAPE_SPACE || value % to_sector(PAGE_SIZE)))) {
				rs->ti->error = "Bogus data_offset value";
				return -EINVAL;
			}
			rs->data_offset = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_DELTA_DISKS))) {
			 
			if (test_and_set_bit(__CTR_FLAG_DELTA_DISKS, &rs->ctr_flags)) {
				rs->ti->error = "Only one delta_disks argument pair allowed";
				return -EINVAL;
			}
			 
			if (!__within_range(abs(value), 1, MAX_RAID_DEVICES - rt->minimal_devs)) {
				rs->ti->error = "Too many delta_disk requested";
				return -EINVAL;
			}

			rs->delta_disks = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_STRIPE_CACHE))) {
			if (test_and_set_bit(__CTR_FLAG_STRIPE_CACHE, &rs->ctr_flags)) {
				rs->ti->error = "Only one stripe_cache argument pair allowed";
				return -EINVAL;
			}

			if (!rt_is_raid456(rt)) {
				rs->ti->error = "Inappropriate argument: stripe_cache";
				return -EINVAL;
			}

			if (value < 0) {
				rs->ti->error = "Bogus stripe cache entries value";
				return -EINVAL;
			}
			rs->stripe_cache_entries = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_MIN_RECOVERY_RATE))) {
			if (test_and_set_bit(__CTR_FLAG_MIN_RECOVERY_RATE, &rs->ctr_flags)) {
				rs->ti->error = "Only one min_recovery_rate argument pair allowed";
				return -EINVAL;
			}

			if (value < 0) {
				rs->ti->error = "min_recovery_rate out of range";
				return -EINVAL;
			}
			rs->md.sync_speed_min = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_MAX_RECOVERY_RATE))) {
			if (test_and_set_bit(__CTR_FLAG_MAX_RECOVERY_RATE, &rs->ctr_flags)) {
				rs->ti->error = "Only one max_recovery_rate argument pair allowed";
				return -EINVAL;
			}

			if (value < 0) {
				rs->ti->error = "max_recovery_rate out of range";
				return -EINVAL;
			}
			rs->md.sync_speed_max = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_REGION_SIZE))) {
			if (test_and_set_bit(__CTR_FLAG_REGION_SIZE, &rs->ctr_flags)) {
				rs->ti->error = "Only one region_size argument pair allowed";
				return -EINVAL;
			}

			region_size = value;
			rs->requested_bitmap_chunk_sectors = value;
		} else if (!strcasecmp(key, dm_raid_arg_name_by_flag(CTR_FLAG_RAID10_COPIES))) {
			if (test_and_set_bit(__CTR_FLAG_RAID10_COPIES, &rs->ctr_flags)) {
				rs->ti->error = "Only one raid10_copies argument pair allowed";
				return -EINVAL;
			}

			if (!__within_range(value, 2, rs->md.raid_disks)) {
				rs->ti->error = "Bad value for 'raid10_copies'";
				return -EINVAL;
			}

			raid10_copies = value;
		} else {
			DMERR("Unable to parse RAID parameter: %s", key);
			rs->ti->error = "Unable to parse RAID parameter";
			return -EINVAL;
		}
	}

	if (test_bit(__CTR_FLAG_SYNC, &rs->ctr_flags) &&
	    test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags)) {
		rs->ti->error = "sync and nosync are mutually exclusive";
		return -EINVAL;
	}

	if (test_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags) &&
	    (test_bit(__CTR_FLAG_SYNC, &rs->ctr_flags) ||
	     test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags))) {
		rs->ti->error = "sync/nosync and rebuild are mutually exclusive";
		return -EINVAL;
	}

	if (write_mostly >= rs->md.raid_disks) {
		rs->ti->error = "Can't set all raid1 devices to write_mostly";
		return -EINVAL;
	}

	if (rs->md.sync_speed_max &&
	    rs->md.sync_speed_min > rs->md.sync_speed_max) {
		rs->ti->error = "Bogus recovery rates";
		return -EINVAL;
	}

	if (validate_region_size(rs, region_size))
		return -EINVAL;

	if (rs->md.chunk_sectors)
		max_io_len = rs->md.chunk_sectors;
	else
		max_io_len = region_size;

	if (dm_set_target_max_io_len(rs->ti, max_io_len))
		return -EINVAL;

	if (rt_is_raid10(rt)) {
		if (raid10_copies > rs->md.raid_disks) {
			rs->ti->error = "Not enough devices to satisfy specification";
			return -EINVAL;
		}

		rs->md.new_layout = raid10_format_to_md_layout(rs, raid10_format, raid10_copies);
		if (rs->md.new_layout < 0) {
			rs->ti->error = "Error getting raid10 format";
			return rs->md.new_layout;
		}

		rt = get_raid_type_by_ll(10, rs->md.new_layout);
		if (!rt) {
			rs->ti->error = "Failed to recognize new raid10 layout";
			return -EINVAL;
		}

		if ((rt->algorithm == ALGORITHM_RAID10_DEFAULT ||
		     rt->algorithm == ALGORITHM_RAID10_NEAR) &&
		    test_bit(__CTR_FLAG_RAID10_USE_NEAR_SETS, &rs->ctr_flags)) {
			rs->ti->error = "RAID10 format 'near' and 'raid10_use_near_sets' are incompatible";
			return -EINVAL;
		}
	}

	rs->raid10_copies = raid10_copies;

	 
	rs->md.persistent = 0;
	rs->md.external = 1;

	 
	return rs_check_for_valid_flags(rs);
}

 
static int rs_set_raid456_stripe_cache(struct raid_set *rs)
{
	int r;
	struct r5conf *conf;
	struct mddev *mddev = &rs->md;
	uint32_t min_stripes = max(mddev->chunk_sectors, mddev->new_chunk_sectors) / 2;
	uint32_t nr_stripes = rs->stripe_cache_entries;

	if (!rt_is_raid456(rs->raid_type)) {
		rs->ti->error = "Inappropriate raid level; cannot change stripe_cache size";
		return -EINVAL;
	}

	if (nr_stripes < min_stripes) {
		DMINFO("Adjusting requested %u stripe cache entries to %u to suit stripe size",
		       nr_stripes, min_stripes);
		nr_stripes = min_stripes;
	}

	conf = mddev->private;
	if (!conf) {
		rs->ti->error = "Cannot change stripe_cache size on inactive RAID set";
		return -EINVAL;
	}

	 
	if (conf->min_nr_stripes != nr_stripes) {
		r = raid5_set_cache_size(mddev, nr_stripes);
		if (r) {
			rs->ti->error = "Failed to set raid4/5/6 stripe cache size";
			return r;
		}

		DMINFO("%u stripe cache entries", nr_stripes);
	}

	return 0;
}

 
static unsigned int mddev_data_stripes(struct raid_set *rs)
{
	return rs->md.raid_disks - rs->raid_type->parity_devs;
}

 
static unsigned int rs_data_stripes(struct raid_set *rs)
{
	return rs->raid_disks - rs->raid_type->parity_devs;
}

 
static sector_t __rdev_sectors(struct raid_set *rs)
{
	int i;

	for (i = 0; i < rs->raid_disks; i++) {
		struct md_rdev *rdev = &rs->dev[i].rdev;

		if (!test_bit(Journal, &rdev->flags) &&
		    rdev->bdev && rdev->sectors)
			return rdev->sectors;
	}

	return 0;
}

 
static int _check_data_dev_sectors(struct raid_set *rs)
{
	sector_t ds = ~0;
	struct md_rdev *rdev;

	rdev_for_each(rdev, &rs->md)
		if (!test_bit(Journal, &rdev->flags) && rdev->bdev) {
			ds = min(ds, bdev_nr_sectors(rdev->bdev));
			if (ds < rs->md.dev_sectors) {
				rs->ti->error = "Component device(s) too small";
				return -EINVAL;
			}
		}

	return 0;
}

 
static int rs_set_dev_and_array_sectors(struct raid_set *rs, sector_t sectors, bool use_mddev)
{
	int delta_disks;
	unsigned int data_stripes;
	sector_t array_sectors = sectors, dev_sectors = sectors;
	struct mddev *mddev = &rs->md;

	if (use_mddev) {
		delta_disks = mddev->delta_disks;
		data_stripes = mddev_data_stripes(rs);
	} else {
		delta_disks = rs->delta_disks;
		data_stripes = rs_data_stripes(rs);
	}

	 
	if (rt_is_raid1(rs->raid_type))
		;
	else if (rt_is_raid10(rs->raid_type)) {
		if (rs->raid10_copies < 2 ||
		    delta_disks < 0) {
			rs->ti->error = "Bogus raid10 data copies or delta disks";
			return -EINVAL;
		}

		dev_sectors *= rs->raid10_copies;
		if (sector_div(dev_sectors, data_stripes))
			goto bad;

		array_sectors = (data_stripes + delta_disks) * dev_sectors;
		if (sector_div(array_sectors, rs->raid10_copies))
			goto bad;

	} else if (sector_div(dev_sectors, data_stripes))
		goto bad;

	else
		 
		array_sectors = (data_stripes + delta_disks) * dev_sectors;

	mddev->array_sectors = array_sectors;
	mddev->dev_sectors = dev_sectors;
	rs_set_rdev_sectors(rs);

	return _check_data_dev_sectors(rs);
bad:
	rs->ti->error = "Target length not divisible by number of data devices";
	return -EINVAL;
}

 
static void rs_setup_recovery(struct raid_set *rs, sector_t dev_sectors)
{
	 
	if (rs_is_raid0(rs))
		rs->md.recovery_cp = MaxSector;
	 
	else if (rs_is_raid6(rs))
		rs->md.recovery_cp = dev_sectors;
	 
	else
		rs->md.recovery_cp = test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags)
				     ? MaxSector : dev_sectors;
}

static void do_table_event(struct work_struct *ws)
{
	struct raid_set *rs = container_of(ws, struct raid_set, md.event_work);

	smp_rmb();  
	if (!rs_is_reshaping(rs)) {
		if (rs_is_raid10(rs))
			rs_set_rdev_sectors(rs);
		rs_set_capacity(rs);
	}
	dm_table_event(rs->ti->table);
}

 
static int rs_check_takeover(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;
	unsigned int near_copies;

	if (rs->md.degraded) {
		rs->ti->error = "Can't takeover degraded raid set";
		return -EPERM;
	}

	if (rs_is_reshaping(rs)) {
		rs->ti->error = "Can't takeover reshaping raid set";
		return -EPERM;
	}

	switch (mddev->level) {
	case 0:
		 
		if ((mddev->new_level == 1 || mddev->new_level == 5) &&
		    mddev->raid_disks == 1)
			return 0;

		 
		if (mddev->new_level == 10 &&
		    !(rs->raid_disks % mddev->raid_disks))
			return 0;

		 
		if (__within_range(mddev->new_level, 4, 6) &&
		    mddev->new_layout == ALGORITHM_PARITY_N &&
		    mddev->raid_disks > 1)
			return 0;

		break;

	case 10:
		 
		if (__is_raid10_offset(mddev->layout))
			break;

		near_copies = __raid10_near_copies(mddev->layout);

		 
		if (mddev->new_level == 0) {
			 
			if (near_copies > 1 &&
			    !(mddev->raid_disks % near_copies)) {
				mddev->raid_disks /= near_copies;
				mddev->delta_disks = mddev->raid_disks;
				return 0;
			}

			 
			if (near_copies == 1 &&
			    __raid10_far_copies(mddev->layout) > 1)
				return 0;

			break;
		}

		 
		if (mddev->new_level == 1 &&
		    max(near_copies, __raid10_far_copies(mddev->layout)) == mddev->raid_disks)
			return 0;

		 
		if (__within_range(mddev->new_level, 4, 5) &&
		    mddev->raid_disks == 2)
			return 0;
		break;

	case 1:
		 
		if (__within_range(mddev->new_level, 4, 5) &&
		    mddev->raid_disks == 2) {
			mddev->degraded = 1;
			return 0;
		}

		 
		if (mddev->new_level == 0 &&
		    mddev->raid_disks == 1)
			return 0;

		 
		if (mddev->new_level == 10)
			return 0;
		break;

	case 4:
		 
		if (mddev->new_level == 0)
			return 0;

		 
		if ((mddev->new_level == 1 || mddev->new_level == 5) &&
		    mddev->raid_disks == 2)
			return 0;

		 
		if (__within_range(mddev->new_level, 5, 6) &&
		    mddev->layout == ALGORITHM_PARITY_N)
			return 0;
		break;

	case 5:
		 
		if (mddev->new_level == 0 &&
		    mddev->layout == ALGORITHM_PARITY_N)
			return 0;

		 
		if (mddev->new_level == 4 &&
		    mddev->layout == ALGORITHM_PARITY_N)
			return 0;

		 
		if ((mddev->new_level == 1 || mddev->new_level == 4 || mddev->new_level == 10) &&
		    mddev->raid_disks == 2)
			return 0;

		 
		if (mddev->new_level == 6 &&
		    ((mddev->layout == ALGORITHM_PARITY_N && mddev->new_layout == ALGORITHM_PARITY_N) ||
		      __within_range(mddev->new_layout, ALGORITHM_LEFT_ASYMMETRIC_6, ALGORITHM_RIGHT_SYMMETRIC_6)))
			return 0;
		break;

	case 6:
		 
		if (mddev->new_level == 0 &&
		    mddev->layout == ALGORITHM_PARITY_N)
			return 0;

		 
		if (mddev->new_level == 4 &&
		    mddev->layout == ALGORITHM_PARITY_N)
			return 0;

		 
		if (mddev->new_level == 5 &&
		    ((mddev->layout == ALGORITHM_PARITY_N && mddev->new_layout == ALGORITHM_PARITY_N) ||
		     __within_range(mddev->new_layout, ALGORITHM_LEFT_ASYMMETRIC, ALGORITHM_RIGHT_SYMMETRIC)))
			return 0;
		break;

	default:
		break;
	}

	rs->ti->error = "takeover not possible";
	return -EINVAL;
}

 
static bool rs_takeover_requested(struct raid_set *rs)
{
	return rs->md.new_level != rs->md.level;
}

 
static bool rs_is_layout_change(struct raid_set *rs, bool use_mddev)
{
	return (use_mddev ? rs->md.delta_disks : rs->delta_disks) ||
	       rs->md.new_layout != rs->md.layout ||
	       rs->md.new_chunk_sectors != rs->md.chunk_sectors;
}

 
static bool rs_reshape_requested(struct raid_set *rs)
{
	bool change;
	struct mddev *mddev = &rs->md;

	if (rs_takeover_requested(rs))
		return false;

	if (rs_is_raid0(rs))
		return false;

	change = rs_is_layout_change(rs, false);

	 
	if (rs_is_raid1(rs)) {
		if (rs->delta_disks)
			return !!rs->delta_disks;

		return !change &&
		       mddev->raid_disks != rs->raid_disks;
	}

	if (rs_is_raid10(rs))
		return change &&
		       !__is_raid10_far(mddev->new_layout) &&
		       rs->delta_disks >= 0;

	return change;
}

 
#define	FEATURE_FLAG_SUPPORTS_V190	0x1  

 
#define	SB_FLAG_RESHAPE_ACTIVE		0x1
#define	SB_FLAG_RESHAPE_BACKWARDS	0x2

 
#define DM_RAID_MAGIC 0x64526D44
struct dm_raid_superblock {
	__le32 magic;		 
	__le32 compat_features;	 

	__le32 num_devices;	 
	__le32 array_position;	 

	__le64 events;		 
	__le64 failed_devices;	 
				 

	 
	__le64 disk_recovery_offset;

	 
	__le64 array_resync_offset;

	 
	__le32 level;
	__le32 layout;
	__le32 stripe_sectors;

	 

	__le32 flags;  

	 
	__le64 reshape_position;

	 
	__le32 new_level;
	__le32 new_layout;
	__le32 new_stripe_sectors;
	__le32 delta_disks;

	__le64 array_sectors;  

	 
	__le64 data_offset;
	__le64 new_data_offset;

	__le64 sectors;  

	 
	__le64 extended_failed_devices[DISKS_ARRAY_ELEMS - 1];

	__le32 incompat_features;	 

	 
} __packed;

 
static int rs_check_reshape(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;

	if (!mddev->pers || !mddev->pers->check_reshape)
		rs->ti->error = "Reshape not supported";
	else if (mddev->degraded)
		rs->ti->error = "Can't reshape degraded raid set";
	else if (rs_is_recovering(rs))
		rs->ti->error = "Convert request on recovering raid set prohibited";
	else if (rs_is_reshaping(rs))
		rs->ti->error = "raid set already reshaping!";
	else if (!(rs_is_raid1(rs) || rs_is_raid10(rs) || rs_is_raid456(rs)))
		rs->ti->error = "Reshaping only supported for raid1/4/5/6/10";
	else
		return 0;

	return -EPERM;
}

static int read_disk_sb(struct md_rdev *rdev, int size, bool force_reload)
{
	BUG_ON(!rdev->sb_page);

	if (rdev->sb_loaded && !force_reload)
		return 0;

	rdev->sb_loaded = 0;

	if (!sync_page_io(rdev, 0, size, rdev->sb_page, REQ_OP_READ, true)) {
		DMERR("Failed to read superblock of device at position %d",
		      rdev->raid_disk);
		md_error(rdev->mddev, rdev);
		set_bit(Faulty, &rdev->flags);
		return -EIO;
	}

	rdev->sb_loaded = 1;

	return 0;
}

static void sb_retrieve_failed_devices(struct dm_raid_superblock *sb, uint64_t *failed_devices)
{
	failed_devices[0] = le64_to_cpu(sb->failed_devices);
	memset(failed_devices + 1, 0, sizeof(sb->extended_failed_devices));

	if (le32_to_cpu(sb->compat_features) & FEATURE_FLAG_SUPPORTS_V190) {
		int i = ARRAY_SIZE(sb->extended_failed_devices);

		while (i--)
			failed_devices[i+1] = le64_to_cpu(sb->extended_failed_devices[i]);
	}
}

static void sb_update_failed_devices(struct dm_raid_superblock *sb, uint64_t *failed_devices)
{
	int i = ARRAY_SIZE(sb->extended_failed_devices);

	sb->failed_devices = cpu_to_le64(failed_devices[0]);
	while (i--)
		sb->extended_failed_devices[i] = cpu_to_le64(failed_devices[i+1]);
}

 
static void super_sync(struct mddev *mddev, struct md_rdev *rdev)
{
	bool update_failed_devices = false;
	unsigned int i;
	uint64_t failed_devices[DISKS_ARRAY_ELEMS];
	struct dm_raid_superblock *sb;
	struct raid_set *rs = container_of(mddev, struct raid_set, md);

	 
	if (!rdev->meta_bdev)
		return;

	BUG_ON(!rdev->sb_page);

	sb = page_address(rdev->sb_page);

	sb_retrieve_failed_devices(sb, failed_devices);

	for (i = 0; i < rs->raid_disks; i++)
		if (!rs->dev[i].data_dev || test_bit(Faulty, &rs->dev[i].rdev.flags)) {
			update_failed_devices = true;
			set_bit(i, (void *) failed_devices);
		}

	if (update_failed_devices)
		sb_update_failed_devices(sb, failed_devices);

	sb->magic = cpu_to_le32(DM_RAID_MAGIC);
	sb->compat_features = cpu_to_le32(FEATURE_FLAG_SUPPORTS_V190);

	sb->num_devices = cpu_to_le32(mddev->raid_disks);
	sb->array_position = cpu_to_le32(rdev->raid_disk);

	sb->events = cpu_to_le64(mddev->events);

	sb->disk_recovery_offset = cpu_to_le64(rdev->recovery_offset);
	sb->array_resync_offset = cpu_to_le64(mddev->recovery_cp);

	sb->level = cpu_to_le32(mddev->level);
	sb->layout = cpu_to_le32(mddev->layout);
	sb->stripe_sectors = cpu_to_le32(mddev->chunk_sectors);

	 
	sb->new_level = cpu_to_le32(mddev->new_level);
	sb->new_layout = cpu_to_le32(mddev->new_layout);
	sb->new_stripe_sectors = cpu_to_le32(mddev->new_chunk_sectors);

	sb->delta_disks = cpu_to_le32(mddev->delta_disks);

	smp_rmb();  
	sb->reshape_position = cpu_to_le64(mddev->reshape_position);
	if (le64_to_cpu(sb->reshape_position) != MaxSector) {
		 
		sb->flags |= cpu_to_le32(SB_FLAG_RESHAPE_ACTIVE);

		if (mddev->delta_disks < 0 || mddev->reshape_backwards)
			sb->flags |= cpu_to_le32(SB_FLAG_RESHAPE_BACKWARDS);
	} else {
		 
		sb->flags &= ~(cpu_to_le32(SB_FLAG_RESHAPE_ACTIVE|SB_FLAG_RESHAPE_BACKWARDS));
	}

	sb->array_sectors = cpu_to_le64(mddev->array_sectors);
	sb->data_offset = cpu_to_le64(rdev->data_offset);
	sb->new_data_offset = cpu_to_le64(rdev->new_data_offset);
	sb->sectors = cpu_to_le64(rdev->sectors);
	sb->incompat_features = cpu_to_le32(0);

	 
	memset(sb + 1, 0, rdev->sb_size - sizeof(*sb));
}

 
static int super_load(struct md_rdev *rdev, struct md_rdev *refdev)
{
	int r;
	struct dm_raid_superblock *sb;
	struct dm_raid_superblock *refsb;
	uint64_t events_sb, events_refsb;

	r = read_disk_sb(rdev, rdev->sb_size, false);
	if (r)
		return r;

	sb = page_address(rdev->sb_page);

	 
	if ((sb->magic != cpu_to_le32(DM_RAID_MAGIC)) ||
	    (!test_bit(In_sync, &rdev->flags) && !rdev->recovery_offset)) {
		super_sync(rdev->mddev, rdev);

		set_bit(FirstUse, &rdev->flags);
		sb->compat_features = cpu_to_le32(FEATURE_FLAG_SUPPORTS_V190);

		 
		set_bit(MD_SB_CHANGE_DEVS, &rdev->mddev->sb_flags);

		 
		return refdev ? 0 : 1;
	}

	if (!refdev)
		return 1;

	events_sb = le64_to_cpu(sb->events);

	refsb = page_address(refdev->sb_page);
	events_refsb = le64_to_cpu(refsb->events);

	return (events_sb > events_refsb) ? 1 : 0;
}

static int super_init_validation(struct raid_set *rs, struct md_rdev *rdev)
{
	int role;
	struct mddev *mddev = &rs->md;
	uint64_t events_sb;
	uint64_t failed_devices[DISKS_ARRAY_ELEMS];
	struct dm_raid_superblock *sb;
	uint32_t new_devs = 0, rebuild_and_new = 0, rebuilds = 0;
	struct md_rdev *r;
	struct dm_raid_superblock *sb2;

	sb = page_address(rdev->sb_page);
	events_sb = le64_to_cpu(sb->events);

	 
	mddev->events = events_sb ? : 1;

	mddev->reshape_position = MaxSector;

	mddev->raid_disks = le32_to_cpu(sb->num_devices);
	mddev->level = le32_to_cpu(sb->level);
	mddev->layout = le32_to_cpu(sb->layout);
	mddev->chunk_sectors = le32_to_cpu(sb->stripe_sectors);

	 
	if (le32_to_cpu(sb->compat_features) & FEATURE_FLAG_SUPPORTS_V190) {
		 
		mddev->new_level = le32_to_cpu(sb->new_level);
		mddev->new_layout = le32_to_cpu(sb->new_layout);
		mddev->new_chunk_sectors = le32_to_cpu(sb->new_stripe_sectors);
		mddev->delta_disks = le32_to_cpu(sb->delta_disks);
		mddev->array_sectors = le64_to_cpu(sb->array_sectors);

		 
		if (le32_to_cpu(sb->flags) & SB_FLAG_RESHAPE_ACTIVE) {
			if (test_bit(__CTR_FLAG_DELTA_DISKS, &rs->ctr_flags)) {
				DMERR("Reshape requested but raid set is still reshaping");
				return -EINVAL;
			}

			if (mddev->delta_disks < 0 ||
			    (!mddev->delta_disks && (le32_to_cpu(sb->flags) & SB_FLAG_RESHAPE_BACKWARDS)))
				mddev->reshape_backwards = 1;
			else
				mddev->reshape_backwards = 0;

			mddev->reshape_position = le64_to_cpu(sb->reshape_position);
			rs->raid_type = get_raid_type_by_ll(mddev->level, mddev->layout);
		}

	} else {
		 
		struct raid_type *rt_cur = get_raid_type_by_ll(mddev->level, mddev->layout);
		struct raid_type *rt_new = get_raid_type_by_ll(mddev->new_level, mddev->new_layout);

		if (rs_takeover_requested(rs)) {
			if (rt_cur && rt_new)
				DMERR("Takeover raid sets from %s to %s not yet supported by metadata. (raid level change)",
				      rt_cur->name, rt_new->name);
			else
				DMERR("Takeover raid sets not yet supported by metadata. (raid level change)");
			return -EINVAL;
		} else if (rs_reshape_requested(rs)) {
			DMERR("Reshaping raid sets not yet supported by metadata. (raid layout change keeping level)");
			if (mddev->layout != mddev->new_layout) {
				if (rt_cur && rt_new)
					DMERR("	 current layout %s vs new layout %s",
					      rt_cur->name, rt_new->name);
				else
					DMERR("	 current layout 0x%X vs new layout 0x%X",
					      le32_to_cpu(sb->layout), mddev->new_layout);
			}
			if (mddev->chunk_sectors != mddev->new_chunk_sectors)
				DMERR("	 current stripe sectors %u vs new stripe sectors %u",
				      mddev->chunk_sectors, mddev->new_chunk_sectors);
			if (rs->delta_disks)
				DMERR("	 current %u disks vs new %u disks",
				      mddev->raid_disks, mddev->raid_disks + rs->delta_disks);
			if (rs_is_raid10(rs)) {
				DMERR("	 Old layout: %s w/ %u copies",
				      raid10_md_layout_to_format(mddev->layout),
				      raid10_md_layout_to_copies(mddev->layout));
				DMERR("	 New layout: %s w/ %u copies",
				      raid10_md_layout_to_format(mddev->new_layout),
				      raid10_md_layout_to_copies(mddev->new_layout));
			}
			return -EINVAL;
		}

		DMINFO("Discovered old metadata format; upgrading to extended metadata format");
	}

	if (!test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags))
		mddev->recovery_cp = le64_to_cpu(sb->array_resync_offset);

	 
	rdev_for_each(r, mddev) {
		if (test_bit(Journal, &rdev->flags))
			continue;

		if (test_bit(FirstUse, &r->flags))
			new_devs++;

		if (!test_bit(In_sync, &r->flags)) {
			DMINFO("Device %d specified for rebuild; clearing superblock",
				r->raid_disk);
			rebuilds++;

			if (test_bit(FirstUse, &r->flags))
				rebuild_and_new++;
		}
	}

	if (new_devs == rs->raid_disks || !rebuilds) {
		 
		if (new_devs == rs->raid_disks) {
			DMINFO("Superblocks created for new raid set");
			set_bit(MD_ARRAY_FIRST_USE, &mddev->flags);
		} else if (new_devs != rebuilds &&
			   new_devs != rs->delta_disks) {
			DMERR("New device injected into existing raid set without "
			      "'delta_disks' or 'rebuild' parameter specified");
			return -EINVAL;
		}
	} else if (new_devs && new_devs != rebuilds) {
		DMERR("%u 'rebuild' devices cannot be injected into"
		      " a raid set with %u other first-time devices",
		      rebuilds, new_devs);
		return -EINVAL;
	} else if (rebuilds) {
		if (rebuild_and_new && rebuilds != rebuild_and_new) {
			DMERR("new device%s provided without 'rebuild'",
			      new_devs > 1 ? "s" : "");
			return -EINVAL;
		} else if (!test_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags) && rs_is_recovering(rs)) {
			DMERR("'rebuild' specified while raid set is not in-sync (recovery_cp=%llu)",
			      (unsigned long long) mddev->recovery_cp);
			return -EINVAL;
		} else if (rs_is_reshaping(rs)) {
			DMERR("'rebuild' specified while raid set is being reshaped (reshape_position=%llu)",
			      (unsigned long long) mddev->reshape_position);
			return -EINVAL;
		}
	}

	 
	sb_retrieve_failed_devices(sb, failed_devices);
	rdev_for_each(r, mddev) {
		if (test_bit(Journal, &rdev->flags) ||
		    !r->sb_page)
			continue;
		sb2 = page_address(r->sb_page);
		sb2->failed_devices = 0;
		memset(sb2->extended_failed_devices, 0, sizeof(sb2->extended_failed_devices));

		 
		if (!test_bit(FirstUse, &r->flags) && (r->raid_disk >= 0)) {
			role = le32_to_cpu(sb2->array_position);
			if (role < 0)
				continue;

			if (role != r->raid_disk) {
				if (rs_is_raid10(rs) && __is_raid10_near(mddev->layout)) {
					if (mddev->raid_disks % __raid10_near_copies(mddev->layout) ||
					    rs->raid_disks % rs->raid10_copies) {
						rs->ti->error =
							"Cannot change raid10 near set to odd # of devices!";
						return -EINVAL;
					}

					sb2->array_position = cpu_to_le32(r->raid_disk);

				} else if (!(rs_is_raid10(rs) && rt_is_raid0(rs->raid_type)) &&
					   !(rs_is_raid0(rs) && rt_is_raid10(rs->raid_type)) &&
					   !rt_is_raid1(rs->raid_type)) {
					rs->ti->error = "Cannot change device positions in raid set";
					return -EINVAL;
				}

				DMINFO("raid device #%d now at position #%d", role, r->raid_disk);
			}

			 
			if (test_bit(role, (void *) failed_devices))
				set_bit(Faulty, &r->flags);
		}
	}

	return 0;
}

static int super_validate(struct raid_set *rs, struct md_rdev *rdev)
{
	struct mddev *mddev = &rs->md;
	struct dm_raid_superblock *sb;

	if (rs_is_raid0(rs) || !rdev->sb_page || rdev->raid_disk < 0)
		return 0;

	sb = page_address(rdev->sb_page);

	 
	if (!mddev->events && super_init_validation(rs, rdev))
		return -EINVAL;

	if (le32_to_cpu(sb->compat_features) &&
	    le32_to_cpu(sb->compat_features) != FEATURE_FLAG_SUPPORTS_V190) {
		rs->ti->error = "Unable to assemble array: Unknown flag(s) in compatible feature flags";
		return -EINVAL;
	}

	if (sb->incompat_features) {
		rs->ti->error = "Unable to assemble array: No incompatible feature flags supported yet";
		return -EINVAL;
	}

	 
	mddev->bitmap_info.offset = (rt_is_raid0(rs->raid_type) || rs->journal_dev.dev) ? 0 : to_sector(4096);
	mddev->bitmap_info.default_offset = mddev->bitmap_info.offset;

	if (!test_and_clear_bit(FirstUse, &rdev->flags)) {
		 
		if (le32_to_cpu(sb->compat_features) & FEATURE_FLAG_SUPPORTS_V190)
			rdev->sectors = le64_to_cpu(sb->sectors);

		rdev->recovery_offset = le64_to_cpu(sb->disk_recovery_offset);
		if (rdev->recovery_offset == MaxSector)
			set_bit(In_sync, &rdev->flags);
		 
		else if (!rs_is_reshaping(rs))
			clear_bit(In_sync, &rdev->flags);  
	}

	 
	if (test_and_clear_bit(Faulty, &rdev->flags)) {
		rdev->recovery_offset = 0;
		clear_bit(In_sync, &rdev->flags);
		rdev->saved_raid_disk = rdev->raid_disk;
	}

	 
	rdev->data_offset = le64_to_cpu(sb->data_offset);
	rdev->new_data_offset = le64_to_cpu(sb->new_data_offset);

	return 0;
}

 
static int analyse_superblocks(struct dm_target *ti, struct raid_set *rs)
{
	int r;
	struct md_rdev *rdev, *freshest;
	struct mddev *mddev = &rs->md;

	freshest = NULL;
	rdev_for_each(rdev, mddev) {
		if (test_bit(Journal, &rdev->flags))
			continue;

		if (!rdev->meta_bdev)
			continue;

		 
		rdev->sb_start = 0;
		rdev->sb_size = bdev_logical_block_size(rdev->meta_bdev);
		if (rdev->sb_size < sizeof(struct dm_raid_superblock) || rdev->sb_size > PAGE_SIZE) {
			DMERR("superblock size of a logical block is no longer valid");
			return -EINVAL;
		}

		 
		if (test_bit(__CTR_FLAG_SYNC, &rs->ctr_flags))
			continue;

		r = super_load(rdev, freshest);

		switch (r) {
		case 1:
			freshest = rdev;
			break;
		case 0:
			break;
		default:
			 
			 
			if (rs_is_raid0(rs))
				continue;

			 
			rdev->raid_disk = rdev->saved_raid_disk = -1;
			break;
		}
	}

	if (!freshest)
		return 0;

	 
	rs->ti->error = "Unable to assemble array: Invalid superblocks";
	if (super_validate(rs, freshest))
		return -EINVAL;

	if (validate_raid_redundancy(rs)) {
		rs->ti->error = "Insufficient redundancy to activate array";
		return -EINVAL;
	}

	rdev_for_each(rdev, mddev)
		if (!test_bit(Journal, &rdev->flags) &&
		    rdev != freshest &&
		    super_validate(rs, rdev))
			return -EINVAL;
	return 0;
}

 
static int rs_adjust_data_offsets(struct raid_set *rs)
{
	sector_t data_offset = 0, new_data_offset = 0;
	struct md_rdev *rdev;

	 
	if (!test_bit(__CTR_FLAG_DATA_OFFSET, &rs->ctr_flags)) {
		if (!rs_is_reshapable(rs))
			goto out;

		return 0;
	}

	 
	rdev = &rs->dev[0].rdev;

	if (rs->delta_disks < 0) {
		 
		data_offset = 0;
		new_data_offset = rs->data_offset;

	} else if (rs->delta_disks > 0) {
		 
		data_offset = rs->data_offset;
		new_data_offset = 0;

	} else {
		 
		data_offset = rs->data_offset ? rdev->data_offset : 0;
		new_data_offset = data_offset ? 0 : rs->data_offset;
		set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
	}

	 
	if (rs->data_offset &&
	    bdev_nr_sectors(rdev->bdev) - rs->md.dev_sectors < MIN_FREE_RESHAPE_SPACE) {
		rs->ti->error = data_offset ? "No space for forward reshape" :
					      "No space for backward reshape";
		return -ENOSPC;
	}
out:
	 
	if (rs->md.recovery_cp < rs->md.dev_sectors)
		rs->md.recovery_cp += rs->dev[0].rdev.data_offset;

	 
	rdev_for_each(rdev, &rs->md) {
		if (!test_bit(Journal, &rdev->flags)) {
			rdev->data_offset = data_offset;
			rdev->new_data_offset = new_data_offset;
		}
	}

	return 0;
}

 
static void __reorder_raid_disk_indexes(struct raid_set *rs)
{
	int i = 0;
	struct md_rdev *rdev;

	rdev_for_each(rdev, &rs->md) {
		if (!test_bit(Journal, &rdev->flags)) {
			rdev->raid_disk = i++;
			rdev->saved_raid_disk = rdev->new_raid_disk = -1;
		}
	}
}

 
static int rs_setup_takeover(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;
	struct md_rdev *rdev;
	unsigned int d = mddev->raid_disks = rs->raid_disks;
	sector_t new_data_offset = rs->dev[0].rdev.data_offset ? 0 : rs->data_offset;

	if (rt_is_raid10(rs->raid_type)) {
		if (rs_is_raid0(rs)) {
			 
			__reorder_raid_disk_indexes(rs);

			 
			mddev->layout = raid10_format_to_md_layout(rs, ALGORITHM_RAID10_FAR,
								   rs->raid10_copies);
		} else if (rs_is_raid1(rs))
			 
			mddev->layout = raid10_format_to_md_layout(rs, ALGORITHM_RAID10_NEAR,
								   rs->raid_disks);
		else
			return -EINVAL;

	}

	clear_bit(MD_ARRAY_FIRST_USE, &mddev->flags);
	mddev->recovery_cp = MaxSector;

	while (d--) {
		rdev = &rs->dev[d].rdev;

		if (test_bit(d, (void *) rs->rebuild_disks)) {
			clear_bit(In_sync, &rdev->flags);
			clear_bit(Faulty, &rdev->flags);
			mddev->recovery_cp = rdev->recovery_offset = 0;
			 
			set_bit(MD_ARRAY_FIRST_USE, &mddev->flags);
		}

		rdev->new_data_offset = new_data_offset;
	}

	return 0;
}

 
static int rs_prepare_reshape(struct raid_set *rs)
{
	bool reshape;
	struct mddev *mddev = &rs->md;

	if (rs_is_raid10(rs)) {
		if (rs->raid_disks != mddev->raid_disks &&
		    __is_raid10_near(mddev->layout) &&
		    rs->raid10_copies &&
		    rs->raid10_copies != __raid10_near_copies(mddev->layout)) {
			 
			if (rs->raid_disks % rs->raid10_copies) {
				rs->ti->error = "Can't reshape raid10 mirror groups";
				return -EINVAL;
			}

			 
			__reorder_raid_disk_indexes(rs);
			mddev->layout = raid10_format_to_md_layout(rs, ALGORITHM_RAID10_NEAR,
								   rs->raid10_copies);
			mddev->new_layout = mddev->layout;
			reshape = false;
		} else
			reshape = true;

	} else if (rs_is_raid456(rs))
		reshape = true;

	else if (rs_is_raid1(rs)) {
		if (rs->delta_disks) {
			 
			mddev->degraded = rs->delta_disks < 0 ? -rs->delta_disks : rs->delta_disks;
			reshape = true;
		} else {
			 
			mddev->raid_disks = rs->raid_disks;
			reshape = false;
		}
	} else {
		rs->ti->error = "Called with bogus raid type";
		return -EINVAL;
	}

	if (reshape) {
		set_bit(RT_FLAG_RESHAPE_RS, &rs->runtime_flags);
		set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
	} else if (mddev->raid_disks < rs->raid_disks)
		 
		set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);

	return 0;
}

 
static sector_t _get_reshape_sectors(struct raid_set *rs)
{
	struct md_rdev *rdev;
	sector_t reshape_sectors = 0;

	rdev_for_each(rdev, &rs->md)
		if (!test_bit(Journal, &rdev->flags)) {
			reshape_sectors = (rdev->data_offset > rdev->new_data_offset) ?
					rdev->data_offset - rdev->new_data_offset :
					rdev->new_data_offset - rdev->data_offset;
			break;
		}

	return max(reshape_sectors, (sector_t) rs->data_offset);
}

 
static int rs_setup_reshape(struct raid_set *rs)
{
	int r = 0;
	unsigned int cur_raid_devs, d;
	sector_t reshape_sectors = _get_reshape_sectors(rs);
	struct mddev *mddev = &rs->md;
	struct md_rdev *rdev;

	mddev->delta_disks = rs->delta_disks;
	cur_raid_devs = mddev->raid_disks;

	 
	if (mddev->delta_disks &&
	    mddev->layout != mddev->new_layout) {
		DMINFO("Ignoring invalid layout change with delta_disks=%d", rs->delta_disks);
		mddev->new_layout = mddev->layout;
	}

	 

	 
	if (rs->delta_disks > 0) {
		 
		for (d = cur_raid_devs; d < rs->raid_disks; d++) {
			rdev = &rs->dev[d].rdev;
			clear_bit(In_sync, &rdev->flags);

			 
			rdev->saved_raid_disk = -1;
			rdev->raid_disk = d;

			rdev->sectors = mddev->dev_sectors;
			rdev->recovery_offset = rs_is_raid1(rs) ? 0 : MaxSector;
		}

		mddev->reshape_backwards = 0;  

	 
	} else if (rs->delta_disks < 0) {
		r = rs_set_dev_and_array_sectors(rs, rs->ti->len, true);
		mddev->reshape_backwards = 1;  

	 
	} else {
		 
		mddev->reshape_backwards = rs->dev[0].rdev.data_offset ? 0 : 1;
	}

	 
	if (!mddev->reshape_backwards)
		rdev_for_each(rdev, &rs->md)
			if (!test_bit(Journal, &rdev->flags))
				rdev->sectors += reshape_sectors;

	return r;
}

 
static void rs_reset_inconclusive_reshape(struct raid_set *rs)
{
	if (!rs_is_reshaping(rs) && rs_is_layout_change(rs, true)) {
		rs_set_cur(rs);
		rs->md.delta_disks = 0;
		rs->md.reshape_backwards = 0;
	}
}

 
static void configure_discard_support(struct raid_set *rs)
{
	int i;
	bool raid456;
	struct dm_target *ti = rs->ti;

	 
	raid456 = rs_is_raid456(rs);

	for (i = 0; i < rs->raid_disks; i++) {
		if (!rs->dev[i].rdev.bdev ||
		    !bdev_max_discard_sectors(rs->dev[i].rdev.bdev))
			return;

		if (raid456) {
			if (!devices_handle_discard_safely) {
				DMERR("raid456 discard support disabled due to discard_zeroes_data uncertainty.");
				DMERR("Set dm-raid.devices_handle_discard_safely=Y to override.");
				return;
			}
		}
	}

	ti->num_discard_bios = 1;
}

 
static int raid_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	int r;
	bool resize = false;
	struct raid_type *rt;
	unsigned int num_raid_params, num_raid_devs;
	sector_t sb_array_sectors, rdev_sectors, reshape_sectors;
	struct raid_set *rs = NULL;
	const char *arg;
	struct rs_layout rs_layout;
	struct dm_arg_set as = { argc, argv }, as_nrd;
	struct dm_arg _args[] = {
		{ 0, as.argc, "Cannot understand number of raid parameters" },
		{ 1, 254, "Cannot understand number of raid devices parameters" }
	};

	arg = dm_shift_arg(&as);
	if (!arg) {
		ti->error = "No arguments";
		return -EINVAL;
	}

	rt = get_raid_type(arg);
	if (!rt) {
		ti->error = "Unrecognised raid_type";
		return -EINVAL;
	}

	 
	if (dm_read_arg_group(_args, &as, &num_raid_params, &ti->error))
		return -EINVAL;

	 
	as_nrd = as;
	dm_consume_args(&as_nrd, num_raid_params);
	_args[1].max = (as_nrd.argc - 1) / 2;
	if (dm_read_arg(_args + 1, &as_nrd, &num_raid_devs, &ti->error))
		return -EINVAL;

	if (!__within_range(num_raid_devs, 1, MAX_RAID_DEVICES)) {
		ti->error = "Invalid number of supplied raid devices";
		return -EINVAL;
	}

	rs = raid_set_alloc(ti, rt, num_raid_devs);
	if (IS_ERR(rs))
		return PTR_ERR(rs);

	r = parse_raid_params(rs, &as, num_raid_params);
	if (r)
		goto bad;

	r = parse_dev_params(rs, &as);
	if (r)
		goto bad;

	rs->md.sync_super = super_sync;

	 
	r = rs_set_dev_and_array_sectors(rs, rs->ti->len, false);
	if (r)
		goto bad;

	 
	rs->array_sectors = rs->md.array_sectors;
	rs->dev_sectors = rs->md.dev_sectors;

	 
	rs_config_backup(rs, &rs_layout);

	r = analyse_superblocks(ti, rs);
	if (r)
		goto bad;

	 
	sb_array_sectors = rs->md.array_sectors;
	rdev_sectors = __rdev_sectors(rs);
	if (!rdev_sectors) {
		ti->error = "Invalid rdev size";
		r = -EINVAL;
		goto bad;
	}


	reshape_sectors = _get_reshape_sectors(rs);
	if (rs->dev_sectors != rdev_sectors) {
		resize = (rs->dev_sectors != rdev_sectors - reshape_sectors);
		if (rs->dev_sectors > rdev_sectors - reshape_sectors)
			set_bit(RT_FLAG_RS_GROW, &rs->runtime_flags);
	}

	INIT_WORK(&rs->md.event_work, do_table_event);
	ti->private = rs;
	ti->num_flush_bios = 1;
	ti->needs_bio_set_dev = true;

	 
	rs_config_restore(rs, &rs_layout);

	 
	if (test_bit(MD_ARRAY_FIRST_USE, &rs->md.flags)) {
		 
		if (rs_is_raid6(rs) &&
		    test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags)) {
			ti->error = "'nosync' not allowed for new raid6 set";
			r = -EINVAL;
			goto bad;
		}
		rs_setup_recovery(rs, 0);
		set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
		rs_set_new(rs);
	} else if (rs_is_recovering(rs)) {
		 
		goto size_check;
	} else if (rs_is_reshaping(rs)) {
		 
		if (resize) {
			ti->error = "Can't resize a reshaping raid set";
			r = -EPERM;
			goto bad;
		}
		 
	} else if (rs_takeover_requested(rs)) {
		if (rs_is_reshaping(rs)) {
			ti->error = "Can't takeover a reshaping raid set";
			r = -EPERM;
			goto bad;
		}

		 
		if (test_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags)) {
			ti->error = "Can't takeover a journaled raid4/5/6 set";
			r = -EPERM;
			goto bad;
		}

		 
		r = rs_check_takeover(rs);
		if (r)
			goto bad;

		r = rs_setup_takeover(rs);
		if (r)
			goto bad;

		set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
		 
		rs_setup_recovery(rs, MaxSector);
		rs_set_new(rs);
	} else if (rs_reshape_requested(rs)) {
		 
		clear_bit(RT_FLAG_RS_GROW, &rs->runtime_flags);

		 

		 
		if (test_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags)) {
			ti->error = "Can't reshape a journaled raid4/5/6 set";
			r = -EPERM;
			goto bad;
		}

		 
		if (reshape_sectors || rs_is_raid1(rs)) {
			 
			r = rs_prepare_reshape(rs);
			if (r)
				goto bad;

			 
			rs_setup_recovery(rs, MaxSector);
		}
		rs_set_cur(rs);
	} else {
size_check:
		 
		if (test_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags)) {
			clear_bit(RT_FLAG_RS_GROW, &rs->runtime_flags);
			set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
			rs_setup_recovery(rs, MaxSector);
		} else if (test_bit(RT_FLAG_RS_GROW, &rs->runtime_flags)) {
			 
			r = rs_set_dev_and_array_sectors(rs, sb_array_sectors, false);
			if (r)
				goto bad;

			rs_setup_recovery(rs, rs->md.recovery_cp < rs->md.dev_sectors ? rs->md.recovery_cp : rs->md.dev_sectors);
		} else {
			 
			r = rs_set_dev_and_array_sectors(rs, rs->ti->len, false);
			if (r)
				goto bad;

			if (sb_array_sectors > rs->array_sectors)
				set_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags);
		}
		rs_set_cur(rs);
	}

	 
	r = rs_adjust_data_offsets(rs);
	if (r)
		goto bad;

	 
	rs_reset_inconclusive_reshape(rs);

	 
	rs->md.ro = 1;
	rs->md.in_sync = 1;

	 
	set_bit(MD_RECOVERY_FROZEN, &rs->md.recovery);

	 
	mddev_lock_nointr(&rs->md);
	r = md_run(&rs->md);
	rs->md.in_sync = 0;  
	if (r) {
		ti->error = "Failed to run raid array";
		mddev_unlock(&rs->md);
		goto bad;
	}

	r = md_start(&rs->md);
	if (r) {
		ti->error = "Failed to start raid array";
		goto bad_unlock;
	}

	 
	if (test_bit(__CTR_FLAG_JOURNAL_MODE, &rs->ctr_flags)) {
		r = r5c_journal_mode_set(&rs->md, rs->journal_dev.mode);
		if (r) {
			ti->error = "Failed to set raid4/5/6 journal mode";
			goto bad_unlock;
		}
	}

	mddev_suspend(&rs->md);
	set_bit(RT_FLAG_RS_SUSPENDED, &rs->runtime_flags);

	 
	if (rs_is_raid456(rs)) {
		r = rs_set_raid456_stripe_cache(rs);
		if (r)
			goto bad_unlock;
	}

	 
	if (test_bit(RT_FLAG_RESHAPE_RS, &rs->runtime_flags)) {
		r = rs_check_reshape(rs);
		if (r)
			goto bad_unlock;

		 
		rs_config_restore(rs, &rs_layout);

		if (rs->md.pers->start_reshape) {
			r = rs->md.pers->check_reshape(&rs->md);
			if (r) {
				ti->error = "Reshape check failed";
				goto bad_unlock;
			}
		}
	}

	 
	configure_discard_support(rs);

	mddev_unlock(&rs->md);
	return 0;

bad_unlock:
	md_stop(&rs->md);
	mddev_unlock(&rs->md);
bad:
	raid_set_free(rs);

	return r;
}

static void raid_dtr(struct dm_target *ti)
{
	struct raid_set *rs = ti->private;

	mddev_lock_nointr(&rs->md);
	md_stop(&rs->md);
	mddev_unlock(&rs->md);
	raid_set_free(rs);
}

static int raid_map(struct dm_target *ti, struct bio *bio)
{
	struct raid_set *rs = ti->private;
	struct mddev *mddev = &rs->md;

	 
	if (unlikely(bio_end_sector(bio) > mddev->array_sectors))
		return DM_MAPIO_REQUEUE;

	md_handle_request(mddev, bio);

	return DM_MAPIO_SUBMITTED;
}

 
enum sync_state { st_frozen, st_reshape, st_resync, st_check, st_repair, st_recover, st_idle };
static const char *sync_str(enum sync_state state)
{
	 
	static const char *sync_strs[] = {
		"frozen",
		"reshape",
		"resync",
		"check",
		"repair",
		"recover",
		"idle"
	};

	return __within_range(state, 0, ARRAY_SIZE(sync_strs) - 1) ? sync_strs[state] : "undef";
};

 
static enum sync_state decipher_sync_action(struct mddev *mddev, unsigned long recovery)
{
	if (test_bit(MD_RECOVERY_FROZEN, &recovery))
		return st_frozen;

	 
	if (!test_bit(MD_RECOVERY_DONE, &recovery) &&
	    (test_bit(MD_RECOVERY_RUNNING, &recovery) ||
	     (!mddev->ro && test_bit(MD_RECOVERY_NEEDED, &recovery)))) {
		if (test_bit(MD_RECOVERY_RESHAPE, &recovery))
			return st_reshape;

		if (test_bit(MD_RECOVERY_SYNC, &recovery)) {
			if (!test_bit(MD_RECOVERY_REQUESTED, &recovery))
				return st_resync;
			if (test_bit(MD_RECOVERY_CHECK, &recovery))
				return st_check;
			return st_repair;
		}

		if (test_bit(MD_RECOVERY_RECOVER, &recovery))
			return st_recover;

		if (mddev->reshape_position != MaxSector)
			return st_reshape;
	}

	return st_idle;
}

 
static const char *__raid_dev_status(struct raid_set *rs, struct md_rdev *rdev)
{
	if (!rdev->bdev)
		return "-";
	else if (test_bit(Faulty, &rdev->flags))
		return "D";
	else if (test_bit(Journal, &rdev->flags))
		return (rs->journal_dev.mode == R5C_JOURNAL_MODE_WRITE_THROUGH) ? "A" : "a";
	else if (test_bit(RT_FLAG_RS_RESYNCING, &rs->runtime_flags) ||
		 (!test_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags) &&
		  !test_bit(In_sync, &rdev->flags)))
		return "a";
	else
		return "A";
}

 
static sector_t rs_get_progress(struct raid_set *rs, unsigned long recovery,
				enum sync_state state, sector_t resync_max_sectors)
{
	sector_t r;
	struct mddev *mddev = &rs->md;

	clear_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);
	clear_bit(RT_FLAG_RS_RESYNCING, &rs->runtime_flags);

	if (rs_is_raid0(rs)) {
		r = resync_max_sectors;
		set_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);

	} else {
		if (state == st_idle && !test_bit(MD_RECOVERY_INTR, &recovery))
			r = mddev->recovery_cp;
		else
			r = mddev->curr_resync_completed;

		if (state == st_idle && r >= resync_max_sectors) {
			 
			 
			if (test_bit(MD_RECOVERY_RECOVER, &recovery))
				set_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);

		} else if (state == st_recover)
			 
			;

		else if (state == st_resync || state == st_reshape)
			 
			set_bit(RT_FLAG_RS_RESYNCING, &rs->runtime_flags);

		else if (state == st_check || state == st_repair)
			 
			set_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);

		else if (test_bit(MD_RECOVERY_NEEDED, &recovery))
			 
			set_bit(RT_FLAG_RS_RESYNCING, &rs->runtime_flags);

		else {
			 
			struct md_rdev *rdev;

			set_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);
			rdev_for_each(rdev, mddev)
				if (!test_bit(Journal, &rdev->flags) &&
				    !test_bit(In_sync, &rdev->flags)) {
					clear_bit(RT_FLAG_RS_IN_SYNC, &rs->runtime_flags);
					break;
				}
		}
	}

	return min(r, resync_max_sectors);
}

 
static const char *__get_dev_name(struct dm_dev *dev)
{
	return dev ? dev->name : "-";
}

static void raid_status(struct dm_target *ti, status_type_t type,
			unsigned int status_flags, char *result, unsigned int maxlen)
{
	struct raid_set *rs = ti->private;
	struct mddev *mddev = &rs->md;
	struct r5conf *conf = rs_is_raid456(rs) ? mddev->private : NULL;
	int i, max_nr_stripes = conf ? conf->max_nr_stripes : 0;
	unsigned long recovery;
	unsigned int raid_param_cnt = 1;  
	unsigned int sz = 0;
	unsigned int rebuild_writemostly_count = 0;
	sector_t progress, resync_max_sectors, resync_mismatches;
	enum sync_state state;
	struct raid_type *rt;

	switch (type) {
	case STATUSTYPE_INFO:
		 
		rt = get_raid_type_by_ll(mddev->new_level, mddev->new_layout);
		if (!rt)
			return;

		DMEMIT("%s %d ", rt->name, mddev->raid_disks);

		 
		smp_rmb();
		 
		resync_max_sectors = test_bit(RT_FLAG_RS_PRERESUMED, &rs->runtime_flags) ?
				      mddev->resync_max_sectors : mddev->dev_sectors;
		recovery = rs->md.recovery;
		state = decipher_sync_action(mddev, recovery);
		progress = rs_get_progress(rs, recovery, state, resync_max_sectors);
		resync_mismatches = (mddev->last_sync_action && !strcasecmp(mddev->last_sync_action, "check")) ?
				    atomic64_read(&mddev->resync_mismatches) : 0;

		 
		for (i = 0; i < rs->raid_disks; i++)
			DMEMIT(__raid_dev_status(rs, &rs->dev[i].rdev));

		 
		DMEMIT(" %llu/%llu", (unsigned long long) progress,
				     (unsigned long long) resync_max_sectors);

		 
		DMEMIT(" %s", sync_str(state));

		 
		DMEMIT(" %llu", (unsigned long long) resync_mismatches);

		 
		DMEMIT(" %llu", (unsigned long long) rs->dev[0].rdev.data_offset);

		 
		DMEMIT(" %s", test_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags) ?
			      __raid_dev_status(rs, &rs->journal_dev.rdev) : "-");
		break;

	case STATUSTYPE_TABLE:
		 

		 
		for (i = 0; i < rs->raid_disks; i++) {
			rebuild_writemostly_count += (test_bit(i, (void *) rs->rebuild_disks) ? 2 : 0) +
						     (test_bit(WriteMostly, &rs->dev[i].rdev.flags) ? 2 : 0);
		}
		rebuild_writemostly_count -= (test_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags) ? 2 : 0) +
					     (test_bit(__CTR_FLAG_WRITE_MOSTLY, &rs->ctr_flags) ? 2 : 0);
		 
		raid_param_cnt += rebuild_writemostly_count +
				  hweight32(rs->ctr_flags & CTR_FLAG_OPTIONS_NO_ARGS) +
				  hweight32(rs->ctr_flags & CTR_FLAG_OPTIONS_ONE_ARG) * 2;
		 
		 
		DMEMIT("%s %u %u", rs->raid_type->name, raid_param_cnt, mddev->new_chunk_sectors);
		if (test_bit(__CTR_FLAG_SYNC, &rs->ctr_flags))
			DMEMIT(" %s", dm_raid_arg_name_by_flag(CTR_FLAG_SYNC));
		if (test_bit(__CTR_FLAG_NOSYNC, &rs->ctr_flags))
			DMEMIT(" %s", dm_raid_arg_name_by_flag(CTR_FLAG_NOSYNC));
		if (test_bit(__CTR_FLAG_REBUILD, &rs->ctr_flags))
			for (i = 0; i < rs->raid_disks; i++)
				if (test_bit(i, (void *) rs->rebuild_disks))
					DMEMIT(" %s %u", dm_raid_arg_name_by_flag(CTR_FLAG_REBUILD), i);
		if (test_bit(__CTR_FLAG_DAEMON_SLEEP, &rs->ctr_flags))
			DMEMIT(" %s %lu", dm_raid_arg_name_by_flag(CTR_FLAG_DAEMON_SLEEP),
					  mddev->bitmap_info.daemon_sleep);
		if (test_bit(__CTR_FLAG_MIN_RECOVERY_RATE, &rs->ctr_flags))
			DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_MIN_RECOVERY_RATE),
					 mddev->sync_speed_min);
		if (test_bit(__CTR_FLAG_MAX_RECOVERY_RATE, &rs->ctr_flags))
			DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_MAX_RECOVERY_RATE),
					 mddev->sync_speed_max);
		if (test_bit(__CTR_FLAG_WRITE_MOSTLY, &rs->ctr_flags))
			for (i = 0; i < rs->raid_disks; i++)
				if (test_bit(WriteMostly, &rs->dev[i].rdev.flags))
					DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_WRITE_MOSTLY),
					       rs->dev[i].rdev.raid_disk);
		if (test_bit(__CTR_FLAG_MAX_WRITE_BEHIND, &rs->ctr_flags))
			DMEMIT(" %s %lu", dm_raid_arg_name_by_flag(CTR_FLAG_MAX_WRITE_BEHIND),
					  mddev->bitmap_info.max_write_behind);
		if (test_bit(__CTR_FLAG_STRIPE_CACHE, &rs->ctr_flags))
			DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_STRIPE_CACHE),
					 max_nr_stripes);
		if (test_bit(__CTR_FLAG_REGION_SIZE, &rs->ctr_flags))
			DMEMIT(" %s %llu", dm_raid_arg_name_by_flag(CTR_FLAG_REGION_SIZE),
					   (unsigned long long) to_sector(mddev->bitmap_info.chunksize));
		if (test_bit(__CTR_FLAG_RAID10_COPIES, &rs->ctr_flags))
			DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_RAID10_COPIES),
					 raid10_md_layout_to_copies(mddev->layout));
		if (test_bit(__CTR_FLAG_RAID10_FORMAT, &rs->ctr_flags))
			DMEMIT(" %s %s", dm_raid_arg_name_by_flag(CTR_FLAG_RAID10_FORMAT),
					 raid10_md_layout_to_format(mddev->layout));
		if (test_bit(__CTR_FLAG_DELTA_DISKS, &rs->ctr_flags))
			DMEMIT(" %s %d", dm_raid_arg_name_by_flag(CTR_FLAG_DELTA_DISKS),
					 max(rs->delta_disks, mddev->delta_disks));
		if (test_bit(__CTR_FLAG_DATA_OFFSET, &rs->ctr_flags))
			DMEMIT(" %s %llu", dm_raid_arg_name_by_flag(CTR_FLAG_DATA_OFFSET),
					   (unsigned long long) rs->data_offset);
		if (test_bit(__CTR_FLAG_JOURNAL_DEV, &rs->ctr_flags))
			DMEMIT(" %s %s", dm_raid_arg_name_by_flag(CTR_FLAG_JOURNAL_DEV),
					__get_dev_name(rs->journal_dev.dev));
		if (test_bit(__CTR_FLAG_JOURNAL_MODE, &rs->ctr_flags))
			DMEMIT(" %s %s", dm_raid_arg_name_by_flag(CTR_FLAG_JOURNAL_MODE),
					 md_journal_mode_to_dm_raid(rs->journal_dev.mode));
		DMEMIT(" %d", rs->raid_disks);
		for (i = 0; i < rs->raid_disks; i++)
			DMEMIT(" %s %s", __get_dev_name(rs->dev[i].meta_dev),
					 __get_dev_name(rs->dev[i].data_dev));
		break;

	case STATUSTYPE_IMA:
		rt = get_raid_type_by_ll(mddev->new_level, mddev->new_layout);
		if (!rt)
			return;

		DMEMIT_TARGET_NAME_VERSION(ti->type);
		DMEMIT(",raid_type=%s,raid_disks=%d", rt->name, mddev->raid_disks);

		 
		smp_rmb();
		recovery = rs->md.recovery;
		state = decipher_sync_action(mddev, recovery);
		DMEMIT(",raid_state=%s", sync_str(state));

		for (i = 0; i < rs->raid_disks; i++) {
			DMEMIT(",raid_device_%d_status=", i);
			DMEMIT(__raid_dev_status(rs, &rs->dev[i].rdev));
		}

		if (rt_is_raid456(rt)) {
			DMEMIT(",journal_dev_mode=");
			switch (rs->journal_dev.mode) {
			case R5C_JOURNAL_MODE_WRITE_THROUGH:
				DMEMIT("%s",
				       _raid456_journal_mode[R5C_JOURNAL_MODE_WRITE_THROUGH].param);
				break;
			case R5C_JOURNAL_MODE_WRITE_BACK:
				DMEMIT("%s",
				       _raid456_journal_mode[R5C_JOURNAL_MODE_WRITE_BACK].param);
				break;
			default:
				DMEMIT("invalid");
				break;
			}
		}
		DMEMIT(";");
		break;
	}
}

static int raid_message(struct dm_target *ti, unsigned int argc, char **argv,
			char *result, unsigned int maxlen)
{
	struct raid_set *rs = ti->private;
	struct mddev *mddev = &rs->md;

	if (!mddev->pers || !mddev->pers->sync_request)
		return -EINVAL;

	if (!strcasecmp(argv[0], "frozen"))
		set_bit(MD_RECOVERY_FROZEN, &mddev->recovery);
	else
		clear_bit(MD_RECOVERY_FROZEN, &mddev->recovery);

	if (!strcasecmp(argv[0], "idle") || !strcasecmp(argv[0], "frozen")) {
		if (mddev->sync_thread) {
			set_bit(MD_RECOVERY_INTR, &mddev->recovery);
			md_reap_sync_thread(mddev);
		}
	} else if (decipher_sync_action(mddev, mddev->recovery) != st_idle)
		return -EBUSY;
	else if (!strcasecmp(argv[0], "resync"))
		;  
	else if (!strcasecmp(argv[0], "recover"))
		set_bit(MD_RECOVERY_RECOVER, &mddev->recovery);
	else {
		if (!strcasecmp(argv[0], "check")) {
			set_bit(MD_RECOVERY_CHECK, &mddev->recovery);
			set_bit(MD_RECOVERY_REQUESTED, &mddev->recovery);
			set_bit(MD_RECOVERY_SYNC, &mddev->recovery);
		} else if (!strcasecmp(argv[0], "repair")) {
			set_bit(MD_RECOVERY_REQUESTED, &mddev->recovery);
			set_bit(MD_RECOVERY_SYNC, &mddev->recovery);
		} else
			return -EINVAL;
	}
	if (mddev->ro == 2) {
		 
		mddev->ro = 0;
		if (!mddev->suspended)
			md_wakeup_thread(mddev->sync_thread);
	}
	set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	if (!mddev->suspended)
		md_wakeup_thread(mddev->thread);

	return 0;
}

static int raid_iterate_devices(struct dm_target *ti,
				iterate_devices_callout_fn fn, void *data)
{
	struct raid_set *rs = ti->private;
	unsigned int i;
	int r = 0;

	for (i = 0; !r && i < rs->raid_disks; i++) {
		if (rs->dev[i].data_dev) {
			r = fn(ti, rs->dev[i].data_dev,
			       0,  
			       rs->md.dev_sectors, data);
		}
	}

	return r;
}

static void raid_io_hints(struct dm_target *ti, struct queue_limits *limits)
{
	struct raid_set *rs = ti->private;
	unsigned int chunk_size_bytes = to_bytes(rs->md.chunk_sectors);

	blk_limits_io_min(limits, chunk_size_bytes);
	blk_limits_io_opt(limits, chunk_size_bytes * mddev_data_stripes(rs));
}

static void raid_postsuspend(struct dm_target *ti)
{
	struct raid_set *rs = ti->private;

	if (!test_and_set_bit(RT_FLAG_RS_SUSPENDED, &rs->runtime_flags)) {
		 
		if (!test_bit(MD_RECOVERY_FROZEN, &rs->md.recovery))
			md_stop_writes(&rs->md);

		mddev_lock_nointr(&rs->md);
		mddev_suspend(&rs->md);
		mddev_unlock(&rs->md);
	}
}

static void attempt_restore_of_faulty_devices(struct raid_set *rs)
{
	int i;
	uint64_t cleared_failed_devices[DISKS_ARRAY_ELEMS];
	unsigned long flags;
	bool cleared = false;
	struct dm_raid_superblock *sb;
	struct mddev *mddev = &rs->md;
	struct md_rdev *r;

	 
	if (!mddev->pers || !mddev->pers->hot_add_disk || !mddev->pers->hot_remove_disk)
		return;

	memset(cleared_failed_devices, 0, sizeof(cleared_failed_devices));

	for (i = 0; i < rs->raid_disks; i++) {
		r = &rs->dev[i].rdev;
		 
		if (test_bit(Journal, &r->flags))
			continue;

		if (test_bit(Faulty, &r->flags) &&
		    r->meta_bdev && !read_disk_sb(r, r->sb_size, true)) {
			DMINFO("Faulty %s device #%d has readable super block."
			       "  Attempting to revive it.",
			       rs->raid_type->name, i);

			 
			flags = r->flags;
			clear_bit(In_sync, &r->flags);  
			if (r->raid_disk >= 0) {
				if (mddev->pers->hot_remove_disk(mddev, r)) {
					 
					r->flags = flags;
					continue;
				}
			} else
				r->raid_disk = r->saved_raid_disk = i;

			clear_bit(Faulty, &r->flags);
			clear_bit(WriteErrorSeen, &r->flags);

			if (mddev->pers->hot_add_disk(mddev, r)) {
				 
				r->raid_disk = r->saved_raid_disk = -1;
				r->flags = flags;
			} else {
				clear_bit(In_sync, &r->flags);
				r->recovery_offset = 0;
				set_bit(i, (void *) cleared_failed_devices);
				cleared = true;
			}
		}
	}

	 
	if (cleared) {
		uint64_t failed_devices[DISKS_ARRAY_ELEMS];

		rdev_for_each(r, &rs->md) {
			if (test_bit(Journal, &r->flags))
				continue;

			sb = page_address(r->sb_page);
			sb_retrieve_failed_devices(sb, failed_devices);

			for (i = 0; i < DISKS_ARRAY_ELEMS; i++)
				failed_devices[i] &= ~cleared_failed_devices[i];

			sb_update_failed_devices(sb, failed_devices);
		}
	}
}

static int __load_dirty_region_bitmap(struct raid_set *rs)
{
	int r = 0;

	 
	if (!rs_is_raid0(rs) &&
	    !test_and_set_bit(RT_FLAG_RS_BITMAP_LOADED, &rs->runtime_flags)) {
		r = md_bitmap_load(&rs->md);
		if (r)
			DMERR("Failed to load bitmap");
	}

	return r;
}

 
static void rs_update_sbs(struct raid_set *rs)
{
	struct mddev *mddev = &rs->md;
	int ro = mddev->ro;

	set_bit(MD_SB_CHANGE_DEVS, &mddev->sb_flags);
	mddev->ro = 0;
	md_update_sb(mddev, 1);
	mddev->ro = ro;
}

 
static int rs_start_reshape(struct raid_set *rs)
{
	int r;
	struct mddev *mddev = &rs->md;
	struct md_personality *pers = mddev->pers;

	 
	set_bit(MD_RECOVERY_WAIT, &mddev->recovery);

	r = rs_setup_reshape(rs);
	if (r)
		return r;

	 
	r = pers->check_reshape(mddev);
	if (r) {
		rs->ti->error = "pers->check_reshape() failed";
		return r;
	}

	 
	if (pers->start_reshape) {
		r = pers->start_reshape(mddev);
		if (r) {
			rs->ti->error = "pers->start_reshape() failed";
			return r;
		}
	}

	 
	rs_update_sbs(rs);

	return 0;
}

static int raid_preresume(struct dm_target *ti)
{
	int r;
	struct raid_set *rs = ti->private;
	struct mddev *mddev = &rs->md;

	 
	if (test_and_set_bit(RT_FLAG_RS_PRERESUMED, &rs->runtime_flags))
		return 0;

	 
	if (test_bit(RT_FLAG_UPDATE_SBS, &rs->runtime_flags))
		rs_update_sbs(rs);

	 
	r = __load_dirty_region_bitmap(rs);
	if (r)
		return r;

	 
	if (test_bit(RT_FLAG_RS_GROW, &rs->runtime_flags)) {
		mddev->array_sectors = rs->array_sectors;
		mddev->dev_sectors = rs->dev_sectors;
		rs_set_rdev_sectors(rs);
		rs_set_capacity(rs);
	}

	 
	if (test_bit(RT_FLAG_RS_BITMAP_LOADED, &rs->runtime_flags) && mddev->bitmap &&
	    (test_bit(RT_FLAG_RS_GROW, &rs->runtime_flags) ||
	     (rs->requested_bitmap_chunk_sectors &&
	       mddev->bitmap_info.chunksize != to_bytes(rs->requested_bitmap_chunk_sectors)))) {
		int chunksize = to_bytes(rs->requested_bitmap_chunk_sectors) ?: mddev->bitmap_info.chunksize;

		r = md_bitmap_resize(mddev->bitmap, mddev->dev_sectors, chunksize, 0);
		if (r)
			DMERR("Failed to resize bitmap");
	}

	 
	 
	set_bit(MD_RECOVERY_FROZEN, &mddev->recovery);
	if (mddev->recovery_cp && mddev->recovery_cp < MaxSector) {
		set_bit(MD_RECOVERY_REQUESTED, &mddev->recovery);
		mddev->resync_min = mddev->recovery_cp;
		if (test_bit(RT_FLAG_RS_GROW, &rs->runtime_flags))
			mddev->resync_max_sectors = mddev->dev_sectors;
	}

	 
	if (test_bit(RT_FLAG_RESHAPE_RS, &rs->runtime_flags)) {
		 
		rs_set_rdev_sectors(rs);
		mddev_lock_nointr(mddev);
		r = rs_start_reshape(rs);
		mddev_unlock(mddev);
		if (r)
			DMWARN("Failed to check/start reshape, continuing without change");
		r = 0;
	}

	return r;
}

static void raid_resume(struct dm_target *ti)
{
	struct raid_set *rs = ti->private;
	struct mddev *mddev = &rs->md;

	if (test_and_set_bit(RT_FLAG_RS_RESUMED, &rs->runtime_flags)) {
		 
		attempt_restore_of_faulty_devices(rs);
	}

	if (test_and_clear_bit(RT_FLAG_RS_SUSPENDED, &rs->runtime_flags)) {
		 
		if (mddev->delta_disks < 0)
			rs_set_capacity(rs);

		mddev_lock_nointr(mddev);
		clear_bit(MD_RECOVERY_FROZEN, &mddev->recovery);
		mddev->ro = 0;
		mddev->in_sync = 0;
		mddev_resume(mddev);
		mddev_unlock(mddev);
	}
}

static struct target_type raid_target = {
	.name = "raid",
	.version = {1, 15, 1},
	.module = THIS_MODULE,
	.ctr = raid_ctr,
	.dtr = raid_dtr,
	.map = raid_map,
	.status = raid_status,
	.message = raid_message,
	.iterate_devices = raid_iterate_devices,
	.io_hints = raid_io_hints,
	.postsuspend = raid_postsuspend,
	.preresume = raid_preresume,
	.resume = raid_resume,
};
module_dm(raid);

module_param(devices_handle_discard_safely, bool, 0644);
MODULE_PARM_DESC(devices_handle_discard_safely,
		 "Set to Y if all devices in each array reliably return zeroes on reads from discarded regions");

MODULE_DESCRIPTION(DM_NAME " raid0/1/10/4/5/6 target");
MODULE_ALIAS("dm-raid0");
MODULE_ALIAS("dm-raid1");
MODULE_ALIAS("dm-raid10");
MODULE_ALIAS("dm-raid4");
MODULE_ALIAS("dm-raid5");
MODULE_ALIAS("dm-raid6");
MODULE_AUTHOR("Neil Brown <dm-devel@redhat.com>");
MODULE_AUTHOR("Heinz Mauelshagen <dm-devel@redhat.com>");
MODULE_LICENSE("GPL");
