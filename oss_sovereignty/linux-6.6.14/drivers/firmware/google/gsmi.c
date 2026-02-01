
 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/panic_notifier.h>
#include <linux/ioctl.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/dmi.h>
#include <linux/kdebug.h>
#include <linux/reboot.h>
#include <linux/efi.h>
#include <linux/module.h>
#include <linux/ucs2_string.h>
#include <linux/suspend.h>

#define GSMI_SHUTDOWN_CLEAN	0	 
 
#define GSMI_SHUTDOWN_NMIWDT	1	 
#define GSMI_SHUTDOWN_PANIC	2	 
#define GSMI_SHUTDOWN_OOPS	3	 
#define GSMI_SHUTDOWN_DIE	4	 
#define GSMI_SHUTDOWN_MCE	5	 
#define GSMI_SHUTDOWN_SOFTWDT	6	 
#define GSMI_SHUTDOWN_MBE	7	 
#define GSMI_SHUTDOWN_TRIPLE	8	 

#define DRIVER_VERSION		"1.0"
#define GSMI_GUID_SIZE		16
#define GSMI_BUF_SIZE		1024
#define GSMI_BUF_ALIGN		sizeof(u64)
#define GSMI_CALLBACK		0xef

 
#define GSMI_SUCCESS		0x00
#define GSMI_UNSUPPORTED2	0x03
#define GSMI_LOG_FULL		0x0b
#define GSMI_VAR_NOT_FOUND	0x0e
#define GSMI_HANDSHAKE_SPIN	0x7d
#define GSMI_HANDSHAKE_CF	0x7e
#define GSMI_HANDSHAKE_NONE	0x7f
#define GSMI_INVALID_PARAMETER	0x82
#define GSMI_UNSUPPORTED	0x83
#define GSMI_BUFFER_TOO_SMALL	0x85
#define GSMI_NOT_READY		0x86
#define GSMI_DEVICE_ERROR	0x87
#define GSMI_NOT_FOUND		0x8e

#define QUIRKY_BOARD_HASH 0x78a30a50

 
#define GSMI_CMD_GET_NVRAM_VAR		0x01
#define GSMI_CMD_GET_NEXT_VAR		0x02
#define GSMI_CMD_SET_NVRAM_VAR		0x03
#define GSMI_CMD_SET_EVENT_LOG		0x08
#define GSMI_CMD_CLEAR_EVENT_LOG	0x09
#define GSMI_CMD_LOG_S0IX_SUSPEND	0x0a
#define GSMI_CMD_LOG_S0IX_RESUME	0x0b
#define GSMI_CMD_CLEAR_CONFIG		0x20
#define GSMI_CMD_HANDSHAKE_TYPE		0xC1
#define GSMI_CMD_RESERVED		0xff

 
#define GSMI_LOG_ENTRY_TYPE_KERNEL     0xDEAD

 
struct gsmi_buf {
	u8 *start;			 
	size_t length;			 
	u32 address;			 
};

static struct gsmi_device {
	struct platform_device *pdev;	 
	struct gsmi_buf *name_buf;	 
	struct gsmi_buf *data_buf;	 
	struct gsmi_buf *param_buf;	 
	spinlock_t lock;		 
	u16 smi_cmd;			 
	int handshake_type;		 
	struct kmem_cache *mem_pool;	 
} gsmi_dev;

 
struct gsmi_nvram_var_param {
	efi_guid_t	guid;
	u32		name_ptr;
	u32		attributes;
	u32		data_len;
	u32		data_ptr;
} __packed;

struct gsmi_get_next_var_param {
	u8	guid[GSMI_GUID_SIZE];
	u32	name_ptr;
	u32	name_len;
} __packed;

struct gsmi_set_eventlog_param {
	u32	data_ptr;
	u32	data_len;
	u32	type;
} __packed;

 
struct gsmi_log_entry_type_1 {
	u16	type;
	u32	instance;
} __packed;

 
#define GSMI_DEFAULT_SPINCOUNT	0x10000
static unsigned int spincount = GSMI_DEFAULT_SPINCOUNT;
module_param(spincount, uint, 0600);
MODULE_PARM_DESC(spincount,
	"The number of loop iterations to use when using the spin handshake.");

 
