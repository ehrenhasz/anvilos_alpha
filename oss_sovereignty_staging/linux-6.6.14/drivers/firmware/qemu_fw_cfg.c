 

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <uapi/linux/qemu_fw_cfg.h>
#include <linux/delay.h>
#include <linux/crash_dump.h>
#include <linux/crash_core.h>

MODULE_AUTHOR("Gabriel L. Somlo <somlo@cmu.edu>");
MODULE_DESCRIPTION("QEMU fw_cfg sysfs support");
MODULE_LICENSE("GPL");

 
static u32 fw_cfg_rev;

 
static bool fw_cfg_is_mmio;
static phys_addr_t fw_cfg_p_base;
static resource_size_t fw_cfg_p_size;
static void __iomem *fw_cfg_dev_base;
static void __iomem *fw_cfg_reg_ctrl;
static void __iomem *fw_cfg_reg_data;
static void __iomem *fw_cfg_reg_dma;

 
static DEFINE_MUTEX(fw_cfg_dev_lock);

 
static void fw_cfg_sel_endianness(u16 key)
{
	if (fw_cfg_is_mmio)
		iowrite16be(key, fw_cfg_reg_ctrl);
	else
		iowrite16(key, fw_cfg_reg_ctrl);
}

#ifdef CONFIG_CRASH_CORE
static inline bool fw_cfg_dma_enabled(void)
{
	return (fw_cfg_rev & FW_CFG_VERSION_DMA) && fw_cfg_reg_dma;
}

 
static void fw_cfg_wait_for_control(struct fw_cfg_dma_access *d)
{
	for (;;) {
		u32 ctrl = be32_to_cpu(READ_ONCE(d->control));

		 
		rmb();
		if ((ctrl & ~FW_CFG_DMA_CTL_ERROR) == 0)
			return;

		cpu_relax();
	}
}

static ssize_t fw_cfg_dma_transfer(void *address, u32 length, u32 control)
{
	phys_addr_t dma;
	struct fw_cfg_dma_access *d = NULL;
	ssize_t ret = length;

	d = kmalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto end;
	}

	 
	*d = (struct fw_cfg_dma_access) {
		.address = cpu_to_be64(address ? virt_to_phys(address) : 0),
		.length = cpu_to_be32(length),
		.control = cpu_to_be32(control)
	};

	dma = virt_to_phys(d);

	iowrite32be((u64)dma >> 32, fw_cfg_reg_dma);
	 
	wmb();
	iowrite32be(dma, fw_cfg_reg_dma + 4);

	fw_cfg_wait_for_control(d);

	if (be32_to_cpu(READ_ONCE(d->control)) & FW_CFG_DMA_CTL_ERROR) {
		ret = -EIO;
	}

end:
	kfree(d);

	return ret;
}
#endif

 
static ssize_t fw_cfg_read_blob(u16 key,
				void *buf, loff_t pos, size_t count)
{
	u32 glk = -1U;
	acpi_status status;

	 
	status = acpi_acquire_global_lock(ACPI_WAIT_FOREVER, &glk);
	if (ACPI_FAILURE(status) && status != AE_NOT_CONFIGURED) {
		 
		WARN(1, "fw_cfg_read_blob: Failed to lock ACPI!\n");
		memset(buf, 0, count);
		return -EINVAL;
	}

	mutex_lock(&fw_cfg_dev_lock);
	fw_cfg_sel_endianness(key);
	while (pos-- > 0)
		ioread8(fw_cfg_reg_data);
	ioread8_rep(fw_cfg_reg_data, buf, count);
	mutex_unlock(&fw_cfg_dev_lock);

	acpi_release_global_lock(glk);
	return count;
}

#ifdef CONFIG_CRASH_CORE
 
