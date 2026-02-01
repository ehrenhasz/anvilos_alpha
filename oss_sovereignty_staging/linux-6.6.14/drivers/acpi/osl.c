
 

#define pr_fmt(fmt) "ACPI: OSL: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/lockdep.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/nmi.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/security.h>

#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/io-64-nonatomic-lo-hi.h>

#include "acpica/accommon.h"
#include "internal.h"

 
#define _COMPONENT		ACPI_OS_SERVICES
ACPI_MODULE_NAME("osl");

struct acpi_os_dpc {
	acpi_osd_exec_callback function;
	void *context;
	struct work_struct work;
};

#ifdef ENABLE_DEBUGGER
#include <linux/kdb.h>

 
int acpi_in_debugger;
EXPORT_SYMBOL(acpi_in_debugger);
#endif				 

static int (*__acpi_os_prepare_sleep)(u8 sleep_state, u32 pm1a_ctrl,
				      u32 pm1b_ctrl);
static int (*__acpi_os_prepare_extended_sleep)(u8 sleep_state, u32 val_a,
				      u32 val_b);

static acpi_osd_handler acpi_irq_handler;
static void *acpi_irq_context;
static struct workqueue_struct *kacpid_wq;
static struct workqueue_struct *kacpi_notify_wq;
static struct workqueue_struct *kacpi_hotplug_wq;
static bool acpi_os_initialized;
unsigned int acpi_sci_irq = INVALID_ACPI_IRQ;
bool acpi_permanent_mmap = false;

 
struct acpi_ioremap {
	struct list_head list;
	void __iomem *virt;
	acpi_physical_address phys;
	acpi_size size;
	union {
		unsigned long refcount;
		struct rcu_work rwork;
	} track;
};

static LIST_HEAD(acpi_ioremaps);
static DEFINE_MUTEX(acpi_ioremap_lock);
#define acpi_ioremap_lock_held() lock_is_held(&acpi_ioremap_lock.dep_map)

static void __init acpi_request_region (struct acpi_generic_address *gas,
	unsigned int length, char *desc)
{
	u64 addr;

	 
	memcpy(&addr, &gas->address, sizeof(addr));
	if (!addr || !length)
		return;

	 
	if (gas->space_id == ACPI_ADR_SPACE_SYSTEM_IO)
		request_region(addr, length, desc);
	else if (gas->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY)
		request_mem_region(addr, length, desc);
}

static int __init acpi_reserve_resources(void)
{
	acpi_request_region(&acpi_gbl_FADT.xpm1a_event_block, acpi_gbl_FADT.pm1_event_length,
		"ACPI PM1a_EVT_BLK");

	acpi_request_region(&acpi_gbl_FADT.xpm1b_event_block, acpi_gbl_FADT.pm1_event_length,
		"ACPI PM1b_EVT_BLK");

	acpi_request_region(&acpi_gbl_FADT.xpm1a_control_block, acpi_gbl_FADT.pm1_control_length,
		"ACPI PM1a_CNT_BLK");

	acpi_request_region(&acpi_gbl_FADT.xpm1b_control_block, acpi_gbl_FADT.pm1_control_length,
		"ACPI PM1b_CNT_BLK");

	if (acpi_gbl_FADT.pm_timer_length == 4)
		acpi_request_region(&acpi_gbl_FADT.xpm_timer_block, 4, "ACPI PM_TMR");

	acpi_request_region(&acpi_gbl_FADT.xpm2_control_block, acpi_gbl_FADT.pm2_control_length,
		"ACPI PM2_CNT_BLK");

	 

	if (!(acpi_gbl_FADT.gpe0_block_length & 0x1))
		acpi_request_region(&acpi_gbl_FADT.xgpe0_block,
			       acpi_gbl_FADT.gpe0_block_length, "ACPI GPE0_BLK");

	if (!(acpi_gbl_FADT.gpe1_block_length & 0x1))
		acpi_request_region(&acpi_gbl_FADT.xgpe1_block,
			       acpi_gbl_FADT.gpe1_block_length, "ACPI GPE1_BLK");

	return 0;
}
fs_initcall_sync(acpi_reserve_resources);

void acpi_os_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	acpi_os_vprintf(fmt, args);
	va_end(args);
}
EXPORT_SYMBOL(acpi_os_printf);

void acpi_os_vprintf(const char *fmt, va_list args)
{
	static char buffer[512];

	vsprintf(buffer, fmt, args);

#ifdef ENABLE_DEBUGGER
	if (acpi_in_debugger) {
		kdb_printf("%s", buffer);
	} else {
		if (printk_get_level(buffer))
			printk("%s", buffer);
		else
			printk(KERN_CONT "%s", buffer);
	}
#else
	if (acpi_debugger_write_log(buffer) < 0) {
		if (printk_get_level(buffer))
			printk("%s", buffer);
		else
			printk(KERN_CONT "%s", buffer);
	}
#endif
}

#ifdef CONFIG_KEXEC
static unsigned long acpi_rsdp;
static int __init setup_acpi_rsdp(char *arg)
{
	return kstrtoul(arg, 16, &acpi_rsdp);
}
early_param("acpi_rsdp", setup_acpi_rsdp);
#endif