static bool s0ix_logging_enable = true;
module_param(s0ix_logging_enable, bool, 0600);

static struct gsmi_buf *gsmi_buf_alloc(void)
{
	struct gsmi_buf *smibuf;

	smibuf = kzalloc(sizeof(*smibuf), GFP_KERNEL);
	if (!smibuf) {
		printk(KERN_ERR "gsmi: out of memory\n");
		return NULL;
	}

	 
	smibuf->start = kmem_cache_alloc(gsmi_dev.mem_pool, GFP_KERNEL);
	if (!smibuf->start) {
		printk(KERN_ERR "gsmi: failed to allocate name buffer\n");
		kfree(smibuf);
		return NULL;
	}

	 
	smibuf->length = GSMI_BUF_SIZE;
	smibuf->address = (u32)virt_to_phys(smibuf->start);

	return smibuf;
}

static void gsmi_buf_free(struct gsmi_buf *smibuf)
{
	if (smibuf) {
		if (smibuf->start)
			kmem_cache_free(gsmi_dev.mem_pool, smibuf->start);
		kfree(smibuf);
	}
}

 
static int gsmi_exec(u8 func, u8 sub)
{
	u16 cmd = (sub << 8) | func;
	u16 result = 0;
	int rc = 0;

	 
	if (gsmi_dev.handshake_type == GSMI_HANDSHAKE_CF) {
		 
		asm volatile (
			"stc\n"
			"outb %%al, %%dx\n"
		"1:      jc 1b\n"
			: "=a" (result)
			: "0" (cmd),
			  "d" (gsmi_dev.smi_cmd),
			  "b" (gsmi_dev.param_buf->address)
			: "memory", "cc"
		);
	} else if (gsmi_dev.handshake_type == GSMI_HANDSHAKE_SPIN) {
		 
		asm volatile (
			"outb %%al, %%dx\n"
		"1:      loop 1b\n"
			: "=a" (result)
			: "0" (cmd),
			  "d" (gsmi_dev.smi_cmd),
			  "b" (gsmi_dev.param_buf->address),
			  "c" (spincount)
			: "memory", "cc"
		);
	} else {
		 
		asm volatile (
			"outb %%al, %%dx\n\t"
			: "=a" (result)
			: "0" (cmd),
			  "d" (gsmi_dev.smi_cmd),
			  "b" (gsmi_dev.param_buf->address)
			: "memory", "cc"
		);
	}

	 
	switch (result) {
	case GSMI_SUCCESS:
		break;
	case GSMI_VAR_NOT_FOUND:
		 
		rc = 1;
		break;
	case GSMI_INVALID_PARAMETER:
		printk(KERN_ERR "gsmi: exec 0x%04x: Invalid parameter\n", cmd);
		rc = -EINVAL;
		break;
	case GSMI_BUFFER_TOO_SMALL:
		printk(KERN_ERR "gsmi: exec 0x%04x: Buffer too small\n", cmd);
		rc = -ENOMEM;
		break;
	case GSMI_UNSUPPORTED:
	case GSMI_UNSUPPORTED2:
		if (sub != GSMI_CMD_HANDSHAKE_TYPE)
			printk(KERN_ERR "gsmi: exec 0x%04x: Not supported\n",
			       cmd);
		rc = -ENOSYS;
		break;
	case GSMI_NOT_READY:
		printk(KERN_ERR "gsmi: exec 0x%04x: Not ready\n", cmd);
		rc = -EBUSY;
		break;
	case GSMI_DEVICE_ERROR:
		printk(KERN_ERR "gsmi: exec 0x%04x: Device error\n", cmd);
		rc = -EFAULT;
		break;
	case GSMI_NOT_FOUND:
		printk(KERN_ERR "gsmi: exec 0x%04x: Data not found\n", cmd);
		rc = -ENOENT;
		break;
	case GSMI_LOG_FULL:
		printk(KERN_ERR "gsmi: exec 0x%04x: Log full\n", cmd);
		rc = -ENOSPC;
		break;
	case GSMI_HANDSHAKE_CF:
	case GSMI_HANDSHAKE_SPIN:
	case GSMI_HANDSHAKE_NONE:
		rc = result;
		break;
	default:
		printk(KERN_ERR "gsmi: exec 0x%04x: Unknown error 0x%04x\n",
		       cmd, result);
		rc = -ENXIO;
	}

	return rc;
}

