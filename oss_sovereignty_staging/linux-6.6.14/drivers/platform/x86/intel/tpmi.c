
 

#include <linux/auxiliary_bus.h>
#include <linux/bitfield.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/intel_tpmi.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/security.h>
#include <linux/sizes.h>
#include <linux/string_helpers.h>

#include "vsec.h"

 
struct intel_tpmi_pfs_entry {
	u64 tpmi_id:8;
	u64 num_entries:8;
	u64 entry_size:16;
	u64 cap_offset:16;
	u64 attribute:2;
	u64 reserved:14;
} __packed;

 
struct intel_tpmi_pm_feature {
	struct intel_tpmi_pfs_entry pfs_header;
	unsigned int vsec_offset;
	struct intel_vsec_device *vsec_dev;
};

 
struct intel_tpmi_info {
	struct intel_tpmi_pm_feature *tpmi_features;
	struct intel_vsec_device *vsec_dev;
	int feature_count;
	u64 pfs_start;
	struct intel_tpmi_plat_info plat_info;
	void __iomem *tpmi_control_mem;
	struct dentry *dbgfs_dir;
};

 
struct tpmi_info_header {
	u64 fn:3;
	u64 dev:5;
	u64 bus:8;
	u64 pkg:8;
	u64 reserved:39;
	u64 lock:1;
} __packed;

 
enum intel_tpmi_id {
	TPMI_ID_RAPL = 0,  
	TPMI_ID_PEM = 1,  
	TPMI_ID_UNCORE = 2,  
	TPMI_ID_SST = 5,  
	TPMI_CONTROL_ID = 0x80,  
	TPMI_INFO_ID = 0x81,  
};

 
#define TPMI_GET_SINGLE_ENTRY_SIZE(pfs)							\
({											\
	pfs->pfs_header.entry_size > SZ_1K ? 0 : pfs->pfs_header.entry_size << 2;	\
})

 
static DEFINE_IDA(intel_vsec_tpmi_ida);

struct intel_tpmi_plat_info *tpmi_get_platform_data(struct auxiliary_device *auxdev)
{
	struct intel_vsec_device *vsec_dev = auxdev_to_ivdev(auxdev);

	return vsec_dev->priv_data;
}
EXPORT_SYMBOL_NS_GPL(tpmi_get_platform_data, INTEL_TPMI);