acpi_physical_address __init acpi_os_get_root_pointer(void)
{
	acpi_physical_address pa;

#ifdef CONFIG_KEXEC
	 
	if (acpi_rsdp && !security_locked_down(LOCKDOWN_ACPI_TABLES)) {
		acpi_arch_set_root_pointer(acpi_rsdp);
		return acpi_rsdp;
	}
#endif
	pa = acpi_arch_get_root_pointer();
	if (pa)
		return pa;

	if (efi_enabled(EFI_CONFIG_TABLES)) {
		if (efi.acpi20 != EFI_INVALID_TABLE_ADDR)
			return efi.acpi20;
		if (efi.acpi != EFI_INVALID_TABLE_ADDR)
			return efi.acpi;
		pr_err("System description tables not found\n");
	} else if (IS_ENABLED(CONFIG_ACPI_LEGACY_TABLES_LOOKUP)) {
		acpi_find_root_pointer(&pa);
	}

	return pa;
}

 
static struct acpi_ioremap *
acpi_map_lookup(acpi_physical_address phys, acpi_size size)
{
	struct acpi_ioremap *map;

	list_for_each_entry_rcu(map, &acpi_ioremaps, list, acpi_ioremap_lock_held())
		if (map->phys <= phys &&
		    phys + size <= map->phys + map->size)
			return map;

	return NULL;
}

 
static void __iomem *
acpi_map_vaddr_lookup(acpi_physical_address phys, unsigned int size)
{
	struct acpi_ioremap *map;

	map = acpi_map_lookup(phys, size);
	if (map)
		return map->virt + (phys - map->phys);

	return NULL;
}

void __iomem *acpi_os_get_iomem(acpi_physical_address phys, unsigned int size)
{
	struct acpi_ioremap *map;
	void __iomem *virt = NULL;

	mutex_lock(&acpi_ioremap_lock);
	map = acpi_map_lookup(phys, size);
	if (map) {
		virt = map->virt + (phys - map->phys);
		map->track.refcount++;
	}
	mutex_unlock(&acpi_ioremap_lock);
	return virt;
}
EXPORT_SYMBOL_GPL(acpi_os_get_iomem);

 
static struct acpi_ioremap *
acpi_map_lookup_virt(void __iomem *virt, acpi_size size)
{
	struct acpi_ioremap *map;

	list_for_each_entry_rcu(map, &acpi_ioremaps, list, acpi_ioremap_lock_held())
		if (map->virt <= virt &&
		    virt + size <= map->virt + map->size)
			return map;

	return NULL;
}

#if defined(CONFIG_IA64) || defined(CONFIG_ARM64) || defined(CONFIG_RISCV)
 
#define should_use_kmap(pfn)   0
#else
#define should_use_kmap(pfn)   page_is_ram(pfn)
#endif

static void __iomem *acpi_map(acpi_physical_address pg_off, unsigned long pg_sz)
{
	unsigned long pfn;

	pfn = pg_off >> PAGE_SHIFT;
	if (should_use_kmap(pfn)) {
		if (pg_sz > PAGE_SIZE)
			return NULL;
		return (void __iomem __force *)kmap(pfn_to_page(pfn));
	} else
		return acpi_os_ioremap(pg_off, pg_sz);
}

static void acpi_unmap(acpi_physical_address pg_off, void __iomem *vaddr)
{
	unsigned long pfn;

	pfn = pg_off >> PAGE_SHIFT;
	if (should_use_kmap(pfn))
		kunmap(pfn_to_page(pfn));
	else
		iounmap(vaddr);
}

 
void __iomem __ref
*acpi_os_map_iomem(acpi_physical_address phys, acpi_size size)
{
	struct acpi_ioremap *map;
	void __iomem *virt;
	acpi_physical_address pg_off;
	acpi_size pg_sz;

	if (phys > ULONG_MAX) {
		pr_err("Cannot map memory that high: 0x%llx\n", phys);
		return NULL;
	}

	if (!acpi_permanent_mmap)
		return __acpi_map_table((unsigned long)phys, size);

	mutex_lock(&acpi_ioremap_lock);
	 
	map = acpi_map_lookup(phys, size);
	if (map) {
		map->track.refcount++;
		goto out;
	}

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		return NULL;
	}

	pg_off = round_down(phys, PAGE_SIZE);
	pg_sz = round_up(phys + size, PAGE_SIZE) - pg_off;
	virt = acpi_map(phys, size);
	if (!virt) {
		mutex_unlock(&acpi_ioremap_lock);
		kfree(map);
		return NULL;
	}

	INIT_LIST_HEAD(&map->list);
	map->virt = (void __iomem __force *)((unsigned long)virt & PAGE_MASK);
	map->phys = pg_off;
	map->size = pg_sz;
	map->track.refcount = 1;

	list_add_tail_rcu(&map->list, &acpi_ioremaps);

out:
	mutex_unlock(&acpi_ioremap_lock);
	return map->virt + (phys - map->phys);
}
EXPORT_SYMBOL_GPL(acpi_os_map_iomem);

void *__ref acpi_os_map_memory(acpi_physical_address phys, acpi_size size)
{
	return (void *)acpi_os_map_iomem(phys, size);
}
EXPORT_SYMBOL_GPL(acpi_os_map_memory);