#ifdef CONFIG_EFI

static struct efivars efivars;

static efi_status_t gsmi_get_variable(efi_char16_t *name,
				      efi_guid_t *vendor, u32 *attr,
				      unsigned long *data_size,
				      void *data)
{
	struct gsmi_nvram_var_param param = {
		.name_ptr = gsmi_dev.name_buf->address,
		.data_ptr = gsmi_dev.data_buf->address,
		.data_len = (u32)*data_size,
	};
	efi_status_t ret = EFI_SUCCESS;
	unsigned long flags;
	size_t name_len = ucs2_strnlen(name, GSMI_BUF_SIZE / 2);
	int rc;

	if (name_len >= GSMI_BUF_SIZE / 2)
		return EFI_BAD_BUFFER_SIZE;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memcpy(&param.guid, vendor, sizeof(param.guid));

	 
	memset(gsmi_dev.name_buf->start, 0, gsmi_dev.name_buf->length);
	memcpy(gsmi_dev.name_buf->start, name, name_len * 2);

	 
	memset(gsmi_dev.data_buf->start, 0, gsmi_dev.data_buf->length);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_GET_NVRAM_VAR);
	if (rc < 0) {
		printk(KERN_ERR "gsmi: Get Variable failed\n");
		ret = EFI_LOAD_ERROR;
	} else if (rc == 1) {
		 
		ret = EFI_NOT_FOUND;
	} else {
		 
		memcpy(&param, gsmi_dev.param_buf->start, sizeof(param));

		 
		*data_size = min_t(unsigned long, *data_size,
						gsmi_dev.data_buf->length);
		*data_size = min_t(unsigned long, *data_size, param.data_len);

		 
		memcpy(data, gsmi_dev.data_buf->start, *data_size);

		 
		if (attr)
			*attr = EFI_VARIABLE_NON_VOLATILE |
				EFI_VARIABLE_BOOTSERVICE_ACCESS |
				EFI_VARIABLE_RUNTIME_ACCESS;
	}

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	return ret;
}

static efi_status_t gsmi_get_next_variable(unsigned long *name_size,
					   efi_char16_t *name,
					   efi_guid_t *vendor)
{
	struct gsmi_get_next_var_param param = {
		.name_ptr = gsmi_dev.name_buf->address,
		.name_len = gsmi_dev.name_buf->length,
	};
	efi_status_t ret = EFI_SUCCESS;
	int rc;
	unsigned long flags;

	 
	if (*name_size != GSMI_BUF_SIZE)
		return EFI_BAD_BUFFER_SIZE;

	 
	if (ucs2_strnlen(name, GSMI_BUF_SIZE / 2) == GSMI_BUF_SIZE / 2)
		return EFI_INVALID_PARAMETER;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memcpy(&param.guid, vendor, sizeof(param.guid));

	 
	memcpy(gsmi_dev.name_buf->start, name, *name_size);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_GET_NEXT_VAR);
	if (rc < 0) {
		printk(KERN_ERR "gsmi: Get Next Variable Name failed\n");
		ret = EFI_LOAD_ERROR;
	} else if (rc == 1) {
		 
		ret = EFI_NOT_FOUND;
	} else {
		 
		memcpy(&param, gsmi_dev.param_buf->start, sizeof(param));

		 
		memcpy(name, gsmi_dev.name_buf->start, GSMI_BUF_SIZE);
		*name_size = ucs2_strnlen(name, GSMI_BUF_SIZE / 2) * 2;

		 
		memcpy(vendor, &param.guid, sizeof(param.guid));
		ret = EFI_SUCCESS;
	}

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	return ret;
}

