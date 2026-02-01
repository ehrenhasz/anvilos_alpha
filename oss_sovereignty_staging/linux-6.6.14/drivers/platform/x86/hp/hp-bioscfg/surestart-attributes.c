
 

#include "bioscfg.h"
#include <linux/types.h>

 
#define LOG_MAX_ENTRIES		254

 
#define LOG_ENTRY_SIZE		16

 
static ssize_t audit_log_entry_count_show(struct kobject *kobj,
					  struct kobj_attribute *attr, char *buf)
{
	int ret;
	u32 count = 0;

	ret = hp_wmi_perform_query(HPWMI_SURESTART_GET_LOG_COUNT,
				   HPWMI_SURESTART,
				   &count, 1, sizeof(count));

	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%d,%d,%d\n", count, LOG_ENTRY_SIZE,
			  LOG_MAX_ENTRIES);
}

 
static ssize_t audit_log_entries_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	int ret;
	int i;
	u32 count = 0;
	u8 audit_log_buffer[128];

	
	ret = hp_wmi_perform_query(HPWMI_SURESTART_GET_LOG_COUNT,
				   HPWMI_SURESTART,
				   &count, 1, sizeof(count));

	if (ret < 0)
		return ret;

	 
	if (count * LOG_ENTRY_SIZE > PAGE_SIZE)
		return -EIO;

	 
	for (i = 0; i < count; i++) {
		audit_log_buffer[0] = i + 1;

		 
		ret = hp_wmi_perform_query(HPWMI_SURESTART_GET_LOG,
					   HPWMI_SURESTART,
					   audit_log_buffer, 1, 128);

		if (ret < 0 || (LOG_ENTRY_SIZE * i) > PAGE_SIZE) {
			 
			break;
		} else {
			memcpy(buf, audit_log_buffer, LOG_ENTRY_SIZE);
			buf += LOG_ENTRY_SIZE;
		}
	}

	return i * LOG_ENTRY_SIZE;
}

static struct kobj_attribute sure_start_audit_log_entry_count = __ATTR_RO(audit_log_entry_count);
static struct kobj_attribute sure_start_audit_log_entries = __ATTR_RO(audit_log_entries);

static struct attribute *sure_start_attrs[] = {
	&sure_start_audit_log_entry_count.attr,
	&sure_start_audit_log_entries.attr,
	NULL
};

static const struct attribute_group sure_start_attr_group = {
	.attrs = sure_start_attrs,
};

void hp_exit_sure_start_attributes(void)
{
	sysfs_remove_group(bioscfg_drv.sure_start_attr_kobj,
			   &sure_start_attr_group);
}

int hp_populate_sure_start_data(struct kobject *attr_name_kobj)
{
	bioscfg_drv.sure_start_attr_kobj = attr_name_kobj;
	return sysfs_create_group(attr_name_kobj, &sure_start_attr_group);
}
