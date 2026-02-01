 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/thermal.h>
#include <asm/cpu_device_id.h>
#include <asm/iosf_mbi.h>

 
#define QRK_DTS_REG_OFFSET_RESET	0x34
#define QRK_DTS_RESET_BIT		BIT(0)

 
#define QRK_DTS_REG_OFFSET_ENABLE	0xB0
#define QRK_DTS_ENABLE_BIT		BIT(15)

 
#define QRK_DTS_REG_OFFSET_TEMP		0xB1
#define QRK_DTS_MASK_TEMP		0xFF
#define QRK_DTS_OFFSET_TEMP		0
#define QRK_DTS_OFFSET_REL_TEMP		16
#define QRK_DTS_TEMP_BASE		50

 
#define QRK_DTS_REG_OFFSET_PTPS		0xB2
#define QRK_DTS_MASK_TP_THRES		0xFF
#define QRK_DTS_SHIFT_TP		8
#define QRK_DTS_ID_TP_CRITICAL		0
#define QRK_DTS_ID_TP_HOT		1
#define QRK_DTS_SAFE_TP_THRES		105

 
#define QRK_DTS_REG_OFFSET_LOCK		0x71
#define QRK_DTS_LOCK_BIT		BIT(5)

 
#define QRK_MAX_DTS_TRIPS	2
 
#define QRK_DTS_WR_MASK_SET	0x3
 
#define QRK_DTS_WR_MASK_CLR	0

#define DEFAULT_POLL_DELAY	2000

struct soc_sensor_entry {
	bool locked;
	u32 store_ptps;
	u32 store_dts_enable;
	struct thermal_zone_device *tzone;
	struct thermal_trip trips[QRK_MAX_DTS_TRIPS];
};

static struct soc_sensor_entry *soc_dts;

static int polling_delay = DEFAULT_POLL_DELAY;
module_param(polling_delay, int, 0644);
MODULE_PARM_DESC(polling_delay,
	"Polling interval for checking trip points (in milliseconds)");

static DEFINE_MUTEX(dts_update_mutex);

static int soc_dts_enable(struct thermal_zone_device *tzd)
{
	u32 out;
	struct soc_sensor_entry *aux_entry = thermal_zone_device_priv(tzd);
	int ret;

	ret = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			    QRK_DTS_REG_OFFSET_ENABLE, &out);
	if (ret)
		return ret;

	if (out & QRK_DTS_ENABLE_BIT)
		return 0;

	if (!aux_entry->locked) {
		out |= QRK_DTS_ENABLE_BIT;
		ret = iosf_mbi_write(QRK_MBI_UNIT_RMU, MBI_REG_WRITE,
				     QRK_DTS_REG_OFFSET_ENABLE, out);
		if (ret)
			return ret;
	} else {
		pr_info("DTS is locked. Cannot enable DTS\n");
		ret = -EPERM;
	}

	return ret;
}

static int soc_dts_disable(struct thermal_zone_device *tzd)
{
	u32 out;
	struct soc_sensor_entry *aux_entry = thermal_zone_device_priv(tzd);
	int ret;

	ret = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			    QRK_DTS_REG_OFFSET_ENABLE, &out);
	if (ret)
		return ret;

	if (!(out & QRK_DTS_ENABLE_BIT))
		return 0;

	if (!aux_entry->locked) {
		out &= ~QRK_DTS_ENABLE_BIT;
		ret = iosf_mbi_write(QRK_MBI_UNIT_RMU, MBI_REG_WRITE,
				     QRK_DTS_REG_OFFSET_ENABLE, out);

		if (ret)
			return ret;
	} else {
		pr_info("DTS is locked. Cannot disable DTS\n");
		ret = -EPERM;
	}

	return ret;
}

static int get_trip_temp(int trip)
{
	int status, temp;
	u32 out;

	mutex_lock(&dts_update_mutex);
	status = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			       QRK_DTS_REG_OFFSET_PTPS, &out);
	mutex_unlock(&dts_update_mutex);

	if (status)
		return THERMAL_TEMP_INVALID;

	 
	temp = (out >> (trip * QRK_DTS_SHIFT_TP)) & QRK_DTS_MASK_TP_THRES;
	temp -= QRK_DTS_TEMP_BASE;

	return temp;
}

static int update_trip_temp(struct soc_sensor_entry *aux_entry,
				int trip, int temp)
{
	u32 out;
	u32 temp_out;
	u32 store_ptps;
	int ret;

	mutex_lock(&dts_update_mutex);
	if (aux_entry->locked) {
		ret = -EPERM;
		goto failed;
	}

	ret = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			    QRK_DTS_REG_OFFSET_PTPS, &store_ptps);
	if (ret)
		goto failed;

	 
	if (temp > QRK_DTS_SAFE_TP_THRES)
		temp = QRK_DTS_SAFE_TP_THRES;

	 
	temp_out = temp + QRK_DTS_TEMP_BASE;
	out = (store_ptps & ~(QRK_DTS_MASK_TP_THRES <<
		(trip * QRK_DTS_SHIFT_TP)));
	out |= (temp_out & QRK_DTS_MASK_TP_THRES) <<
		(trip * QRK_DTS_SHIFT_TP);

	ret = iosf_mbi_write(QRK_MBI_UNIT_RMU, MBI_REG_WRITE,
			     QRK_DTS_REG_OFFSET_PTPS, out);

failed:
	mutex_unlock(&dts_update_mutex);
	return ret;
}

static inline int sys_set_trip_temp(struct thermal_zone_device *tzd, int trip,
				int temp)
{
	return update_trip_temp(thermal_zone_device_priv(tzd), trip, temp);
}