static efi_status_t gsmi_set_variable(efi_char16_t *name,
				      efi_guid_t *vendor,
				      u32 attr,
				      unsigned long data_size,
				      void *data)
{
	struct gsmi_nvram_var_param param = {
		.name_ptr = gsmi_dev.name_buf->address,
		.data_ptr = gsmi_dev.data_buf->address,
		.data_len = (u32)data_size,
		.attributes = EFI_VARIABLE_NON_VOLATILE |
			      EFI_VARIABLE_BOOTSERVICE_ACCESS |
			      EFI_VARIABLE_RUNTIME_ACCESS,
	};
	size_t name_len = ucs2_strnlen(name, GSMI_BUF_SIZE / 2);
	efi_status_t ret = EFI_SUCCESS;
	int rc;
	unsigned long flags;

	if (name_len >= GSMI_BUF_SIZE / 2)
		return EFI_BAD_BUFFER_SIZE;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memcpy(&param.guid, vendor, sizeof(param.guid));

	 
	memset(gsmi_dev.name_buf->start, 0, gsmi_dev.name_buf->length);
	memcpy(gsmi_dev.name_buf->start, name, name_len * 2);

	 
	memset(gsmi_dev.data_buf->start, 0, gsmi_dev.data_buf->length);
	memcpy(gsmi_dev.data_buf->start, data, data_size);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_SET_NVRAM_VAR);
	if (rc < 0) {
		printk(KERN_ERR "gsmi: Set Variable failed\n");
		ret = EFI_INVALID_PARAMETER;
	}

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	return ret;
}

static const struct efivar_operations efivar_ops = {
	.get_variable = gsmi_get_variable,
	.set_variable = gsmi_set_variable,
	.get_next_variable = gsmi_get_next_variable,
};

#endif  

static ssize_t eventlog_write(struct file *filp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buf, loff_t pos, size_t count)
{
	struct gsmi_set_eventlog_param param = {
		.data_ptr = gsmi_dev.data_buf->address,
	};
	int rc = 0;
	unsigned long flags;

	 
	if (count < sizeof(u32))
		return -EINVAL;
	param.type = *(u32 *)buf;
	buf += sizeof(u32);

	 
	if ((count - sizeof(u32)) > gsmi_dev.data_buf->length)
		return -EINVAL;
	param.data_len = count - sizeof(u32);

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memset(gsmi_dev.data_buf->start, 0, gsmi_dev.data_buf->length);
	memcpy(gsmi_dev.data_buf->start, buf, param.data_len);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_SET_EVENT_LOG);
	if (rc < 0)
		printk(KERN_ERR "gsmi: Set Event Log failed\n");

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	return (rc == 0) ? count : rc;

}

static struct bin_attribute eventlog_bin_attr = {
	.attr = {.name = "append_to_eventlog", .mode = 0200},
	.write = eventlog_write,
};

static ssize_t gsmi_clear_eventlog_store(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int rc;
	unsigned long flags;
	unsigned long val;
	struct {
		u32 percentage;
		u32 data_type;
	} param;

	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;

	 
	if (val > 100)
		return -EINVAL;

	 
	param.percentage = val;
	param.data_type = 0;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_CLEAR_EVENT_LOG);

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	if (rc)
		return rc;
	return count;
}

static struct kobj_attribute gsmi_clear_eventlog_attr = {
	.attr = {.name = "clear_eventlog", .mode = 0200},
	.store = gsmi_clear_eventlog_store,
};

static ssize_t gsmi_clear_config_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	 
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_CLEAR_CONFIG);

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	if (rc)
		return rc;
	return count;
}

static struct kobj_attribute gsmi_clear_config_attr = {
	.attr = {.name = "clear_config", .mode = 0200},
	.store = gsmi_clear_config_store,
};

static const struct attribute *gsmi_attrs[] = {
	&gsmi_clear_config_attr.attr,
	&gsmi_clear_eventlog_attr.attr,
	NULL,
};