static ssize_t fw_cfg_write_blob(u16 key,
				 void *buf, loff_t pos, size_t count)
{
	u32 glk = -1U;
	acpi_status status;
	ssize_t ret = count;

	 
	status = acpi_acquire_global_lock(ACPI_WAIT_FOREVER, &glk);
	if (ACPI_FAILURE(status) && status != AE_NOT_CONFIGURED) {
		 
		WARN(1, "%s: Failed to lock ACPI!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&fw_cfg_dev_lock);
	if (pos == 0) {
		ret = fw_cfg_dma_transfer(buf, count, key << 16
					  | FW_CFG_DMA_CTL_SELECT
					  | FW_CFG_DMA_CTL_WRITE);
	} else {
		fw_cfg_sel_endianness(key);
		ret = fw_cfg_dma_transfer(NULL, pos, FW_CFG_DMA_CTL_SKIP);
		if (ret < 0)
			goto end;
		ret = fw_cfg_dma_transfer(buf, count, FW_CFG_DMA_CTL_WRITE);
	}

end:
	mutex_unlock(&fw_cfg_dev_lock);

	acpi_release_global_lock(glk);

	return ret;
}
#endif  

 
static void fw_cfg_io_cleanup(void)
{
	if (fw_cfg_is_mmio) {
		iounmap(fw_cfg_dev_base);
		release_mem_region(fw_cfg_p_base, fw_cfg_p_size);
	} else {
		ioport_unmap(fw_cfg_dev_base);
		release_region(fw_cfg_p_base, fw_cfg_p_size);
	}
}

 
#if !(defined(FW_CFG_CTRL_OFF) && defined(FW_CFG_DATA_OFF))
# if (defined(CONFIG_ARM) || defined(CONFIG_ARM64))
#  define FW_CFG_CTRL_OFF 0x08
#  define FW_CFG_DATA_OFF 0x00
#  define FW_CFG_DMA_OFF 0x10
# elif defined(CONFIG_PARISC)	 
#  define FW_CFG_CTRL_OFF 0x00
#  define FW_CFG_DATA_OFF 0x04
# elif (defined(CONFIG_PPC_PMAC) || defined(CONFIG_SPARC32))  
#  define FW_CFG_CTRL_OFF 0x00
#  define FW_CFG_DATA_OFF 0x02
# elif (defined(CONFIG_X86) || defined(CONFIG_SPARC64))  
#  define FW_CFG_CTRL_OFF 0x00
#  define FW_CFG_DATA_OFF 0x01
#  define FW_CFG_DMA_OFF 0x04
# else
#  error "QEMU FW_CFG not available on this architecture!"
# endif
#endif

 
static int fw_cfg_do_platform_probe(struct platform_device *pdev)
{
	char sig[FW_CFG_SIG_SIZE];
	struct resource *range, *ctrl, *data, *dma;

	 
	fw_cfg_is_mmio = false;
	range = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!range) {
		fw_cfg_is_mmio = true;
		range = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!range)
			return -EINVAL;
	}
	fw_cfg_p_base = range->start;
	fw_cfg_p_size = resource_size(range);

	if (fw_cfg_is_mmio) {
		if (!request_mem_region(fw_cfg_p_base,
					fw_cfg_p_size, "fw_cfg_mem"))
			return -EBUSY;
		fw_cfg_dev_base = ioremap(fw_cfg_p_base, fw_cfg_p_size);
		if (!fw_cfg_dev_base) {
			release_mem_region(fw_cfg_p_base, fw_cfg_p_size);
			return -EFAULT;
		}
	} else {
		if (!request_region(fw_cfg_p_base,
				    fw_cfg_p_size, "fw_cfg_io"))
			return -EBUSY;
		fw_cfg_dev_base = ioport_map(fw_cfg_p_base, fw_cfg_p_size);
		if (!fw_cfg_dev_base) {
			release_region(fw_cfg_p_base, fw_cfg_p_size);
			return -EFAULT;
		}
	}

	 
	ctrl = platform_get_resource_byname(pdev, IORESOURCE_REG, "ctrl");
	data = platform_get_resource_byname(pdev, IORESOURCE_REG, "data");
	dma = platform_get_resource_byname(pdev, IORESOURCE_REG, "dma");
	if (ctrl && data) {
		fw_cfg_reg_ctrl = fw_cfg_dev_base + ctrl->start;
		fw_cfg_reg_data = fw_cfg_dev_base + data->start;
	} else {
		 
		fw_cfg_reg_ctrl = fw_cfg_dev_base + FW_CFG_CTRL_OFF;
		fw_cfg_reg_data = fw_cfg_dev_base + FW_CFG_DATA_OFF;
	}

	if (dma)
		fw_cfg_reg_dma = fw_cfg_dev_base + dma->start;
#ifdef FW_CFG_DMA_OFF
	else
		fw_cfg_reg_dma = fw_cfg_dev_base + FW_CFG_DMA_OFF;
#endif

	 
	if (fw_cfg_read_blob(FW_CFG_SIGNATURE, sig,
				0, FW_CFG_SIG_SIZE) < 0 ||
		memcmp(sig, "QEMU", FW_CFG_SIG_SIZE) != 0) {
		fw_cfg_io_cleanup();
		return -ENODEV;
	}

	return 0;
}

