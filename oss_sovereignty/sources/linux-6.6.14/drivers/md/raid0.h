
#ifndef _RAID0_H
#define _RAID0_H

struct strip_zone {
	sector_t zone_end;	
	sector_t dev_start;	
	int	 nb_dev;	
	int	 disk_shift;	
};



enum r0layout {
	RAID0_ORIG_LAYOUT = 1,
	RAID0_ALT_MULTIZONE_LAYOUT = 2,
};
struct r0conf {
	struct strip_zone	*strip_zone;
	struct md_rdev		**devlist; 
	int			nr_strip_zones;
	enum r0layout		layout;
};

#endif
