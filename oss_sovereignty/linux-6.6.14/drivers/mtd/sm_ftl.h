#include <linux/mtd/blktrans.h>
#include <linux/kfifo.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/mtd/mtd.h>
struct ftl_zone {
	bool initialized;
	int16_t *lba_to_phys_table;		 
	struct kfifo free_sectors;	 
};
struct sm_ftl {
	struct mtd_blktrans_dev *trans;
	struct mutex mutex;		 
	struct ftl_zone *zones;		 
	int block_size;			 
	int zone_size;			 
	int zone_count;			 
	int max_lba;			 
	int smallpagenand;		 
	bool readonly;			 
	bool unstable;
	int cis_block;			 
	int cis_boffset;		 
	int cis_page_offset;		 
	void *cis_buffer;		 
	int cache_block;		 
	int cache_zone;			 
	unsigned char *cache_data;	 
	long unsigned int cache_data_invalid_bitmap;
	bool cache_clean;
	struct work_struct flush_work;
	struct timer_list timer;
	int heads;
	int sectors;
	int cylinders;
	struct attribute_group *disk_attributes;
};
struct chs_entry {
	unsigned long size;
	unsigned short cyl;
	unsigned char head;
	unsigned char sec;
};
#define SM_FTL_PARTN_BITS	3
#define sm_printk(format, ...) \
	printk(KERN_WARNING "sm_ftl" ": " format "\n", ## __VA_ARGS__)
#define dbg(format, ...) \
	if (debug) \
		printk(KERN_DEBUG "sm_ftl" ": " format "\n", ## __VA_ARGS__)
#define dbg_verbose(format, ...) \
	if (debug > 1) \
		printk(KERN_DEBUG "sm_ftl" ": " format "\n", ## __VA_ARGS__)
static int sm_erase_block(struct sm_ftl *ftl, int zone_num, uint16_t block,
								int put_free);
static void sm_mark_block_bad(struct sm_ftl *ftl, int zone_num, int block);
static int sm_recheck_media(struct sm_ftl *ftl);