static ssize_t fw_cfg_showrev(struct kobject *k, struct kobj_attribute *a,
			      char *buf)
{
	return sprintf(buf, "%u\n", fw_cfg_rev);
}

static const struct kobj_attribute fw_cfg_rev_attr = {
	.attr = { .name = "rev", .mode = S_IRUSR },
	.show = fw_cfg_showrev,
};

 
struct fw_cfg_sysfs_entry {
	struct kobject kobj;
	u32 size;
	u16 select;
	char name[FW_CFG_MAX_FILE_PATH];
	struct list_head list;
};

#ifdef CONFIG_CRASH_CORE
static ssize_t fw_cfg_write_vmcoreinfo(const struct fw_cfg_file *f)
{
	static struct fw_cfg_vmcoreinfo *data;
	ssize_t ret;

	data = kmalloc(sizeof(struct fw_cfg_vmcoreinfo), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	*data = (struct fw_cfg_vmcoreinfo) {
		.guest_format = cpu_to_le16(FW_CFG_VMCOREINFO_FORMAT_ELF),
		.size = cpu_to_le32(VMCOREINFO_NOTE_SIZE),
		.paddr = cpu_to_le64(paddr_vmcoreinfo_note())
	};
	 
	ret = fw_cfg_write_blob(be16_to_cpu(f->select), data,
				0, sizeof(struct fw_cfg_vmcoreinfo));

	kfree(data);
	return ret;
}
#endif  

 
static inline struct fw_cfg_sysfs_entry *to_entry(struct kobject *kobj)
{
	return container_of(kobj, struct fw_cfg_sysfs_entry, kobj);
}

 
struct fw_cfg_sysfs_attribute {
	struct attribute attr;
	ssize_t (*show)(struct fw_cfg_sysfs_entry *entry, char *buf);
};

 
static inline struct fw_cfg_sysfs_attribute *to_attr(struct attribute *attr)
{
	return container_of(attr, struct fw_cfg_sysfs_attribute, attr);
}

 
static LIST_HEAD(fw_cfg_entry_cache);

 
static DEFINE_SPINLOCK(fw_cfg_cache_lock);

static inline void fw_cfg_sysfs_cache_enlist(struct fw_cfg_sysfs_entry *entry)
{
	spin_lock(&fw_cfg_cache_lock);
	list_add_tail(&entry->list, &fw_cfg_entry_cache);
	spin_unlock(&fw_cfg_cache_lock);
}

static inline void fw_cfg_sysfs_cache_delist(struct fw_cfg_sysfs_entry *entry)
{
	spin_lock(&fw_cfg_cache_lock);
	list_del(&entry->list);
	spin_unlock(&fw_cfg_cache_lock);
}

static void fw_cfg_sysfs_cache_cleanup(void)
{
	struct fw_cfg_sysfs_entry *entry, *next;

	list_for_each_entry_safe(entry, next, &fw_cfg_entry_cache, list) {
		fw_cfg_sysfs_cache_delist(entry);
		kobject_del(&entry->kobj);
		kobject_put(&entry->kobj);
	}
}

 

#define FW_CFG_SYSFS_ATTR(_attr) \
struct fw_cfg_sysfs_attribute fw_cfg_sysfs_attr_##_attr = { \
	.attr = { .name = __stringify(_attr), .mode = S_IRUSR }, \
	.show = fw_cfg_sysfs_show_##_attr, \
}