static int gsmi_shutdown_reason(int reason)
{
	struct gsmi_log_entry_type_1 entry = {
		.type     = GSMI_LOG_ENTRY_TYPE_KERNEL,
		.instance = reason,
	};
	struct gsmi_set_eventlog_param param = {
		.data_len = sizeof(entry),
		.type     = 1,
	};
	static int saved_reason;
	int rc = 0;
	unsigned long flags;

	 
	if (saved_reason & (1 << reason))
		return 0;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	saved_reason |= (1 << reason);

	 
	memset(gsmi_dev.data_buf->start, 0, gsmi_dev.data_buf->length);
	memcpy(gsmi_dev.data_buf->start, &entry, sizeof(entry));

	 
	param.data_ptr = gsmi_dev.data_buf->address;
	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);
	memcpy(gsmi_dev.param_buf->start, &param, sizeof(param));

	rc = gsmi_exec(GSMI_CALLBACK, GSMI_CMD_SET_EVENT_LOG);

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	if (rc < 0)
		printk(KERN_ERR "gsmi: Log Shutdown Reason failed\n");
	else
		printk(KERN_EMERG "gsmi: Log Shutdown Reason 0x%02x\n",
		       reason);

	return rc;
}

static int gsmi_reboot_callback(struct notifier_block *nb,
				unsigned long reason, void *arg)
{
	gsmi_shutdown_reason(GSMI_SHUTDOWN_CLEAN);
	return NOTIFY_DONE;
}

static struct notifier_block gsmi_reboot_notifier = {
	.notifier_call = gsmi_reboot_callback
};

static int gsmi_die_callback(struct notifier_block *nb,
			     unsigned long reason, void *arg)
{
	if (reason == DIE_OOPS)
		gsmi_shutdown_reason(GSMI_SHUTDOWN_OOPS);
	return NOTIFY_DONE;
}

static struct notifier_block gsmi_die_notifier = {
	.notifier_call = gsmi_die_callback
};

static int gsmi_panic_callback(struct notifier_block *nb,
			       unsigned long reason, void *arg)
{

	 
	if (spin_is_locked(&gsmi_dev.lock))
		return NOTIFY_DONE;

	gsmi_shutdown_reason(GSMI_SHUTDOWN_PANIC);
	return NOTIFY_DONE;
}

static struct notifier_block gsmi_panic_notifier = {
	.notifier_call = gsmi_panic_callback,
};

 
static u64 __init local_hash_64(u64 val, unsigned bits)
{
	u64 hash = val;

	 
	u64 n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;

	 
	return hash >> (64 - bits);
}

static u32 __init hash_oem_table_id(char s[8])
{
	u64 input;
	memcpy(&input, s, 8);
	return local_hash_64(input, 32);
}

static const struct dmi_system_id gsmi_dmi_table[] __initconst = {
	{
		.ident = "Google Board",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Google, Inc."),
		},
	},
	{
		.ident = "Coreboot Firmware",
		.matches = {
			DMI_MATCH(DMI_BIOS_VENDOR, "coreboot"),
		},
	},
	{}
};
MODULE_DEVICE_TABLE(dmi, gsmi_dmi_table);

static __init int gsmi_system_valid(void)
{
	u32 hash;
	u16 cmd, result;

	if (!dmi_check_system(gsmi_dmi_table))
		return -ENODEV;

	 
	if (!strncmp(acpi_gbl_FADT.header.oem_table_id, "FACP", 4)) {
		printk(KERN_INFO "gsmi: Board is too old\n");
		return -ENODEV;
	}

	 
	hash = hash_oem_table_id(acpi_gbl_FADT.header.oem_table_id);
	if (hash == QUIRKY_BOARD_HASH) {
		const char *bios_ver = dmi_get_system_info(DMI_BIOS_VERSION);
		if (strncmp(bios_ver, "1.0", 3) == 0) {
			pr_info("gsmi: disabled on this board's BIOS %s\n",
				bios_ver);
			return -ENODEV;
		}
	}

	 
	if (acpi_gbl_FADT.smi_command == 0) {
		pr_info("gsmi: missing smi_command\n");
		return -ENODEV;
	}

	 
	cmd = GSMI_CALLBACK | GSMI_CMD_RESERVED << 8;
	asm volatile (
		"outb %%al, %%dx\n\t"
		: "=a" (result)
		: "0" (cmd),
		  "d" (acpi_gbl_FADT.smi_command)
		: "memory", "cc"
		);
	if (cmd == result) {
		pr_info("gsmi: no gsmi handler in firmware\n");
		return -ENODEV;
	}

	 
	return 0;
}

