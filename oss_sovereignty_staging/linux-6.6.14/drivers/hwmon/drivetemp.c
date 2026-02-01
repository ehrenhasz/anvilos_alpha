
 

#include <linux/ata.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_proto.h>

struct drivetemp_data {
	struct list_head list;		 
	struct mutex lock;		 
	struct scsi_device *sdev;	 
	struct device *dev;		 
	struct device *hwdev;		 
	u8 smartdata[ATA_SECT_SIZE];	 
	int (*get_temp)(struct drivetemp_data *st, u32 attr, long *val);
	bool have_temp_lowest;		 
	bool have_temp_highest;		 
	bool have_temp_min;		 
	bool have_temp_max;		 
	bool have_temp_lcrit;		 
	bool have_temp_crit;		 
	int temp_min;			 
	int temp_max;			 
	int temp_lcrit;			 
	int temp_crit;			 
};

static LIST_HEAD(drivetemp_devlist);

#define ATA_MAX_SMART_ATTRS	30
#define SMART_TEMP_PROP_190	190
#define SMART_TEMP_PROP_194	194

#define SCT_STATUS_REQ_ADDR	0xe0
#define  SCT_STATUS_VERSION_LOW		0	 
#define  SCT_STATUS_VERSION_HIGH	1
#define  SCT_STATUS_TEMP		200
#define  SCT_STATUS_TEMP_LOWEST		201
#define  SCT_STATUS_TEMP_HIGHEST	202
#define SCT_READ_LOG_ADDR	0xe1
#define  SMART_READ_LOG			0xd5
#define  SMART_WRITE_LOG		0xd6

#define INVALID_TEMP		0x80

#define temp_is_valid(temp)	((temp) != INVALID_TEMP)
#define temp_from_sct(temp)	(((s8)(temp)) * 1000)

static inline bool ata_id_smart_supported(u16 *id)
{
	return id[ATA_ID_COMMAND_SET_1] & BIT(0);
}

static inline bool ata_id_smart_enabled(u16 *id)
{
	return id[ATA_ID_CFS_ENABLE_1] & BIT(0);
}

