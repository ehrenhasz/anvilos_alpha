
 

 

#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stringify.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/miscdevice.h>
#include <linux/mtd/partitions.h>
#include <linux/log2.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/major.h>
#include "ubi.h"

 
#define MTD_PARAM_LEN_MAX 64

 
#define MTD_PARAM_MAX_COUNT 5

 
#define MAX_MTD_UBI_BEB_LIMIT 768

#ifdef CONFIG_MTD_UBI_MODULE
#define ubi_is_module() 1
#else
#define ubi_is_module() 0
#endif

 
struct mtd_dev_param {
	char name[MTD_PARAM_LEN_MAX];
	int ubi_num;
	int vid_hdr_offs;
	int max_beb_per1024;
	int enable_fm;
};

 
static int mtd_devs;

 
static struct mtd_dev_param mtd_dev_param[UBI_MAX_DEVICES];
#ifdef CONFIG_MTD_UBI_FASTMAP
 
static bool fm_autoconvert;
static bool fm_debug;
#endif

 
struct kmem_cache *ubi_wl_entry_slab;

 
static struct miscdevice ubi_ctrl_cdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ubi_ctrl",
	.fops = &ubi_ctrl_cdev_operations,
};

 
static struct ubi_device *ubi_devices[UBI_MAX_DEVICES];

 
DEFINE_MUTEX(ubi_devices_mutex);

 
static DEFINE_SPINLOCK(ubi_devices_lock);

 
 
static ssize_t version_show(const struct class *class, const struct class_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", UBI_VERSION);
}
static CLASS_ATTR_RO(version);

static struct attribute *ubi_class_attrs[] = {
	&class_attr_version.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ubi_class);

 
struct class ubi_class = {
	.name		= UBI_NAME_STR,
	.class_groups	= ubi_class_groups,
};

static ssize_t dev_attribute_show(struct device *dev,
				  struct device_attribute *attr, char *buf);

 