static void acpi_os_map_remove(struct work_struct *work)
{
	struct acpi_ioremap *map = container_of(to_rcu_work(work),
						struct acpi_ioremap,
						track.rwork);

	acpi_unmap(map->phys, map->virt);
	kfree(map);
}

 
static void acpi_os_drop_map_ref(struct acpi_ioremap *map)
{
	if (--map->track.refcount)
		return;

	list_del_rcu(&map->list);

	INIT_RCU_WORK(&map->track.rwork, acpi_os_map_remove);
	queue_rcu_work(system_wq, &map->track.rwork);
}

 
void __ref acpi_os_unmap_iomem(void __iomem *virt, acpi_size size)
{
	struct acpi_ioremap *map;

	if (!acpi_permanent_mmap) {
		__acpi_unmap_table(virt, size);
		return;
	}

	mutex_lock(&acpi_ioremap_lock);

	map = acpi_map_lookup_virt(virt, size);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		WARN(true, "ACPI: %s: bad address %p\n", __func__, virt);
		return;
	}
	acpi_os_drop_map_ref(map);

	mutex_unlock(&acpi_ioremap_lock);
}
EXPORT_SYMBOL_GPL(acpi_os_unmap_iomem);

 
void __ref acpi_os_unmap_memory(void *virt, acpi_size size)
{
	acpi_os_unmap_iomem((void __iomem *)virt, size);
}
EXPORT_SYMBOL_GPL(acpi_os_unmap_memory);

void __iomem *acpi_os_map_generic_address(struct acpi_generic_address *gas)
{
	u64 addr;

	if (gas->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY)
		return NULL;

	 
	memcpy(&addr, &gas->address, sizeof(addr));
	if (!addr || !gas->bit_width)
		return NULL;

	return acpi_os_map_iomem(addr, gas->bit_width / 8);
}
EXPORT_SYMBOL(acpi_os_map_generic_address);

void acpi_os_unmap_generic_address(struct acpi_generic_address *gas)
{
	u64 addr;
	struct acpi_ioremap *map;

	if (gas->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY)
		return;

	 
	memcpy(&addr, &gas->address, sizeof(addr));
	if (!addr || !gas->bit_width)
		return;

	mutex_lock(&acpi_ioremap_lock);

	map = acpi_map_lookup(addr, gas->bit_width / 8);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		return;
	}
	acpi_os_drop_map_ref(map);

	mutex_unlock(&acpi_ioremap_lock);
}
EXPORT_SYMBOL(acpi_os_unmap_generic_address);

#ifdef ACPI_FUTURE_USAGE
acpi_status
acpi_os_get_physical_address(void *virt, acpi_physical_address * phys)
{
	if (!phys || !virt)
		return AE_BAD_PARAMETER;

	*phys = virt_to_phys(virt);

	return AE_OK;
}
#endif

#ifdef CONFIG_ACPI_REV_OVERRIDE_POSSIBLE
static bool acpi_rev_override;

int __init acpi_rev_override_setup(char *str)
{
	acpi_rev_override = true;
	return 1;
}
__setup("acpi_rev_override", acpi_rev_override_setup);
#else
#define acpi_rev_override	false
#endif

#define ACPI_MAX_OVERRIDE_LEN 100

static char acpi_os_name[ACPI_MAX_OVERRIDE_LEN];

acpi_status
acpi_os_predefined_override(const struct acpi_predefined_names *init_val,
			    acpi_string *new_val)
{
	if (!init_val || !new_val)
		return AE_BAD_PARAMETER;

	*new_val = NULL;
	if (!memcmp(init_val->name, "_OS_", 4) && strlen(acpi_os_name)) {
		pr_info("Overriding _OS definition to '%s'\n", acpi_os_name);
		*new_val = acpi_os_name;
	}

	if (!memcmp(init_val->name, "_REV", 4) && acpi_rev_override) {
		pr_info("Overriding _REV return value to 5\n");
		*new_val = (char *)5;
	}

	return AE_OK;
}

static irqreturn_t acpi_irq(int irq, void *dev_id)
{
	u32 handled;

	handled = (*acpi_irq_handler) (acpi_irq_context);

	if (handled) {
		acpi_irq_handled++;
		return IRQ_HANDLED;
	} else {
		acpi_irq_not_handled++;
		return IRQ_NONE;
	}
}

acpi_status
acpi_os_install_interrupt_handler(u32 gsi, acpi_osd_handler handler,
				  void *context)
{
	unsigned int irq;

	acpi_irq_stats_init();

	 
	if (gsi != acpi_gbl_FADT.sci_interrupt)
		return AE_BAD_PARAMETER;

	if (acpi_irq_handler)
		return AE_ALREADY_ACQUIRED;

	if (acpi_gsi_to_irq(gsi, &irq) < 0) {
		pr_err("SCI (ACPI GSI %d) not registered\n", gsi);
		return AE_OK;
	}

	acpi_irq_handler = handler;
	acpi_irq_context = context;
	if (request_irq(irq, acpi_irq, IRQF_SHARED, "acpi", acpi_irq)) {
		pr_err("SCI (IRQ%d) allocation failed\n", irq);
		acpi_irq_handler = NULL;
		return AE_NOT_ACQUIRED;
	}
	acpi_sci_irq = irq;

	return AE_OK;
}

acpi_status acpi_os_remove_interrupt_handler(u32 gsi, acpi_osd_handler handler)
{
	if (gsi != acpi_gbl_FADT.sci_interrupt || !acpi_sci_irq_valid())
		return AE_BAD_PARAMETER;

	free_irq(acpi_sci_irq, acpi_irq);
	acpi_irq_handler = NULL;
	acpi_sci_irq = INVALID_ACPI_IRQ;

	return AE_OK;
}

 

void acpi_os_sleep(u64 ms)
{
	msleep(ms);
}