static int drivetemp_scsi_command(struct drivetemp_data *st,
				 u8 ata_command, u8 feature,
				 u8 lba_low, u8 lba_mid, u8 lba_high)
{
	u8 scsi_cmd[MAX_COMMAND_SIZE];
	enum req_op op;

	memset(scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd[0] = ATA_16;
	if (ata_command == ATA_CMD_SMART && feature == SMART_WRITE_LOG) {
		scsi_cmd[1] = (5 << 1);	 
		 
		scsi_cmd[2] = 0x06;
		op = REQ_OP_DRV_OUT;
	} else {
		scsi_cmd[1] = (4 << 1);	 
		 
		scsi_cmd[2] = 0x0e;
		op = REQ_OP_DRV_IN;
	}
	scsi_cmd[4] = feature;
	scsi_cmd[6] = 1;	 
	scsi_cmd[8] = lba_low;
	scsi_cmd[10] = lba_mid;
	scsi_cmd[12] = lba_high;
	scsi_cmd[14] = ata_command;

	return scsi_execute_cmd(st->sdev, scsi_cmd, op, st->smartdata,
				ATA_SECT_SIZE, HZ, 5, NULL);
}

static int drivetemp_ata_command(struct drivetemp_data *st, u8 feature,
				 u8 select)
{
	return drivetemp_scsi_command(st, ATA_CMD_SMART, feature, select,
				     ATA_SMART_LBAM_PASS, ATA_SMART_LBAH_PASS);
}

static int drivetemp_get_smarttemp(struct drivetemp_data *st, u32 attr,
				  long *temp)
{
	u8 *buf = st->smartdata;
	bool have_temp = false;
	u8 temp_raw;
	u8 csum;
	int err;
	int i;

	err = drivetemp_ata_command(st, ATA_SMART_READ_VALUES, 0);
	if (err)
		return err;

	 
	csum = 0;
	for (i = 0; i < ATA_SECT_SIZE; i++)
		csum += buf[i];
	if (csum) {
		dev_dbg(&st->sdev->sdev_gendev,
			"checksum error reading SMART values\n");
		return -EIO;
	}

	for (i = 0; i < ATA_MAX_SMART_ATTRS; i++) {
		u8 *attr = buf + i * 12;
		int id = attr[2];

		if (!id)
			continue;

		if (id == SMART_TEMP_PROP_190) {
			temp_raw = attr[7];
			have_temp = true;
		}
		if (id == SMART_TEMP_PROP_194) {
			temp_raw = attr[7];
			have_temp = true;
			break;
		}
	}

	if (have_temp) {
		*temp = temp_raw * 1000;
		return 0;
	}

	return -ENXIO;
}

static int drivetemp_get_scttemp(struct drivetemp_data *st, u32 attr, long *val)
{
	u8 *buf = st->smartdata;
	int err;

	err = drivetemp_ata_command(st, SMART_READ_LOG, SCT_STATUS_REQ_ADDR);
	if (err)
		return err;
	switch (attr) {
	case hwmon_temp_input:
		if (!temp_is_valid(buf[SCT_STATUS_TEMP]))
			return -ENODATA;
		*val = temp_from_sct(buf[SCT_STATUS_TEMP]);
		break;
	case hwmon_temp_lowest:
		if (!temp_is_valid(buf[SCT_STATUS_TEMP_LOWEST]))
			return -ENODATA;
		*val = temp_from_sct(buf[SCT_STATUS_TEMP_LOWEST]);
		break;
	case hwmon_temp_highest:
		if (!temp_is_valid(buf[SCT_STATUS_TEMP_HIGHEST]))
			return -ENODATA;
		*val = temp_from_sct(buf[SCT_STATUS_TEMP_HIGHEST]);
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

static const char * const sct_avoid_models[] = {
 
	"TOSHIBA DT01ACA",
};

static bool drivetemp_sct_avoid(struct drivetemp_data *st)
{
	struct scsi_device *sdev = st->sdev;
	unsigned int ctr;

	if (!sdev->model)
		return false;

	 
	for (ctr = 0; ctr < ARRAY_SIZE(sct_avoid_models); ctr++)
		if (!strncmp(sdev->model, sct_avoid_models[ctr],
			     strlen(sct_avoid_models[ctr])))
			return true;

	return false;
}

static int drivetemp_identify_sata(struct drivetemp_data *st)
{
	struct scsi_device *sdev = st->sdev;
	u8 *buf = st->smartdata;
	struct scsi_vpd *vpd;
	bool is_ata, is_sata;
	bool have_sct_data_table;
	bool have_sct_temp;
	bool have_smart;
	bool have_sct;
	u16 *ata_id;
	u16 version;
	long temp;
	int err;

	 
	rcu_read_lock();
	vpd = rcu_dereference(sdev->vpd_pg89);

	 
	if (!vpd || vpd->len < 572 || vpd->data[56] != ATA_CMD_ID_ATA ||
	    vpd->data[36] != 0x34) {
		rcu_read_unlock();
		return -ENODEV;
	}
	ata_id = (u16 *)&vpd->data[60];
	is_ata = ata_id_is_ata(ata_id);
	is_sata = ata_id_is_sata(ata_id);
	have_sct = ata_id_sct_supported(ata_id);
	have_sct_data_table = ata_id_sct_data_tables(ata_id);
	have_smart = ata_id_smart_supported(ata_id) &&
				ata_id_smart_enabled(ata_id);

	rcu_read_unlock();

	 
	if (!is_ata || !is_sata)
		return -ENODEV;

	if (have_sct && drivetemp_sct_avoid(st)) {
		dev_notice(&sdev->sdev_gendev,
			   "will avoid using SCT for temperature monitoring\n");
		have_sct = false;
	}

	if (!have_sct)
		goto skip_sct;

	err = drivetemp_ata_command(st, SMART_READ_LOG, SCT_STATUS_REQ_ADDR);
	if (err)
		goto skip_sct;

	version = (buf[SCT_STATUS_VERSION_HIGH] << 8) |
		  buf[SCT_STATUS_VERSION_LOW];
	if (version != 2 && version != 3)
		goto skip_sct;

	have_sct_temp = temp_is_valid(buf[SCT_STATUS_TEMP]);
	if (!have_sct_temp)
		goto skip_sct;

	st->have_temp_lowest = temp_is_valid(buf[SCT_STATUS_TEMP_LOWEST]);
	st->have_temp_highest = temp_is_valid(buf[SCT_STATUS_TEMP_HIGHEST]);

	if (!have_sct_data_table)
		goto skip_sct_data;

	 
	memset(buf, '\0', sizeof(st->smartdata));
	buf[0] = 5;	 
	buf[2] = 1;	 
	buf[4] = 2;	 

	err = drivetemp_ata_command(st, SMART_WRITE_LOG, SCT_STATUS_REQ_ADDR);
	if (err)
		goto skip_sct_data;

	err = drivetemp_ata_command(st, SMART_READ_LOG, SCT_READ_LOG_ADDR);
	if (err)
		goto skip_sct_data;

	 
	st->have_temp_max = temp_is_valid(buf[6]);
	st->have_temp_crit = temp_is_valid(buf[7]);
	st->have_temp_min = temp_is_valid(buf[8]);
	st->have_temp_lcrit = temp_is_valid(buf[9]);

	st->temp_max = temp_from_sct(buf[6]);
	st->temp_crit = temp_from_sct(buf[7]);
	st->temp_min = temp_from_sct(buf[8]);
	st->temp_lcrit = temp_from_sct(buf[9]);

skip_sct_data:
	if (have_sct_temp) {
		st->get_temp = drivetemp_get_scttemp;
		return 0;
	}
skip_sct:
	if (!have_smart)
		return -ENODEV;
	st->get_temp = drivetemp_get_smarttemp;
	return drivetemp_get_smarttemp(st, hwmon_temp_input, &temp);
}

static int drivetemp_identify(struct drivetemp_data *st)
{
	struct scsi_device *sdev = st->sdev;

	 
	if (!sdev->inquiry || sdev->inquiry_len < 16)
		return -ENODEV;

	 
	if (sdev->type != TYPE_DISK && sdev->type != TYPE_ZBC)
		return -ENODEV;

	return drivetemp_identify_sata(st);
}

static int drivetemp_read(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long *val)
{
	struct drivetemp_data *st = dev_get_drvdata(dev);
	int err = 0;

	if (type != hwmon_temp)
		return -EINVAL;

	switch (attr) {
	case hwmon_temp_input:
	case hwmon_temp_lowest:
	case hwmon_temp_highest:
		mutex_lock(&st->lock);
		err = st->get_temp(st, attr, val);
		mutex_unlock(&st->lock);
		break;
	case hwmon_temp_lcrit:
		*val = st->temp_lcrit;
		break;
	case hwmon_temp_min:
		*val = st->temp_min;
		break;
	case hwmon_temp_max:
		*val = st->temp_max;
		break;
	case hwmon_temp_crit:
		*val = st->temp_crit;
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

static umode_t drivetemp_is_visible(const void *data,
				   enum hwmon_sensor_types type,
				   u32 attr, int channel)
{
	const struct drivetemp_data *st = data;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return 0444;
		case hwmon_temp_lowest:
			if (st->have_temp_lowest)
				return 0444;
			break;
		case hwmon_temp_highest:
			if (st->have_temp_highest)
				return 0444;
			break;
		case hwmon_temp_min:
			if (st->have_temp_min)
				return 0444;
			break;
		case hwmon_temp_max:
			if (st->have_temp_max)
				return 0444;
			break;
		case hwmon_temp_lcrit:
			if (st->have_temp_lcrit)
				return 0444;
			break;
		case hwmon_temp_crit:
			if (st->have_temp_crit)
				return 0444;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

static const struct hwmon_channel_info * const drivetemp_info[] = {
	HWMON_CHANNEL_INFO(chip,
			   HWMON_C_REGISTER_TZ),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT |
			   HWMON_T_LOWEST | HWMON_T_HIGHEST |
			   HWMON_T_MIN | HWMON_T_MAX |
			   HWMON_T_LCRIT | HWMON_T_CRIT),
	NULL
};

static const struct hwmon_ops drivetemp_ops = {
	.is_visible = drivetemp_is_visible,
	.read = drivetemp_read,
};

static const struct hwmon_chip_info drivetemp_chip_info = {
	.ops = &drivetemp_ops,
	.info = drivetemp_info,
};

 
static int drivetemp_add(struct device *dev)
{
	struct scsi_device *sdev = to_scsi_device(dev->parent);
	struct drivetemp_data *st;
	int err;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->sdev = sdev;
	st->dev = dev;
	mutex_init(&st->lock);

	if (drivetemp_identify(st)) {
		err = -ENODEV;
		goto abort;
	}

	st->hwdev = hwmon_device_register_with_info(dev->parent, "drivetemp",
						    st, &drivetemp_chip_info,
						    NULL);
	if (IS_ERR(st->hwdev)) {
		err = PTR_ERR(st->hwdev);
		goto abort;
	}

	list_add(&st->list, &drivetemp_devlist);
	return 0;

abort:
	kfree(st);
	return err;
}

static void drivetemp_remove(struct device *dev)
{
	struct drivetemp_data *st, *tmp;

	list_for_each_entry_safe(st, tmp, &drivetemp_devlist, list) {
		if (st->dev == dev) {
			list_del(&st->list);
			hwmon_device_unregister(st->hwdev);
			kfree(st);
			break;
		}
	}
}

static struct class_interface drivetemp_interface = {
	.add_dev = drivetemp_add,
	.remove_dev = drivetemp_remove,
};

static int __init drivetemp_init(void)
{
	return scsi_register_interface(&drivetemp_interface);
}

static void __exit drivetemp_exit(void)
{
	scsi_unregister_interface(&drivetemp_interface);
}

module_init(drivetemp_init);
module_exit(drivetemp_exit);

MODULE_AUTHOR("Guenter Roeck <linus@roeck-us.net>");
MODULE_DESCRIPTION("Hard drive temperature monitor");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:drivetemp");