static int sys_get_curr_temp(struct thermal_zone_device *tzd,
				int *temp)
{
	u32 out;
	int ret;

	mutex_lock(&dts_update_mutex);
	ret = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			    QRK_DTS_REG_OFFSET_TEMP, &out);
	mutex_unlock(&dts_update_mutex);

	if (ret)
		return ret;

	 
	out = (out >> QRK_DTS_OFFSET_TEMP) & QRK_DTS_MASK_TEMP;
	*temp = out - QRK_DTS_TEMP_BASE;

	return 0;
}

static int sys_change_mode(struct thermal_zone_device *tzd,
			   enum thermal_device_mode mode)
{
	int ret;

	mutex_lock(&dts_update_mutex);
	if (mode == THERMAL_DEVICE_ENABLED)
		ret = soc_dts_enable(tzd);
	else
		ret = soc_dts_disable(tzd);
	mutex_unlock(&dts_update_mutex);

	return ret;
}

static struct thermal_zone_device_ops tzone_ops = {
	.get_temp = sys_get_curr_temp,
	.set_trip_temp = sys_set_trip_temp,
	.change_mode = sys_change_mode,
};

static void free_soc_dts(struct soc_sensor_entry *aux_entry)
{
	if (aux_entry) {
		if (!aux_entry->locked) {
			mutex_lock(&dts_update_mutex);
			iosf_mbi_write(QRK_MBI_UNIT_RMU, MBI_REG_WRITE,
				       QRK_DTS_REG_OFFSET_ENABLE,
				       aux_entry->store_dts_enable);

			iosf_mbi_write(QRK_MBI_UNIT_RMU, MBI_REG_WRITE,
				       QRK_DTS_REG_OFFSET_PTPS,
				       aux_entry->store_ptps);
			mutex_unlock(&dts_update_mutex);
		}
		thermal_zone_device_unregister(aux_entry->tzone);
		kfree(aux_entry);
	}
}

static struct soc_sensor_entry *alloc_soc_dts(void)
{
	struct soc_sensor_entry *aux_entry;
	int err;
	u32 out;
	int wr_mask;

	aux_entry = kzalloc(sizeof(*aux_entry), GFP_KERNEL);
	if (!aux_entry) {
		err = -ENOMEM;
		return ERR_PTR(-ENOMEM);
	}

	 
	err = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
			    QRK_DTS_REG_OFFSET_LOCK, &out);
	if (err)
		goto err_ret;

	if (out & QRK_DTS_LOCK_BIT) {
		aux_entry->locked = true;
		wr_mask = QRK_DTS_WR_MASK_CLR;
	} else {
		aux_entry->locked = false;
		wr_mask = QRK_DTS_WR_MASK_SET;
	}

	 
	if (!aux_entry->locked) {
		 
		err = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
				    QRK_DTS_REG_OFFSET_ENABLE,
				    &aux_entry->store_dts_enable);
		if (err)
			goto err_ret;

		 
		err = iosf_mbi_read(QRK_MBI_UNIT_RMU, MBI_REG_READ,
				    QRK_DTS_REG_OFFSET_PTPS,
				    &aux_entry->store_ptps);
		if (err)
			goto err_ret;
	}

	aux_entry->trips[QRK_DTS_ID_TP_CRITICAL].temperature = get_trip_temp(QRK_DTS_ID_TP_CRITICAL);
	aux_entry->trips[QRK_DTS_ID_TP_CRITICAL].type = THERMAL_TRIP_CRITICAL;

	aux_entry->trips[QRK_DTS_ID_TP_HOT].temperature = get_trip_temp(QRK_DTS_ID_TP_HOT);
	aux_entry->trips[QRK_DTS_ID_TP_HOT].type = THERMAL_TRIP_HOT;

	aux_entry->tzone = thermal_zone_device_register_with_trips("quark_dts",
								   aux_entry->trips,
								   QRK_MAX_DTS_TRIPS,
								   wr_mask,
								   aux_entry, &tzone_ops,
								   NULL, 0, polling_delay);
	if (IS_ERR(aux_entry->tzone)) {
		err = PTR_ERR(aux_entry->tzone);
		goto err_ret;
	}

	err = thermal_zone_device_enable(aux_entry->tzone);
	if (err)
		goto err_aux_status;

	return aux_entry;

err_aux_status:
	thermal_zone_device_unregister(aux_entry->tzone);
err_ret:
	kfree(aux_entry);
	return ERR_PTR(err);
}

static const struct x86_cpu_id qrk_thermal_ids[] __initconst  = {
	X86_MATCH_VENDOR_FAM_MODEL(INTEL, 5, INTEL_FAM5_QUARK_X1000, NULL),
	{}
};
MODULE_DEVICE_TABLE(x86cpu, qrk_thermal_ids);

static int __init intel_quark_thermal_init(void)
{
	if (!x86_match_cpu(qrk_thermal_ids) || !iosf_mbi_available())
		return -ENODEV;

	soc_dts = alloc_soc_dts();
	if (IS_ERR(soc_dts))
		return PTR_ERR(soc_dts);

	return 0;
}

static void __exit intel_quark_thermal_exit(void)
{
	free_soc_dts(soc_dts);
}

module_init(intel_quark_thermal_init)
module_exit(intel_quark_thermal_exit)

MODULE_DESCRIPTION("Intel Quark DTS Thermal Driver");
MODULE_AUTHOR("Ong Boon Leong <boon.leong.ong@intel.com>");
MODULE_LICENSE("Dual BSD/GPL");