void acpi_os_stall(u32 us)
{
	while (us) {
		u32 delay = 1000;

		if (delay > us)
			delay = us;
		udelay(delay);
		touch_nmi_watchdog();
		us -= delay;
	}
}

 
u64 acpi_os_get_timer(void)
{
	return (get_jiffies_64() - INITIAL_JIFFIES) *
		(ACPI_100NSEC_PER_SEC / HZ);
}

acpi_status acpi_os_read_port(acpi_io_address port, u32 *value, u32 width)
{
	u32 dummy;

	if (value)
		*value = 0;
	else
		value = &dummy;

	if (width <= 8) {
		*value = inb(port);
	} else if (width <= 16) {
		*value = inw(port);
	} else if (width <= 32) {
		*value = inl(port);
	} else {
		pr_debug("%s: Access width %d not supported\n", __func__, width);
		return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

EXPORT_SYMBOL(acpi_os_read_port);

acpi_status acpi_os_write_port(acpi_io_address port, u32 value, u32 width)
{
	if (width <= 8) {
		outb(value, port);
	} else if (width <= 16) {
		outw(value, port);
	} else if (width <= 32) {
		outl(value, port);
	} else {
		pr_debug("%s: Access width %d not supported\n", __func__, width);
		return AE_BAD_PARAMETER;
	}

	return AE_OK;
}

EXPORT_SYMBOL(acpi_os_write_port);

int acpi_os_read_iomem(void __iomem *virt_addr, u64 *value, u32 width)
{

	switch (width) {
	case 8:
		*(u8 *) value = readb(virt_addr);
		break;
	case 16:
		*(u16 *) value = readw(virt_addr);
		break;
	case 32:
		*(u32 *) value = readl(virt_addr);
		break;
	case 64:
		*(u64 *) value = readq(virt_addr);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

acpi_status
acpi_os_read_memory(acpi_physical_address phys_addr, u64 *value, u32 width)
{
	void __iomem *virt_addr;
	unsigned int size = width / 8;
	bool unmap = false;
	u64 dummy;
	int error;

	rcu_read_lock();
	virt_addr = acpi_map_vaddr_lookup(phys_addr, size);
	if (!virt_addr) {
		rcu_read_unlock();
		virt_addr = acpi_os_ioremap(phys_addr, size);
		if (!virt_addr)
			return AE_BAD_ADDRESS;
		unmap = true;
	}

	if (!value)
		value = &dummy;

	error = acpi_os_read_iomem(virt_addr, value, width);
	BUG_ON(error);

	if (unmap)
		iounmap(virt_addr);
	else
		rcu_read_unlock();

	return AE_OK;
}

acpi_status
acpi_os_write_memory(acpi_physical_address phys_addr, u64 value, u32 width)
{
	void __iomem *virt_addr;
	unsigned int size = width / 8;
	bool unmap = false;

	rcu_read_lock();
	virt_addr = acpi_map_vaddr_lookup(phys_addr, size);
	if (!virt_addr) {
		rcu_read_unlock();
		virt_addr = acpi_os_ioremap(phys_addr, size);
		if (!virt_addr)
			return AE_BAD_ADDRESS;
		unmap = true;
	}

	switch (width) {
	case 8:
		writeb(value, virt_addr);
		break;
	case 16:
		writew(value, virt_addr);
		break;
	case 32:
		writel(value, virt_addr);
		break;
	case 64:
		writeq(value, virt_addr);
		break;
	default:
		BUG();
	}

	if (unmap)
		iounmap(virt_addr);
	else
		rcu_read_unlock();

	return AE_OK;
}

#ifdef CONFIG_PCI
acpi_status
acpi_os_read_pci_configuration(struct acpi_pci_id * pci_id, u32 reg,
			       u64 *value, u32 width)
{
	int result, size;
	u32 value32;

	if (!value)
		return AE_BAD_PARAMETER;

	switch (width) {
	case 8:
		size = 1;
		break;
	case 16:
		size = 2;
		break;
	case 32:
		size = 4;
		break;
	default:
		return AE_ERROR;
	}

	result = raw_pci_read(pci_id->segment, pci_id->bus,
				PCI_DEVFN(pci_id->device, pci_id->function),
				reg, size, &value32);
	*value = value32;

	return (result ? AE_ERROR : AE_OK);
}

acpi_status
acpi_os_write_pci_configuration(struct acpi_pci_id * pci_id, u32 reg,
				u64 value, u32 width)
{
	int result, size;

	switch (width) {
	case 8:
		size = 1;
		break;
	case 16:
		size = 2;
		break;
	case 32:
		size = 4;
		break;
	default:
		return AE_ERROR;
	}

	result = raw_pci_write(pci_id->segment, pci_id->bus,
				PCI_DEVFN(pci_id->device, pci_id->function),
				reg, size, value);

	return (result ? AE_ERROR : AE_OK);
}
#endif

static void acpi_os_execute_deferred(struct work_struct *work)
{
	struct acpi_os_dpc *dpc = container_of(work, struct acpi_os_dpc, work);

	dpc->function(dpc->context);
	kfree(dpc);
}

#ifdef CONFIG_ACPI_DEBUGGER
static struct acpi_debugger acpi_debugger;
static bool acpi_debugger_initialized;

int acpi_register_debugger(struct module *owner,
			   const struct acpi_debugger_ops *ops)
{
	int ret = 0;

	mutex_lock(&acpi_debugger.lock);
	if (acpi_debugger.ops) {
		ret = -EBUSY;
		goto err_lock;
	}

	acpi_debugger.owner = owner;
	acpi_debugger.ops = ops;

err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}
EXPORT_SYMBOL(acpi_register_debugger);

void acpi_unregister_debugger(const struct acpi_debugger_ops *ops)
{
	mutex_lock(&acpi_debugger.lock);
	if (ops == acpi_debugger.ops) {
		acpi_debugger.ops = NULL;
		acpi_debugger.owner = NULL;
	}
	mutex_unlock(&acpi_debugger.lock);
}
EXPORT_SYMBOL(acpi_unregister_debugger);

int acpi_debugger_create_thread(acpi_osd_exec_callback function, void *context)
{
	int ret;
	int (*func)(acpi_osd_exec_callback, void *);
	struct module *owner;

	if (!acpi_debugger_initialized)
		return -ENODEV;
	mutex_lock(&acpi_debugger.lock);
	if (!acpi_debugger.ops) {
		ret = -ENODEV;
		goto err_lock;
	}
	if (!try_module_get(acpi_debugger.owner)) {
		ret = -ENODEV;
		goto err_lock;
	}
	func = acpi_debugger.ops->create_thread;
	owner = acpi_debugger.owner;
	mutex_unlock(&acpi_debugger.lock);

	ret = func(function, context);

	mutex_lock(&acpi_debugger.lock);
	module_put(owner);
err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}

ssize_t acpi_debugger_write_log(const char *msg)
{
	ssize_t ret;
	ssize_t (*func)(const char *);
	struct module *owner;

	if (!acpi_debugger_initialized)
		return -ENODEV;
	mutex_lock(&acpi_debugger.lock);
	if (!acpi_debugger.ops) {
		ret = -ENODEV;
		goto err_lock;
	}
	if (!try_module_get(acpi_debugger.owner)) {
		ret = -ENODEV;
		goto err_lock;
	}
	func = acpi_debugger.ops->write_log;
	owner = acpi_debugger.owner;
	mutex_unlock(&acpi_debugger.lock);

	ret = func(msg);

	mutex_lock(&acpi_debugger.lock);
	module_put(owner);
err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}

ssize_t acpi_debugger_read_cmd(char *buffer, size_t buffer_length)
{
	ssize_t ret;
	ssize_t (*func)(char *, size_t);
	struct module *owner;

	if (!acpi_debugger_initialized)
		return -ENODEV;
	mutex_lock(&acpi_debugger.lock);
	if (!acpi_debugger.ops) {
		ret = -ENODEV;
		goto err_lock;
	}
	if (!try_module_get(acpi_debugger.owner)) {
		ret = -ENODEV;
		goto err_lock;
	}
	func = acpi_debugger.ops->read_cmd;
	owner = acpi_debugger.owner;
	mutex_unlock(&acpi_debugger.lock);

	ret = func(buffer, buffer_length);

	mutex_lock(&acpi_debugger.lock);
	module_put(owner);
err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}

int acpi_debugger_wait_command_ready(void)
{
	int ret;
	int (*func)(bool, char *, size_t);
	struct module *owner;

	if (!acpi_debugger_initialized)
		return -ENODEV;
	mutex_lock(&acpi_debugger.lock);
	if (!acpi_debugger.ops) {
		ret = -ENODEV;
		goto err_lock;
	}
	if (!try_module_get(acpi_debugger.owner)) {
		ret = -ENODEV;
		goto err_lock;
	}
	func = acpi_debugger.ops->wait_command_ready;
	owner = acpi_debugger.owner;
	mutex_unlock(&acpi_debugger.lock);

	ret = func(acpi_gbl_method_executing,
		   acpi_gbl_db_line_buf, ACPI_DB_LINE_BUFFER_SIZE);

	mutex_lock(&acpi_debugger.lock);
	module_put(owner);
err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}

int acpi_debugger_notify_command_complete(void)
{
	int ret;
	int (*func)(void);
	struct module *owner;

	if (!acpi_debugger_initialized)
		return -ENODEV;
	mutex_lock(&acpi_debugger.lock);
	if (!acpi_debugger.ops) {
		ret = -ENODEV;
		goto err_lock;
	}
	if (!try_module_get(acpi_debugger.owner)) {
		ret = -ENODEV;
		goto err_lock;
	}
	func = acpi_debugger.ops->notify_command_complete;
	owner = acpi_debugger.owner;
	mutex_unlock(&acpi_debugger.lock);

	ret = func();

	mutex_lock(&acpi_debugger.lock);
	module_put(owner);
err_lock:
	mutex_unlock(&acpi_debugger.lock);
	return ret;
}

int __init acpi_debugger_init(void)
{
	mutex_init(&acpi_debugger.lock);
	acpi_debugger_initialized = true;
	return 0;
}
#endif

 

acpi_status acpi_os_execute(acpi_execute_type type,
			    acpi_osd_exec_callback function, void *context)
{
	acpi_status status = AE_OK;
	struct acpi_os_dpc *dpc;
	struct workqueue_struct *queue;
	int ret;
	ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
			  "Scheduling function [%p(%p)] for deferred execution.\n",
			  function, context));

	if (type == OSL_DEBUGGER_MAIN_THREAD) {
		ret = acpi_debugger_create_thread(function, context);
		if (ret) {
			pr_err("Kernel thread creation failed\n");
			status = AE_ERROR;
		}
		goto out_thread;
	}

	 

	dpc = kzalloc(sizeof(struct acpi_os_dpc), GFP_ATOMIC);
	if (!dpc)
		return AE_NO_MEMORY;

	dpc->function = function;
	dpc->context = context;

	 
	if (type == OSL_NOTIFY_HANDLER) {
		queue = kacpi_notify_wq;
		INIT_WORK(&dpc->work, acpi_os_execute_deferred);
	} else if (type == OSL_GPE_HANDLER) {
		queue = kacpid_wq;
		INIT_WORK(&dpc->work, acpi_os_execute_deferred);
	} else {
		pr_err("Unsupported os_execute type %d.\n", type);
		status = AE_ERROR;
	}

	if (ACPI_FAILURE(status))
		goto err_workqueue;

	 
	ret = queue_work_on(0, queue, &dpc->work);
	if (!ret) {
		pr_err("Unable to queue work\n");
		status = AE_ERROR;
	}
err_workqueue:
	if (ACPI_FAILURE(status))
		kfree(dpc);
out_thread:
	return status;
}
EXPORT_SYMBOL(acpi_os_execute);

void acpi_os_wait_events_complete(void)
{
	 
	if (acpi_sci_irq_valid())
		synchronize_hardirq(acpi_sci_irq);
	flush_workqueue(kacpid_wq);
	flush_workqueue(kacpi_notify_wq);
}
EXPORT_SYMBOL(acpi_os_wait_events_complete);

struct acpi_hp_work {
	struct work_struct work;
	struct acpi_device *adev;
	u32 src;
};

static void acpi_hotplug_work_fn(struct work_struct *work)
{
	struct acpi_hp_work *hpw = container_of(work, struct acpi_hp_work, work);

	acpi_os_wait_events_complete();
	acpi_device_hotplug(hpw->adev, hpw->src);
	kfree(hpw);
}

acpi_status acpi_hotplug_schedule(struct acpi_device *adev, u32 src)
{
	struct acpi_hp_work *hpw;

	acpi_handle_debug(adev->handle,
			  "Scheduling hotplug event %u for deferred handling\n",
			   src);

	hpw = kmalloc(sizeof(*hpw), GFP_KERNEL);
	if (!hpw)
		return AE_NO_MEMORY;

	INIT_WORK(&hpw->work, acpi_hotplug_work_fn);
	hpw->adev = adev;
	hpw->src = src;
	 
	if (!queue_work(kacpi_hotplug_wq, &hpw->work)) {
		kfree(hpw);
		return AE_ERROR;
	}
	return AE_OK;
}

bool acpi_queue_hotplug_work(struct work_struct *work)
{
	return queue_work(kacpi_hotplug_wq, work);
}

acpi_status
acpi_os_create_semaphore(u32 max_units, u32 initial_units, acpi_handle * handle)
{
	struct semaphore *sem = NULL;

	sem = acpi_os_allocate_zeroed(sizeof(struct semaphore));
	if (!sem)
		return AE_NO_MEMORY;

	sema_init(sem, initial_units);

	*handle = (acpi_handle *) sem;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "Creating semaphore[%p|%d].\n",
			  *handle, initial_units));

	return AE_OK;
}

 