static ssize_t fw_cfg_sysfs_show_size(struct fw_cfg_sysfs_entry *e, char *buf)
{
	return sprintf(buf, "%u\n", e->size);
}

static ssize_t fw_cfg_sysfs_show_key(struct fw_cfg_sysfs_entry *e, char *buf)
{
	return sprintf(buf, "%u\n", e->select);
}

static ssize_t fw_cfg_sysfs_show_name(struct fw_cfg_sysfs_entry *e, char *buf)
{
	return sprintf(buf, "%s\n", e->name);
}

static FW_CFG_SYSFS_ATTR(size);
static FW_CFG_SYSFS_ATTR(key);
static FW_CFG_SYSFS_ATTR(name);

static struct attribute *fw_cfg_sysfs_entry_attrs[] = {
	&fw_cfg_sysfs_attr_size.attr,
	&fw_cfg_sysfs_attr_key.attr,
	&fw_cfg_sysfs_attr_name.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fw_cfg_sysfs_entry);

 
static ssize_t fw_cfg_sysfs_attr_show(struct kobject *kobj, struct attribute *a,
				      char *buf)
{
	struct fw_cfg_sysfs_entry *entry = to_entry(kobj);
	struct fw_cfg_sysfs_attribute *attr = to_attr(a);

	return attr->show(entry, buf);
}

static const struct sysfs_ops fw_cfg_sysfs_attr_ops = {
	.show = fw_cfg_sysfs_attr_show,
};

 
static void fw_cfg_sysfs_release_entry(struct kobject *kobj)
{
	struct fw_cfg_sysfs_entry *entry = to_entry(kobj);

	kfree(entry);
}

 
static struct kobj_type fw_cfg_sysfs_entry_ktype = {
	.default_groups = fw_cfg_sysfs_entry_groups,
	.sysfs_ops = &fw_cfg_sysfs_attr_ops,
	.release = fw_cfg_sysfs_release_entry,
};

 
static ssize_t fw_cfg_sysfs_read_raw(struct file *filp, struct kobject *kobj,
				     struct bin_attribute *bin_attr,
				     char *buf, loff_t pos, size_t count)
{
	struct fw_cfg_sysfs_entry *entry = to_entry(kobj);

	if (pos > entry->size)
		return -EINVAL;

	if (count > entry->size - pos)
		count = entry->size - pos;

	return fw_cfg_read_blob(entry->select, buf, pos, count);
}

static struct bin_attribute fw_cfg_sysfs_attr_raw = {
	.attr = { .name = "raw", .mode = S_IRUSR },
	.read = fw_cfg_sysfs_read_raw,
};

 
static int fw_cfg_build_symlink(struct kset *dir,
				struct kobject *target, const char *name)
{
	int ret;
	struct kset *subdir;
	struct kobject *ko;
	char *name_copy, *p, *tok;