static struct kobject *gsmi_kobj;

static const struct platform_device_info gsmi_dev_info = {
	.name		= "gsmi",
	.id		= -1,
	 
	.dma_mask	= DMA_BIT_MASK(32),
};

#ifdef CONFIG_PM
static void gsmi_log_s0ix_info(u8 cmd)
{
	unsigned long flags;

	 
	if (!s0ix_logging_enable)
		return;

	spin_lock_irqsave(&gsmi_dev.lock, flags);

	memset(gsmi_dev.param_buf->start, 0, gsmi_dev.param_buf->length);

	gsmi_exec(GSMI_CALLBACK, cmd);

	spin_unlock_irqrestore(&gsmi_dev.lock, flags);
}

static int gsmi_log_s0ix_suspend(struct device *dev)
{
	 
	if (!pm_suspend_via_firmware())
		gsmi_log_s0ix_info(GSMI_CMD_LOG_S0IX_SUSPEND);

	 
	return 0;
}

static int gsmi_log_s0ix_resume(struct device *dev)
{
	 
	if (!pm_resume_via_firmware())
		gsmi_log_s0ix_info(GSMI_CMD_LOG_S0IX_RESUME);

	 
	return 0;
}

static const struct dev_pm_ops gsmi_pm_ops = {
	.suspend_noirq = gsmi_log_s0ix_suspend,
	.resume_noirq = gsmi_log_s0ix_resume,
};