acpi_status acpi_os_delete_semaphore(acpi_handle handle)
{
	struct semaphore *sem = (struct semaphore *)handle;

	if (!sem)
		return AE_BAD_PARAMETER;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "Deleting semaphore[%p].\n", handle));

	BUG_ON(!list_empty(&sem->wait_list));
	kfree(sem);
	sem = NULL;

	return AE_OK;
}

 
acpi_status acpi_os_wait_semaphore(acpi_handle handle, u32 units, u16 timeout)
{
	acpi_status status = AE_OK;
	struct semaphore *sem = (struct semaphore *)handle;
	long jiffies;
	int ret = 0;

	if (!acpi_os_initialized)
		return AE_OK;

	if (!sem || (units < 1))
		return AE_BAD_PARAMETER;

	if (units > 1)
		return AE_SUPPORT;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "Waiting for semaphore[%p|%d|%d]\n",
			  handle, units, timeout));

	if (timeout == ACPI_WAIT_FOREVER)
		jiffies = MAX_SCHEDULE_TIMEOUT;
	else
		jiffies = msecs_to_jiffies(timeout);

	ret = down_timeout(sem, jiffies);
	if (ret)
		status = AE_TIME;

	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
				  "Failed to acquire semaphore[%p|%d|%d], %s",
				  handle, units, timeout,
				  acpi_format_exception(status)));
	} else {
		ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
				  "Acquired semaphore[%p|%d|%d]", handle,
				  units, timeout));
	}

	return status;
}

 
acpi_status acpi_os_signal_semaphore(acpi_handle handle, u32 units)
{
	struct semaphore *sem = (struct semaphore *)handle;

	if (!acpi_os_initialized)
		return AE_OK;

	if (!sem || (units < 1))
		return AE_BAD_PARAMETER;

	if (units > 1)
		return AE_SUPPORT;

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "Signaling semaphore[%p|%d]\n", handle,
			  units));

	up(sem);

	return AE_OK;
}