	if (!dir || !target || !name || !*name)
		return -EINVAL;

	 
	name_copy = p = kstrdup(name, GFP_KERNEL);
	if (!name_copy)
		return -ENOMEM;

	 
	while ((tok = strsep(&p, "/")) && *tok) {

		 
		if (!p || !*p) {
			ret = sysfs_create_link(&dir->kobj, target, tok);
			break;
		}

		 
		ko = kset_find_obj(dir, tok);
		if (ko) {
			 
			kobject_put(ko);

			 
			if (ko->ktype != dir->kobj.ktype) {
				ret = -EINVAL;
				break;
			}

			 
			dir = to_kset(ko);
		} else {
			 
			subdir = kzalloc(sizeof(struct kset), GFP_KERNEL);
			if (!subdir) {
				ret = -ENOMEM;
				break;
			}
			subdir->kobj.kset = dir;
			subdir->kobj.ktype = dir->kobj.ktype;
			ret = kobject_set_name(&subdir->kobj, "%s", tok);
			if (ret) {
				kfree(subdir);
				break;
			}
			ret = kset_register(subdir);
			if (ret) {
				kfree(subdir);
				break;
			}

			 
			dir = subdir;
		}
	}

	 
	kfree(name_copy);
	return ret;
}

 
static void fw_cfg_kset_unregister_recursive(struct kset *kset)
{
	struct kobject *k, *next;

	list_for_each_entry_safe(k, next, &kset->list, entry)
		 
		if (k->ktype == kset->kobj.ktype)
			fw_cfg_kset_unregister_recursive(to_kset(k));

	 
	kset_unregister(kset);
}

 
static struct kobject *fw_cfg_top_ko;
static struct kobject *fw_cfg_sel_ko;
static struct kset *fw_cfg_fname_kset;

 
static int fw_cfg_register_file(const struct fw_cfg_file *f)
{
	int err;
	struct fw_cfg_sysfs_entry *entry;

#ifdef CONFIG_CRASH_CORE
	if (fw_cfg_dma_enabled() &&
		strcmp(f->name, FW_CFG_VMCOREINFO_FILENAME) == 0 &&
		!is_kdump_kernel()) {
		if (fw_cfg_write_vmcoreinfo(f) < 0)
			pr_warn("fw_cfg: failed to write vmcoreinfo");
	}
#endif

	 
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	 
	entry->size = be32_to_cpu(f->size);
	entry->select = be16_to_cpu(f->select);
	strscpy(entry->name, f->name, FW_CFG_MAX_FILE_PATH);

	 
	err = kobject_init_and_add(&entry->kobj, &fw_cfg_sysfs_entry_ktype,
				   fw_cfg_sel_ko, "%d", entry->select);
	if (err)
		goto err_put_entry;

	 
	err = sysfs_create_bin_file(&entry->kobj, &fw_cfg_sysfs_attr_raw);
	if (err)
		goto err_del_entry;

	 
	fw_cfg_build_symlink(fw_cfg_fname_kset, &entry->kobj, entry->name);

	 
	fw_cfg_sysfs_cache_enlist(entry);
	return 0;

err_del_entry:
	kobject_del(&entry->kobj);
err_put_entry:
	kobject_put(&entry->kobj);
	return err;
}

 
static int fw_cfg_register_dir_entries(void)
{
	int ret = 0;
	__be32 files_count;
	u32 count, i;
	struct fw_cfg_file *dir;
	size_t dir_size;

	ret = fw_cfg_read_blob(FW_CFG_FILE_DIR, &files_count,
			0, sizeof(files_count));
	if (ret < 0)
		return ret;

	count = be32_to_cpu(files_count);
	dir_size = count * sizeof(struct fw_cfg_file);

	dir = kmalloc(dir_size, GFP_KERNEL);
	if (!dir)
		return -ENOMEM;

	ret = fw_cfg_read_blob(FW_CFG_FILE_DIR, dir,
			sizeof(files_count), dir_size);
	if (ret < 0)
		goto end;

	for (i = 0; i < count; i++) {
		ret = fw_cfg_register_file(&dir[i]);
		if (ret)
			break;
	}

end:
	kfree(dir);
	return ret;
}

 
static inline void fw_cfg_kobj_cleanup(struct kobject *kobj)
{
	kobject_del(kobj);
	kobject_put(kobj);
}