static int gsmi_platform_driver_probe(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver gsmi_driver_info = {
	.driver = {
		.name = "gsmi",
		.pm = &gsmi_pm_ops,
	},
	.probe = gsmi_platform_driver_probe,
};
#endif

static __init int gsmi_init(void)
{
	unsigned long flags;
	int ret;

	ret = gsmi_system_valid();
	if (ret)
		return ret;

	gsmi_dev.smi_cmd = acpi_gbl_FADT.smi_command;

#ifdef CONFIG_PM
	ret = platform_driver_register(&gsmi_driver_info);
	if (unlikely(ret)) {
		printk(KERN_ERR "gsmi: unable to register platform driver\n");
		return ret;
	}
#endif

	 
	gsmi_dev.pdev = platform_device_register_full(&gsmi_dev_info);
	if (IS_ERR(gsmi_dev.pdev)) {
		printk(KERN_ERR "gsmi: unable to register platform device\n");
		return PTR_ERR(gsmi_dev.pdev);
	}

	 
	spin_lock_init(&gsmi_dev.lock);

	ret = -ENOMEM;

	 
	gsmi_dev.mem_pool = kmem_cache_create("gsmi", GSMI_BUF_SIZE,
					      GSMI_BUF_ALIGN,
					      SLAB_CACHE_DMA32, NULL);
	if (!gsmi_dev.mem_pool)
		goto out_err;

	 
	gsmi_dev.name_buf = gsmi_buf_alloc();
	if (!gsmi_dev.name_buf) {
		printk(KERN_ERR "gsmi: failed to allocate name buffer\n");
		goto out_err;
	}

	gsmi_dev.data_buf = gsmi_buf_alloc();
	if (!gsmi_dev.data_buf) {
		printk(KERN_ERR "gsmi: failed to allocate data buffer\n");
		goto out_err;
	}

	gsmi_dev.param_buf = gsmi_buf_alloc();
	if (!gsmi_dev.param_buf) {
		printk(KERN_ERR "gsmi: failed to allocate param buffer\n");
		goto out_err;
	}

	 
	spin_lock_irqsave(&gsmi_dev.lock, flags);
	gsmi_dev.handshake_type = GSMI_HANDSHAKE_SPIN;
	gsmi_dev.handshake_type =
	    gsmi_exec(GSMI_CALLBACK, GSMI_CMD_HANDSHAKE_TYPE);
	if (gsmi_dev.handshake_type == -ENOSYS)
		gsmi_dev.handshake_type = GSMI_HANDSHAKE_NONE;
	spin_unlock_irqrestore(&gsmi_dev.lock, flags);

	 
	if (gsmi_dev.handshake_type == -ENXIO) {
		printk(KERN_INFO "gsmi version " DRIVER_VERSION
		       " failed to load\n");
		ret = -ENODEV;
		goto out_err;
	}

	 
	ret = -ENOMEM;
	gsmi_kobj = kobject_create_and_add("gsmi", firmware_kobj);
	if (!gsmi_kobj) {
		printk(KERN_INFO "gsmi: Failed to create firmware kobj\n");
		goto out_err;
	}

	 
	ret = sysfs_create_bin_file(gsmi_kobj, &eventlog_bin_attr);
	if (ret) {
		printk(KERN_INFO "gsmi: Failed to setup eventlog");
		goto out_err;
	}

	 
	ret = sysfs_create_files(gsmi_kobj, gsmi_attrs);
	if (ret) {
		printk(KERN_INFO "gsmi: Failed to add attrs");
		goto out_remove_bin_file;
	}

#ifdef CONFIG_EFI
	ret = efivars_register(&efivars, &efivar_ops);
	if (ret) {
		printk(KERN_INFO "gsmi: Failed to register efivars\n");
		sysfs_remove_files(gsmi_kobj, gsmi_attrs);
		goto out_remove_bin_file;
	}
#endif

	register_reboot_notifier(&gsmi_reboot_notifier);
	register_die_notifier(&gsmi_die_notifier);
	atomic_notifier_chain_register(&panic_notifier_list,
				       &gsmi_panic_notifier);

	printk(KERN_INFO "gsmi version " DRIVER_VERSION " loaded\n");

	return 0;

out_remove_bin_file:
	sysfs_remove_bin_file(gsmi_kobj, &eventlog_bin_attr);
out_err:
	kobject_put(gsmi_kobj);
	gsmi_buf_free(gsmi_dev.param_buf);
	gsmi_buf_free(gsmi_dev.data_buf);
	gsmi_buf_free(gsmi_dev.name_buf);
	kmem_cache_destroy(gsmi_dev.mem_pool);
	platform_device_unregister(gsmi_dev.pdev);
	pr_info("gsmi: failed to load: %d\n", ret);
#ifdef CONFIG_PM
	platform_driver_unregister(&gsmi_driver_info);
#endif
	return ret;
}

static void __exit gsmi_exit(void)
{
	unregister_reboot_notifier(&gsmi_reboot_notifier);
	unregister_die_notifier(&gsmi_die_notifier);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &gsmi_panic_notifier);
#ifdef CONFIG_EFI
	efivars_unregister(&efivars);
#endif

	sysfs_remove_files(gsmi_kobj, gsmi_attrs);
	sysfs_remove_bin_file(gsmi_kobj, &eventlog_bin_attr);
	kobject_put(gsmi_kobj);
	gsmi_buf_free(gsmi_dev.param_buf);
	gsmi_buf_free(gsmi_dev.data_buf);
	gsmi_buf_free(gsmi_dev.name_buf);
	kmem_cache_destroy(gsmi_dev.mem_pool);
	platform_device_unregister(gsmi_dev.pdev);
#ifdef CONFIG_PM
	platform_driver_unregister(&gsmi_driver_info);
#endif
}

module_init(gsmi_init);
module_exit(gsmi_exit);

MODULE_AUTHOR("Google, Inc.");
MODULE_LICENSE("GPL");