int tpmi_get_resource_count(struct auxiliary_device *auxdev)
{
	struct intel_vsec_device *vsec_dev = auxdev_to_ivdev(auxdev);

	if (vsec_dev)
		return vsec_dev->num_resources;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(tpmi_get_resource_count, INTEL_TPMI);

struct resource *tpmi_get_resource_at_index(struct auxiliary_device *auxdev, int index)
{
	struct intel_vsec_device *vsec_dev = auxdev_to_ivdev(auxdev);

	if (vsec_dev && index < vsec_dev->num_resources)
		return &vsec_dev->resource[index];

	return NULL;
}
EXPORT_SYMBOL_NS_GPL(tpmi_get_resource_at_index, INTEL_TPMI);

 

#define TPMI_CONTROL_STATUS_OFFSET	0x00
#define TPMI_COMMAND_OFFSET		0x08

 
#define TPMI_CONTROL_TIMEOUT_US		(10 * USEC_PER_MSEC)
#define TPMI_CONTROL_TIMEOUT_MAX_US	(1 * USEC_PER_SEC)

#define TPMI_RB_TIMEOUT_US		(10 * USEC_PER_MSEC)
#define TPMI_RB_TIMEOUT_MAX_US		USEC_PER_SEC

 

#define TPMI_CONTROL_STATUS_RB		BIT_ULL(0)

#define TPMI_CONTROL_STATUS_OWNER	GENMASK_ULL(5, 4)
#define TPMI_OWNER_NONE			0
#define TPMI_OWNER_IN_BAND		1

#define TPMI_CONTROL_STATUS_CPL		BIT_ULL(6)
#define TPMI_CONTROL_STATUS_RESULT	GENMASK_ULL(15, 8)
#define TPMI_CONTROL_STATUS_LEN		GENMASK_ULL(31, 16)

#define TPMI_CMD_PKT_LEN		2
#define TPMI_CMD_STATUS_SUCCESS		0x40

 
#define TMPI_CONTROL_DATA_CMD		GENMASK_ULL(7, 0)
#define TMPI_CONTROL_DATA_VAL		GENMASK_ULL(63, 32)
#define TPMI_CONTROL_DATA_VAL_FEATURE	GENMASK_ULL(48, 40)

 
#define TPMI_CONTROL_GET_STATE_CMD	0x10

#define TPMI_CONTROL_CMD_MASK		GENMASK_ULL(48, 40)

#define TPMI_CMD_LEN_MASK		GENMASK_ULL(18, 16)

#define TPMI_STATE_DISABLED		BIT_ULL(0)
#define TPMI_STATE_LOCKED		BIT_ULL(31)

 
static DEFINE_MUTEX(tpmi_dev_lock);

static int tpmi_wait_for_owner(struct intel_tpmi_info *tpmi_info, u8 owner)
{
	u64 control;

	return readq_poll_timeout(tpmi_info->tpmi_control_mem + TPMI_CONTROL_STATUS_OFFSET,
				  control, owner == FIELD_GET(TPMI_CONTROL_STATUS_OWNER, control),
				  TPMI_CONTROL_TIMEOUT_US, TPMI_CONTROL_TIMEOUT_MAX_US);
}

static int tpmi_read_feature_status(struct intel_tpmi_info *tpmi_info, int feature_id,
				    int *locked, int *disabled)
{
	u64 control, data;
	int ret;

	if (!tpmi_info->tpmi_control_mem)
		return -EFAULT;

	mutex_lock(&tpmi_dev_lock);

	 
	ret = tpmi_wait_for_owner(tpmi_info, TPMI_OWNER_NONE);
	if (ret)
		goto err_unlock;

	 
	data = FIELD_PREP(TMPI_CONTROL_DATA_CMD, TPMI_CONTROL_GET_STATE_CMD);

	 
	data |= FIELD_PREP(TPMI_CONTROL_DATA_VAL_FEATURE, feature_id);

	 
	writeq(data, tpmi_info->tpmi_control_mem + TPMI_COMMAND_OFFSET);

	 
	ret = tpmi_wait_for_owner(tpmi_info, TPMI_OWNER_IN_BAND);
	if (ret)
		goto err_unlock;

	 
	control = TPMI_CONTROL_STATUS_RB;
	control |= FIELD_PREP(TPMI_CONTROL_STATUS_LEN, TPMI_CMD_PKT_LEN);

	 
	writeq(control, tpmi_info->tpmi_control_mem + TPMI_CONTROL_STATUS_OFFSET);

	 
	ret = readq_poll_timeout(tpmi_info->tpmi_control_mem + TPMI_CONTROL_STATUS_OFFSET,
				 control, !(control & TPMI_CONTROL_STATUS_RB),
				 TPMI_RB_TIMEOUT_US, TPMI_RB_TIMEOUT_MAX_US);
	if (ret)
		goto done_proc;

	control = FIELD_GET(TPMI_CONTROL_STATUS_RESULT, control);
	if (control != TPMI_CMD_STATUS_SUCCESS) {
		ret = -EBUSY;
		goto done_proc;
	}

	 
	data = readq(tpmi_info->tpmi_control_mem + TPMI_COMMAND_OFFSET);
	data = FIELD_GET(TMPI_CONTROL_DATA_VAL, data);

	*disabled = 0;
	*locked = 0;

	if (!(data & TPMI_STATE_DISABLED))
		*disabled = 1;

	if (data & TPMI_STATE_LOCKED)
		*locked = 1;

	ret = 0;

done_proc:
	 
	writeq(TPMI_CONTROL_STATUS_CPL, tpmi_info->tpmi_control_mem + TPMI_CONTROL_STATUS_OFFSET);

err_unlock:
	mutex_unlock(&tpmi_dev_lock);

	return ret;
}

int tpmi_get_feature_status(struct auxiliary_device *auxdev, int feature_id,
			    int *locked, int *disabled)
{
	struct intel_vsec_device *intel_vsec_dev = dev_to_ivdev(auxdev->dev.parent);
	struct intel_tpmi_info *tpmi_info = auxiliary_get_drvdata(&intel_vsec_dev->auxdev);

	return tpmi_read_feature_status(tpmi_info, feature_id, locked, disabled);
}
EXPORT_SYMBOL_NS_GPL(tpmi_get_feature_status, INTEL_TPMI);

static int tpmi_pfs_dbg_show(struct seq_file *s, void *unused)
{
	struct intel_tpmi_info *tpmi_info = s->private;
	struct intel_tpmi_pm_feature *pfs;
	int locked, disabled, ret, i;

	seq_printf(s, "tpmi PFS start offset 0x:%llx\n", tpmi_info->pfs_start);
	seq_puts(s, "tpmi_id\t\tentries\t\tsize\t\tcap_offset\tattribute\tvsec_offset\tlocked\tdisabled\n");
	for (i = 0; i < tpmi_info->feature_count; ++i) {
		pfs = &tpmi_info->tpmi_features[i];
		ret = tpmi_read_feature_status(tpmi_info, pfs->pfs_header.tpmi_id, &locked,
					       &disabled);
		if (ret) {
			locked = 'U';
			disabled = 'U';
		} else {
			disabled = disabled ? 'Y' : 'N';
			locked = locked ? 'Y' : 'N';
		}
		seq_printf(s, "0x%02x\t\t0x%02x\t\t0x%04x\t\t0x%04x\t\t0x%02x\t\t0x%08x\t%c\t%c\n",
			   pfs->pfs_header.tpmi_id, pfs->pfs_header.num_entries,
			   pfs->pfs_header.entry_size, pfs->pfs_header.cap_offset,
			   pfs->pfs_header.attribute, pfs->vsec_offset, locked, disabled);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(tpmi_pfs_dbg);

#define MEM_DUMP_COLUMN_COUNT	8

static int tpmi_mem_dump_show(struct seq_file *s, void *unused)
{
	size_t row_size = MEM_DUMP_COLUMN_COUNT * sizeof(u32);
	struct intel_tpmi_pm_feature *pfs = s->private;
	int count, ret = 0;
	void __iomem *mem;
	u32 off, size;
	u8 *buffer;

	size = TPMI_GET_SINGLE_ENTRY_SIZE(pfs);
	if (!size)
		return -EIO;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	off = pfs->vsec_offset;

	mutex_lock(&tpmi_dev_lock);

	for (count = 0; count < pfs->pfs_header.num_entries; ++count) {
		seq_printf(s, "TPMI Instance:%d offset:0x%x\n", count, off);

		mem = ioremap(off, size);
		if (!mem) {
			ret = -ENOMEM;
			break;
		}

		memcpy_fromio(buffer, mem, size);

		seq_hex_dump(s, " ", DUMP_PREFIX_OFFSET, row_size, sizeof(u32), buffer, size,
			     false);

		iounmap(mem);

		off += size;
	}

	mutex_unlock(&tpmi_dev_lock);

	kfree(buffer);

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(tpmi_mem_dump);

static ssize_t mem_write(struct file *file, const char __user *userbuf, size_t len, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct intel_tpmi_pm_feature *pfs = m->private;
	u32 addr, value, punit, size;
	u32 num_elems, *array;
	void __iomem *mem;
	int ret;

	size = TPMI_GET_SINGLE_ENTRY_SIZE(pfs);
	if (!size)
		return -EIO;

	ret = parse_int_array_user(userbuf, len, (int **)&array);
	if (ret < 0)
		return ret;

	num_elems = *array;
	if (num_elems != 3) {
		ret = -EINVAL;
		goto exit_write;
	}

	punit = array[1];
	addr = array[2];
	value = array[3];

	if (punit >= pfs->pfs_header.num_entries) {
		ret = -EINVAL;
		goto exit_write;
	}

	if (addr >= size) {
		ret = -EINVAL;
		goto exit_write;
	}

	mutex_lock(&tpmi_dev_lock);

	mem = ioremap(pfs->vsec_offset + punit * size, size);
	if (!mem) {
		ret = -ENOMEM;
		goto unlock_mem_write;
	}

	writel(value, mem + addr);

	iounmap(mem);

	ret = len;

unlock_mem_write:
	mutex_unlock(&tpmi_dev_lock);

exit_write:
	kfree(array);

	return ret;
}

static int mem_write_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int mem_write_open(struct inode *inode, struct file *file)
{
	return single_open(file, mem_write_show, inode->i_private);
}

static const struct file_operations mem_write_ops = {
	.open           = mem_write_open,
	.read           = seq_read,
	.write          = mem_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#define tpmi_to_dev(info)	(&info->vsec_dev->pcidev->dev)

static void tpmi_dbgfs_register(struct intel_tpmi_info *tpmi_info)
{
	char name[64];
	int i;

	snprintf(name, sizeof(name), "tpmi-%s", dev_name(tpmi_to_dev(tpmi_info)));
	tpmi_info->dbgfs_dir = debugfs_create_dir(name, NULL);

	debugfs_create_file("pfs_dump", 0444, tpmi_info->dbgfs_dir, tpmi_info, &tpmi_pfs_dbg_fops);

	for (i = 0; i < tpmi_info->feature_count; ++i) {
		struct intel_tpmi_pm_feature *pfs;
		struct dentry *dir;

		pfs = &tpmi_info->tpmi_features[i];
		snprintf(name, sizeof(name), "tpmi-id-%02x", pfs->pfs_header.tpmi_id);
		dir = debugfs_create_dir(name, tpmi_info->dbgfs_dir);

		debugfs_create_file("mem_dump", 0444, dir, pfs, &tpmi_mem_dump_fops);
		debugfs_create_file("mem_write", 0644, dir, pfs, &mem_write_ops);
	}
}

static void tpmi_set_control_base(struct auxiliary_device *auxdev,
				  struct intel_tpmi_info *tpmi_info,
				  struct intel_tpmi_pm_feature *pfs)
{
	void __iomem *mem;
	u32 size;

	size = TPMI_GET_SINGLE_ENTRY_SIZE(pfs);
	if (!size)
		return;

	mem = devm_ioremap(&auxdev->dev, pfs->vsec_offset, size);
	if (!mem)
		return;

	 
	tpmi_info->tpmi_control_mem = mem;
}

static const char *intel_tpmi_name(enum intel_tpmi_id id)
{
	switch (id) {
	case TPMI_ID_RAPL:
		return "rapl";
	case TPMI_ID_PEM:
		return "pem";
	case TPMI_ID_UNCORE:
		return "uncore";
	case TPMI_ID_SST:
		return "sst";
	default:
		return NULL;
	}
}

 
#define TPMI_FEATURE_NAME_LEN	14

static int tpmi_create_device(struct intel_tpmi_info *tpmi_info,
			      struct intel_tpmi_pm_feature *pfs,
			      u64 pfs_start)
{
	struct intel_vsec_device *vsec_dev = tpmi_info->vsec_dev;
	char feature_id_name[TPMI_FEATURE_NAME_LEN];
	struct intel_vsec_device *feature_vsec_dev;
	struct resource *res, *tmp;
	const char *name;
	int i;

	name = intel_tpmi_name(pfs->pfs_header.tpmi_id);
	if (!name)
		return -EOPNOTSUPP;

	res = kcalloc(pfs->pfs_header.num_entries, sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	feature_vsec_dev = kzalloc(sizeof(*feature_vsec_dev), GFP_KERNEL);
	if (!feature_vsec_dev) {
		kfree(res);
		return -ENOMEM;
	}

	snprintf(feature_id_name, sizeof(feature_id_name), "tpmi-%s", name);

	for (i = 0, tmp = res; i < pfs->pfs_header.num_entries; i++, tmp++) {
		u64 entry_size_bytes = pfs->pfs_header.entry_size * sizeof(u32);

		tmp->start = pfs->vsec_offset + entry_size_bytes * i;
		tmp->end = tmp->start + entry_size_bytes - 1;
		tmp->flags = IORESOURCE_MEM;
	}

	feature_vsec_dev->pcidev = vsec_dev->pcidev;
	feature_vsec_dev->resource = res;
	feature_vsec_dev->num_resources = pfs->pfs_header.num_entries;
	feature_vsec_dev->priv_data = &tpmi_info->plat_info;
	feature_vsec_dev->priv_data_size = sizeof(tpmi_info->plat_info);
	feature_vsec_dev->ida = &intel_vsec_tpmi_ida;

	 
	return intel_vsec_add_aux(vsec_dev->pcidev, &vsec_dev->auxdev.dev,
				  feature_vsec_dev, feature_id_name);
}

static int tpmi_create_devices(struct intel_tpmi_info *tpmi_info)
{
	struct intel_vsec_device *vsec_dev = tpmi_info->vsec_dev;
	int ret, i;

	for (i = 0; i < vsec_dev->num_resources; i++) {
		ret = tpmi_create_device(tpmi_info, &tpmi_info->tpmi_features[i],
					 tpmi_info->pfs_start);
		 
		if (ret && ret != -EOPNOTSUPP)
			return ret;
	}

	return 0;
}

#define TPMI_INFO_BUS_INFO_OFFSET	0x08

static int tpmi_process_info(struct intel_tpmi_info *tpmi_info,
			     struct intel_tpmi_pm_feature *pfs)
{
	struct tpmi_info_header header;
	void __iomem *info_mem;

	info_mem = ioremap(pfs->vsec_offset + TPMI_INFO_BUS_INFO_OFFSET,
			   pfs->pfs_header.entry_size * sizeof(u32) - TPMI_INFO_BUS_INFO_OFFSET);
	if (!info_mem)
		return -ENOMEM;

	memcpy_fromio(&header, info_mem, sizeof(header));

	tpmi_info->plat_info.package_id = header.pkg;
	tpmi_info->plat_info.bus_number = header.bus;
	tpmi_info->plat_info.device_number = header.dev;
	tpmi_info->plat_info.function_number = header.fn;

	iounmap(info_mem);

	return 0;
}

static int tpmi_fetch_pfs_header(struct intel_tpmi_pm_feature *pfs, u64 start, int size)
{
	void __iomem *pfs_mem;

	pfs_mem = ioremap(start, size);
	if (!pfs_mem)
		return -ENOMEM;

	memcpy_fromio(&pfs->pfs_header, pfs_mem, sizeof(pfs->pfs_header));

	iounmap(pfs_mem);

	return 0;
}

#define TPMI_CAP_OFFSET_UNIT	1024

static int intel_vsec_tpmi_init(struct auxiliary_device *auxdev)
{
	struct intel_vsec_device *vsec_dev = auxdev_to_ivdev(auxdev);
	struct pci_dev *pci_dev = vsec_dev->pcidev;
	struct intel_tpmi_info *tpmi_info;
	u64 pfs_start = 0;
	int ret, i;

	tpmi_info = devm_kzalloc(&auxdev->dev, sizeof(*tpmi_info), GFP_KERNEL);
	if (!tpmi_info)
		return -ENOMEM;

	tpmi_info->vsec_dev = vsec_dev;
	tpmi_info->feature_count = vsec_dev->num_resources;
	tpmi_info->plat_info.bus_number = pci_dev->bus->number;

	tpmi_info->tpmi_features = devm_kcalloc(&auxdev->dev, vsec_dev->num_resources,
						sizeof(*tpmi_info->tpmi_features),
						GFP_KERNEL);
	if (!tpmi_info->tpmi_features)
		return -ENOMEM;

	for (i = 0; i < vsec_dev->num_resources; i++) {
		struct intel_tpmi_pm_feature *pfs;
		struct resource *res;
		u64 res_start;
		int size, ret;

		pfs = &tpmi_info->tpmi_features[i];
		pfs->vsec_dev = vsec_dev;

		res = &vsec_dev->resource[i];
		if (!res)
			continue;

		res_start = res->start;
		size = resource_size(res);
		if (size < 0)
			continue;

		ret = tpmi_fetch_pfs_header(pfs, res_start, size);
		if (ret)
			continue;

		if (!pfs_start)
			pfs_start = res_start;

		pfs->vsec_offset = pfs_start + pfs->pfs_header.cap_offset * TPMI_CAP_OFFSET_UNIT;

		 
		if (pfs->pfs_header.tpmi_id == TPMI_INFO_ID)
			tpmi_process_info(tpmi_info, pfs);

		if (pfs->pfs_header.tpmi_id == TPMI_CONTROL_ID)
			tpmi_set_control_base(auxdev, tpmi_info, pfs);
	}

	tpmi_info->pfs_start = pfs_start;

	auxiliary_set_drvdata(auxdev, tpmi_info);

	ret = tpmi_create_devices(tpmi_info);
	if (ret)
		return ret;

	 
	if (!security_locked_down(LOCKDOWN_DEV_MEM) && capable(CAP_SYS_RAWIO))
		tpmi_dbgfs_register(tpmi_info);

	return 0;
}

static int tpmi_probe(struct auxiliary_device *auxdev,
		      const struct auxiliary_device_id *id)
{
	return intel_vsec_tpmi_init(auxdev);
}

static void tpmi_remove(struct auxiliary_device *auxdev)
{
	struct intel_tpmi_info *tpmi_info = auxiliary_get_drvdata(auxdev);

	debugfs_remove_recursive(tpmi_info->dbgfs_dir);
}

static const struct auxiliary_device_id tpmi_id_table[] = {
	{ .name = "intel_vsec.tpmi" },
	{}
};
MODULE_DEVICE_TABLE(auxiliary, tpmi_id_table);

static struct auxiliary_driver tpmi_aux_driver = {
	.id_table	= tpmi_id_table,
	.probe		= tpmi_probe,
	.remove         = tpmi_remove,
};

module_auxiliary_driver(tpmi_aux_driver);

MODULE_IMPORT_NS(INTEL_VSEC);
MODULE_DESCRIPTION("Intel TPMI enumeration module");
MODULE_LICENSE("GPL");