static int fw_cfg_sysfs_probe(struct platform_device *pdev)
{
	int err;
	__le32 rev;

	 
	if (fw_cfg_sel_ko)
		return -EBUSY;

	 
	err = -ENOMEM;
	fw_cfg_sel_ko = kobject_create_and_add("by_key", fw_cfg_top_ko);
	if (!fw_cfg_sel_ko)
		goto err_sel;
	fw_cfg_fname_kset = kset_create_and_add("by_name", NULL, fw_cfg_top_ko);
	if (!fw_cfg_fname_kset)
		goto err_name;

	 
	err = fw_cfg_do_platform_probe(pdev);
	if (err)
		goto err_probe;

	 
	err = fw_cfg_read_blob(FW_CFG_ID, &rev, 0, sizeof(rev));
	if (err < 0)
		goto err_probe;

	fw_cfg_rev = le32_to_cpu(rev);
	err = sysfs_create_file(fw_cfg_top_ko, &fw_cfg_rev_attr.attr);
	if (err)
		goto err_rev;

	 
	err = fw_cfg_register_dir_entries();
	if (err)
		goto err_dir;

	 
	pr_debug("fw_cfg: loaded.\n");
	return 0;

err_dir:
	fw_cfg_sysfs_cache_cleanup();
	sysfs_remove_file(fw_cfg_top_ko, &fw_cfg_rev_attr.attr);
err_rev:
	fw_cfg_io_cleanup();
err_probe:
	fw_cfg_kset_unregister_recursive(fw_cfg_fname_kset);
err_name:
	fw_cfg_kobj_cleanup(fw_cfg_sel_ko);
err_sel:
	return err;
}

static int fw_cfg_sysfs_remove(struct platform_device *pdev)
{
	pr_debug("fw_cfg: unloading.\n");
	fw_cfg_sysfs_cache_cleanup();
	sysfs_remove_file(fw_cfg_top_ko, &fw_cfg_rev_attr.attr);
	fw_cfg_io_cleanup();
	fw_cfg_kset_unregister_recursive(fw_cfg_fname_kset);
	fw_cfg_kobj_cleanup(fw_cfg_sel_ko);
	return 0;
}

