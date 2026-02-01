
 
#define pr_fmt(fmt) "mokvar: " fmt

#include <linux/capability.h>
#include <linux/efi.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <asm/early_ioremap.h>

 
static size_t efi_mokvar_table_size;

 
static struct efi_mokvar_table_entry *efi_mokvar_table_va;

 
struct efi_mokvar_sysfs_attr {
	struct bin_attribute bin_attr;
	struct list_head node;
};

static LIST_HEAD(efi_mokvar_sysfs_list);
static struct kobject *mokvar_kobj;

 
void __init efi_mokvar_table_init(void)
{
	efi_memory_desc_t md;
	void *va = NULL;
	unsigned long cur_offset = 0;
	unsigned long offset_limit;
	unsigned long map_size = 0;
	unsigned long map_size_needed = 0;
	unsigned long size;
	struct efi_mokvar_table_entry *mokvar_entry;
	int err;

	if (!efi_enabled(EFI_MEMMAP))
		return;

	if (efi.mokvar_table == EFI_INVALID_TABLE_ADDR)
		return;
	 
	err = efi_mem_desc_lookup(efi.mokvar_table, &md);
	if (err) {
		pr_warn("EFI MOKvar config table is not within the EFI memory map\n");
		return;
	}

	offset_limit = efi_mem_desc_end(&md) - efi.mokvar_table;

	 
	err = -EINVAL;
	while (cur_offset + sizeof(*mokvar_entry) <= offset_limit) {
		mokvar_entry = va + cur_offset;
		map_size_needed = cur_offset + sizeof(*mokvar_entry);
		if (map_size_needed > map_size) {
			if (va)
				early_memunmap(va, map_size);
			 
			map_size = min(map_size_needed + 2*EFI_PAGE_SIZE,
				       offset_limit);
			va = early_memremap(efi.mokvar_table, map_size);
			if (!va) {
				pr_err("Failed to map EFI MOKvar config table pa=0x%lx, size=%lu.\n",
				       efi.mokvar_table, map_size);
				return;
			}
			mokvar_entry = va + cur_offset;
		}

		 
		if (mokvar_entry->name[0] == '\0') {
			if (mokvar_entry->data_size != 0)
				break;
			err = 0;
			break;
		}

		 
		size = strnlen(mokvar_entry->name,
			       sizeof(mokvar_entry->name));
		if (size >= sizeof(mokvar_entry->name))
			break;

		 
		cur_offset = map_size_needed + mokvar_entry->data_size;
	}

	if (va)
		early_memunmap(va, map_size);
	if (err) {
		pr_err("EFI MOKvar config table is not valid\n");
		return;
	}

	if (md.type == EFI_BOOT_SERVICES_DATA)
		efi_mem_reserve(efi.mokvar_table, map_size_needed);

	efi_mokvar_table_size = map_size_needed;
}

 
struct efi_mokvar_table_entry *efi_mokvar_entry_next(
			struct efi_mokvar_table_entry **mokvar_entry)
{
	struct efi_mokvar_table_entry *mokvar_cur;
	struct efi_mokvar_table_entry *mokvar_next;
	size_t size_cur;

	mokvar_cur = *mokvar_entry;
	*mokvar_entry = NULL;

	if (efi_mokvar_table_va == NULL)
		return NULL;

	if (mokvar_cur == NULL) {
		mokvar_next = efi_mokvar_table_va;
	} else {
		if (mokvar_cur->name[0] == '\0')
			return NULL;
		size_cur = sizeof(*mokvar_cur) + mokvar_cur->data_size;
		mokvar_next = (void *)mokvar_cur + size_cur;
	}

	if (mokvar_next->name[0] == '\0')
		return NULL;

	*mokvar_entry = mokvar_next;
	return mokvar_next;
}

 
struct efi_mokvar_table_entry *efi_mokvar_entry_find(const char *name)
{
	struct efi_mokvar_table_entry *mokvar_entry = NULL;

	while (efi_mokvar_entry_next(&mokvar_entry)) {
		if (!strncmp(name, mokvar_entry->name,
			     sizeof(mokvar_entry->name)))
			return mokvar_entry;
	}
	return NULL;
}

 
static ssize_t efi_mokvar_sysfs_read(struct file *file, struct kobject *kobj,
				 struct bin_attribute *bin_attr, char *buf,
				 loff_t off, size_t count)
{
	struct efi_mokvar_table_entry *mokvar_entry = bin_attr->private;

	if (!capable(CAP_SYS_ADMIN))
		return 0;

	if (off >= mokvar_entry->data_size)
		return 0;
	if (count >  mokvar_entry->data_size - off)
		count = mokvar_entry->data_size - off;

	memcpy(buf, mokvar_entry->data + off, count);
	return count;
}

 
static int __init efi_mokvar_sysfs_init(void)
{
	void *config_va;
	struct efi_mokvar_table_entry *mokvar_entry = NULL;
	struct efi_mokvar_sysfs_attr *mokvar_sysfs = NULL;
	int err = 0;

	if (efi_mokvar_table_size == 0)
		return -ENOENT;

	config_va = memremap(efi.mokvar_table, efi_mokvar_table_size,
			     MEMREMAP_WB);
	if (!config_va) {
		pr_err("Failed to map EFI MOKvar config table\n");
		return -ENOMEM;
	}
	efi_mokvar_table_va = config_va;

	mokvar_kobj = kobject_create_and_add("mok-variables", efi_kobj);
	if (!mokvar_kobj) {
		pr_err("Failed to create EFI mok-variables sysfs entry\n");
		return -ENOMEM;
	}

	while (efi_mokvar_entry_next(&mokvar_entry)) {
		mokvar_sysfs = kzalloc(sizeof(*mokvar_sysfs), GFP_KERNEL);
		if (!mokvar_sysfs) {
			err = -ENOMEM;
			break;
		}

		sysfs_bin_attr_init(&mokvar_sysfs->bin_attr);
		mokvar_sysfs->bin_attr.private = mokvar_entry;
		mokvar_sysfs->bin_attr.attr.name = mokvar_entry->name;
		mokvar_sysfs->bin_attr.attr.mode = 0400;
		mokvar_sysfs->bin_attr.size = mokvar_entry->data_size;
		mokvar_sysfs->bin_attr.read = efi_mokvar_sysfs_read;

		err = sysfs_create_bin_file(mokvar_kobj,
					   &mokvar_sysfs->bin_attr);
		if (err)
			break;

		list_add_tail(&mokvar_sysfs->node, &efi_mokvar_sysfs_list);
	}

	if (err) {
		pr_err("Failed to create some EFI mok-variables sysfs entries\n");
		kfree(mokvar_sysfs);
	}
	return err;
}
fs_initcall(efi_mokvar_sysfs_init);