acpi_status acpi_os_get_line(char *buffer, u32 buffer_length, u32 *bytes_read)
{
#ifdef ENABLE_DEBUGGER
	if (acpi_in_debugger) {
		u32 chars;

		kdb_read(buffer, buffer_length);

		 
		chars = strlen(buffer) - 1;
		buffer[chars] = '\0';
	}
#else
	int ret;

	ret = acpi_debugger_read_cmd(buffer, buffer_length);
	if (ret < 0)
		return AE_ERROR;
	if (bytes_read)
		*bytes_read = ret;
#endif

	return AE_OK;
}
EXPORT_SYMBOL(acpi_os_get_line);

acpi_status acpi_os_wait_command_ready(void)
{
	int ret;

	ret = acpi_debugger_wait_command_ready();
	if (ret < 0)
		return AE_ERROR;
	return AE_OK;
}

acpi_status acpi_os_notify_command_complete(void)
{
	int ret;

	ret = acpi_debugger_notify_command_complete();
	if (ret < 0)
		return AE_ERROR;
	return AE_OK;
}

acpi_status acpi_os_signal(u32 function, void *info)
{
	switch (function) {
	case ACPI_SIGNAL_FATAL:
		pr_err("Fatal opcode executed\n");
		break;
	case ACPI_SIGNAL_BREAKPOINT:
		 
		break;
	default:
		break;
	}

	return AE_OK;
}

