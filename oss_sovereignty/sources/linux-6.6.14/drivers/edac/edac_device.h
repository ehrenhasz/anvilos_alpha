

#ifndef _EDAC_DEVICE_H_
#define _EDAC_DEVICE_H_

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/edac.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>




struct edac_device_counter {
	u32 ue_count;
	u32 ce_count;
};


struct edac_device_ctl_info;
struct edac_device_block;


struct edac_dev_sysfs_attribute {
	struct attribute attr;
	ssize_t (*show)(struct edac_device_ctl_info *, char *);
	ssize_t (*store)(struct edac_device_ctl_info *, const char *, size_t);
};


struct edac_dev_sysfs_block_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *,
			const char *, size_t);
	struct edac_device_block *block;

	unsigned int value;
};


struct edac_device_block {
	struct edac_device_instance *instance;	
	char name[EDAC_DEVICE_NAME_LEN + 1];

	struct edac_device_counter counters;	

	int nr_attribs;		

	
	struct edac_dev_sysfs_block_attribute *block_attributes;

	
	struct kobject kobj;
};


struct edac_device_instance {
	struct edac_device_ctl_info *ctl;	
	char name[EDAC_DEVICE_NAME_LEN + 4];

	struct edac_device_counter counters;	

	u32 nr_blocks;		
	struct edac_device_block *blocks;	

	
	struct kobject kobj;
};



struct edac_device_ctl_info {
	
	struct list_head link;

	struct module *owner;	

	int dev_idx;

	
	int log_ue;		
	int log_ce;		
	int panic_on_ue;	
	unsigned poll_msec;	
	unsigned long delay;	

	
	struct edac_dev_sysfs_attribute *sysfs_attributes;

	
	struct bus_type *edac_subsys;

	
	int op_state;
	
	struct delayed_work work;

	
	void (*edac_check) (struct edac_device_ctl_info * edac_dev);

	struct device *dev;	

	const char *mod_name;	
	const char *ctl_name;	
	const char *dev_name;	

	void *pvt_info;		

	unsigned long start_time;	

	struct completion removal_complete;

	
	char name[EDAC_DEVICE_NAME_LEN + 1];

	
	u32 nr_instances;
	struct edac_device_instance *instances;
	struct edac_device_block *blocks;
	struct edac_dev_sysfs_block_attribute *attribs;

	
	struct edac_device_counter counters;

	
	struct kobject kobj;
};


#define to_edac_mem_ctl_work(w) \
		container_of(w, struct mem_ctl_info, work)

#define to_edac_device_ctl_work(w) \
		container_of(w,struct edac_device_ctl_info,work)


extern struct edac_device_ctl_info *edac_device_alloc_ctl_info(
		unsigned sizeof_private,
		char *edac_device_name, unsigned nr_instances,
		char *edac_block_name, unsigned nr_blocks,
		unsigned offset_value,
		struct edac_dev_sysfs_block_attribute *block_attributes,
		unsigned nr_attribs,
		int device_index);


#define	BLOCK_OFFSET_VALUE_OFF	((unsigned) -1)

extern void edac_device_free_ctl_info(struct edac_device_ctl_info *ctl_info);


extern int edac_device_add_device(struct edac_device_ctl_info *edac_dev);


extern struct edac_device_ctl_info *edac_device_del_device(struct device *dev);


void edac_device_handle_ce_count(struct edac_device_ctl_info *edac_dev,
				 unsigned int count, int inst_nr, int block_nr,
				 const char *msg);


void edac_device_handle_ue_count(struct edac_device_ctl_info *edac_dev,
				 unsigned int count, int inst_nr, int block_nr,
				 const char *msg);


static inline void
edac_device_handle_ce(struct edac_device_ctl_info *edac_dev, int inst_nr,
		      int block_nr, const char *msg)
{
	edac_device_handle_ce_count(edac_dev, 1, inst_nr, block_nr, msg);
}


static inline void
edac_device_handle_ue(struct edac_device_ctl_info *edac_dev, int inst_nr,
		      int block_nr, const char *msg)
{
	edac_device_handle_ue_count(edac_dev, 1, inst_nr, block_nr, msg);
}


extern int edac_device_alloc_index(void);
extern const char *edac_layer_name[];


static inline void __edac_device_free_ctl_info(struct edac_device_ctl_info *ci)
{
	if (ci) {
		kfree(ci->pvt_info);
		kfree(ci->attribs);
		kfree(ci->blocks);
		kfree(ci->instances);
		kfree(ci);
	}
}
#endif