static const struct of_device_id fw_cfg_sysfs_mmio_match[] = {
	{ .compatible = "qemu,fw-cfg-mmio", },
	{},
};
MODULE_DEVICE_TABLE(of, fw_cfg_sysfs_mmio_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id fw_cfg_sysfs_acpi_match[] = {
	{ FW_CFG_ACPI_DEVICE_ID, },
	{},
};
MODULE_DEVICE_TABLE(acpi, fw_cfg_sysfs_acpi_match);
#endif

static struct platform_driver fw_cfg_sysfs_driver = {
	.probe = fw_cfg_sysfs_probe,
	.remove = fw_cfg_sysfs_remove,
	.driver = {
		.name = "fw_cfg",
		.of_match_table = fw_cfg_sysfs_mmio_match,
		.acpi_match_table = ACPI_PTR(fw_cfg_sysfs_acpi_match),
	},
};

#ifdef CONFIG_FW_CFG_SYSFS_CMDLINE

static struct platform_device *fw_cfg_cmdline_dev;

 
#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define __PHYS_ADDR_PREFIX "ll"
#else
#define __PHYS_ADDR_PREFIX ""
#endif

 
#define PH_ADDR_SCAN_FMT "@%" __PHYS_ADDR_PREFIX "i%n" \
			 ":%" __PHYS_ADDR_PREFIX "i" \
			 ":%" __PHYS_ADDR_PREFIX "i%n" \
			 ":%" __PHYS_ADDR_PREFIX "i%n"

#define PH_ADDR_PR_1_FMT "0x%" __PHYS_ADDR_PREFIX "x@" \
			 "0x%" __PHYS_ADDR_PREFIX "x"

#define PH_ADDR_PR_3_FMT PH_ADDR_PR_1_FMT \
			 ":%" __PHYS_ADDR_PREFIX "u" \
			 ":%" __PHYS_ADDR_PREFIX "u"

#define PH_ADDR_PR_4_FMT PH_ADDR_PR_3_FMT \
			 ":%" __PHYS_ADDR_PREFIX "u"

static int fw_cfg_cmdline_set(const char *arg, const struct kernel_param *kp)
{
	struct resource res[4] = {};
	char *str;
	phys_addr_t base;
	resource_size_t size, ctrl_off, data_off, dma_off;
	int processed, consumed = 0;

	 
	if (fw_cfg_cmdline_dev) {
		 
		platform_device_unregister(fw_cfg_cmdline_dev);
		return -EINVAL;
	}

	 
	size = memparse(arg, &str);

	 
	processed = sscanf(str, PH_ADDR_SCAN_FMT,
			   &base, &consumed,
			   &ctrl_off, &data_off, &consumed,
			   &dma_off, &consumed);

	 
	if (str[consumed] ||
	    (processed != 1 && processed != 3 && processed != 4))
		return -EINVAL;

	res[0].start = base;
	res[0].end = base + size - 1;
	res[0].flags = !strcmp(kp->name, "mmio") ? IORESOURCE_MEM :
						   IORESOURCE_IO;

	 
	if (processed > 1) {
		res[1].name = "ctrl";
		res[1].start = ctrl_off;
		res[1].flags = IORESOURCE_REG;
		res[2].name = "data";
		res[2].start = data_off;
		res[2].flags = IORESOURCE_REG;
	}
	if (processed > 3) {
		res[3].name = "dma";
		res[3].start = dma_off;
		res[3].flags = IORESOURCE_REG;
	}

	 
	fw_cfg_cmdline_dev = platform_device_register_simple("fw_cfg",
					PLATFORM_DEVID_NONE, res, processed);

	return PTR_ERR_OR_ZERO(fw_cfg_cmdline_dev);
}

static int fw_cfg_cmdline_get(char *buf, const struct kernel_param *kp)
{
	 
	if (!fw_cfg_cmdline_dev ||
	    (!strcmp(kp->name, "mmio") ^
	     (fw_cfg_cmdline_dev->resource[0].flags == IORESOURCE_MEM)))
		return 0;

	switch (fw_cfg_cmdline_dev->num_resources) {
	case 1:
		return snprintf(buf, PAGE_SIZE, PH_ADDR_PR_1_FMT,
				resource_size(&fw_cfg_cmdline_dev->resource[0]),
				fw_cfg_cmdline_dev->resource[0].start);
	case 3:
		return snprintf(buf, PAGE_SIZE, PH_ADDR_PR_3_FMT,
				resource_size(&fw_cfg_cmdline_dev->resource[0]),
				fw_cfg_cmdline_dev->resource[0].start,
				fw_cfg_cmdline_dev->resource[1].start,
				fw_cfg_cmdline_dev->resource[2].start);
	case 4:
		return snprintf(buf, PAGE_SIZE, PH_ADDR_PR_4_FMT,
				resource_size(&fw_cfg_cmdline_dev->resource[0]),
				fw_cfg_cmdline_dev->resource[0].start,
				fw_cfg_cmdline_dev->resource[1].start,
				fw_cfg_cmdline_dev->resource[2].start,
				fw_cfg_cmdline_dev->resource[3].start);
	}

	 
	WARN(1, "Unexpected number of resources: %d\n",
		fw_cfg_cmdline_dev->num_resources);
	return 0;
}

static const struct kernel_param_ops fw_cfg_cmdline_param_ops = {
	.set = fw_cfg_cmdline_set,
	.get = fw_cfg_cmdline_get,
};

device_param_cb(ioport, &fw_cfg_cmdline_param_ops, NULL, S_IRUSR);
device_param_cb(mmio, &fw_cfg_cmdline_param_ops, NULL, S_IRUSR);

#endif  

static int __init fw_cfg_sysfs_init(void)
{
	int ret;

	 
	fw_cfg_top_ko = kobject_create_and_add("qemu_fw_cfg", firmware_kobj);
	if (!fw_cfg_top_ko)
		return -ENOMEM;

	ret = platform_driver_register(&fw_cfg_sysfs_driver);
	if (ret)
		fw_cfg_kobj_cleanup(fw_cfg_top_ko);

	return ret;
}

static void __exit fw_cfg_sysfs_exit(void)
{
	platform_driver_unregister(&fw_cfg_sysfs_driver);

#ifdef CONFIG_FW_CFG_SYSFS_CMDLINE
	platform_device_unregister(fw_cfg_cmdline_dev);
#endif

	 
	fw_cfg_kobj_cleanup(fw_cfg_top_ko);
}

module_init(fw_cfg_sysfs_init);
module_exit(fw_cfg_sysfs_exit);