static int __init acpi_os_name_setup(char *str)
{
	char *p = acpi_os_name;
	int count = ACPI_MAX_OVERRIDE_LEN - 1;

	if (!str || !*str)
		return 0;

	for (; count-- && *str; str++) {
		if (isalnum(*str) || *str == ' ' || *str == ':')
			*p++ = *str;
		else if (*str == '\'' || *str == '"')
			continue;
		else
			break;
	}
	*p = 0;

	return 1;

}

__setup("acpi_os_name=", acpi_os_name_setup);

 
static int __init acpi_no_auto_serialize_setup(char *str)
{
	acpi_gbl_auto_serialize_methods = FALSE;
	pr_info("Auto-serialization disabled\n");

	return 1;
}

__setup("acpi_no_auto_serialize", acpi_no_auto_serialize_setup);

 
#define ENFORCE_RESOURCES_STRICT 2
#define ENFORCE_RESOURCES_LAX    1
#define ENFORCE_RESOURCES_NO     0

static unsigned int acpi_enforce_resources = ENFORCE_RESOURCES_STRICT;

static int __init acpi_enforce_resources_setup(char *str)
{
	if (str == NULL || *str == '\0')
		return 0;

	if (!strcmp("strict", str))
		acpi_enforce_resources = ENFORCE_RESOURCES_STRICT;
	else if (!strcmp("lax", str))
		acpi_enforce_resources = ENFORCE_RESOURCES_LAX;
	else if (!strcmp("no", str))
		acpi_enforce_resources = ENFORCE_RESOURCES_NO;

	return 1;
}

__setup("acpi_enforce_resources=", acpi_enforce_resources_setup);

 
int acpi_check_resource_conflict(const struct resource *res)
{
	acpi_adr_space_type space_id;

	if (acpi_enforce_resources == ENFORCE_RESOURCES_NO)
		return 0;

	if (res->flags & IORESOURCE_IO)
		space_id = ACPI_ADR_SPACE_SYSTEM_IO;
	else if (res->flags & IORESOURCE_MEM)
		space_id = ACPI_ADR_SPACE_SYSTEM_MEMORY;
	else
		return 0;

	if (!acpi_check_address_range(space_id, res->start, resource_size(res), 1))
		return 0;

	pr_info("Resource conflict; ACPI support missing from driver?\n");

	if (acpi_enforce_resources == ENFORCE_RESOURCES_STRICT)
		return -EBUSY;

	if (acpi_enforce_resources == ENFORCE_RESOURCES_LAX)
		pr_notice("Resource conflict: System may be unstable or behave erratically\n");

	return 0;
}
EXPORT_SYMBOL(acpi_check_resource_conflict);

int acpi_check_region(resource_size_t start, resource_size_t n,
		      const char *name)
{
	struct resource res = DEFINE_RES_IO_NAMED(start, n, name);

	return acpi_check_resource_conflict(&res);
}
EXPORT_SYMBOL(acpi_check_region);

 
int acpi_resources_are_enforced(void)
{
	return acpi_enforce_resources == ENFORCE_RESOURCES_STRICT;
}
EXPORT_SYMBOL(acpi_resources_are_enforced);

 
void acpi_os_delete_lock(acpi_spinlock handle)
{
	ACPI_FREE(handle);
}

 

acpi_cpu_flags acpi_os_acquire_lock(acpi_spinlock lockp)
	__acquires(lockp)
{
	acpi_cpu_flags flags;
	spin_lock_irqsave(lockp, flags);
	return flags;
}

 

void acpi_os_release_lock(acpi_spinlock lockp, acpi_cpu_flags flags)
	__releases(lockp)
{
	spin_unlock_irqrestore(lockp, flags);
}

#ifndef ACPI_USE_LOCAL_CACHE

 

acpi_status
acpi_os_create_cache(char *name, u16 size, u16 depth, acpi_cache_t ** cache)
{
	*cache = kmem_cache_create(name, size, 0, 0, NULL);
	if (*cache == NULL)
		return AE_ERROR;
	else
		return AE_OK;
}

 

acpi_status acpi_os_purge_cache(acpi_cache_t * cache)
{
	kmem_cache_shrink(cache);
	return (AE_OK);
}

 

acpi_status acpi_os_delete_cache(acpi_cache_t * cache)
{
	kmem_cache_destroy(cache);
	return (AE_OK);
}

 