static struct device_attribute dev_eraseblock_size =
	__ATTR(eraseblock_size, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_avail_eraseblocks =
	__ATTR(avail_eraseblocks, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_total_eraseblocks =
	__ATTR(total_eraseblocks, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_volumes_count =
	__ATTR(volumes_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_max_ec =
	__ATTR(max_ec, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_reserved_for_bad =
	__ATTR(reserved_for_bad, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_bad_peb_count =
	__ATTR(bad_peb_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_max_vol_count =
	__ATTR(max_vol_count, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_min_io_size =
	__ATTR(min_io_size, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_bgt_enabled =
	__ATTR(bgt_enabled, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_mtd_num =
	__ATTR(mtd_num, S_IRUGO, dev_attribute_show, NULL);
static struct device_attribute dev_ro_mode =
	__ATTR(ro_mode, S_IRUGO, dev_attribute_show, NULL);

 
int ubi_volume_notify(struct ubi_device *ubi, struct ubi_volume *vol, int ntype)
{
	int ret;
	struct ubi_notification nt;

	ubi_do_get_device_info(ubi, &nt.di);
	ubi_do_get_volume_info(ubi, vol, &nt.vi);

	switch (ntype) {
	case UBI_VOLUME_ADDED:
	case UBI_VOLUME_REMOVED:
	case UBI_VOLUME_RESIZED:
	case UBI_VOLUME_RENAMED:
		ret = ubi_update_fastmap(ubi);
		if (ret)
			ubi_msg(ubi, "Unable to write a new fastmap: %i", ret);
	}

	return blocking_notifier_call_chain(&ubi_notifiers, ntype, &nt);
}

 
int ubi_notify_all(struct ubi_device *ubi, int ntype, struct notifier_block *nb)
{
	struct ubi_notification nt;
	int i, count = 0;

	ubi_do_get_device_info(ubi, &nt.di);

	mutex_lock(&ubi->device_mutex);
	for (i = 0; i < ubi->vtbl_slots; i++) {
		 
		if (!ubi->volumes[i])
			continue;

		ubi_do_get_volume_info(ubi, ubi->volumes[i], &nt.vi);
		if (nb)
			nb->notifier_call(nb, ntype, &nt);
		else
			blocking_notifier_call_chain(&ubi_notifiers, ntype,
						     &nt);
		count += 1;
	}
	mutex_unlock(&ubi->device_mutex);

	return count;
}

 
int ubi_enumerate_volumes(struct notifier_block *nb)
{
	int i, count = 0;

	 
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		struct ubi_device *ubi = ubi_devices[i];

		if (!ubi)
			continue;
		count += ubi_notify_all(ubi, UBI_VOLUME_ADDED, nb);
	}

	return count;
}

 
struct ubi_device *ubi_get_device(int ubi_num)
{
	struct ubi_device *ubi;

	spin_lock(&ubi_devices_lock);
	ubi = ubi_devices[ubi_num];
	if (ubi) {
		ubi_assert(ubi->ref_count >= 0);
		ubi->ref_count += 1;
		get_device(&ubi->dev);
	}
	spin_unlock(&ubi_devices_lock);

	return ubi;
}

 
void ubi_put_device(struct ubi_device *ubi)
{
	spin_lock(&ubi_devices_lock);
	ubi->ref_count -= 1;
	put_device(&ubi->dev);
	spin_unlock(&ubi_devices_lock);
}

 
struct ubi_device *ubi_get_by_major(int major)
{
	int i;
	struct ubi_device *ubi;

	spin_lock(&ubi_devices_lock);
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_devices[i];
		if (ubi && MAJOR(ubi->cdev.dev) == major) {
			ubi_assert(ubi->ref_count >= 0);
			ubi->ref_count += 1;
			get_device(&ubi->dev);
			spin_unlock(&ubi_devices_lock);
			return ubi;
		}
	}
	spin_unlock(&ubi_devices_lock);

	return NULL;
}

 
int ubi_major2num(int major)
{
	int i, ubi_num = -ENODEV;

	spin_lock(&ubi_devices_lock);
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		struct ubi_device *ubi = ubi_devices[i];

		if (ubi && MAJOR(ubi->cdev.dev) == major) {
			ubi_num = ubi->ubi_num;
			break;
		}
	}
	spin_unlock(&ubi_devices_lock);

	return ubi_num;
}

 
static ssize_t dev_attribute_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct ubi_device *ubi;

	 
	ubi = container_of(dev, struct ubi_device, dev);

	if (attr == &dev_eraseblock_size)
		ret = sprintf(buf, "%d\n", ubi->leb_size);
	else if (attr == &dev_avail_eraseblocks)
		ret = sprintf(buf, "%d\n", ubi->avail_pebs);
	else if (attr == &dev_total_eraseblocks)
		ret = sprintf(buf, "%d\n", ubi->good_peb_count);
	else if (attr == &dev_volumes_count)
		ret = sprintf(buf, "%d\n", ubi->vol_count - UBI_INT_VOL_COUNT);
	else if (attr == &dev_max_ec)
		ret = sprintf(buf, "%d\n", ubi->max_ec);
	else if (attr == &dev_reserved_for_bad)
		ret = sprintf(buf, "%d\n", ubi->beb_rsvd_pebs);
	else if (attr == &dev_bad_peb_count)
		ret = sprintf(buf, "%d\n", ubi->bad_peb_count);
	else if (attr == &dev_max_vol_count)
		ret = sprintf(buf, "%d\n", ubi->vtbl_slots);
	else if (attr == &dev_min_io_size)
		ret = sprintf(buf, "%d\n", ubi->min_io_size);
	else if (attr == &dev_bgt_enabled)
		ret = sprintf(buf, "%d\n", ubi->thread_enabled);
	else if (attr == &dev_mtd_num)
		ret = sprintf(buf, "%d\n", ubi->mtd->index);
	else if (attr == &dev_ro_mode)
		ret = sprintf(buf, "%d\n", ubi->ro_mode);
	else
		ret = -EINVAL;

	return ret;
}

static struct attribute *ubi_dev_attrs[] = {
	&dev_eraseblock_size.attr,
	&dev_avail_eraseblocks.attr,
	&dev_total_eraseblocks.attr,
	&dev_volumes_count.attr,
	&dev_max_ec.attr,
	&dev_reserved_for_bad.attr,
	&dev_bad_peb_count.attr,
	&dev_max_vol_count.attr,
	&dev_min_io_size.attr,
	&dev_bgt_enabled.attr,
	&dev_mtd_num.attr,
	&dev_ro_mode.attr,
	NULL
};
ATTRIBUTE_GROUPS(ubi_dev);

static void dev_release(struct device *dev)
{
	struct ubi_device *ubi = container_of(dev, struct ubi_device, dev);

	kfree(ubi);
}

 
static void kill_volumes(struct ubi_device *ubi)
{
	int i;

	for (i = 0; i < ubi->vtbl_slots; i++)
		if (ubi->volumes[i])
			ubi_free_volume(ubi, ubi->volumes[i]);
}

 
static int uif_init(struct ubi_device *ubi)
{
	int i, err;
	dev_t dev;

	sprintf(ubi->ubi_name, UBI_NAME_STR "%d", ubi->ubi_num);

	 
	err = alloc_chrdev_region(&dev, 0, ubi->vtbl_slots + 1, ubi->ubi_name);
	if (err) {
		ubi_err(ubi, "cannot register UBI character devices");
		return err;
	}

	ubi->dev.devt = dev;

	ubi_assert(MINOR(dev) == 0);
	cdev_init(&ubi->cdev, &ubi_cdev_operations);
	dbg_gen("%s major is %u", ubi->ubi_name, MAJOR(dev));
	ubi->cdev.owner = THIS_MODULE;

	dev_set_name(&ubi->dev, UBI_NAME_STR "%d", ubi->ubi_num);
	err = cdev_device_add(&ubi->cdev, &ubi->dev);
	if (err)
		goto out_unreg;

	for (i = 0; i < ubi->vtbl_slots; i++)
		if (ubi->volumes[i]) {
			err = ubi_add_volume(ubi, ubi->volumes[i]);
			if (err) {
				ubi_err(ubi, "cannot add volume %d", i);
				ubi->volumes[i] = NULL;
				goto out_volumes;
			}
		}

	return 0;

out_volumes:
	kill_volumes(ubi);
	cdev_device_del(&ubi->cdev, &ubi->dev);
out_unreg:
	unregister_chrdev_region(ubi->cdev.dev, ubi->vtbl_slots + 1);
	ubi_err(ubi, "cannot initialize UBI %s, error %d",
		ubi->ubi_name, err);
	return err;
}

 
static void uif_close(struct ubi_device *ubi)
{
	kill_volumes(ubi);
	cdev_device_del(&ubi->cdev, &ubi->dev);
	unregister_chrdev_region(ubi->cdev.dev, ubi->vtbl_slots + 1);
}

 
static void ubi_free_volumes_from(struct ubi_device *ubi, int from)
{
	int i;

	for (i = from; i < ubi->vtbl_slots + UBI_INT_VOL_COUNT; i++) {
		if (!ubi->volumes[i])
			continue;
		ubi_eba_replace_table(ubi->volumes[i], NULL);
		ubi_fastmap_destroy_checkmap(ubi->volumes[i]);
		kfree(ubi->volumes[i]);
		ubi->volumes[i] = NULL;
	}
}

 
void ubi_free_all_volumes(struct ubi_device *ubi)
{
	ubi_free_volumes_from(ubi, 0);
}

 
void ubi_free_internal_volumes(struct ubi_device *ubi)
{
	ubi_free_volumes_from(ubi, ubi->vtbl_slots);
}

static int get_bad_peb_limit(const struct ubi_device *ubi, int max_beb_per1024)
{
	int limit, device_pebs;
	uint64_t device_size;

	if (!max_beb_per1024) {
		 
		limit = mtd_max_bad_blocks(ubi->mtd, 0, ubi->mtd->size);
		if (limit < 0)
			return 0;
		return limit;
	}

	 
	device_size = mtd_get_device_size(ubi->mtd);
	device_pebs = mtd_div_by_eb(device_size, ubi->mtd);
	limit = mult_frac(device_pebs, max_beb_per1024, 1024);

	 
	if (mult_frac(limit, 1024, max_beb_per1024) < device_pebs)
		limit += 1;

	return limit;
}

 
static int io_init(struct ubi_device *ubi, int max_beb_per1024)
{
	dbg_gen("sizeof(struct ubi_ainf_peb) %zu", sizeof(struct ubi_ainf_peb));
	dbg_gen("sizeof(struct ubi_wl_entry) %zu", sizeof(struct ubi_wl_entry));

	if (ubi->mtd->numeraseregions != 0) {
		 
		ubi_err(ubi, "multiple regions, not implemented");
		return -EINVAL;
	}

	if (ubi->vid_hdr_offset < 0)
		return -EINVAL;

	 

	ubi->peb_size   = ubi->mtd->erasesize;
	ubi->peb_count  = mtd_div_by_eb(ubi->mtd->size, ubi->mtd);
	ubi->flash_size = ubi->mtd->size;

	if (mtd_can_have_bb(ubi->mtd)) {
		ubi->bad_allowed = 1;
		ubi->bad_peb_limit = get_bad_peb_limit(ubi, max_beb_per1024);
	}

	if (ubi->mtd->type == MTD_NORFLASH)
		ubi->nor_flash = 1;

	ubi->min_io_size = ubi->mtd->writesize;
	ubi->hdrs_min_io_size = ubi->mtd->writesize >> ubi->mtd->subpage_sft;

	 
	if (!is_power_of_2(ubi->min_io_size)) {
		ubi_err(ubi, "min. I/O unit (%d) is not power of 2",
			ubi->min_io_size);
		return -EINVAL;
	}

	ubi_assert(ubi->hdrs_min_io_size > 0);
	ubi_assert(ubi->hdrs_min_io_size <= ubi->min_io_size);
	ubi_assert(ubi->min_io_size % ubi->hdrs_min_io_size == 0);

	ubi->max_write_size = ubi->mtd->writebufsize;
	 
	if (ubi->max_write_size < ubi->min_io_size ||
	    ubi->max_write_size % ubi->min_io_size ||
	    !is_power_of_2(ubi->max_write_size)) {
		ubi_err(ubi, "bad write buffer size %d for %d min. I/O unit",
			ubi->max_write_size, ubi->min_io_size);
		return -EINVAL;
	}

	 
	ubi->ec_hdr_alsize = ALIGN(UBI_EC_HDR_SIZE, ubi->hdrs_min_io_size);
	ubi->vid_hdr_alsize = ALIGN(UBI_VID_HDR_SIZE, ubi->hdrs_min_io_size);

	dbg_gen("min_io_size      %d", ubi->min_io_size);
	dbg_gen("max_write_size   %d", ubi->max_write_size);
	dbg_gen("hdrs_min_io_size %d", ubi->hdrs_min_io_size);
	dbg_gen("ec_hdr_alsize    %d", ubi->ec_hdr_alsize);
	dbg_gen("vid_hdr_alsize   %d", ubi->vid_hdr_alsize);

	if (ubi->vid_hdr_offset == 0)
		 
		ubi->vid_hdr_offset = ubi->vid_hdr_aloffset =
				      ubi->ec_hdr_alsize;
	else {
		ubi->vid_hdr_aloffset = ubi->vid_hdr_offset &
						~(ubi->hdrs_min_io_size - 1);
		ubi->vid_hdr_shift = ubi->vid_hdr_offset -
						ubi->vid_hdr_aloffset;
	}

	 
	if ((ubi->vid_hdr_shift + UBI_VID_HDR_SIZE) > ubi->vid_hdr_alsize) {
		ubi_err(ubi, "Invalid VID header offset %d, VID header shift(%d)"
			" + VID header size(%zu) > VID header aligned size(%d).",
			ubi->vid_hdr_offset, ubi->vid_hdr_shift,
			UBI_VID_HDR_SIZE, ubi->vid_hdr_alsize);
		return -EINVAL;
	}

	 
	ubi->leb_start = ubi->vid_hdr_offset + UBI_VID_HDR_SIZE;
	ubi->leb_start = ALIGN(ubi->leb_start, ubi->min_io_size);

	dbg_gen("vid_hdr_offset   %d", ubi->vid_hdr_offset);
	dbg_gen("vid_hdr_aloffset %d", ubi->vid_hdr_aloffset);
	dbg_gen("vid_hdr_shift    %d", ubi->vid_hdr_shift);
	dbg_gen("leb_start        %d", ubi->leb_start);

	 
	if (ubi->vid_hdr_shift % 4) {
		ubi_err(ubi, "unaligned VID header shift %d",
			ubi->vid_hdr_shift);
		return -EINVAL;
	}

	 
	if (ubi->vid_hdr_offset < UBI_EC_HDR_SIZE ||
	    ubi->leb_start < ubi->vid_hdr_offset + UBI_VID_HDR_SIZE ||
	    ubi->leb_start > ubi->peb_size - UBI_VID_HDR_SIZE ||
	    ubi->leb_start & (ubi->min_io_size - 1)) {
		ubi_err(ubi, "bad VID header (%d) or data offsets (%d)",
			ubi->vid_hdr_offset, ubi->leb_start);
		return -EINVAL;
	}

	 
	ubi->max_erroneous = ubi->peb_count / 10;
	if (ubi->max_erroneous < 16)
		ubi->max_erroneous = 16;
	dbg_gen("max_erroneous    %d", ubi->max_erroneous);

	 
	if (ubi->vid_hdr_offset + UBI_VID_HDR_SIZE <= ubi->hdrs_min_io_size) {
		ubi_warn(ubi, "EC and VID headers are in the same minimal I/O unit, switch to read-only mode");
		ubi->ro_mode = 1;
	}

	ubi->leb_size = ubi->peb_size - ubi->leb_start;

	if (!(ubi->mtd->flags & MTD_WRITEABLE)) {
		ubi_msg(ubi, "MTD device %d is write-protected, attach in read-only mode",
			ubi->mtd->index);
		ubi->ro_mode = 1;
	}

	 

	return 0;
}

 
static int autoresize(struct ubi_device *ubi, int vol_id)
{
	struct ubi_volume_desc desc;
	struct ubi_volume *vol = ubi->volumes[vol_id];
	int err, old_reserved_pebs = vol->reserved_pebs;

	if (ubi->ro_mode) {
		ubi_warn(ubi, "skip auto-resize because of R/O mode");
		return 0;
	}

	 
	ubi->vtbl[vol_id].flags &= ~UBI_VTBL_AUTORESIZE_FLG;

	if (ubi->avail_pebs == 0) {
		struct ubi_vtbl_record vtbl_rec;

		 
		vtbl_rec = ubi->vtbl[vol_id];
		err = ubi_change_vtbl_record(ubi, vol_id, &vtbl_rec);
		if (err)
			ubi_err(ubi, "cannot clean auto-resize flag for volume %d",
				vol_id);
	} else {
		desc.vol = vol;
		err = ubi_resize_volume(&desc,
					old_reserved_pebs + ubi->avail_pebs);
		if (err)
			ubi_err(ubi, "cannot auto-resize volume %d",
				vol_id);
	}

	if (err)
		return err;

	ubi_msg(ubi, "volume %d (\"%s\") re-sized from %d to %d LEBs",
		vol_id, vol->name, old_reserved_pebs, vol->reserved_pebs);
	return 0;
}

 
int ubi_attach_mtd_dev(struct mtd_info *mtd, int ubi_num,
		       int vid_hdr_offset, int max_beb_per1024, bool disable_fm)
{
	struct ubi_device *ubi;
	int i, err;

	if (max_beb_per1024 < 0 || max_beb_per1024 > MAX_MTD_UBI_BEB_LIMIT)
		return -EINVAL;

	if (!max_beb_per1024)
		max_beb_per1024 = CONFIG_MTD_UBI_BEB_LIMIT;

	 
	for (i = 0; i < UBI_MAX_DEVICES; i++) {
		ubi = ubi_devices[i];
		if (ubi && mtd->index == ubi->mtd->index) {
			pr_err("ubi: mtd%d is already attached to ubi%d\n",
				mtd->index, i);
			return -EEXIST;
		}
	}

	 
	if (mtd->type == MTD_UBIVOLUME) {
		pr_err("ubi: refuse attaching mtd%d - it is already emulated on top of UBI\n",
			mtd->index);
		return -EINVAL;
	}

	 
	if (mtd->type == MTD_MLCNANDFLASH &&
	    !(mtd->flags & MTD_SLC_ON_MLC_EMULATION)) {
		pr_err("ubi: refuse attaching mtd%d - MLC NAND is not supported\n",
			mtd->index);
		return -EINVAL;
	}

	 
	if (!mtd->erasesize) {
		pr_err("ubi: refuse attaching mtd%d - zero erasesize flash is not supported\n",
			mtd->index);
		return -EINVAL;
	}

	if (ubi_num == UBI_DEV_NUM_AUTO) {
		 
		for (ubi_num = 0; ubi_num < UBI_MAX_DEVICES; ubi_num++)
			if (!ubi_devices[ubi_num])
				break;
		if (ubi_num == UBI_MAX_DEVICES) {
			pr_err("ubi: only %d UBI devices may be created\n",
				UBI_MAX_DEVICES);
			return -ENFILE;
		}
	} else {
		if (ubi_num >= UBI_MAX_DEVICES)
			return -EINVAL;

		 
		if (ubi_devices[ubi_num]) {
			pr_err("ubi: ubi%i already exists\n", ubi_num);
			return -EEXIST;
		}
	}

	ubi = kzalloc(sizeof(struct ubi_device), GFP_KERNEL);
	if (!ubi)
		return -ENOMEM;

	device_initialize(&ubi->dev);
	ubi->dev.release = dev_release;
	ubi->dev.class = &ubi_class;
	ubi->dev.groups = ubi_dev_groups;
	ubi->dev.parent = &mtd->dev;

	ubi->mtd = mtd;
	ubi->ubi_num = ubi_num;
	ubi->vid_hdr_offset = vid_hdr_offset;
	ubi->autoresize_vol_id = -1;

#ifdef CONFIG_MTD_UBI_FASTMAP
	ubi->fm_pool.used = ubi->fm_pool.size = 0;
	ubi->fm_wl_pool.used = ubi->fm_wl_pool.size = 0;

	 
	ubi->fm_pool.max_size = min(((int)mtd_div_by_eb(ubi->mtd->size,
		ubi->mtd) / 100) * 5, UBI_FM_MAX_POOL_SIZE);
	ubi->fm_pool.max_size = max(ubi->fm_pool.max_size,
		UBI_FM_MIN_POOL_SIZE);

	ubi->fm_wl_pool.max_size = ubi->fm_pool.max_size / 2;
	ubi->fm_disabled = (!fm_autoconvert || disable_fm) ? 1 : 0;
	if (fm_debug)
		ubi_enable_dbg_chk_fastmap(ubi);

	if (!ubi->fm_disabled && (int)mtd_div_by_eb(ubi->mtd->size, ubi->mtd)
	    <= UBI_FM_MAX_START) {
		ubi_err(ubi, "More than %i PEBs are needed for fastmap, sorry.",
			UBI_FM_MAX_START);
		ubi->fm_disabled = 1;
	}

	ubi_msg(ubi, "default fastmap pool size: %d", ubi->fm_pool.max_size);
	ubi_msg(ubi, "default fastmap WL pool size: %d",
		ubi->fm_wl_pool.max_size);
#else
	ubi->fm_disabled = 1;
#endif
	mutex_init(&ubi->buf_mutex);
	mutex_init(&ubi->ckvol_mutex);
	mutex_init(&ubi->device_mutex);
	spin_lock_init(&ubi->volumes_lock);
	init_rwsem(&ubi->fm_protect);
	init_rwsem(&ubi->fm_eba_sem);

	ubi_msg(ubi, "attaching mtd%d", mtd->index);

	err = io_init(ubi, max_beb_per1024);
	if (err)
		goto out_free;

	err = -ENOMEM;
	ubi->peb_buf = vmalloc(ubi->peb_size);
	if (!ubi->peb_buf)
		goto out_free;

#ifdef CONFIG_MTD_UBI_FASTMAP
	ubi->fm_size = ubi_calc_fm_size(ubi);
	ubi->fm_buf = vzalloc(ubi->fm_size);
	if (!ubi->fm_buf)
		goto out_free;
#endif
	err = ubi_attach(ubi, disable_fm ? 1 : 0);
	if (err) {
		ubi_err(ubi, "failed to attach mtd%d, error %d",
			mtd->index, err);
		goto out_free;
	}

	if (ubi->autoresize_vol_id != -1) {
		err = autoresize(ubi, ubi->autoresize_vol_id);
		if (err)
			goto out_detach;
	}

	err = uif_init(ubi);
	if (err)
		goto out_detach;

	err = ubi_debugfs_init_dev(ubi);
	if (err)
		goto out_uif;

	ubi->bgt_thread = kthread_create(ubi_thread, ubi, "%s", ubi->bgt_name);
	if (IS_ERR(ubi->bgt_thread)) {
		err = PTR_ERR(ubi->bgt_thread);
		ubi_err(ubi, "cannot spawn \"%s\", error %d",
			ubi->bgt_name, err);
		goto out_debugfs;
	}

	ubi_msg(ubi, "attached mtd%d (name \"%s\", size %llu MiB)",
		mtd->index, mtd->name, ubi->flash_size >> 20);
	ubi_msg(ubi, "PEB size: %d bytes (%d KiB), LEB size: %d bytes",
		ubi->peb_size, ubi->peb_size >> 10, ubi->leb_size);
	ubi_msg(ubi, "min./max. I/O unit sizes: %d/%d, sub-page size %d",
		ubi->min_io_size, ubi->max_write_size, ubi->hdrs_min_io_size);
	ubi_msg(ubi, "VID header offset: %d (aligned %d), data offset: %d",
		ubi->vid_hdr_offset, ubi->vid_hdr_aloffset, ubi->leb_start);
	ubi_msg(ubi, "good PEBs: %d, bad PEBs: %d, corrupted PEBs: %d",
		ubi->good_peb_count, ubi->bad_peb_count, ubi->corr_peb_count);
	ubi_msg(ubi, "user volume: %d, internal volumes: %d, max. volumes count: %d",
		ubi->vol_count - UBI_INT_VOL_COUNT, UBI_INT_VOL_COUNT,
		ubi->vtbl_slots);
	ubi_msg(ubi, "max/mean erase counter: %d/%d, WL threshold: %d, image sequence number: %u",
		ubi->max_ec, ubi->mean_ec, CONFIG_MTD_UBI_WL_THRESHOLD,
		ubi->image_seq);
	ubi_msg(ubi, "available PEBs: %d, total reserved PEBs: %d, PEBs reserved for bad PEB handling: %d",
		ubi->avail_pebs, ubi->rsvd_pebs, ubi->beb_rsvd_pebs);

	 
	spin_lock(&ubi->wl_lock);
	ubi->thread_enabled = 1;
	wake_up_process(ubi->bgt_thread);
	spin_unlock(&ubi->wl_lock);

	ubi_devices[ubi_num] = ubi;
	ubi_notify_all(ubi, UBI_VOLUME_ADDED, NULL);
	return ubi_num;

out_debugfs:
	ubi_debugfs_exit_dev(ubi);
out_uif:
	uif_close(ubi);
out_detach:
	ubi_wl_close(ubi);
	ubi_free_all_volumes(ubi);
	vfree(ubi->vtbl);
out_free:
	vfree(ubi->peb_buf);
	vfree(ubi->fm_buf);
	put_device(&ubi->dev);
	return err;
}

 
int ubi_detach_mtd_dev(int ubi_num, int anyway)
{
	struct ubi_device *ubi;

	if (ubi_num < 0 || ubi_num >= UBI_MAX_DEVICES)
		return -EINVAL;

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return -EINVAL;

	spin_lock(&ubi_devices_lock);
	put_device(&ubi->dev);
	ubi->ref_count -= 1;
	if (ubi->ref_count) {
		if (!anyway) {
			spin_unlock(&ubi_devices_lock);
			return -EBUSY;
		}
		 
		ubi_err(ubi, "%s reference count %d, destroy anyway",
			ubi->ubi_name, ubi->ref_count);
	}
	ubi_devices[ubi_num] = NULL;
	spin_unlock(&ubi_devices_lock);

	ubi_assert(ubi_num == ubi->ubi_num);
	ubi_notify_all(ubi, UBI_VOLUME_REMOVED, NULL);
	ubi_msg(ubi, "detaching mtd%d", ubi->mtd->index);
#ifdef CONFIG_MTD_UBI_FASTMAP
	 
	if (!ubi_dbg_chk_fastmap(ubi))
		ubi_update_fastmap(ubi);
#endif
	 
	if (ubi->bgt_thread)
		kthread_stop(ubi->bgt_thread);

#ifdef CONFIG_MTD_UBI_FASTMAP
	cancel_work_sync(&ubi->fm_work);
#endif
	ubi_debugfs_exit_dev(ubi);
	uif_close(ubi);

	ubi_wl_close(ubi);
	ubi_free_internal_volumes(ubi);
	vfree(ubi->vtbl);
	vfree(ubi->peb_buf);
	vfree(ubi->fm_buf);
	ubi_msg(ubi, "mtd%d is detached", ubi->mtd->index);
	put_mtd_device(ubi->mtd);
	put_device(&ubi->dev);
	return 0;
}

 
static struct mtd_info * __init open_mtd_by_chdev(const char *mtd_dev)
{
	int err, minor;
	struct path path;
	struct kstat stat;

	 
	err = kern_path(mtd_dev, LOOKUP_FOLLOW, &path);
	if (err)
		return ERR_PTR(err);

	err = vfs_getattr(&path, &stat, STATX_TYPE, AT_STATX_SYNC_AS_STAT);
	path_put(&path);
	if (err)
		return ERR_PTR(err);

	 
	if (MAJOR(stat.rdev) != MTD_CHAR_MAJOR || !S_ISCHR(stat.mode))
		return ERR_PTR(-EINVAL);

	minor = MINOR(stat.rdev);

	if (minor & 1)
		 
		return ERR_PTR(-EINVAL);

	return get_mtd_device(NULL, minor / 2);
}

 
static struct mtd_info * __init open_mtd_device(const char *mtd_dev)
{
	struct mtd_info *mtd;
	int mtd_num;
	char *endp;

	mtd_num = simple_strtoul(mtd_dev, &endp, 0);
	if (*endp != '\0' || mtd_dev == endp) {
		 
		mtd = get_mtd_device_nm(mtd_dev);
		if (PTR_ERR(mtd) == -ENODEV)
			 
			mtd = open_mtd_by_chdev(mtd_dev);
	} else
		mtd = get_mtd_device(NULL, mtd_num);

	return mtd;
}

static int __init ubi_init(void)
{
	int err, i, k;

	 
	BUILD_BUG_ON(sizeof(struct ubi_ec_hdr) != 64);
	BUILD_BUG_ON(sizeof(struct ubi_vid_hdr) != 64);

	if (mtd_devs > UBI_MAX_DEVICES) {
		pr_err("UBI error: too many MTD devices, maximum is %d\n",
		       UBI_MAX_DEVICES);
		return -EINVAL;
	}

	 
	err = class_register(&ubi_class);
	if (err < 0)
		return err;

	err = misc_register(&ubi_ctrl_cdev);
	if (err) {
		pr_err("UBI error: cannot register device\n");
		goto out;
	}

	ubi_wl_entry_slab = kmem_cache_create("ubi_wl_entry_slab",
					      sizeof(struct ubi_wl_entry),
					      0, 0, NULL);
	if (!ubi_wl_entry_slab) {
		err = -ENOMEM;
		goto out_dev_unreg;
	}

	err = ubi_debugfs_init();
	if (err)
		goto out_slab;


	 
	for (i = 0; i < mtd_devs; i++) {
		struct mtd_dev_param *p = &mtd_dev_param[i];
		struct mtd_info *mtd;

		cond_resched();

		mtd = open_mtd_device(p->name);
		if (IS_ERR(mtd)) {
			err = PTR_ERR(mtd);
			pr_err("UBI error: cannot open mtd %s, error %d\n",
			       p->name, err);
			 
			if (ubi_is_module())
				goto out_detach;
			continue;
		}

		mutex_lock(&ubi_devices_mutex);
		err = ubi_attach_mtd_dev(mtd, p->ubi_num,
					 p->vid_hdr_offs, p->max_beb_per1024,
					 p->enable_fm == 0);
		mutex_unlock(&ubi_devices_mutex);
		if (err < 0) {
			pr_err("UBI error: cannot attach mtd%d\n",
			       mtd->index);
			put_mtd_device(mtd);

			 
			if (ubi_is_module())
				goto out_detach;
		}
	}

	err = ubiblock_init();
	if (err) {
		pr_err("UBI error: block: cannot initialize, error %d\n", err);

		 
		if (ubi_is_module())
			goto out_detach;
	}

	return 0;

out_detach:
	for (k = 0; k < i; k++)
		if (ubi_devices[k]) {
			mutex_lock(&ubi_devices_mutex);
			ubi_detach_mtd_dev(ubi_devices[k]->ubi_num, 1);
			mutex_unlock(&ubi_devices_mutex);
		}
	ubi_debugfs_exit();
out_slab:
	kmem_cache_destroy(ubi_wl_entry_slab);
out_dev_unreg:
	misc_deregister(&ubi_ctrl_cdev);
out:
	class_unregister(&ubi_class);
	pr_err("UBI error: cannot initialize UBI, error %d\n", err);
	return err;
}
late_initcall(ubi_init);

static void __exit ubi_exit(void)
{
	int i;

	ubiblock_exit();

	for (i = 0; i < UBI_MAX_DEVICES; i++)
		if (ubi_devices[i]) {
			mutex_lock(&ubi_devices_mutex);
			ubi_detach_mtd_dev(ubi_devices[i]->ubi_num, 1);
			mutex_unlock(&ubi_devices_mutex);
		}
	ubi_debugfs_exit();
	kmem_cache_destroy(ubi_wl_entry_slab);
	misc_deregister(&ubi_ctrl_cdev);
	class_unregister(&ubi_class);
}
module_exit(ubi_exit);

 
static int bytes_str_to_int(const char *str)
{
	char *endp;
	unsigned long result;

	result = simple_strtoul(str, &endp, 0);
	if (str == endp || result >= INT_MAX) {
		pr_err("UBI error: incorrect bytes count: \"%s\"\n", str);
		return -EINVAL;
	}

	switch (*endp) {
	case 'G':
		result *= 1024;
		fallthrough;
	case 'M':
		result *= 1024;
		fallthrough;
	case 'K':
		result *= 1024;
		break;
	case '\0':
		break;
	default:
		pr_err("UBI error: incorrect bytes count: \"%s\"\n", str);
		return -EINVAL;
	}

	return result;
}

 
static int ubi_mtd_param_parse(const char *val, const struct kernel_param *kp)
{
	int i, len;
	struct mtd_dev_param *p;
	char buf[MTD_PARAM_LEN_MAX];
	char *pbuf = &buf[0];
	char *tokens[MTD_PARAM_MAX_COUNT], *token;

	if (!val)
		return -EINVAL;

	if (mtd_devs == UBI_MAX_DEVICES) {
		pr_err("UBI error: too many parameters, max. is %d\n",
		       UBI_MAX_DEVICES);
		return -EINVAL;
	}

	len = strnlen(val, MTD_PARAM_LEN_MAX);
	if (len == MTD_PARAM_LEN_MAX) {
		pr_err("UBI error: parameter \"%s\" is too long, max. is %d\n",
		       val, MTD_PARAM_LEN_MAX);
		return -EINVAL;
	}

	if (len == 0) {
		pr_warn("UBI warning: empty 'mtd=' parameter - ignored\n");
		return 0;
	}

	strcpy(buf, val);

	 
	if (buf[len - 1] == '\n')
		buf[len - 1] = '\0';

	for (i = 0; i < MTD_PARAM_MAX_COUNT; i++)
		tokens[i] = strsep(&pbuf, ",");

	if (pbuf) {
		pr_err("UBI error: too many arguments at \"%s\"\n", val);
		return -EINVAL;
	}

	p = &mtd_dev_param[mtd_devs];
	strcpy(&p->name[0], tokens[0]);

	token = tokens[1];
	if (token) {
		p->vid_hdr_offs = bytes_str_to_int(token);

		if (p->vid_hdr_offs < 0)
			return p->vid_hdr_offs;
	}

	token = tokens[2];
	if (token) {
		int err = kstrtoint(token, 10, &p->max_beb_per1024);

		if (err) {
			pr_err("UBI error: bad value for max_beb_per1024 parameter: %s\n",
			       token);
			return -EINVAL;
		}
	}

	token = tokens[3];
	if (token) {
		int err = kstrtoint(token, 10, &p->ubi_num);

		if (err) {
			pr_err("UBI error: bad value for ubi_num parameter: %s\n",
			       token);
			return -EINVAL;
		}
	} else
		p->ubi_num = UBI_DEV_NUM_AUTO;

	token = tokens[4];
	if (token) {
		int err = kstrtoint(token, 10, &p->enable_fm);

		if (err) {
			pr_err("UBI error: bad value for enable_fm parameter: %s\n",
				token);
			return -EINVAL;
		}
	} else
		p->enable_fm = 0;

	mtd_devs += 1;
	return 0;
}

module_param_call(mtd, ubi_mtd_param_parse, NULL, NULL, 0400);
MODULE_PARM_DESC(mtd, "MTD devices to attach. Parameter format: mtd=<name|num|path>[,<vid_hdr_offs>[,max_beb_per1024[,ubi_num]]].\n"
		      "Multiple \"mtd\" parameters may be specified.\n"
		      "MTD devices may be specified by their number, name, or path to the MTD character device node.\n"
		      "Optional \"vid_hdr_offs\" parameter specifies UBI VID header position to be used by UBI. (default value if 0)\n"
		      "Optional \"max_beb_per1024\" parameter specifies the maximum expected bad eraseblock per 1024 eraseblocks. (default value ("
		      __stringify(CONFIG_MTD_UBI_BEB_LIMIT) ") if 0)\n"
		      "Optional \"ubi_num\" parameter specifies UBI device number which have to be assigned to the newly created UBI device (assigned automatically by default)\n"
		      "Optional \"enable_fm\" parameter determines whether to enable fastmap during attach. If the value is non-zero, fastmap is enabled. Default value is 0.\n"
		      "\n"
		      "Example 1: mtd=/dev/mtd0 - attach MTD device /dev/mtd0.\n"
		      "Example 2: mtd=content,1984 mtd=4 - attach MTD device with name \"content\" using VID header offset 1984, and MTD device number 4 with default VID header offset.\n"
		      "Example 3: mtd=/dev/mtd1,0,25 - attach MTD device /dev/mtd1 using default VID header offset and reserve 25*nand_size_in_blocks/1024 erase blocks for bad block handling.\n"
		      "Example 4: mtd=/dev/mtd1,0,0,5 - attach MTD device /dev/mtd1 to UBI 5 and using default values for the other fields.\n"
		      "example 5: mtd=1,0,0,5 mtd=2,0,0,6,1 - attach MTD device /dev/mtd1 to UBI 5 and disable fastmap; attach MTD device /dev/mtd2 to UBI 6 and enable fastmap.(only works when fastmap is enabled and fm_autoconvert=Y).\n"
		      "\t(e.g. if the NAND *chipset* has 4096 PEB, 100 will be reserved for this UBI device).");
#ifdef CONFIG_MTD_UBI_FASTMAP
module_param(fm_autoconvert, bool, 0644);
MODULE_PARM_DESC(fm_autoconvert, "Set this parameter to enable fastmap automatically on images without a fastmap.");
module_param(fm_debug, bool, 0);
MODULE_PARM_DESC(fm_debug, "Set this parameter to enable fastmap debugging by default. Warning, this will make fastmap slow!");
#endif
MODULE_VERSION(__stringify(UBI_VERSION));
MODULE_DESCRIPTION("UBI - Unsorted Block Images");
MODULE_AUTHOR("Artem Bityutskiy");
MODULE_LICENSE("GPL");
