 
 

#ifndef _CORESIGHT_CORESIGHT_CTI_H
#define _CORESIGHT_CORESIGHT_CTI_H

#include <linux/coresight.h>
#include <linux/device.h>
#include <linux/fwnode.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "coresight-priv.h"

 
 
#define CTICONTROL		0x000
#define CTIINTACK		0x010
#define CTIAPPSET		0x014
#define CTIAPPCLEAR		0x018
#define CTIAPPPULSE		0x01C
#define CTIINEN(n)		(0x020 + (4 * n))
#define CTIOUTEN(n)		(0x0A0 + (4 * n))
#define CTITRIGINSTATUS		0x130
#define CTITRIGOUTSTATUS	0x134
#define CTICHINSTATUS		0x138
#define CTICHOUTSTATUS		0x13C
#define CTIGATE			0x140
#define ASICCTL			0x144
 
#define ITCHINACK		0xEDC  
#define ITTRIGINACK		0xEE0  
#define ITCHOUT			0xEE4  
#define ITTRIGOUT		0xEE8  
#define ITCHOUTACK		0xEEC  
#define ITTRIGOUTACK		0xEF0  
#define ITCHIN			0xEF4  
#define ITTRIGIN		0xEF8  
 
#define CTIDEVAFF0		0xFA8
#define CTIDEVAFF1		0xFAC

 
#define CTIINOUTEN_MAX		32

 
struct cti_trig_grp {
	int nr_sigs;
	u32 used_mask;
	int sig_types[];
};

 
struct cti_trig_con {
	struct cti_trig_grp *con_in;
	struct cti_trig_grp *con_out;
	struct coresight_device *con_dev;
	const char *con_dev_name;
	struct list_head node;
	struct attribute **con_attrs;
	struct attribute_group *attr_group;
};

 
struct cti_device {
	int nr_trig_con;
	u32 ctm_id;
	struct list_head trig_cons;
	int cpu;
	const struct attribute_group **con_groups;
};

 
struct cti_config {
	 
	int nr_ctm_channels;
	int nr_trig_max;

	 
	int enable_req_count;
	bool hw_enabled;
	bool hw_powered;

	 
	u32 trig_in_use;
	u32 trig_out_use;
	u32 trig_out_filter;
	bool trig_filter_enable;
	u8 xtrig_rchan_sel;

	 
	u32 ctiappset;
	u8 ctiinout_sel;
	u32 ctiinen[CTIINOUTEN_MAX];
	u32 ctiouten[CTIINOUTEN_MAX];
	u32 ctigate;
	u32 asicctl;
};

 
struct cti_drvdata {
	void __iomem *base;
	struct coresight_device	*csdev;
	struct cti_device ctidev;
	spinlock_t spinlock;
	struct cti_config config;
	struct list_head node;
	void (*csdev_release)(struct device *dev);
};

 
enum cti_chan_op {
	CTI_CHAN_ATTACH,
	CTI_CHAN_DETACH,
};

enum cti_trig_dir {
	CTI_TRIG_IN,
	CTI_TRIG_OUT,
};

enum cti_chan_gate_op {
	CTI_GATE_CHAN_ENABLE,
	CTI_GATE_CHAN_DISABLE,
};

enum cti_chan_set_op {
	CTI_CHAN_SET,
	CTI_CHAN_CLR,
	CTI_CHAN_PULSE,
};

 
extern const struct attribute_group *coresight_cti_groups[];
int cti_add_default_connection(struct device *dev,
			       struct cti_drvdata *drvdata);
int cti_add_connection_entry(struct device *dev, struct cti_drvdata *drvdata,
			     struct cti_trig_con *tc,
			     struct coresight_device *csdev,
			     const char *assoc_dev_name);
struct cti_trig_con *cti_allocate_trig_con(struct device *dev, int in_sigs,
					   int out_sigs);
int cti_enable(struct coresight_device *csdev, enum cs_mode mode, void *data);
int cti_disable(struct coresight_device *csdev, void *data);
void cti_write_all_hw_regs(struct cti_drvdata *drvdata);
void cti_write_intack(struct device *dev, u32 ackval);
void cti_write_single_reg(struct cti_drvdata *drvdata, int offset, u32 value);
int cti_channel_trig_op(struct device *dev, enum cti_chan_op op,
			enum cti_trig_dir direction, u32 channel_idx,
			u32 trigger_idx);
int cti_channel_gate_op(struct device *dev, enum cti_chan_gate_op op,
			u32 channel_idx);
int cti_channel_setop(struct device *dev, enum cti_chan_set_op op,
		      u32 channel_idx);
int cti_create_cons_sysfs(struct device *dev, struct cti_drvdata *drvdata);
struct coresight_platform_data *
coresight_cti_get_platform_data(struct device *dev);
const char *cti_plat_get_node_name(struct fwnode_handle *fwnode);

 
static inline bool cti_active(struct cti_config *cfg)
{
	return cfg->hw_powered && cfg->hw_enabled;
}

#endif   