acpi_status acpi_os_release_object(acpi_cache_t * cache, void *object)
{
	kmem_cache_free(cache, object);
	return (AE_OK);
}
#endif

static int __init acpi_no_static_ssdt_setup(char *s)
{
	acpi_gbl_disable_ssdt_table_install = TRUE;
	pr_info("Static SSDT installation disabled\n");

	return 0;
}

early_param("acpi_no_static_ssdt", acpi_no_static_ssdt_setup);

static int __init acpi_disable_return_repair(char *s)
{
	pr_notice("Predefined validation mechanism disabled\n");
	acpi_gbl_disable_auto_repair = TRUE;

	return 1;
}

__setup("acpica_no_return_repair", acpi_disable_return_repair);

acpi_status __init acpi_os_initialize(void)
{
	acpi_os_map_generic_address(&acpi_gbl_FADT.xpm1a_event_block);
	acpi_os_map_generic_address(&acpi_gbl_FADT.xpm1b_event_block);

	acpi_gbl_xgpe0_block_logical_address =
		(unsigned long)acpi_os_map_generic_address(&acpi_gbl_FADT.xgpe0_block);
	acpi_gbl_xgpe1_block_logical_address =
		(unsigned long)acpi_os_map_generic_address(&acpi_gbl_FADT.xgpe1_block);

	if (acpi_gbl_FADT.flags & ACPI_FADT_RESET_REGISTER) {
		 
		void *rv;

		rv = acpi_os_map_generic_address(&acpi_gbl_FADT.reset_register);
		pr_debug("%s: Reset register mapping %s\n", __func__,
			 rv ? "successful" : "failed");
	}
	acpi_os_initialized = true;

	return AE_OK;
}

acpi_status __init acpi_os_initialize1(void)
{
	kacpid_wq = alloc_workqueue("kacpid", 0, 1);
	kacpi_notify_wq = alloc_workqueue("kacpi_notify", 0, 1);
	kacpi_hotplug_wq = alloc_ordered_workqueue("kacpi_hotplug", 0);
	BUG_ON(!kacpid_wq);
	BUG_ON(!kacpi_notify_wq);
	BUG_ON(!kacpi_hotplug_wq);
	acpi_osi_init();
	return AE_OK;
}

acpi_status acpi_os_terminate(void)
{
	if (acpi_irq_handler) {
		acpi_os_remove_interrupt_handler(acpi_gbl_FADT.sci_interrupt,
						 acpi_irq_handler);
	}

	acpi_os_unmap_generic_address(&acpi_gbl_FADT.xgpe1_block);
	acpi_os_unmap_generic_address(&acpi_gbl_FADT.xgpe0_block);
	acpi_gbl_xgpe0_block_logical_address = 0UL;
	acpi_gbl_xgpe1_block_logical_address = 0UL;

	acpi_os_unmap_generic_address(&acpi_gbl_FADT.xpm1b_event_block);
	acpi_os_unmap_generic_address(&acpi_gbl_FADT.xpm1a_event_block);

	if (acpi_gbl_FADT.flags & ACPI_FADT_RESET_REGISTER)
		acpi_os_unmap_generic_address(&acpi_gbl_FADT.reset_register);

	destroy_workqueue(kacpid_wq);
	destroy_workqueue(kacpi_notify_wq);
	destroy_workqueue(kacpi_hotplug_wq);

	return AE_OK;
}

acpi_status acpi_os_prepare_sleep(u8 sleep_state, u32 pm1a_control,
				  u32 pm1b_control)
{
	int rc = 0;
	if (__acpi_os_prepare_sleep)
		rc = __acpi_os_prepare_sleep(sleep_state,
					     pm1a_control, pm1b_control);
	if (rc < 0)
		return AE_ERROR;
	else if (rc > 0)
		return AE_CTRL_TERMINATE;

	return AE_OK;
}

void acpi_os_set_prepare_sleep(int (*func)(u8 sleep_state,
			       u32 pm1a_ctrl, u32 pm1b_ctrl))
{
	__acpi_os_prepare_sleep = func;
}

#if (ACPI_REDUCED_HARDWARE)
acpi_status acpi_os_prepare_extended_sleep(u8 sleep_state, u32 val_a,
				  u32 val_b)
{
	int rc = 0;
	if (__acpi_os_prepare_extended_sleep)
		rc = __acpi_os_prepare_extended_sleep(sleep_state,
					     val_a, val_b);
	if (rc < 0)
		return AE_ERROR;
	else if (rc > 0)
		return AE_CTRL_TERMINATE;

	return AE_OK;
}
#else
acpi_status acpi_os_prepare_extended_sleep(u8 sleep_state, u32 val_a,
				  u32 val_b)
{
	return AE_OK;
}
#endif

void acpi_os_set_prepare_extended_sleep(int (*func)(u8 sleep_state,
			       u32 val_a, u32 val_b))
{
	__acpi_os_prepare_extended_sleep = func;
}

acpi_status acpi_os_enter_sleep(u8 sleep_state,
				u32 reg_a_value, u32 reg_b_value)
{
	acpi_status status;

	if (acpi_gbl_reduced_hardware)
		status = acpi_os_prepare_extended_sleep(sleep_state,
							reg_a_value,
							reg_b_value);
	else
		status = acpi_os_prepare_sleep(sleep_state,
					       reg_a_value, reg_b_value);
	return status;
}
