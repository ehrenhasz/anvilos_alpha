
 

#include <linux/acpi.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/log2.h>
#include <linux/logic_pio.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/pci_hotplug.h>
#include <linux/vmalloc.h>
#include <asm/dma.h>
#include <linux/aer.h>
#include <linux/bitfield.h>
#include "pci.h"

DEFINE_MUTEX(pci_slot_mutex);

const char *pci_power_names[] = {
	"error", "D0", "D1", "D2", "D3hot", "D3cold", "unknown",
};
EXPORT_SYMBOL_GPL(pci_power_names);

#ifdef CONFIG_X86_32
int isa_dma_bridge_buggy;
EXPORT_SYMBOL(isa_dma_bridge_buggy);
#endif

int pci_pci_problems;
EXPORT_SYMBOL(pci_pci_problems);

unsigned int pci_pm_d3hot_delay;

static void pci_pme_list_scan(struct work_struct *work);

static LIST_HEAD(pci_pme_list);
static DEFINE_MUTEX(pci_pme_list_mutex);
static DECLARE_DELAYED_WORK(pci_pme_work, pci_pme_list_scan);

struct pci_pme_device {
	struct list_head list;
	struct pci_dev *dev;
};

#define PME_TIMEOUT 1000  

 
#define PCI_RESET_WAIT 1000  

 
#define PCIE_RESET_READY_POLL_MS 60000  

static void pci_dev_d3_sleep(struct pci_dev *dev)
{
	unsigned int delay_ms = max(dev->d3hot_delay, pci_pm_d3hot_delay);
	unsigned int upper;

	if (delay_ms) {
		 
		upper = max(DIV_ROUND_CLOSEST(delay_ms, 5), 1U);
		usleep_range(delay_ms * USEC_PER_MSEC,
			     (delay_ms + upper) * USEC_PER_MSEC);
	}
}

bool pci_reset_supported(struct pci_dev *dev)
{
	return dev->reset_methods[0] != 0;
}

#ifdef CONFIG_PCI_DOMAINS
int pci_domains_supported = 1;
#endif

#define DEFAULT_CARDBUS_IO_SIZE		(256)
#define DEFAULT_CARDBUS_MEM_SIZE	(64*1024*1024)
 
unsigned long pci_cardbus_io_size = DEFAULT_CARDBUS_IO_SIZE;
unsigned long pci_cardbus_mem_size = DEFAULT_CARDBUS_MEM_SIZE;

#define DEFAULT_HOTPLUG_IO_SIZE		(256)
#define DEFAULT_HOTPLUG_MMIO_SIZE	(2*1024*1024)
#define DEFAULT_HOTPLUG_MMIO_PREF_SIZE	(2*1024*1024)
 
unsigned long pci_hotplug_io_size  = DEFAULT_HOTPLUG_IO_SIZE;
 
unsigned long pci_hotplug_mmio_size = DEFAULT_HOTPLUG_MMIO_SIZE;
unsigned long pci_hotplug_mmio_pref_size = DEFAULT_HOTPLUG_MMIO_PREF_SIZE;

#define DEFAULT_HOTPLUG_BUS_SIZE	1
unsigned long pci_hotplug_bus_size = DEFAULT_HOTPLUG_BUS_SIZE;


 
#ifdef CONFIG_PCIE_BUS_TUNE_OFF
enum pcie_bus_config_types pcie_bus_config = PCIE_BUS_TUNE_OFF;
#elif defined CONFIG_PCIE_BUS_SAFE
enum pcie_bus_config_types pcie_bus_config = PCIE_BUS_SAFE;
#elif defined CONFIG_PCIE_BUS_PERFORMANCE
enum pcie_bus_config_types pcie_bus_config = PCIE_BUS_PERFORMANCE;
#elif defined CONFIG_PCIE_BUS_PEER2PEER
enum pcie_bus_config_types pcie_bus_config = PCIE_BUS_PEER2PEER;
#else
enum pcie_bus_config_types pcie_bus_config = PCIE_BUS_DEFAULT;
#endif

 
u8 pci_dfl_cache_line_size = L1_CACHE_BYTES >> 2;
u8 pci_cache_line_size;

 
unsigned int pcibios_max_latency = 255;

 
static bool pcie_ari_disabled;

 
static bool pcie_ats_disabled;

 
bool pci_early_dump;

bool pci_ats_disabled(void)
{
	return pcie_ats_disabled;
}
EXPORT_SYMBOL_GPL(pci_ats_disabled);

 
static bool pci_bridge_d3_disable;
 
static bool pci_bridge_d3_force;

static int __init pcie_port_pm_setup(char *str)
{
	if (!strcmp(str, "off"))
		pci_bridge_d3_disable = true;
	else if (!strcmp(str, "force"))
		pci_bridge_d3_force = true;
	return 1;
}
__setup("pcie_port_pm=", pcie_port_pm_setup);

 
unsigned char pci_bus_max_busnr(struct pci_bus *bus)
{
	struct pci_bus *tmp;
	unsigned char max, n;

	max = bus->busn_res.end;
	list_for_each_entry(tmp, &bus->children, node) {
		n = pci_bus_max_busnr(tmp);
		if (n > max)
			max = n;
	}
	return max;
}
EXPORT_SYMBOL_GPL(pci_bus_max_busnr);

 
int pci_status_get_and_clear_errors(struct pci_dev *pdev)
{
	u16 status;
	int ret;

	ret = pci_read_config_word(pdev, PCI_STATUS, &status);
	if (ret != PCIBIOS_SUCCESSFUL)
		return -EIO;

	status &= PCI_STATUS_ERROR_BITS;
	if (status)
		pci_write_config_word(pdev, PCI_STATUS, status);

	return status;
}
EXPORT_SYMBOL_GPL(pci_status_get_and_clear_errors);

#ifdef CONFIG_HAS_IOMEM
static void __iomem *__pci_ioremap_resource(struct pci_dev *pdev, int bar,
					    bool write_combine)
{
	struct resource *res = &pdev->resource[bar];
	resource_size_t start = res->start;
	resource_size_t size = resource_size(res);

	 
	if (res->flags & IORESOURCE_UNSET || !(res->flags & IORESOURCE_MEM)) {
		pci_err(pdev, "can't ioremap BAR %d: %pR\n", bar, res);
		return NULL;
	}

	if (write_combine)
		return ioremap_wc(start, size);

	return ioremap(start, size);
}

void __iomem *pci_ioremap_bar(struct pci_dev *pdev, int bar)
{
	return __pci_ioremap_resource(pdev, bar, false);
}
EXPORT_SYMBOL_GPL(pci_ioremap_bar);

void __iomem *pci_ioremap_wc_bar(struct pci_dev *pdev, int bar)
{
	return __pci_ioremap_resource(pdev, bar, true);
}
EXPORT_SYMBOL_GPL(pci_ioremap_wc_bar);
#endif

 
static int pci_dev_str_match_path(struct pci_dev *dev, const char *path,
				  const char **endptr)
{
	int ret;
	unsigned int seg, bus, slot, func;
	char *wpath, *p;
	char end;

	*endptr = strchrnul(path, ';');

	wpath = kmemdup_nul(path, *endptr - path, GFP_ATOMIC);
	if (!wpath)
		return -ENOMEM;

	while (1) {
		p = strrchr(wpath, '/');
		if (!p)
			break;
		ret = sscanf(p, "/%x.%x%c", &slot, &func, &end);
		if (ret != 2) {
			ret = -EINVAL;
			goto free_and_exit;
		}

		if (dev->devfn != PCI_DEVFN(slot, func)) {
			ret = 0;
			goto free_and_exit;
		}

		 
		dev = pci_upstream_bridge(dev);
		if (!dev) {
			ret = 0;
			goto free_and_exit;
		}

		*p = 0;
	}

	ret = sscanf(wpath, "%x:%x:%x.%x%c", &seg, &bus, &slot,
		     &func, &end);
	if (ret != 4) {
		seg = 0;
		ret = sscanf(wpath, "%x:%x.%x%c", &bus, &slot, &func, &end);
		if (ret != 3) {
			ret = -EINVAL;
			goto free_and_exit;
		}
	}

	ret = (seg == pci_domain_nr(dev->bus) &&
	       bus == dev->bus->number &&
	       dev->devfn == PCI_DEVFN(slot, func));

free_and_exit:
	kfree(wpath);
	return ret;
}

 
static int pci_dev_str_match(struct pci_dev *dev, const char *p,
			     const char **endptr)
{
	int ret;
	int count;
	unsigned short vendor, device, subsystem_vendor, subsystem_device;

	if (strncmp(p, "pci:", 4) == 0) {
		 
		p += 4;
		ret = sscanf(p, "%hx:%hx:%hx:%hx%n", &vendor, &device,
			     &subsystem_vendor, &subsystem_device, &count);
		if (ret != 4) {
			ret = sscanf(p, "%hx:%hx%n", &vendor, &device, &count);
			if (ret != 2)
				return -EINVAL;

			subsystem_vendor = 0;
			subsystem_device = 0;
		}

		p += count;

		if ((!vendor || vendor == dev->vendor) &&
		    (!device || device == dev->device) &&
		    (!subsystem_vendor ||
			    subsystem_vendor == dev->subsystem_vendor) &&
		    (!subsystem_device ||
			    subsystem_device == dev->subsystem_device))
			goto found;
	} else {
		 
		ret = pci_dev_str_match_path(dev, p, &p);
		if (ret < 0)
			return ret;
		else if (ret)
			goto found;
	}

	*endptr = p;
	return 0;

found:
	*endptr = p;
	return 1;
}

static u8 __pci_find_next_cap_ttl(struct pci_bus *bus, unsigned int devfn,
				  u8 pos, int cap, int *ttl)
{
	u8 id;
	u16 ent;

	pci_bus_read_config_byte(bus, devfn, pos, &pos);

	while ((*ttl)--) {
		if (pos < 0x40)
			break;
		pos &= ~3;
		pci_bus_read_config_word(bus, devfn, pos, &ent);

		id = ent & 0xff;
		if (id == 0xff)
			break;
		if (id == cap)
			return pos;
		pos = (ent >> 8);
	}
	return 0;
}

static u8 __pci_find_next_cap(struct pci_bus *bus, unsigned int devfn,
			      u8 pos, int cap)
{
	int ttl = PCI_FIND_CAP_TTL;

	return __pci_find_next_cap_ttl(bus, devfn, pos, cap, &ttl);
}

u8 pci_find_next_capability(struct pci_dev *dev, u8 pos, int cap)
{
	return __pci_find_next_cap(dev->bus, dev->devfn,
				   pos + PCI_CAP_LIST_NEXT, cap);
}
EXPORT_SYMBOL_GPL(pci_find_next_capability);

static u8 __pci_bus_find_cap_start(struct pci_bus *bus,
				    unsigned int devfn, u8 hdr_type)
{
	u16 status;

	pci_bus_read_config_word(bus, devfn, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;

	switch (hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		return PCI_CAPABILITY_LIST;
	case PCI_HEADER_TYPE_CARDBUS:
		return PCI_CB_CAPABILITY_LIST;
	}

	return 0;
}

 
u8 pci_find_capability(struct pci_dev *dev, int cap)
{
	u8 pos;

	pos = __pci_bus_find_cap_start(dev->bus, dev->devfn, dev->hdr_type);
	if (pos)
		pos = __pci_find_next_cap(dev->bus, dev->devfn, pos, cap);

	return pos;
}
EXPORT_SYMBOL(pci_find_capability);

 
u8 pci_bus_find_capability(struct pci_bus *bus, unsigned int devfn, int cap)
{
	u8 hdr_type, pos;

	pci_bus_read_config_byte(bus, devfn, PCI_HEADER_TYPE, &hdr_type);

	pos = __pci_bus_find_cap_start(bus, devfn, hdr_type & 0x7f);
	if (pos)
		pos = __pci_find_next_cap(bus, devfn, pos, cap);

	return pos;
}
EXPORT_SYMBOL(pci_bus_find_capability);

 
u16 pci_find_next_ext_capability(struct pci_dev *dev, u16 start, int cap)
{
	u32 header;
	int ttl;
	u16 pos = PCI_CFG_SPACE_SIZE;

	 
	ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

	if (dev->cfg_size <= PCI_CFG_SPACE_SIZE)
		return 0;

	if (start)
		pos = start;

	if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
		return 0;

	 
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap && pos != start)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < PCI_CFG_SPACE_SIZE)
			break;

		if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
			break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_find_next_ext_capability);

 
u16 pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	return pci_find_next_ext_capability(dev, 0, cap);
}
EXPORT_SYMBOL_GPL(pci_find_ext_capability);

 
u64 pci_get_dsn(struct pci_dev *dev)
{
	u32 dword;
	u64 dsn;
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DSN);
	if (!pos)
		return 0;

	 
	pos += 4;
	pci_read_config_dword(dev, pos, &dword);
	dsn = (u64)dword;
	pci_read_config_dword(dev, pos + 4, &dword);
	dsn |= ((u64)dword) << 32;

	return dsn;
}
EXPORT_SYMBOL_GPL(pci_get_dsn);

static u8 __pci_find_next_ht_cap(struct pci_dev *dev, u8 pos, int ht_cap)
{
	int rc, ttl = PCI_FIND_CAP_TTL;
	u8 cap, mask;

	if (ht_cap == HT_CAPTYPE_SLAVE || ht_cap == HT_CAPTYPE_HOST)
		mask = HT_3BIT_CAP_MASK;
	else
		mask = HT_5BIT_CAP_MASK;

	pos = __pci_find_next_cap_ttl(dev->bus, dev->devfn, pos,
				      PCI_CAP_ID_HT, &ttl);
	while (pos) {
		rc = pci_read_config_byte(dev, pos + 3, &cap);
		if (rc != PCIBIOS_SUCCESSFUL)
			return 0;

		if ((cap & mask) == ht_cap)
			return pos;

		pos = __pci_find_next_cap_ttl(dev->bus, dev->devfn,
					      pos + PCI_CAP_LIST_NEXT,
					      PCI_CAP_ID_HT, &ttl);
	}

	return 0;
}

 
u8 pci_find_next_ht_capability(struct pci_dev *dev, u8 pos, int ht_cap)
{
	return __pci_find_next_ht_cap(dev, pos + PCI_CAP_LIST_NEXT, ht_cap);
}
EXPORT_SYMBOL_GPL(pci_find_next_ht_capability);

 
u8 pci_find_ht_capability(struct pci_dev *dev, int ht_cap)
{
	u8 pos;

	pos = __pci_bus_find_cap_start(dev->bus, dev->devfn, dev->hdr_type);
	if (pos)
		pos = __pci_find_next_ht_cap(dev, pos, ht_cap);

	return pos;
}
EXPORT_SYMBOL_GPL(pci_find_ht_capability);

 
u16 pci_find_vsec_capability(struct pci_dev *dev, u16 vendor, int cap)
{
	u16 vsec = 0;
	u32 header;
	int ret;

	if (vendor != dev->vendor)
		return 0;

	while ((vsec = pci_find_next_ext_capability(dev, vsec,
						     PCI_EXT_CAP_ID_VNDR))) {
		ret = pci_read_config_dword(dev, vsec + PCI_VNDR_HEADER, &header);
		if (ret != PCIBIOS_SUCCESSFUL)
			continue;

		if (PCI_VNDR_HEADER_ID(header) == cap)
			return vsec;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_find_vsec_capability);

 
u16 pci_find_dvsec_capability(struct pci_dev *dev, u16 vendor, u16 dvsec)
{
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_DVSEC);
	if (!pos)
		return 0;

	while (pos) {
		u16 v, id;

		pci_read_config_word(dev, pos + PCI_DVSEC_HEADER1, &v);
		pci_read_config_word(dev, pos + PCI_DVSEC_HEADER2, &id);
		if (vendor == v && dvsec == id)
			return pos;

		pos = pci_find_next_ext_capability(dev, pos, PCI_EXT_CAP_ID_DVSEC);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_find_dvsec_capability);

 
struct resource *pci_find_parent_resource(const struct pci_dev *dev,
					  struct resource *res)
{
	const struct pci_bus *bus = dev->bus;
	struct resource *r;

	pci_bus_for_each_resource(bus, r) {
		if (!r)
			continue;
		if (resource_contains(r, res)) {

			 
			if (r->flags & IORESOURCE_PREFETCH &&
			    !(res->flags & IORESOURCE_PREFETCH))
				return NULL;

			 
			return r;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(pci_find_parent_resource);

 
struct resource *pci_find_resource(struct pci_dev *dev, struct resource *res)
{
	int i;

	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		struct resource *r = &dev->resource[i];

		if (r->start && resource_contains(r, res))
			return r;
	}

	return NULL;
}
EXPORT_SYMBOL(pci_find_resource);

 
int pci_wait_for_pending(struct pci_dev *dev, int pos, u16 mask)
{
	int i;

	 
	for (i = 0; i < 4; i++) {
		u16 status;
		if (i)
			msleep((1 << (i - 1)) * 100);

		pci_read_config_word(dev, pos, &status);
		if (!(status & mask))
			return 1;
	}

	return 0;
}

static int pci_acs_enable;

 
void pci_request_acs(void)
{
	pci_acs_enable = 1;
}

static const char *disable_acs_redir_param;

 
static void pci_disable_acs_redir(struct pci_dev *dev)
{
	int ret = 0;
	const char *p;
	int pos;
	u16 ctrl;

	if (!disable_acs_redir_param)
		return;

	p = disable_acs_redir_param;
	while (*p) {
		ret = pci_dev_str_match(dev, p, &p);
		if (ret < 0) {
			pr_info_once("PCI: Can't parse disable_acs_redir parameter: %s\n",
				     disable_acs_redir_param);

			break;
		} else if (ret == 1) {
			 
			break;
		}

		if (*p != ';' && *p != ',') {
			 
			break;
		}
		p++;
	}

	if (ret != 1)
		return;

	if (!pci_dev_specific_disable_acs_redir(dev))
		return;

	pos = dev->acs_cap;
	if (!pos) {
		pci_warn(dev, "cannot disable ACS redirect for this hardware as it does not have ACS capabilities\n");
		return;
	}

	pci_read_config_word(dev, pos + PCI_ACS_CTRL, &ctrl);

	 
	ctrl &= ~(PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_EC);

	pci_write_config_word(dev, pos + PCI_ACS_CTRL, ctrl);

	pci_info(dev, "disabled ACS redirect\n");
}

 
static void pci_std_enable_acs(struct pci_dev *dev)
{
	int pos;
	u16 cap;
	u16 ctrl;

	pos = dev->acs_cap;
	if (!pos)
		return;

	pci_read_config_word(dev, pos + PCI_ACS_CAP, &cap);
	pci_read_config_word(dev, pos + PCI_ACS_CTRL, &ctrl);

	 
	ctrl |= (cap & PCI_ACS_SV);

	 
	ctrl |= (cap & PCI_ACS_RR);

	 
	ctrl |= (cap & PCI_ACS_CR);

	 
	ctrl |= (cap & PCI_ACS_UF);

	 
	if (pci_ats_disabled() || dev->external_facing || dev->untrusted)
		ctrl |= (cap & PCI_ACS_TB);

	pci_write_config_word(dev, pos + PCI_ACS_CTRL, ctrl);
}

 
static void pci_enable_acs(struct pci_dev *dev)
{
	if (!pci_acs_enable)
		goto disable_acs_redir;

	if (!pci_dev_specific_enable_acs(dev))
		goto disable_acs_redir;

	pci_std_enable_acs(dev);

disable_acs_redir:
	 
	pci_disable_acs_redir(dev);
}

 
static void pci_restore_bars(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_BRIDGE_RESOURCES; i++)
		pci_update_resource(dev, i);
}

static inline bool platform_pci_power_manageable(struct pci_dev *dev)
{
	if (pci_use_mid_pm())
		return true;

	return acpi_pci_power_manageable(dev);
}

static inline int platform_pci_set_power_state(struct pci_dev *dev,
					       pci_power_t t)
{
	if (pci_use_mid_pm())
		return mid_pci_set_power_state(dev, t);

	return acpi_pci_set_power_state(dev, t);
}

static inline pci_power_t platform_pci_get_power_state(struct pci_dev *dev)
{
	if (pci_use_mid_pm())
		return mid_pci_get_power_state(dev);

	return acpi_pci_get_power_state(dev);
}

static inline void platform_pci_refresh_power_state(struct pci_dev *dev)
{
	if (!pci_use_mid_pm())
		acpi_pci_refresh_power_state(dev);
}

static inline pci_power_t platform_pci_choose_state(struct pci_dev *dev)
{
	if (pci_use_mid_pm())
		return PCI_POWER_ERROR;

	return acpi_pci_choose_state(dev);
}

static inline int platform_pci_set_wakeup(struct pci_dev *dev, bool enable)
{
	if (pci_use_mid_pm())
		return PCI_POWER_ERROR;

	return acpi_pci_wakeup(dev, enable);
}

static inline bool platform_pci_need_resume(struct pci_dev *dev)
{
	if (pci_use_mid_pm())
		return false;

	return acpi_pci_need_resume(dev);
}

static inline bool platform_pci_bridge_d3(struct pci_dev *dev)
{
	if (pci_use_mid_pm())
		return false;

	return acpi_pci_bridge_d3(dev);
}

 
void pci_update_current_state(struct pci_dev *dev, pci_power_t state)
{
	if (platform_pci_get_power_state(dev) == PCI_D3cold) {
		dev->current_state = PCI_D3cold;
	} else if (dev->pm_cap) {
		u16 pmcsr;

		pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
		if (PCI_POSSIBLE_ERROR(pmcsr)) {
			dev->current_state = PCI_D3cold;
			return;
		}
		dev->current_state = pmcsr & PCI_PM_CTRL_STATE_MASK;
	} else {
		dev->current_state = state;
	}
}

 
void pci_refresh_power_state(struct pci_dev *dev)
{
	platform_pci_refresh_power_state(dev);
	pci_update_current_state(dev, dev->current_state);
}

 
int pci_platform_power_transition(struct pci_dev *dev, pci_power_t state)
{
	int error;

	error = platform_pci_set_power_state(dev, state);
	if (!error)
		pci_update_current_state(dev, state);
	else if (!dev->pm_cap)  
		dev->current_state = PCI_D0;

	return error;
}
EXPORT_SYMBOL_GPL(pci_platform_power_transition);

static int pci_resume_one(struct pci_dev *pci_dev, void *ign)
{
	pm_request_resume(&pci_dev->dev);
	return 0;
}

 
void pci_resume_bus(struct pci_bus *bus)
{
	if (bus)
		pci_walk_bus(bus, pci_resume_one, NULL);
}

static int pci_dev_wait(struct pci_dev *dev, char *reset_type, int timeout)
{
	int delay = 1;
	bool retrain = false;
	struct pci_dev *bridge;

	if (pci_is_pcie(dev)) {
		bridge = pci_upstream_bridge(dev);
		if (bridge)
			retrain = true;
	}

	 
	for (;;) {
		u32 id;

		pci_read_config_dword(dev, PCI_COMMAND, &id);
		if (!PCI_POSSIBLE_ERROR(id))
			break;

		if (delay > timeout) {
			pci_warn(dev, "not ready %dms after %s; giving up\n",
				 delay - 1, reset_type);
			return -ENOTTY;
		}

		if (delay > PCI_RESET_WAIT) {
			if (retrain) {
				retrain = false;
				if (pcie_failed_link_retrain(bridge)) {
					delay = 1;
					continue;
				}
			}
			pci_info(dev, "not ready %dms after %s; waiting\n",
				 delay - 1, reset_type);
		}

		msleep(delay);
		delay *= 2;
	}

	if (delay > PCI_RESET_WAIT)
		pci_info(dev, "ready %dms after %s\n", delay - 1,
			 reset_type);

	return 0;
}

 
int pci_power_up(struct pci_dev *dev)
{
	bool need_restore;
	pci_power_t state;
	u16 pmcsr;

	platform_pci_set_power_state(dev, PCI_D0);

	if (!dev->pm_cap) {
		state = platform_pci_get_power_state(dev);
		if (state == PCI_UNKNOWN)
			dev->current_state = PCI_D0;
		else
			dev->current_state = state;

		return -EIO;
	}

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	if (PCI_POSSIBLE_ERROR(pmcsr)) {
		pci_err(dev, "Unable to change power state from %s to D0, device inaccessible\n",
			pci_power_name(dev->current_state));
		dev->current_state = PCI_D3cold;
		return -EIO;
	}

	state = pmcsr & PCI_PM_CTRL_STATE_MASK;

	need_restore = (state == PCI_D3hot || dev->current_state >= PCI_D3hot) &&
			!(pmcsr & PCI_PM_CTRL_NO_SOFT_RESET);

	if (state == PCI_D0)
		goto end;

	 
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, 0);

	 
	if (state == PCI_D3hot)
		pci_dev_d3_sleep(dev);
	else if (state == PCI_D2)
		udelay(PCI_PM_D2_DELAY);

end:
	dev->current_state = PCI_D0;
	if (need_restore)
		return 1;

	return 0;
}

 
static int pci_set_full_power_state(struct pci_dev *dev)
{
	u16 pmcsr;
	int ret;

	ret = pci_power_up(dev);
	if (ret < 0) {
		if (dev->current_state == PCI_D0)
			return 0;

		return ret;
	}

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	dev->current_state = pmcsr & PCI_PM_CTRL_STATE_MASK;
	if (dev->current_state != PCI_D0) {
		pci_info_ratelimited(dev, "Refused to change power state from %s to D0\n",
				     pci_power_name(dev->current_state));
	} else if (ret > 0) {
		 
		pci_restore_bars(dev);
	}

	if (dev->bus->self)
		pcie_aspm_pm_state_change(dev->bus->self);

	return 0;
}

 
static int __pci_dev_set_current_state(struct pci_dev *dev, void *data)
{
	pci_power_t state = *(pci_power_t *)data;

	dev->current_state = state;
	return 0;
}

 
void pci_bus_set_current_state(struct pci_bus *bus, pci_power_t state)
{
	if (bus)
		pci_walk_bus(bus, __pci_dev_set_current_state, &state);
}

 
static int pci_set_low_power_state(struct pci_dev *dev, pci_power_t state)
{
	u16 pmcsr;

	if (!dev->pm_cap)
		return -EIO;

	 
	if (dev->current_state <= PCI_D3cold && dev->current_state > state) {
		pci_dbg(dev, "Invalid power transition (from %s to %s)\n",
			pci_power_name(dev->current_state),
			pci_power_name(state));
		return -EINVAL;
	}

	 
	if ((state == PCI_D1 && !dev->d1_support)
	   || (state == PCI_D2 && !dev->d2_support))
		return -EIO;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	if (PCI_POSSIBLE_ERROR(pmcsr)) {
		pci_err(dev, "Unable to change power state from %s to %s, device inaccessible\n",
			pci_power_name(dev->current_state),
			pci_power_name(state));
		dev->current_state = PCI_D3cold;
		return -EIO;
	}

	pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
	pmcsr |= state;

	 
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, pmcsr);

	 
	if (state == PCI_D3hot)
		pci_dev_d3_sleep(dev);
	else if (state == PCI_D2)
		udelay(PCI_PM_D2_DELAY);

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	dev->current_state = pmcsr & PCI_PM_CTRL_STATE_MASK;
	if (dev->current_state != state)
		pci_info_ratelimited(dev, "Refused to change power state from %s to %s\n",
				     pci_power_name(dev->current_state),
				     pci_power_name(state));

	if (dev->bus->self)
		pcie_aspm_pm_state_change(dev->bus->self);

	return 0;
}

 
int pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	int error;

	 
	if (state > PCI_D3cold)
		state = PCI_D3cold;
	else if (state < PCI_D0)
		state = PCI_D0;
	else if ((state == PCI_D1 || state == PCI_D2) && pci_no_d1d2(dev))

		 
		return 0;

	 
	if (dev->current_state == state)
		return 0;

	if (state == PCI_D0)
		return pci_set_full_power_state(dev);

	 
	if (state >= PCI_D3hot && (dev->dev_flags & PCI_DEV_FLAGS_NO_D3))
		return 0;

	if (state == PCI_D3cold) {
		 
		error = pci_set_low_power_state(dev, PCI_D3hot);

		if (pci_platform_power_transition(dev, PCI_D3cold))
			return error;

		 
		if (dev->current_state == PCI_D3cold)
			pci_bus_set_current_state(dev->subordinate, PCI_D3cold);
	} else {
		error = pci_set_low_power_state(dev, state);

		if (pci_platform_power_transition(dev, state))
			return error;
	}

	return 0;
}
EXPORT_SYMBOL(pci_set_power_state);

#define PCI_EXP_SAVE_REGS	7

static struct pci_cap_saved_state *_pci_find_saved_cap(struct pci_dev *pci_dev,
						       u16 cap, bool extended)
{
	struct pci_cap_saved_state *tmp;

	hlist_for_each_entry(tmp, &pci_dev->saved_cap_space, next) {
		if (tmp->cap.cap_extended == extended && tmp->cap.cap_nr == cap)
			return tmp;
	}
	return NULL;
}

struct pci_cap_saved_state *pci_find_saved_cap(struct pci_dev *dev, char cap)
{
	return _pci_find_saved_cap(dev, cap, false);
}

struct pci_cap_saved_state *pci_find_saved_ext_cap(struct pci_dev *dev, u16 cap)
{
	return _pci_find_saved_cap(dev, cap, true);
}

static int pci_save_pcie_state(struct pci_dev *dev)
{
	int i = 0;
	struct pci_cap_saved_state *save_state;
	u16 *cap;

	if (!pci_is_pcie(dev))
		return 0;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_EXP);
	if (!save_state) {
		pci_err(dev, "buffer not found in %s\n", __func__);
		return -ENOMEM;
	}

	cap = (u16 *)&save_state->cap.data[0];
	pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_LNKCTL, &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_SLTCTL, &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_RTCTL,  &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_DEVCTL2, &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_LNKCTL2, &cap[i++]);
	pcie_capability_read_word(dev, PCI_EXP_SLTCTL2, &cap[i++]);

	return 0;
}

void pci_bridge_reconfigure_ltr(struct pci_dev *dev)
{
#ifdef CONFIG_PCIEASPM
	struct pci_dev *bridge;
	u32 ctl;

	bridge = pci_upstream_bridge(dev);
	if (bridge && bridge->ltr_path) {
		pcie_capability_read_dword(bridge, PCI_EXP_DEVCTL2, &ctl);
		if (!(ctl & PCI_EXP_DEVCTL2_LTR_EN)) {
			pci_dbg(bridge, "re-enabling LTR\n");
			pcie_capability_set_word(bridge, PCI_EXP_DEVCTL2,
						 PCI_EXP_DEVCTL2_LTR_EN);
		}
	}
#endif
}

static void pci_restore_pcie_state(struct pci_dev *dev)
{
	int i = 0;
	struct pci_cap_saved_state *save_state;
	u16 *cap;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_EXP);
	if (!save_state)
		return;

	 
	pci_bridge_reconfigure_ltr(dev);

	cap = (u16 *)&save_state->cap.data[0];
	pcie_capability_write_word(dev, PCI_EXP_DEVCTL, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_LNKCTL, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_SLTCTL, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_RTCTL, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_DEVCTL2, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_LNKCTL2, cap[i++]);
	pcie_capability_write_word(dev, PCI_EXP_SLTCTL2, cap[i++]);
}

static int pci_save_pcix_state(struct pci_dev *dev)
{
	int pos;
	struct pci_cap_saved_state *save_state;

	pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!pos)
		return 0;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_PCIX);
	if (!save_state) {
		pci_err(dev, "buffer not found in %s\n", __func__);
		return -ENOMEM;
	}

	pci_read_config_word(dev, pos + PCI_X_CMD,
			     (u16 *)save_state->cap.data);

	return 0;
}

static void pci_restore_pcix_state(struct pci_dev *dev)
{
	int i = 0, pos;
	struct pci_cap_saved_state *save_state;
	u16 *cap;

	save_state = pci_find_saved_cap(dev, PCI_CAP_ID_PCIX);
	pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!save_state || !pos)
		return;
	cap = (u16 *)&save_state->cap.data[0];

	pci_write_config_word(dev, pos + PCI_X_CMD, cap[i++]);
}

static void pci_save_ltr_state(struct pci_dev *dev)
{
	int ltr;
	struct pci_cap_saved_state *save_state;
	u32 *cap;

	if (!pci_is_pcie(dev))
		return;

	ltr = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_LTR);
	if (!ltr)
		return;

	save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_LTR);
	if (!save_state) {
		pci_err(dev, "no suspend buffer for LTR; ASPM issues possible after resume\n");
		return;
	}

	 
	cap = &save_state->cap.data[0];
	pci_read_config_dword(dev, ltr + PCI_LTR_MAX_SNOOP_LAT, cap);
}

static void pci_restore_ltr_state(struct pci_dev *dev)
{
	struct pci_cap_saved_state *save_state;
	int ltr;
	u32 *cap;

	save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_LTR);
	ltr = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_LTR);
	if (!save_state || !ltr)
		return;

	 
	cap = &save_state->cap.data[0];
	pci_write_config_dword(dev, ltr + PCI_LTR_MAX_SNOOP_LAT, *cap);
}

 
int pci_save_state(struct pci_dev *dev)
{
	int i;
	 
	for (i = 0; i < 16; i++) {
		pci_read_config_dword(dev, i * 4, &dev->saved_config_space[i]);
		pci_dbg(dev, "save config %#04x: %#010x\n",
			i * 4, dev->saved_config_space[i]);
	}
	dev->state_saved = true;

	i = pci_save_pcie_state(dev);
	if (i != 0)
		return i;

	i = pci_save_pcix_state(dev);
	if (i != 0)
		return i;

	pci_save_ltr_state(dev);
	pci_save_dpc_state(dev);
	pci_save_aer_state(dev);
	pci_save_ptm_state(dev);
	return pci_save_vc_state(dev);
}
EXPORT_SYMBOL(pci_save_state);

static void pci_restore_config_dword(struct pci_dev *pdev, int offset,
				     u32 saved_val, int retry, bool force)
{
	u32 val;

	pci_read_config_dword(pdev, offset, &val);
	if (!force && val == saved_val)
		return;

	for (;;) {
		pci_dbg(pdev, "restore config %#04x: %#010x -> %#010x\n",
			offset, val, saved_val);
		pci_write_config_dword(pdev, offset, saved_val);
		if (retry-- <= 0)
			return;

		pci_read_config_dword(pdev, offset, &val);
		if (val == saved_val)
			return;

		mdelay(1);
	}
}

static void pci_restore_config_space_range(struct pci_dev *pdev,
					   int start, int end, int retry,
					   bool force)
{
	int index;

	for (index = end; index >= start; index--)
		pci_restore_config_dword(pdev, 4 * index,
					 pdev->saved_config_space[index],
					 retry, force);
}

static void pci_restore_config_space(struct pci_dev *pdev)
{
	if (pdev->hdr_type == PCI_HEADER_TYPE_NORMAL) {
		pci_restore_config_space_range(pdev, 10, 15, 0, false);
		 
		pci_restore_config_space_range(pdev, 4, 9, 10, false);
		pci_restore_config_space_range(pdev, 0, 3, 0, false);
	} else if (pdev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
		pci_restore_config_space_range(pdev, 12, 15, 0, false);

		 
		pci_restore_config_space_range(pdev, 9, 11, 0, true);
		pci_restore_config_space_range(pdev, 0, 8, 0, false);
	} else {
		pci_restore_config_space_range(pdev, 0, 15, 0, false);
	}
}

static void pci_restore_rebar_state(struct pci_dev *pdev)
{
	unsigned int pos, nbars, i;
	u32 ctrl;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_REBAR);
	if (!pos)
		return;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
	nbars = (ctrl & PCI_REBAR_CTRL_NBAR_MASK) >>
		    PCI_REBAR_CTRL_NBAR_SHIFT;

	for (i = 0; i < nbars; i++, pos += 8) {
		struct resource *res;
		int bar_idx, size;

		pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
		bar_idx = ctrl & PCI_REBAR_CTRL_BAR_IDX;
		res = pdev->resource + bar_idx;
		size = pci_rebar_bytes_to_size(resource_size(res));
		ctrl &= ~PCI_REBAR_CTRL_BAR_SIZE;
		ctrl |= size << PCI_REBAR_CTRL_BAR_SHIFT;
		pci_write_config_dword(pdev, pos + PCI_REBAR_CTRL, ctrl);
	}
}

 
void pci_restore_state(struct pci_dev *dev)
{
	if (!dev->state_saved)
		return;

	 
	pci_restore_ltr_state(dev);

	pci_restore_pcie_state(dev);
	pci_restore_pasid_state(dev);
	pci_restore_pri_state(dev);
	pci_restore_ats_state(dev);
	pci_restore_vc_state(dev);
	pci_restore_rebar_state(dev);
	pci_restore_dpc_state(dev);
	pci_restore_ptm_state(dev);

	pci_aer_clear_status(dev);
	pci_restore_aer_state(dev);

	pci_restore_config_space(dev);

	pci_restore_pcix_state(dev);
	pci_restore_msi_state(dev);

	 
	pci_enable_acs(dev);
	pci_restore_iov_state(dev);

	dev->state_saved = false;
}
EXPORT_SYMBOL(pci_restore_state);

struct pci_saved_state {
	u32 config_space[16];
	struct pci_cap_saved_data cap[];
};

 
struct pci_saved_state *pci_store_saved_state(struct pci_dev *dev)
{
	struct pci_saved_state *state;
	struct pci_cap_saved_state *tmp;
	struct pci_cap_saved_data *cap;
	size_t size;

	if (!dev->state_saved)
		return NULL;

	size = sizeof(*state) + sizeof(struct pci_cap_saved_data);

	hlist_for_each_entry(tmp, &dev->saved_cap_space, next)
		size += sizeof(struct pci_cap_saved_data) + tmp->cap.size;

	state = kzalloc(size, GFP_KERNEL);
	if (!state)
		return NULL;

	memcpy(state->config_space, dev->saved_config_space,
	       sizeof(state->config_space));

	cap = state->cap;
	hlist_for_each_entry(tmp, &dev->saved_cap_space, next) {
		size_t len = sizeof(struct pci_cap_saved_data) + tmp->cap.size;
		memcpy(cap, &tmp->cap, len);
		cap = (struct pci_cap_saved_data *)((u8 *)cap + len);
	}
	 

	return state;
}
EXPORT_SYMBOL_GPL(pci_store_saved_state);

 
int pci_load_saved_state(struct pci_dev *dev,
			 struct pci_saved_state *state)
{
	struct pci_cap_saved_data *cap;

	dev->state_saved = false;

	if (!state)
		return 0;

	memcpy(dev->saved_config_space, state->config_space,
	       sizeof(state->config_space));

	cap = state->cap;
	while (cap->size) {
		struct pci_cap_saved_state *tmp;

		tmp = _pci_find_saved_cap(dev, cap->cap_nr, cap->cap_extended);
		if (!tmp || tmp->cap.size != cap->size)
			return -EINVAL;

		memcpy(tmp->cap.data, cap->data, tmp->cap.size);
		cap = (struct pci_cap_saved_data *)((u8 *)cap +
		       sizeof(struct pci_cap_saved_data) + cap->size);
	}

	dev->state_saved = true;
	return 0;
}
EXPORT_SYMBOL_GPL(pci_load_saved_state);

 
int pci_load_and_free_saved_state(struct pci_dev *dev,
				  struct pci_saved_state **state)
{
	int ret = pci_load_saved_state(dev, *state);
	kfree(*state);
	*state = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(pci_load_and_free_saved_state);

int __weak pcibios_enable_device(struct pci_dev *dev, int bars)
{
	return pci_enable_resources(dev, bars);
}

static int do_pci_enable_device(struct pci_dev *dev, int bars)
{
	int err;
	struct pci_dev *bridge;
	u16 cmd;
	u8 pin;

	err = pci_set_power_state(dev, PCI_D0);
	if (err < 0 && err != -EIO)
		return err;

	bridge = pci_upstream_bridge(dev);
	if (bridge)
		pcie_aspm_powersave_config_link(bridge);

	err = pcibios_enable_device(dev, bars);
	if (err < 0)
		return err;
	pci_fixup_device(pci_fixup_enable, dev);

	if (dev->msi_enabled || dev->msix_enabled)
		return 0;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (pin) {
		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		if (cmd & PCI_COMMAND_INTX_DISABLE)
			pci_write_config_word(dev, PCI_COMMAND,
					      cmd & ~PCI_COMMAND_INTX_DISABLE);
	}

	return 0;
}

 
int pci_reenable_device(struct pci_dev *dev)
{
	if (pci_is_enabled(dev))
		return do_pci_enable_device(dev, (1 << PCI_NUM_RESOURCES) - 1);
	return 0;
}
EXPORT_SYMBOL(pci_reenable_device);

static void pci_enable_bridge(struct pci_dev *dev)
{
	struct pci_dev *bridge;
	int retval;

	bridge = pci_upstream_bridge(dev);
	if (bridge)
		pci_enable_bridge(bridge);

	if (pci_is_enabled(dev)) {
		if (!dev->is_busmaster)
			pci_set_master(dev);
		return;
	}

	retval = pci_enable_device(dev);
	if (retval)
		pci_err(dev, "Error enabling bridge (%d), continuing\n",
			retval);
	pci_set_master(dev);
}

static int pci_enable_device_flags(struct pci_dev *dev, unsigned long flags)
{
	struct pci_dev *bridge;
	int err;
	int i, bars = 0;

	 
	pci_update_current_state(dev, dev->current_state);

	if (atomic_inc_return(&dev->enable_cnt) > 1)
		return 0;		 

	bridge = pci_upstream_bridge(dev);
	if (bridge)
		pci_enable_bridge(bridge);

	 
	for (i = 0; i <= PCI_ROM_RESOURCE; i++)
		if (dev->resource[i].flags & flags)
			bars |= (1 << i);
	for (i = PCI_BRIDGE_RESOURCES; i < DEVICE_COUNT_RESOURCE; i++)
		if (dev->resource[i].flags & flags)
			bars |= (1 << i);

	err = do_pci_enable_device(dev, bars);
	if (err < 0)
		atomic_dec(&dev->enable_cnt);
	return err;
}

 
int pci_enable_device_io(struct pci_dev *dev)
{
	return pci_enable_device_flags(dev, IORESOURCE_IO);
}
EXPORT_SYMBOL(pci_enable_device_io);

 
int pci_enable_device_mem(struct pci_dev *dev)
{
	return pci_enable_device_flags(dev, IORESOURCE_MEM);
}
EXPORT_SYMBOL(pci_enable_device_mem);

 
int pci_enable_device(struct pci_dev *dev)
{
	return pci_enable_device_flags(dev, IORESOURCE_MEM | IORESOURCE_IO);
}
EXPORT_SYMBOL(pci_enable_device);

 
struct pci_devres {
	unsigned int enabled:1;
	unsigned int pinned:1;
	unsigned int orig_intx:1;
	unsigned int restore_intx:1;
	unsigned int mwi:1;
	u32 region_mask;
};

static void pcim_release(struct device *gendev, void *res)
{
	struct pci_dev *dev = to_pci_dev(gendev);
	struct pci_devres *this = res;
	int i;

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++)
		if (this->region_mask & (1 << i))
			pci_release_region(dev, i);

	if (this->mwi)
		pci_clear_mwi(dev);

	if (this->restore_intx)
		pci_intx(dev, this->orig_intx);

	if (this->enabled && !this->pinned)
		pci_disable_device(dev);
}

static struct pci_devres *get_pci_dr(struct pci_dev *pdev)
{
	struct pci_devres *dr, *new_dr;

	dr = devres_find(&pdev->dev, pcim_release, NULL, NULL);
	if (dr)
		return dr;

	new_dr = devres_alloc(pcim_release, sizeof(*new_dr), GFP_KERNEL);
	if (!new_dr)
		return NULL;
	return devres_get(&pdev->dev, new_dr, NULL, NULL);
}

static struct pci_devres *find_pci_dr(struct pci_dev *pdev)
{
	if (pci_is_managed(pdev))
		return devres_find(&pdev->dev, pcim_release, NULL, NULL);
	return NULL;
}

 
int pcim_enable_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;
	int rc;

	dr = get_pci_dr(pdev);
	if (unlikely(!dr))
		return -ENOMEM;
	if (dr->enabled)
		return 0;

	rc = pci_enable_device(pdev);
	if (!rc) {
		pdev->is_managed = 1;
		dr->enabled = 1;
	}
	return rc;
}
EXPORT_SYMBOL(pcim_enable_device);

 
void pcim_pin_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;

	dr = find_pci_dr(pdev);
	WARN_ON(!dr || !dr->enabled);
	if (dr)
		dr->pinned = 1;
}
EXPORT_SYMBOL(pcim_pin_device);

 
int __weak pcibios_device_add(struct pci_dev *dev)
{
	return 0;
}

 
void __weak pcibios_release_device(struct pci_dev *dev) {}

 
void __weak pcibios_disable_device(struct pci_dev *dev) {}

 
void __weak pcibios_penalize_isa_irq(int irq, int active) {}

static void do_pci_disable_device(struct pci_dev *dev)
{
	u16 pci_command;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	if (pci_command & PCI_COMMAND_MASTER) {
		pci_command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	}

	pcibios_disable_device(dev);
}

 
void pci_disable_enabled_device(struct pci_dev *dev)
{
	if (pci_is_enabled(dev))
		do_pci_disable_device(dev);
}

 
void pci_disable_device(struct pci_dev *dev)
{
	struct pci_devres *dr;

	dr = find_pci_dr(dev);
	if (dr)
		dr->enabled = 0;

	dev_WARN_ONCE(&dev->dev, atomic_read(&dev->enable_cnt) <= 0,
		      "disabling already-disabled device");

	if (atomic_dec_return(&dev->enable_cnt) != 0)
		return;

	do_pci_disable_device(dev);

	dev->is_busmaster = 0;
}
EXPORT_SYMBOL(pci_disable_device);

 
int __weak pcibios_set_pcie_reset_state(struct pci_dev *dev,
					enum pcie_reset_state state)
{
	return -EINVAL;
}

 
int pci_set_pcie_reset_state(struct pci_dev *dev, enum pcie_reset_state state)
{
	return pcibios_set_pcie_reset_state(dev, state);
}
EXPORT_SYMBOL_GPL(pci_set_pcie_reset_state);

#ifdef CONFIG_PCIEAER
void pcie_clear_device_status(struct pci_dev *dev)
{
	u16 sta;

	pcie_capability_read_word(dev, PCI_EXP_DEVSTA, &sta);
	pcie_capability_write_word(dev, PCI_EXP_DEVSTA, sta);
}
#endif

 
void pcie_clear_root_pme_status(struct pci_dev *dev)
{
	pcie_capability_set_dword(dev, PCI_EXP_RTSTA, PCI_EXP_RTSTA_PME);
}

 
bool pci_check_pme_status(struct pci_dev *dev)
{
	int pmcsr_pos;
	u16 pmcsr;
	bool ret = false;

	if (!dev->pm_cap)
		return false;

	pmcsr_pos = dev->pm_cap + PCI_PM_CTRL;
	pci_read_config_word(dev, pmcsr_pos, &pmcsr);
	if (!(pmcsr & PCI_PM_CTRL_PME_STATUS))
		return false;

	 
	pmcsr |= PCI_PM_CTRL_PME_STATUS;
	if (pmcsr & PCI_PM_CTRL_PME_ENABLE) {
		 
		pmcsr &= ~PCI_PM_CTRL_PME_ENABLE;
		ret = true;
	}

	pci_write_config_word(dev, pmcsr_pos, pmcsr);

	return ret;
}

 
static int pci_pme_wakeup(struct pci_dev *dev, void *pme_poll_reset)
{
	if (pme_poll_reset && dev->pme_poll)
		dev->pme_poll = false;

	if (pci_check_pme_status(dev)) {
		pci_wakeup_event(dev);
		pm_request_resume(&dev->dev);
	}
	return 0;
}

 
void pci_pme_wakeup_bus(struct pci_bus *bus)
{
	if (bus)
		pci_walk_bus(bus, pci_pme_wakeup, (void *)true);
}


 
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state)
{
	if (!dev->pm_cap)
		return false;

	return !!(dev->pme_support & (1 << state));
}
EXPORT_SYMBOL(pci_pme_capable);

static void pci_pme_list_scan(struct work_struct *work)
{
	struct pci_pme_device *pme_dev, *n;

	mutex_lock(&pci_pme_list_mutex);
	list_for_each_entry_safe(pme_dev, n, &pci_pme_list, list) {
		struct pci_dev *pdev = pme_dev->dev;

		if (pdev->pme_poll) {
			struct pci_dev *bridge = pdev->bus->self;
			struct device *dev = &pdev->dev;
			int pm_status;

			 
			if (bridge && bridge->current_state != PCI_D0)
				continue;

			 
			pm_status = pm_runtime_get_if_active(dev, true);
			if (!pm_status)
				continue;

			if (pdev->current_state != PCI_D3cold)
				pci_pme_wakeup(pdev, NULL);

			if (pm_status > 0)
				pm_runtime_put(dev);
		} else {
			list_del(&pme_dev->list);
			kfree(pme_dev);
		}
	}
	if (!list_empty(&pci_pme_list))
		queue_delayed_work(system_freezable_wq, &pci_pme_work,
				   msecs_to_jiffies(PME_TIMEOUT));
	mutex_unlock(&pci_pme_list_mutex);
}

static void __pci_pme_active(struct pci_dev *dev, bool enable)
{
	u16 pmcsr;

	if (!dev->pme_support)
		return;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	 
	pmcsr |= PCI_PM_CTRL_PME_STATUS | PCI_PM_CTRL_PME_ENABLE;
	if (!enable)
		pmcsr &= ~PCI_PM_CTRL_PME_ENABLE;

	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, pmcsr);
}

 
void pci_pme_restore(struct pci_dev *dev)
{
	u16 pmcsr;

	if (!dev->pme_support)
		return;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &pmcsr);
	if (dev->wakeup_prepared) {
		pmcsr |= PCI_PM_CTRL_PME_ENABLE;
		pmcsr &= ~PCI_PM_CTRL_PME_STATUS;
	} else {
		pmcsr &= ~PCI_PM_CTRL_PME_ENABLE;
		pmcsr |= PCI_PM_CTRL_PME_STATUS;
	}
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, pmcsr);
}

 
void pci_pme_active(struct pci_dev *dev, bool enable)
{
	__pci_pme_active(dev, enable);

	 

	if (dev->pme_poll) {
		struct pci_pme_device *pme_dev;
		if (enable) {
			pme_dev = kmalloc(sizeof(struct pci_pme_device),
					  GFP_KERNEL);
			if (!pme_dev) {
				pci_warn(dev, "can't enable PME#\n");
				return;
			}
			pme_dev->dev = dev;
			mutex_lock(&pci_pme_list_mutex);
			list_add(&pme_dev->list, &pci_pme_list);
			if (list_is_singular(&pci_pme_list))
				queue_delayed_work(system_freezable_wq,
						   &pci_pme_work,
						   msecs_to_jiffies(PME_TIMEOUT));
			mutex_unlock(&pci_pme_list_mutex);
		} else {
			mutex_lock(&pci_pme_list_mutex);
			list_for_each_entry(pme_dev, &pci_pme_list, list) {
				if (pme_dev->dev == dev) {
					list_del(&pme_dev->list);
					kfree(pme_dev);
					break;
				}
			}
			mutex_unlock(&pci_pme_list_mutex);
		}
	}

	pci_dbg(dev, "PME# %s\n", enable ? "enabled" : "disabled");
}
EXPORT_SYMBOL(pci_pme_active);

 
static int __pci_enable_wake(struct pci_dev *dev, pci_power_t state, bool enable)
{
	int ret = 0;

	 
	if (!pci_power_manageable(dev))
		return 0;

	 
	if (!!enable == !!dev->wakeup_prepared)
		return 0;

	 

	if (enable) {
		int error;

		 
		if (pci_pme_capable(dev, state) || pci_pme_capable(dev, PCI_D3cold))
			pci_pme_active(dev, true);
		else
			ret = 1;
		error = platform_pci_set_wakeup(dev, true);
		if (ret)
			ret = error;
		if (!ret)
			dev->wakeup_prepared = true;
	} else {
		platform_pci_set_wakeup(dev, false);
		pci_pme_active(dev, false);
		dev->wakeup_prepared = false;
	}

	return ret;
}

 
int pci_enable_wake(struct pci_dev *pci_dev, pci_power_t state, bool enable)
{
	if (enable && !device_may_wakeup(&pci_dev->dev))
		return -EINVAL;

	return __pci_enable_wake(pci_dev, state, enable);
}
EXPORT_SYMBOL(pci_enable_wake);

 
int pci_wake_from_d3(struct pci_dev *dev, bool enable)
{
	return pci_pme_capable(dev, PCI_D3cold) ?
			pci_enable_wake(dev, PCI_D3cold, enable) :
			pci_enable_wake(dev, PCI_D3hot, enable);
}
EXPORT_SYMBOL(pci_wake_from_d3);

 
static pci_power_t pci_target_state(struct pci_dev *dev, bool wakeup)
{
	if (platform_pci_power_manageable(dev)) {
		 
		pci_power_t state = platform_pci_choose_state(dev);

		switch (state) {
		case PCI_POWER_ERROR:
		case PCI_UNKNOWN:
			return PCI_D3hot;

		case PCI_D1:
		case PCI_D2:
			if (pci_no_d1d2(dev))
				return PCI_D3hot;
		}

		return state;
	}

	 
	if (dev->current_state == PCI_D3cold)
		return PCI_D3cold;
	else if (!dev->pm_cap)
		return PCI_D0;

	if (wakeup && dev->pme_support) {
		pci_power_t state = PCI_D3hot;

		 
		while (state && !(dev->pme_support & (1 << state)))
			state--;

		if (state)
			return state;
		else if (dev->pme_support & 1)
			return PCI_D0;
	}

	return PCI_D3hot;
}

 
int pci_prepare_to_sleep(struct pci_dev *dev)
{
	bool wakeup = device_may_wakeup(&dev->dev);
	pci_power_t target_state = pci_target_state(dev, wakeup);
	int error;

	if (target_state == PCI_POWER_ERROR)
		return -EIO;

	pci_enable_wake(dev, target_state, wakeup);

	error = pci_set_power_state(dev, target_state);

	if (error)
		pci_enable_wake(dev, target_state, false);

	return error;
}
EXPORT_SYMBOL(pci_prepare_to_sleep);

 
int pci_back_from_sleep(struct pci_dev *dev)
{
	int ret = pci_set_power_state(dev, PCI_D0);

	if (ret)
		return ret;

	pci_enable_wake(dev, PCI_D0, false);
	return 0;
}
EXPORT_SYMBOL(pci_back_from_sleep);

 
int pci_finish_runtime_suspend(struct pci_dev *dev)
{
	pci_power_t target_state;
	int error;

	target_state = pci_target_state(dev, device_can_wakeup(&dev->dev));
	if (target_state == PCI_POWER_ERROR)
		return -EIO;

	__pci_enable_wake(dev, target_state, pci_dev_run_wake(dev));

	error = pci_set_power_state(dev, target_state);

	if (error)
		pci_enable_wake(dev, target_state, false);

	return error;
}

 
bool pci_dev_run_wake(struct pci_dev *dev)
{
	struct pci_bus *bus = dev->bus;

	if (!dev->pme_support)
		return false;

	 
	if (!pci_pme_capable(dev, pci_target_state(dev, true)))
		return false;

	if (device_can_wakeup(&dev->dev))
		return true;

	while (bus->parent) {
		struct pci_dev *bridge = bus->self;

		if (device_can_wakeup(&bridge->dev))
			return true;

		bus = bus->parent;
	}

	 
	if (bus->bridge)
		return device_can_wakeup(bus->bridge);

	return false;
}
EXPORT_SYMBOL_GPL(pci_dev_run_wake);

 
bool pci_dev_need_resume(struct pci_dev *pci_dev)
{
	struct device *dev = &pci_dev->dev;
	pci_power_t target_state;

	if (!pm_runtime_suspended(dev) || platform_pci_need_resume(pci_dev))
		return true;

	target_state = pci_target_state(pci_dev, device_may_wakeup(dev));

	 
	return target_state != pci_dev->current_state &&
		target_state != PCI_D3cold &&
		pci_dev->current_state != PCI_D3hot;
}

 
void pci_dev_adjust_pme(struct pci_dev *pci_dev)
{
	struct device *dev = &pci_dev->dev;

	spin_lock_irq(&dev->power.lock);

	if (pm_runtime_suspended(dev) && !device_may_wakeup(dev) &&
	    pci_dev->current_state < PCI_D3cold)
		__pci_pme_active(pci_dev, false);

	spin_unlock_irq(&dev->power.lock);
}

 
void pci_dev_complete_resume(struct pci_dev *pci_dev)
{
	struct device *dev = &pci_dev->dev;

	if (!pci_dev_run_wake(pci_dev))
		return;

	spin_lock_irq(&dev->power.lock);

	if (pm_runtime_suspended(dev) && pci_dev->current_state < PCI_D3cold)
		__pci_pme_active(pci_dev, true);

	spin_unlock_irq(&dev->power.lock);
}

 
pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
	if (state.event == PM_EVENT_ON)
		return PCI_D0;

	return pci_target_state(dev, false);
}
EXPORT_SYMBOL(pci_choose_state);

void pci_config_pm_runtime_get(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;

	if (parent)
		pm_runtime_get_sync(parent);
	pm_runtime_get_noresume(dev);
	 
	pm_runtime_barrier(dev);
	 
	if (pdev->current_state == PCI_D3cold)
		pm_runtime_resume(dev);
}

void pci_config_pm_runtime_put(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;

	pm_runtime_put(dev);
	if (parent)
		pm_runtime_put_sync(parent);
}

static const struct dmi_system_id bridge_d3_blacklist[] = {
#ifdef CONFIG_X86
	{
		 
		.ident = "X299 DESIGNARE EX-CF",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Gigabyte Technology Co., Ltd."),
			DMI_MATCH(DMI_BOARD_NAME, "X299 DESIGNARE EX-CF"),
		},
	},
	{
		 
		.ident = "Elo Continental Z2",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Elo Touch Solutions"),
			DMI_MATCH(DMI_BOARD_NAME, "Geminilake"),
			DMI_MATCH(DMI_BOARD_VERSION, "Continental Z2"),
		},
	},
#endif
	{ }
};

 
bool pci_bridge_d3_possible(struct pci_dev *bridge)
{
	if (!pci_is_pcie(bridge))
		return false;

	switch (pci_pcie_type(bridge)) {
	case PCI_EXP_TYPE_ROOT_PORT:
	case PCI_EXP_TYPE_UPSTREAM:
	case PCI_EXP_TYPE_DOWNSTREAM:
		if (pci_bridge_d3_disable)
			return false;

		 
		if (bridge->is_hotplug_bridge && !pciehp_is_native(bridge))
			return false;

		if (pci_bridge_d3_force)
			return true;

		 
		if (bridge->is_thunderbolt)
			return true;

		 
		if (platform_pci_bridge_d3(bridge))
			return true;

		 
		if (bridge->is_hotplug_bridge)
			return false;

		if (dmi_check_system(bridge_d3_blacklist))
			return false;

		 
		if (dmi_get_bios_year() >= 2015)
			return true;
		break;
	}

	return false;
}

static int pci_dev_check_d3cold(struct pci_dev *dev, void *data)
{
	bool *d3cold_ok = data;

	if ( 
	    dev->no_d3cold || !dev->d3cold_allowed ||

	     
	    (device_may_wakeup(&dev->dev) &&
	     !pci_pme_capable(dev, PCI_D3cold)) ||

	     
	    !pci_power_manageable(dev))

		*d3cold_ok = false;

	return !*d3cold_ok;
}

 
void pci_bridge_d3_update(struct pci_dev *dev)
{
	bool remove = !device_is_registered(&dev->dev);
	struct pci_dev *bridge;
	bool d3cold_ok = true;

	bridge = pci_upstream_bridge(dev);
	if (!bridge || !pci_bridge_d3_possible(bridge))
		return;

	 
	if (remove && bridge->bridge_d3)
		return;

	 
	if (!remove)
		pci_dev_check_d3cold(dev, &d3cold_ok);

	 
	if (d3cold_ok && !bridge->bridge_d3)
		pci_walk_bus(bridge->subordinate, pci_dev_check_d3cold,
			     &d3cold_ok);

	if (bridge->bridge_d3 != d3cold_ok) {
		bridge->bridge_d3 = d3cold_ok;
		 
		pci_bridge_d3_update(bridge);
	}
}

 
void pci_d3cold_enable(struct pci_dev *dev)
{
	if (dev->no_d3cold) {
		dev->no_d3cold = false;
		pci_bridge_d3_update(dev);
	}
}
EXPORT_SYMBOL_GPL(pci_d3cold_enable);

 
void pci_d3cold_disable(struct pci_dev *dev)
{
	if (!dev->no_d3cold) {
		dev->no_d3cold = true;
		pci_bridge_d3_update(dev);
	}
}
EXPORT_SYMBOL_GPL(pci_d3cold_disable);

 
void pci_pm_init(struct pci_dev *dev)
{
	int pm;
	u16 status;
	u16 pmc;

	pm_runtime_forbid(&dev->dev);
	pm_runtime_set_active(&dev->dev);
	pm_runtime_enable(&dev->dev);
	device_enable_async_suspend(&dev->dev);
	dev->wakeup_prepared = false;

	dev->pm_cap = 0;
	dev->pme_support = 0;

	 
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	if (!pm)
		return;
	 
	pci_read_config_word(dev, pm + PCI_PM_PMC, &pmc);

	if ((pmc & PCI_PM_CAP_VER_MASK) > 3) {
		pci_err(dev, "unsupported PM cap regs version (%u)\n",
			pmc & PCI_PM_CAP_VER_MASK);
		return;
	}

	dev->pm_cap = pm;
	dev->d3hot_delay = PCI_PM_D3HOT_WAIT;
	dev->d3cold_delay = PCI_PM_D3COLD_WAIT;
	dev->bridge_d3 = pci_bridge_d3_possible(dev);
	dev->d3cold_allowed = true;

	dev->d1_support = false;
	dev->d2_support = false;
	if (!pci_no_d1d2(dev)) {
		if (pmc & PCI_PM_CAP_D1)
			dev->d1_support = true;
		if (pmc & PCI_PM_CAP_D2)
			dev->d2_support = true;

		if (dev->d1_support || dev->d2_support)
			pci_info(dev, "supports%s%s\n",
				   dev->d1_support ? " D1" : "",
				   dev->d2_support ? " D2" : "");
	}

	pmc &= PCI_PM_CAP_PME_MASK;
	if (pmc) {
		pci_info(dev, "PME# supported from%s%s%s%s%s\n",
			 (pmc & PCI_PM_CAP_PME_D0) ? " D0" : "",
			 (pmc & PCI_PM_CAP_PME_D1) ? " D1" : "",
			 (pmc & PCI_PM_CAP_PME_D2) ? " D2" : "",
			 (pmc & PCI_PM_CAP_PME_D3hot) ? " D3hot" : "",
			 (pmc & PCI_PM_CAP_PME_D3cold) ? " D3cold" : "");
		dev->pme_support = pmc >> PCI_PM_CAP_PME_SHIFT;
		dev->pme_poll = true;
		 
		device_set_wakeup_capable(&dev->dev, true);
		 
		pci_pme_active(dev, false);
	}

	pci_read_config_word(dev, PCI_STATUS, &status);
	if (status & PCI_STATUS_IMM_READY)
		dev->imm_ready = 1;
}

static unsigned long pci_ea_flags(struct pci_dev *dev, u8 prop)
{
	unsigned long flags = IORESOURCE_PCI_FIXED | IORESOURCE_PCI_EA_BEI;

	switch (prop) {
	case PCI_EA_P_MEM:
	case PCI_EA_P_VF_MEM:
		flags |= IORESOURCE_MEM;
		break;
	case PCI_EA_P_MEM_PREFETCH:
	case PCI_EA_P_VF_MEM_PREFETCH:
		flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH;
		break;
	case PCI_EA_P_IO:
		flags |= IORESOURCE_IO;
		break;
	default:
		return 0;
	}

	return flags;
}

static struct resource *pci_ea_get_resource(struct pci_dev *dev, u8 bei,
					    u8 prop)
{
	if (bei <= PCI_EA_BEI_BAR5 && prop <= PCI_EA_P_IO)
		return &dev->resource[bei];
#ifdef CONFIG_PCI_IOV
	else if (bei >= PCI_EA_BEI_VF_BAR0 && bei <= PCI_EA_BEI_VF_BAR5 &&
		 (prop == PCI_EA_P_VF_MEM || prop == PCI_EA_P_VF_MEM_PREFETCH))
		return &dev->resource[PCI_IOV_RESOURCES +
				      bei - PCI_EA_BEI_VF_BAR0];
#endif
	else if (bei == PCI_EA_BEI_ROM)
		return &dev->resource[PCI_ROM_RESOURCE];
	else
		return NULL;
}

 
static int pci_ea_read(struct pci_dev *dev, int offset)
{
	struct resource *res;
	int ent_size, ent_offset = offset;
	resource_size_t start, end;
	unsigned long flags;
	u32 dw0, bei, base, max_offset;
	u8 prop;
	bool support_64 = (sizeof(resource_size_t) >= 8);

	pci_read_config_dword(dev, ent_offset, &dw0);
	ent_offset += 4;

	 
	ent_size = ((dw0 & PCI_EA_ES) + 1) << 2;

	if (!(dw0 & PCI_EA_ENABLE))  
		goto out;

	bei = (dw0 & PCI_EA_BEI) >> 4;
	prop = (dw0 & PCI_EA_PP) >> 8;

	 
	if (prop > PCI_EA_P_BRIDGE_IO && prop < PCI_EA_P_MEM_RESERVED)
		prop = (dw0 & PCI_EA_SP) >> 16;
	if (prop > PCI_EA_P_BRIDGE_IO)
		goto out;

	res = pci_ea_get_resource(dev, bei, prop);
	if (!res) {
		pci_err(dev, "Unsupported EA entry BEI: %u\n", bei);
		goto out;
	}

	flags = pci_ea_flags(dev, prop);
	if (!flags) {
		pci_err(dev, "Unsupported EA properties: %#x\n", prop);
		goto out;
	}

	 
	pci_read_config_dword(dev, ent_offset, &base);
	start = (base & PCI_EA_FIELD_MASK);
	ent_offset += 4;

	 
	pci_read_config_dword(dev, ent_offset, &max_offset);
	ent_offset += 4;

	 
	if (base & PCI_EA_IS_64) {
		u32 base_upper;

		pci_read_config_dword(dev, ent_offset, &base_upper);
		ent_offset += 4;

		flags |= IORESOURCE_MEM_64;

		 
		if (!support_64 && base_upper)
			goto out;

		if (support_64)
			start |= ((u64)base_upper << 32);
	}

	end = start + (max_offset | 0x03);

	 
	if (max_offset & PCI_EA_IS_64) {
		u32 max_offset_upper;

		pci_read_config_dword(dev, ent_offset, &max_offset_upper);
		ent_offset += 4;

		flags |= IORESOURCE_MEM_64;

		 
		if (!support_64 && max_offset_upper)
			goto out;

		if (support_64)
			end += ((u64)max_offset_upper << 32);
	}

	if (end < start) {
		pci_err(dev, "EA Entry crosses address boundary\n");
		goto out;
	}

	if (ent_size != ent_offset - offset) {
		pci_err(dev, "EA Entry Size (%d) does not match length read (%d)\n",
			ent_size, ent_offset - offset);
		goto out;
	}

	res->name = pci_name(dev);
	res->start = start;
	res->end = end;
	res->flags = flags;

	if (bei <= PCI_EA_BEI_BAR5)
		pci_info(dev, "BAR %d: %pR (from Enhanced Allocation, properties %#02x)\n",
			   bei, res, prop);
	else if (bei == PCI_EA_BEI_ROM)
		pci_info(dev, "ROM: %pR (from Enhanced Allocation, properties %#02x)\n",
			   res, prop);
	else if (bei >= PCI_EA_BEI_VF_BAR0 && bei <= PCI_EA_BEI_VF_BAR5)
		pci_info(dev, "VF BAR %d: %pR (from Enhanced Allocation, properties %#02x)\n",
			   bei - PCI_EA_BEI_VF_BAR0, res, prop);
	else
		pci_info(dev, "BEI %d res: %pR (from Enhanced Allocation, properties %#02x)\n",
			   bei, res, prop);

out:
	return offset + ent_size;
}

 
void pci_ea_init(struct pci_dev *dev)
{
	int ea;
	u8 num_ent;
	int offset;
	int i;

	 
	ea = pci_find_capability(dev, PCI_CAP_ID_EA);
	if (!ea)
		return;

	 
	pci_bus_read_config_byte(dev->bus, dev->devfn, ea + PCI_EA_NUM_ENT,
					&num_ent);
	num_ent &= PCI_EA_NUM_ENT_MASK;

	offset = ea + PCI_EA_FIRST_ENT;

	 
	if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE)
		offset += 4;

	 
	for (i = 0; i < num_ent; ++i)
		offset = pci_ea_read(dev, offset);
}

static void pci_add_saved_cap(struct pci_dev *pci_dev,
	struct pci_cap_saved_state *new_cap)
{
	hlist_add_head(&new_cap->next, &pci_dev->saved_cap_space);
}

 
static int _pci_add_cap_save_buffer(struct pci_dev *dev, u16 cap,
				    bool extended, unsigned int size)
{
	int pos;
	struct pci_cap_saved_state *save_state;

	if (extended)
		pos = pci_find_ext_capability(dev, cap);
	else
		pos = pci_find_capability(dev, cap);

	if (!pos)
		return 0;

	save_state = kzalloc(sizeof(*save_state) + size, GFP_KERNEL);
	if (!save_state)
		return -ENOMEM;

	save_state->cap.cap_nr = cap;
	save_state->cap.cap_extended = extended;
	save_state->cap.size = size;
	pci_add_saved_cap(dev, save_state);

	return 0;
}

int pci_add_cap_save_buffer(struct pci_dev *dev, char cap, unsigned int size)
{
	return _pci_add_cap_save_buffer(dev, cap, false, size);
}

int pci_add_ext_cap_save_buffer(struct pci_dev *dev, u16 cap, unsigned int size)
{
	return _pci_add_cap_save_buffer(dev, cap, true, size);
}

 
void pci_allocate_cap_save_buffers(struct pci_dev *dev)
{
	int error;

	error = pci_add_cap_save_buffer(dev, PCI_CAP_ID_EXP,
					PCI_EXP_SAVE_REGS * sizeof(u16));
	if (error)
		pci_err(dev, "unable to preallocate PCI Express save buffer\n");

	error = pci_add_cap_save_buffer(dev, PCI_CAP_ID_PCIX, sizeof(u16));
	if (error)
		pci_err(dev, "unable to preallocate PCI-X save buffer\n");

	error = pci_add_ext_cap_save_buffer(dev, PCI_EXT_CAP_ID_LTR,
					    2 * sizeof(u16));
	if (error)
		pci_err(dev, "unable to allocate suspend buffer for LTR\n");

	pci_allocate_vc_save_buffers(dev);
}

void pci_free_cap_save_buffers(struct pci_dev *dev)
{
	struct pci_cap_saved_state *tmp;
	struct hlist_node *n;

	hlist_for_each_entry_safe(tmp, n, &dev->saved_cap_space, next)
		kfree(tmp);
}

 
void pci_configure_ari(struct pci_dev *dev)
{
	u32 cap;
	struct pci_dev *bridge;

	if (pcie_ari_disabled || !pci_is_pcie(dev) || dev->devfn)
		return;

	bridge = dev->bus->self;
	if (!bridge)
		return;

	pcie_capability_read_dword(bridge, PCI_EXP_DEVCAP2, &cap);
	if (!(cap & PCI_EXP_DEVCAP2_ARI))
		return;

	if (pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ARI)) {
		pcie_capability_set_word(bridge, PCI_EXP_DEVCTL2,
					 PCI_EXP_DEVCTL2_ARI);
		bridge->ari_enabled = 1;
	} else {
		pcie_capability_clear_word(bridge, PCI_EXP_DEVCTL2,
					   PCI_EXP_DEVCTL2_ARI);
		bridge->ari_enabled = 0;
	}
}

static bool pci_acs_flags_enabled(struct pci_dev *pdev, u16 acs_flags)
{
	int pos;
	u16 cap, ctrl;

	pos = pdev->acs_cap;
	if (!pos)
		return false;

	 
	pci_read_config_word(pdev, pos + PCI_ACS_CAP, &cap);
	acs_flags &= (cap | PCI_ACS_EC);

	pci_read_config_word(pdev, pos + PCI_ACS_CTRL, &ctrl);
	return (ctrl & acs_flags) == acs_flags;
}

 
bool pci_acs_enabled(struct pci_dev *pdev, u16 acs_flags)
{
	int ret;

	ret = pci_dev_specific_acs_enabled(pdev, acs_flags);
	if (ret >= 0)
		return ret > 0;

	 
	if (!pci_is_pcie(pdev))
		return false;

	switch (pci_pcie_type(pdev)) {
	 
	case PCI_EXP_TYPE_PCIE_BRIDGE:
	 
	case PCI_EXP_TYPE_PCI_BRIDGE:
	case PCI_EXP_TYPE_RC_EC:
		return false;
	 
	case PCI_EXP_TYPE_DOWNSTREAM:
	case PCI_EXP_TYPE_ROOT_PORT:
		return pci_acs_flags_enabled(pdev, acs_flags);
	 
	case PCI_EXP_TYPE_ENDPOINT:
	case PCI_EXP_TYPE_UPSTREAM:
	case PCI_EXP_TYPE_LEG_END:
	case PCI_EXP_TYPE_RC_END:
		if (!pdev->multifunction)
			break;

		return pci_acs_flags_enabled(pdev, acs_flags);
	}

	 
	return true;
}

 
bool pci_acs_path_enabled(struct pci_dev *start,
			  struct pci_dev *end, u16 acs_flags)
{
	struct pci_dev *pdev, *parent = start;

	do {
		pdev = parent;

		if (!pci_acs_enabled(pdev, acs_flags))
			return false;

		if (pci_is_root_bus(pdev->bus))
			return (end == NULL);

		parent = pdev->bus->self;
	} while (pdev != end);

	return true;
}

 
void pci_acs_init(struct pci_dev *dev)
{
	dev->acs_cap = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ACS);

	 
	pci_enable_acs(dev);
}

 
static int pci_rebar_find_pos(struct pci_dev *pdev, int bar)
{
	unsigned int pos, nbars, i;
	u32 ctrl;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_REBAR);
	if (!pos)
		return -ENOTSUPP;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
	nbars = (ctrl & PCI_REBAR_CTRL_NBAR_MASK) >>
		    PCI_REBAR_CTRL_NBAR_SHIFT;

	for (i = 0; i < nbars; i++, pos += 8) {
		int bar_idx;

		pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
		bar_idx = ctrl & PCI_REBAR_CTRL_BAR_IDX;
		if (bar_idx == bar)
			return pos;
	}

	return -ENOENT;
}

 
u32 pci_rebar_get_possible_sizes(struct pci_dev *pdev, int bar)
{
	int pos;
	u32 cap;

	pos = pci_rebar_find_pos(pdev, bar);
	if (pos < 0)
		return 0;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CAP, &cap);
	cap = FIELD_GET(PCI_REBAR_CAP_SIZES, cap);

	 
	if (pdev->vendor == PCI_VENDOR_ID_ATI && pdev->device == 0x731f &&
	    bar == 0 && cap == 0x700)
		return 0x3f00;

	return cap;
}
EXPORT_SYMBOL(pci_rebar_get_possible_sizes);

 
int pci_rebar_get_current_size(struct pci_dev *pdev, int bar)
{
	int pos;
	u32 ctrl;

	pos = pci_rebar_find_pos(pdev, bar);
	if (pos < 0)
		return pos;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
	return (ctrl & PCI_REBAR_CTRL_BAR_SIZE) >> PCI_REBAR_CTRL_BAR_SHIFT;
}

 
int pci_rebar_set_size(struct pci_dev *pdev, int bar, int size)
{
	int pos;
	u32 ctrl;

	pos = pci_rebar_find_pos(pdev, bar);
	if (pos < 0)
		return pos;

	pci_read_config_dword(pdev, pos + PCI_REBAR_CTRL, &ctrl);
	ctrl &= ~PCI_REBAR_CTRL_BAR_SIZE;
	ctrl |= size << PCI_REBAR_CTRL_BAR_SHIFT;
	pci_write_config_dword(pdev, pos + PCI_REBAR_CTRL, ctrl);
	return 0;
}

 
int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask)
{
	struct pci_bus *bus = dev->bus;
	struct pci_dev *bridge;
	u32 cap, ctl2;

	 
	if (dev->is_virtfn)
		return -EINVAL;

	if (!pci_is_pcie(dev))
		return -EINVAL;

	 

	switch (pci_pcie_type(dev)) {
	case PCI_EXP_TYPE_ENDPOINT:
	case PCI_EXP_TYPE_LEG_END:
	case PCI_EXP_TYPE_RC_END:
		break;
	default:
		return -EINVAL;
	}

	while (bus->parent) {
		bridge = bus->self;

		pcie_capability_read_dword(bridge, PCI_EXP_DEVCAP2, &cap);

		switch (pci_pcie_type(bridge)) {
		 
		case PCI_EXP_TYPE_UPSTREAM:
		case PCI_EXP_TYPE_DOWNSTREAM:
			if (!(cap & PCI_EXP_DEVCAP2_ATOMIC_ROUTE))
				return -EINVAL;
			break;

		 
		case PCI_EXP_TYPE_ROOT_PORT:
			if ((cap & cap_mask) != cap_mask)
				return -EINVAL;
			break;
		}

		 
		if (pci_pcie_type(bridge) == PCI_EXP_TYPE_UPSTREAM) {
			pcie_capability_read_dword(bridge, PCI_EXP_DEVCTL2,
						   &ctl2);
			if (ctl2 & PCI_EXP_DEVCTL2_ATOMIC_EGRESS_BLOCK)
				return -EINVAL;
		}

		bus = bus->parent;
	}

	pcie_capability_set_word(dev, PCI_EXP_DEVCTL2,
				 PCI_EXP_DEVCTL2_ATOMIC_REQ);
	return 0;
}
EXPORT_SYMBOL(pci_enable_atomic_ops_to_root);

 
u8 pci_swizzle_interrupt_pin(const struct pci_dev *dev, u8 pin)
{
	int slot;

	if (pci_ari_enabled(dev->bus))
		slot = 0;
	else
		slot = PCI_SLOT(dev->devfn);

	return (((pin - 1) + slot) % 4) + 1;
}

int pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge)
{
	u8 pin;

	pin = dev->pin;
	if (!pin)
		return -1;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*bridge = dev;
	return pin;
}

 
u8 pci_common_swizzle(struct pci_dev *dev, u8 *pinp)
{
	u8 pin = *pinp;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*pinp = pin;
	return PCI_SLOT(dev->devfn);
}
EXPORT_SYMBOL_GPL(pci_common_swizzle);

 
void pci_release_region(struct pci_dev *pdev, int bar)
{
	struct pci_devres *dr;

	if (pci_resource_len(pdev, bar) == 0)
		return;
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO)
		release_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM)
		release_mem_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));

	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask &= ~(1 << bar);
}
EXPORT_SYMBOL(pci_release_region);

 
static int __pci_request_region(struct pci_dev *pdev, int bar,
				const char *res_name, int exclusive)
{
	struct pci_devres *dr;

	if (pci_resource_len(pdev, bar) == 0)
		return 0;

	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
		if (!request_region(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar), res_name))
			goto err_out;
	} else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
		if (!__request_mem_region(pci_resource_start(pdev, bar),
					pci_resource_len(pdev, bar), res_name,
					exclusive))
			goto err_out;
	}

	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask |= 1 << bar;

	return 0;

err_out:
	pci_warn(pdev, "BAR %d: can't reserve %pR\n", bar,
		 &pdev->resource[bar]);
	return -EBUSY;
}

 
int pci_request_region(struct pci_dev *pdev, int bar, const char *res_name)
{
	return __pci_request_region(pdev, bar, res_name, 0);
}
EXPORT_SYMBOL(pci_request_region);

 
void pci_release_selected_regions(struct pci_dev *pdev, int bars)
{
	int i;

	for (i = 0; i < PCI_STD_NUM_BARS; i++)
		if (bars & (1 << i))
			pci_release_region(pdev, i);
}
EXPORT_SYMBOL(pci_release_selected_regions);

static int __pci_request_selected_regions(struct pci_dev *pdev, int bars,
					  const char *res_name, int excl)
{
	int i;

	for (i = 0; i < PCI_STD_NUM_BARS; i++)
		if (bars & (1 << i))
			if (__pci_request_region(pdev, i, res_name, excl))
				goto err_out;
	return 0;

err_out:
	while (--i >= 0)
		if (bars & (1 << i))
			pci_release_region(pdev, i);

	return -EBUSY;
}


 
int pci_request_selected_regions(struct pci_dev *pdev, int bars,
				 const char *res_name)
{
	return __pci_request_selected_regions(pdev, bars, res_name, 0);
}
EXPORT_SYMBOL(pci_request_selected_regions);

int pci_request_selected_regions_exclusive(struct pci_dev *pdev, int bars,
					   const char *res_name)
{
	return __pci_request_selected_regions(pdev, bars, res_name,
			IORESOURCE_EXCLUSIVE);
}
EXPORT_SYMBOL(pci_request_selected_regions_exclusive);

 

void pci_release_regions(struct pci_dev *pdev)
{
	pci_release_selected_regions(pdev, (1 << PCI_STD_NUM_BARS) - 1);
}
EXPORT_SYMBOL(pci_release_regions);

 
int pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	return pci_request_selected_regions(pdev,
			((1 << PCI_STD_NUM_BARS) - 1), res_name);
}
EXPORT_SYMBOL(pci_request_regions);

 
int pci_request_regions_exclusive(struct pci_dev *pdev, const char *res_name)
{
	return pci_request_selected_regions_exclusive(pdev,
				((1 << PCI_STD_NUM_BARS) - 1), res_name);
}
EXPORT_SYMBOL(pci_request_regions_exclusive);

 
int pci_register_io_range(struct fwnode_handle *fwnode, phys_addr_t addr,
			resource_size_t	size)
{
	int ret = 0;
#ifdef PCI_IOBASE
	struct logic_pio_hwaddr *range;

	if (!size || addr + size < addr)
		return -EINVAL;

	range = kzalloc(sizeof(*range), GFP_ATOMIC);
	if (!range)
		return -ENOMEM;

	range->fwnode = fwnode;
	range->size = size;
	range->hw_start = addr;
	range->flags = LOGIC_PIO_CPU_MMIO;

	ret = logic_pio_register_range(range);
	if (ret)
		kfree(range);

	 
	if (ret == -EEXIST)
		ret = 0;
#endif

	return ret;
}

phys_addr_t pci_pio_to_address(unsigned long pio)
{
#ifdef PCI_IOBASE
	if (pio < MMIO_UPPER_LIMIT)
		return logic_pio_to_hwaddr(pio);
#endif

	return (phys_addr_t) OF_BAD_ADDR;
}
EXPORT_SYMBOL_GPL(pci_pio_to_address);

unsigned long __weak pci_address_to_pio(phys_addr_t address)
{
#ifdef PCI_IOBASE
	return logic_pio_trans_cpuaddr(address);
#else
	if (address > IO_SPACE_LIMIT)
		return (unsigned long)-1;

	return (unsigned long) address;
#endif
}

 
#ifndef pci_remap_iospace
int pci_remap_iospace(const struct resource *res, phys_addr_t phys_addr)
{
#if defined(PCI_IOBASE) && defined(CONFIG_MMU)
	unsigned long vaddr = (unsigned long)PCI_IOBASE + res->start;

	if (!(res->flags & IORESOURCE_IO))
		return -EINVAL;

	if (res->end > IO_SPACE_LIMIT)
		return -EINVAL;

	return ioremap_page_range(vaddr, vaddr + resource_size(res), phys_addr,
				  pgprot_device(PAGE_KERNEL));
#else
	 
	WARN_ONCE(1, "This architecture does not support memory mapped I/O\n");
	return -ENODEV;
#endif
}
EXPORT_SYMBOL(pci_remap_iospace);
#endif

 
void pci_unmap_iospace(struct resource *res)
{
#if defined(PCI_IOBASE) && defined(CONFIG_MMU)
	unsigned long vaddr = (unsigned long)PCI_IOBASE + res->start;

	vunmap_range(vaddr, vaddr + resource_size(res));
#endif
}
EXPORT_SYMBOL(pci_unmap_iospace);

static void devm_pci_unmap_iospace(struct device *dev, void *ptr)
{
	struct resource **res = ptr;

	pci_unmap_iospace(*res);
}

 
int devm_pci_remap_iospace(struct device *dev, const struct resource *res,
			   phys_addr_t phys_addr)
{
	const struct resource **ptr;
	int error;

	ptr = devres_alloc(devm_pci_unmap_iospace, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	error = pci_remap_iospace(res, phys_addr);
	if (error) {
		devres_free(ptr);
	} else	{
		*ptr = res;
		devres_add(dev, ptr);
	}

	return error;
}
EXPORT_SYMBOL(devm_pci_remap_iospace);

 
void __iomem *devm_pci_remap_cfgspace(struct device *dev,
				      resource_size_t offset,
				      resource_size_t size)
{
	void __iomem **ptr, *addr;

	ptr = devres_alloc(devm_ioremap_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	addr = pci_remap_cfgspace(offset, size);
	if (addr) {
		*ptr = addr;
		devres_add(dev, ptr);
	} else
		devres_free(ptr);

	return addr;
}
EXPORT_SYMBOL(devm_pci_remap_cfgspace);

 
void __iomem *devm_pci_remap_cfg_resource(struct device *dev,
					  struct resource *res)
{
	resource_size_t size;
	const char *name;
	void __iomem *dest_ptr;

	BUG_ON(!dev);

	if (!res || resource_type(res) != IORESOURCE_MEM) {
		dev_err(dev, "invalid resource\n");
		return IOMEM_ERR_PTR(-EINVAL);
	}

	size = resource_size(res);

	if (res->name)
		name = devm_kasprintf(dev, GFP_KERNEL, "%s %s", dev_name(dev),
				      res->name);
	else
		name = devm_kstrdup(dev, dev_name(dev), GFP_KERNEL);
	if (!name)
		return IOMEM_ERR_PTR(-ENOMEM);

	if (!devm_request_mem_region(dev, res->start, size, name)) {
		dev_err(dev, "can't request region for resource %pR\n", res);
		return IOMEM_ERR_PTR(-EBUSY);
	}

	dest_ptr = devm_pci_remap_cfgspace(dev, res->start, size);
	if (!dest_ptr) {
		dev_err(dev, "ioremap failed for resource %pR\n", res);
		devm_release_mem_region(dev, res->start, size);
		dest_ptr = IOMEM_ERR_PTR(-ENOMEM);
	}

	return dest_ptr;
}
EXPORT_SYMBOL(devm_pci_remap_cfg_resource);

static void __pci_set_master(struct pci_dev *dev, bool enable)
{
	u16 old_cmd, cmd;

	pci_read_config_word(dev, PCI_COMMAND, &old_cmd);
	if (enable)
		cmd = old_cmd | PCI_COMMAND_MASTER;
	else
		cmd = old_cmd & ~PCI_COMMAND_MASTER;
	if (cmd != old_cmd) {
		pci_dbg(dev, "%s bus mastering\n",
			enable ? "enabling" : "disabling");
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	dev->is_busmaster = enable;
}

 
char * __weak __init pcibios_setup(char *str)
{
	return str;
}

 
void __weak pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;

	 
	if (pci_is_pcie(dev))
		return;

	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < 16)
		lat = (64 <= pcibios_max_latency) ? 64 : pcibios_max_latency;
	else if (lat > pcibios_max_latency)
		lat = pcibios_max_latency;
	else
		return;

	pci_write_config_byte(dev, PCI_LATENCY_TIMER, lat);
}

 
void pci_set_master(struct pci_dev *dev)
{
	__pci_set_master(dev, true);
	pcibios_set_master(dev);
}
EXPORT_SYMBOL(pci_set_master);

 
void pci_clear_master(struct pci_dev *dev)
{
	__pci_set_master(dev, false);
}
EXPORT_SYMBOL(pci_clear_master);

 
int pci_set_cacheline_size(struct pci_dev *dev)
{
	u8 cacheline_size;

	if (!pci_cache_line_size)
		return -EINVAL;

	 
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size >= pci_cache_line_size &&
	    (cacheline_size % pci_cache_line_size) == 0)
		return 0;

	 
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cache_line_size);
	 
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size == pci_cache_line_size)
		return 0;

	pci_dbg(dev, "cache line size of %d is not supported\n",
		   pci_cache_line_size << 2);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(pci_set_cacheline_size);

 
int pci_set_mwi(struct pci_dev *dev)
{
#ifdef PCI_DISABLE_MWI
	return 0;
#else
	int rc;
	u16 cmd;

	rc = pci_set_cacheline_size(dev);
	if (rc)
		return rc;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_INVALIDATE)) {
		pci_dbg(dev, "enabling Mem-Wr-Inval\n");
		cmd |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
#endif
}
EXPORT_SYMBOL(pci_set_mwi);

 
int pcim_set_mwi(struct pci_dev *dev)
{
	struct pci_devres *dr;

	dr = find_pci_dr(dev);
	if (!dr)
		return -ENOMEM;

	dr->mwi = 1;
	return pci_set_mwi(dev);
}
EXPORT_SYMBOL(pcim_set_mwi);

 
int pci_try_set_mwi(struct pci_dev *dev)
{
#ifdef PCI_DISABLE_MWI
	return 0;
#else
	return pci_set_mwi(dev);
#endif
}
EXPORT_SYMBOL(pci_try_set_mwi);

 
void pci_clear_mwi(struct pci_dev *dev)
{
#ifndef PCI_DISABLE_MWI
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (cmd & PCI_COMMAND_INVALIDATE) {
		cmd &= ~PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
#endif
}
EXPORT_SYMBOL(pci_clear_mwi);

 
void pci_disable_parity(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (cmd & PCI_COMMAND_PARITY) {
		cmd &= ~PCI_COMMAND_PARITY;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
}

 
void pci_intx(struct pci_dev *pdev, int enable)
{
	u16 pci_command, new;

	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);

	if (enable)
		new = pci_command & ~PCI_COMMAND_INTX_DISABLE;
	else
		new = pci_command | PCI_COMMAND_INTX_DISABLE;

	if (new != pci_command) {
		struct pci_devres *dr;

		pci_write_config_word(pdev, PCI_COMMAND, new);

		dr = find_pci_dr(pdev);
		if (dr && !dr->restore_intx) {
			dr->restore_intx = 1;
			dr->orig_intx = !enable;
		}
	}
}
EXPORT_SYMBOL_GPL(pci_intx);

static bool pci_check_and_set_intx_mask(struct pci_dev *dev, bool mask)
{
	struct pci_bus *bus = dev->bus;
	bool mask_updated = true;
	u32 cmd_status_dword;
	u16 origcmd, newcmd;
	unsigned long flags;
	bool irq_pending;

	 
	BUILD_BUG_ON(PCI_COMMAND % 4);
	BUILD_BUG_ON(PCI_COMMAND + 2 != PCI_STATUS);

	raw_spin_lock_irqsave(&pci_lock, flags);

	bus->ops->read(bus, dev->devfn, PCI_COMMAND, 4, &cmd_status_dword);

	irq_pending = (cmd_status_dword >> 16) & PCI_STATUS_INTERRUPT;

	 
	if (mask != irq_pending) {
		mask_updated = false;
		goto done;
	}

	origcmd = cmd_status_dword;
	newcmd = origcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (mask)
		newcmd |= PCI_COMMAND_INTX_DISABLE;
	if (newcmd != origcmd)
		bus->ops->write(bus, dev->devfn, PCI_COMMAND, 2, newcmd);

done:
	raw_spin_unlock_irqrestore(&pci_lock, flags);

	return mask_updated;
}

 
bool pci_check_and_mask_intx(struct pci_dev *dev)
{
	return pci_check_and_set_intx_mask(dev, true);
}
EXPORT_SYMBOL_GPL(pci_check_and_mask_intx);

 
bool pci_check_and_unmask_intx(struct pci_dev *dev)
{
	return pci_check_and_set_intx_mask(dev, false);
}
EXPORT_SYMBOL_GPL(pci_check_and_unmask_intx);

 
int pci_wait_for_pending_transaction(struct pci_dev *dev)
{
	if (!pci_is_pcie(dev))
		return 1;

	return pci_wait_for_pending(dev, pci_pcie_cap(dev) + PCI_EXP_DEVSTA,
				    PCI_EXP_DEVSTA_TRPND);
}
EXPORT_SYMBOL(pci_wait_for_pending_transaction);

 
int pcie_flr(struct pci_dev *dev)
{
	if (!pci_wait_for_pending_transaction(dev))
		pci_err(dev, "timed out waiting for pending transaction; performing function level reset anyway\n");

	pcie_capability_set_word(dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_BCR_FLR);

	if (dev->imm_ready)
		return 0;

	 
	msleep(100);

	return pci_dev_wait(dev, "FLR", PCIE_RESET_READY_POLL_MS);
}
EXPORT_SYMBOL_GPL(pcie_flr);

 
int pcie_reset_flr(struct pci_dev *dev, bool probe)
{
	if (dev->dev_flags & PCI_DEV_FLAGS_NO_FLR_RESET)
		return -ENOTTY;

	if (!(dev->devcap & PCI_EXP_DEVCAP_FLR))
		return -ENOTTY;

	if (probe)
		return 0;

	return pcie_flr(dev);
}
EXPORT_SYMBOL_GPL(pcie_reset_flr);

static int pci_af_flr(struct pci_dev *dev, bool probe)
{
	int pos;
	u8 cap;

	pos = pci_find_capability(dev, PCI_CAP_ID_AF);
	if (!pos)
		return -ENOTTY;

	if (dev->dev_flags & PCI_DEV_FLAGS_NO_FLR_RESET)
		return -ENOTTY;

	pci_read_config_byte(dev, pos + PCI_AF_CAP, &cap);
	if (!(cap & PCI_AF_CAP_TP) || !(cap & PCI_AF_CAP_FLR))
		return -ENOTTY;

	if (probe)
		return 0;

	 
	if (!pci_wait_for_pending(dev, pos + PCI_AF_CTRL,
				 PCI_AF_STATUS_TP << 8))
		pci_err(dev, "timed out waiting for pending transaction; performing AF function level reset anyway\n");

	pci_write_config_byte(dev, pos + PCI_AF_CTRL, PCI_AF_CTRL_FLR);

	if (dev->imm_ready)
		return 0;

	 
	msleep(100);

	return pci_dev_wait(dev, "AF_FLR", PCIE_RESET_READY_POLL_MS);
}

 
static int pci_pm_reset(struct pci_dev *dev, bool probe)
{
	u16 csr;

	if (!dev->pm_cap || dev->dev_flags & PCI_DEV_FLAGS_NO_PM_RESET)
		return -ENOTTY;

	pci_read_config_word(dev, dev->pm_cap + PCI_PM_CTRL, &csr);
	if (csr & PCI_PM_CTRL_NO_SOFT_RESET)
		return -ENOTTY;

	if (probe)
		return 0;

	if (dev->current_state != PCI_D0)
		return -EINVAL;

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D3hot;
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, csr);
	pci_dev_d3_sleep(dev);

	csr &= ~PCI_PM_CTRL_STATE_MASK;
	csr |= PCI_D0;
	pci_write_config_word(dev, dev->pm_cap + PCI_PM_CTRL, csr);
	pci_dev_d3_sleep(dev);

	return pci_dev_wait(dev, "PM D3hot->D0", PCIE_RESET_READY_POLL_MS);
}

 
static int pcie_wait_for_link_status(struct pci_dev *pdev,
				     bool use_lt, bool active)
{
	u16 lnksta_mask, lnksta_match;
	unsigned long end_jiffies;
	u16 lnksta;

	lnksta_mask = use_lt ? PCI_EXP_LNKSTA_LT : PCI_EXP_LNKSTA_DLLLA;
	lnksta_match = active ? lnksta_mask : 0;

	end_jiffies = jiffies + msecs_to_jiffies(PCIE_LINK_RETRAIN_TIMEOUT_MS);
	do {
		pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &lnksta);
		if ((lnksta & lnksta_mask) == lnksta_match)
			return 0;
		msleep(1);
	} while (time_before(jiffies, end_jiffies));

	return -ETIMEDOUT;
}

 
int pcie_retrain_link(struct pci_dev *pdev, bool use_lt)
{
	int rc;

	 
	rc = pcie_wait_for_link_status(pdev, use_lt, !use_lt);
	if (rc)
		return rc;

	pcie_capability_set_word(pdev, PCI_EXP_LNKCTL, PCI_EXP_LNKCTL_RL);
	if (pdev->clear_retrain_link) {
		 
		pcie_capability_clear_word(pdev, PCI_EXP_LNKCTL, PCI_EXP_LNKCTL_RL);
	}

	return pcie_wait_for_link_status(pdev, use_lt, !use_lt);
}

 
static bool pcie_wait_for_link_delay(struct pci_dev *pdev, bool active,
				     int delay)
{
	int rc;

	 
	if (!pdev->link_active_reporting) {
		msleep(PCIE_LINK_RETRAIN_TIMEOUT_MS + delay);
		return true;
	}

	 
	if (active)
		msleep(20);
	rc = pcie_wait_for_link_status(pdev, false, active);
	if (active) {
		if (rc)
			rc = pcie_failed_link_retrain(pdev);
		if (rc)
			return false;

		msleep(delay);
		return true;
	}

	if (rc)
		return false;

	return true;
}

 
bool pcie_wait_for_link(struct pci_dev *pdev, bool active)
{
	return pcie_wait_for_link_delay(pdev, active, 100);
}

 
static int pci_bus_max_d3cold_delay(const struct pci_bus *bus)
{
	const struct pci_dev *pdev;
	int min_delay = 100;
	int max_delay = 0;

	list_for_each_entry(pdev, &bus->devices, bus_list) {
		if (pdev->d3cold_delay < min_delay)
			min_delay = pdev->d3cold_delay;
		if (pdev->d3cold_delay > max_delay)
			max_delay = pdev->d3cold_delay;
	}

	return max(min_delay, max_delay);
}

 
int pci_bridge_wait_for_secondary_bus(struct pci_dev *dev, char *reset_type)
{
	struct pci_dev *child;
	int delay;

	if (pci_dev_is_disconnected(dev))
		return 0;

	if (!pci_is_bridge(dev))
		return 0;

	down_read(&pci_bus_sem);

	 
	if (!dev->subordinate || list_empty(&dev->subordinate->devices)) {
		up_read(&pci_bus_sem);
		return 0;
	}

	 
	delay = pci_bus_max_d3cold_delay(dev->subordinate);
	if (!delay) {
		up_read(&pci_bus_sem);
		return 0;
	}

	child = list_first_entry(&dev->subordinate->devices, struct pci_dev,
				 bus_list);
	up_read(&pci_bus_sem);

	 
	if (!pci_is_pcie(dev)) {
		pci_dbg(dev, "waiting %d ms for secondary bus\n", 1000 + delay);
		msleep(1000 + delay);
		return 0;
	}

	 
	if (!pcie_downstream_port(dev))
		return 0;

	if (pcie_get_speed_cap(dev) <= PCIE_SPEED_5_0GT) {
		u16 status;

		pci_dbg(dev, "waiting %d ms for downstream link\n", delay);
		msleep(delay);

		if (!pci_dev_wait(child, reset_type, PCI_RESET_WAIT - delay))
			return 0;

		 
		if (!dev->link_active_reporting)
			return -ENOTTY;

		pcie_capability_read_word(dev, PCI_EXP_LNKSTA, &status);
		if (!(status & PCI_EXP_LNKSTA_DLLLA))
			return -ENOTTY;

		return pci_dev_wait(child, reset_type,
				    PCIE_RESET_READY_POLL_MS - PCI_RESET_WAIT);
	}

	pci_dbg(dev, "waiting %d ms for downstream link, after activation\n",
		delay);
	if (!pcie_wait_for_link_delay(dev, true, delay)) {
		 
		pci_info(dev, "Data Link Layer Link Active not set in 1000 msec\n");
		return -ENOTTY;
	}

	return pci_dev_wait(child, reset_type,
			    PCIE_RESET_READY_POLL_MS - delay);
}

void pci_reset_secondary_bus(struct pci_dev *dev)
{
	u16 ctrl;

	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &ctrl);
	ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, ctrl);

	 
	msleep(2);

	ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, ctrl);
}

void __weak pcibios_reset_secondary_bus(struct pci_dev *dev)
{
	pci_reset_secondary_bus(dev);
}

 
int pci_bridge_secondary_bus_reset(struct pci_dev *dev)
{
	pcibios_reset_secondary_bus(dev);

	return pci_bridge_wait_for_secondary_bus(dev, "bus reset");
}
EXPORT_SYMBOL_GPL(pci_bridge_secondary_bus_reset);

static int pci_parent_bus_reset(struct pci_dev *dev, bool probe)
{
	struct pci_dev *pdev;

	if (pci_is_root_bus(dev->bus) || dev->subordinate ||
	    !dev->bus->self || dev->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET)
		return -ENOTTY;

	list_for_each_entry(pdev, &dev->bus->devices, bus_list)
		if (pdev != dev)
			return -ENOTTY;

	if (probe)
		return 0;

	return pci_bridge_secondary_bus_reset(dev->bus->self);
}

static int pci_reset_hotplug_slot(struct hotplug_slot *hotplug, bool probe)
{
	int rc = -ENOTTY;

	if (!hotplug || !try_module_get(hotplug->owner))
		return rc;

	if (hotplug->ops->reset_slot)
		rc = hotplug->ops->reset_slot(hotplug, probe);

	module_put(hotplug->owner);

	return rc;
}

static int pci_dev_reset_slot_function(struct pci_dev *dev, bool probe)
{
	if (dev->multifunction || dev->subordinate || !dev->slot ||
	    dev->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET)
		return -ENOTTY;

	return pci_reset_hotplug_slot(dev->slot->hotplug, probe);
}

static int pci_reset_bus_function(struct pci_dev *dev, bool probe)
{
	int rc;

	rc = pci_dev_reset_slot_function(dev, probe);
	if (rc != -ENOTTY)
		return rc;
	return pci_parent_bus_reset(dev, probe);
}

void pci_dev_lock(struct pci_dev *dev)
{
	 
	device_lock(&dev->dev);
	pci_cfg_access_lock(dev);
}
EXPORT_SYMBOL_GPL(pci_dev_lock);

 
int pci_dev_trylock(struct pci_dev *dev)
{
	if (device_trylock(&dev->dev)) {
		if (pci_cfg_access_trylock(dev))
			return 1;
		device_unlock(&dev->dev);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_dev_trylock);

void pci_dev_unlock(struct pci_dev *dev)
{
	pci_cfg_access_unlock(dev);
	device_unlock(&dev->dev);
}
EXPORT_SYMBOL_GPL(pci_dev_unlock);

static void pci_dev_save_and_disable(struct pci_dev *dev)
{
	const struct pci_error_handlers *err_handler =
			dev->driver ? dev->driver->err_handler : NULL;

	 
	if (err_handler && err_handler->reset_prepare)
		err_handler->reset_prepare(dev);

	 
	pci_set_power_state(dev, PCI_D0);

	pci_save_state(dev);
	 
	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE);
}

static void pci_dev_restore(struct pci_dev *dev)
{
	const struct pci_error_handlers *err_handler =
			dev->driver ? dev->driver->err_handler : NULL;

	pci_restore_state(dev);

	 
	if (err_handler && err_handler->reset_done)
		err_handler->reset_done(dev);
}

 
static const struct pci_reset_fn_method pci_reset_fn_methods[] = {
	{ },
	{ pci_dev_specific_reset, .name = "device_specific" },
	{ pci_dev_acpi_reset, .name = "acpi" },
	{ pcie_reset_flr, .name = "flr" },
	{ pci_af_flr, .name = "af_flr" },
	{ pci_pm_reset, .name = "pm" },
	{ pci_reset_bus_function, .name = "bus" },
};

static ssize_t reset_method_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	ssize_t len = 0;
	int i, m;

	for (i = 0; i < PCI_NUM_RESET_METHODS; i++) {
		m = pdev->reset_methods[i];
		if (!m)
			break;

		len += sysfs_emit_at(buf, len, "%s%s", len ? " " : "",
				     pci_reset_fn_methods[m].name);
	}

	if (len)
		len += sysfs_emit_at(buf, len, "\n");

	return len;
}

static int reset_method_lookup(const char *name)
{
	int m;

	for (m = 1; m < PCI_NUM_RESET_METHODS; m++) {
		if (sysfs_streq(name, pci_reset_fn_methods[m].name))
			return m;
	}

	return 0;	 
}

static ssize_t reset_method_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	char *options, *name;
	int m, n;
	u8 reset_methods[PCI_NUM_RESET_METHODS] = { 0 };

	if (sysfs_streq(buf, "")) {
		pdev->reset_methods[0] = 0;
		pci_warn(pdev, "All device reset methods disabled by user");
		return count;
	}

	if (sysfs_streq(buf, "default")) {
		pci_init_reset_methods(pdev);
		return count;
	}

	options = kstrndup(buf, count, GFP_KERNEL);
	if (!options)
		return -ENOMEM;

	n = 0;
	while ((name = strsep(&options, " ")) != NULL) {
		if (sysfs_streq(name, ""))
			continue;

		name = strim(name);

		m = reset_method_lookup(name);
		if (!m) {
			pci_err(pdev, "Invalid reset method '%s'", name);
			goto error;
		}

		if (pci_reset_fn_methods[m].reset_fn(pdev, PCI_RESET_PROBE)) {
			pci_err(pdev, "Unsupported reset method '%s'", name);
			goto error;
		}

		if (n == PCI_NUM_RESET_METHODS - 1) {
			pci_err(pdev, "Too many reset methods\n");
			goto error;
		}

		reset_methods[n++] = m;
	}

	reset_methods[n] = 0;

	 
	if (pci_reset_fn_methods[1].reset_fn(pdev, PCI_RESET_PROBE) == 0 &&
	    reset_methods[0] != 1)
		pci_warn(pdev, "Device-specific reset disabled/de-prioritized by user");
	memcpy(pdev->reset_methods, reset_methods, sizeof(pdev->reset_methods));
	kfree(options);
	return count;

error:
	 
	kfree(options);
	return -EINVAL;
}
static DEVICE_ATTR_RW(reset_method);

static struct attribute *pci_dev_reset_method_attrs[] = {
	&dev_attr_reset_method.attr,
	NULL,
};

static umode_t pci_dev_reset_method_attr_is_visible(struct kobject *kobj,
						    struct attribute *a, int n)
{
	struct pci_dev *pdev = to_pci_dev(kobj_to_dev(kobj));

	if (!pci_reset_supported(pdev))
		return 0;

	return a->mode;
}

const struct attribute_group pci_dev_reset_method_attr_group = {
	.attrs = pci_dev_reset_method_attrs,
	.is_visible = pci_dev_reset_method_attr_is_visible,
};

 
int __pci_reset_function_locked(struct pci_dev *dev)
{
	int i, m, rc;

	might_sleep();

	 
	for (i = 0; i < PCI_NUM_RESET_METHODS; i++) {
		m = dev->reset_methods[i];
		if (!m)
			return -ENOTTY;

		rc = pci_reset_fn_methods[m].reset_fn(dev, PCI_RESET_DO_RESET);
		if (!rc)
			return 0;
		if (rc != -ENOTTY)
			return rc;
	}

	return -ENOTTY;
}
EXPORT_SYMBOL_GPL(__pci_reset_function_locked);

 
void pci_init_reset_methods(struct pci_dev *dev)
{
	int m, i, rc;

	BUILD_BUG_ON(ARRAY_SIZE(pci_reset_fn_methods) != PCI_NUM_RESET_METHODS);

	might_sleep();

	i = 0;
	for (m = 1; m < PCI_NUM_RESET_METHODS; m++) {
		rc = pci_reset_fn_methods[m].reset_fn(dev, PCI_RESET_PROBE);
		if (!rc)
			dev->reset_methods[i++] = m;
		else if (rc != -ENOTTY)
			break;
	}

	dev->reset_methods[i] = 0;
}

 
int pci_reset_function(struct pci_dev *dev)
{
	int rc;

	if (!pci_reset_supported(dev))
		return -ENOTTY;

	pci_dev_lock(dev);
	pci_dev_save_and_disable(dev);

	rc = __pci_reset_function_locked(dev);

	pci_dev_restore(dev);
	pci_dev_unlock(dev);

	return rc;
}
EXPORT_SYMBOL_GPL(pci_reset_function);

 
int pci_reset_function_locked(struct pci_dev *dev)
{
	int rc;

	if (!pci_reset_supported(dev))
		return -ENOTTY;

	pci_dev_save_and_disable(dev);

	rc = __pci_reset_function_locked(dev);

	pci_dev_restore(dev);

	return rc;
}
EXPORT_SYMBOL_GPL(pci_reset_function_locked);

 
int pci_try_reset_function(struct pci_dev *dev)
{
	int rc;

	if (!pci_reset_supported(dev))
		return -ENOTTY;

	if (!pci_dev_trylock(dev))
		return -EAGAIN;

	pci_dev_save_and_disable(dev);
	rc = __pci_reset_function_locked(dev);
	pci_dev_restore(dev);
	pci_dev_unlock(dev);

	return rc;
}
EXPORT_SYMBOL_GPL(pci_try_reset_function);

 
static bool pci_bus_resettable(struct pci_bus *bus)
{
	struct pci_dev *dev;


	if (bus->self && (bus->self->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET))
		return false;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET ||
		    (dev->subordinate && !pci_bus_resettable(dev->subordinate)))
			return false;
	}

	return true;
}

 
static void pci_bus_lock(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		pci_dev_lock(dev);
		if (dev->subordinate)
			pci_bus_lock(dev->subordinate);
	}
}

 
static void pci_bus_unlock(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->subordinate)
			pci_bus_unlock(dev->subordinate);
		pci_dev_unlock(dev);
	}
}

 
static int pci_bus_trylock(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (!pci_dev_trylock(dev))
			goto unlock;
		if (dev->subordinate) {
			if (!pci_bus_trylock(dev->subordinate)) {
				pci_dev_unlock(dev);
				goto unlock;
			}
		}
	}
	return 1;

unlock:
	list_for_each_entry_continue_reverse(dev, &bus->devices, bus_list) {
		if (dev->subordinate)
			pci_bus_unlock(dev->subordinate);
		pci_dev_unlock(dev);
	}
	return 0;
}

 
static bool pci_slot_resettable(struct pci_slot *slot)
{
	struct pci_dev *dev;

	if (slot->bus->self &&
	    (slot->bus->self->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET))
		return false;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		if (dev->dev_flags & PCI_DEV_FLAGS_NO_BUS_RESET ||
		    (dev->subordinate && !pci_bus_resettable(dev->subordinate)))
			return false;
	}

	return true;
}

 
static void pci_slot_lock(struct pci_slot *slot)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		pci_dev_lock(dev);
		if (dev->subordinate)
			pci_bus_lock(dev->subordinate);
	}
}

 
static void pci_slot_unlock(struct pci_slot *slot)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		if (dev->subordinate)
			pci_bus_unlock(dev->subordinate);
		pci_dev_unlock(dev);
	}
}

 
static int pci_slot_trylock(struct pci_slot *slot)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		if (!pci_dev_trylock(dev))
			goto unlock;
		if (dev->subordinate) {
			if (!pci_bus_trylock(dev->subordinate)) {
				pci_dev_unlock(dev);
				goto unlock;
			}
		}
	}
	return 1;

unlock:
	list_for_each_entry_continue_reverse(dev,
					     &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		if (dev->subordinate)
			pci_bus_unlock(dev->subordinate);
		pci_dev_unlock(dev);
	}
	return 0;
}

 
static void pci_bus_save_and_disable_locked(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		pci_dev_save_and_disable(dev);
		if (dev->subordinate)
			pci_bus_save_and_disable_locked(dev->subordinate);
	}
}

 
static void pci_bus_restore_locked(struct pci_bus *bus)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		pci_dev_restore(dev);
		if (dev->subordinate)
			pci_bus_restore_locked(dev->subordinate);
	}
}

 
static void pci_slot_save_and_disable_locked(struct pci_slot *slot)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		pci_dev_save_and_disable(dev);
		if (dev->subordinate)
			pci_bus_save_and_disable_locked(dev->subordinate);
	}
}

 
static void pci_slot_restore_locked(struct pci_slot *slot)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &slot->bus->devices, bus_list) {
		if (!dev->slot || dev->slot != slot)
			continue;
		pci_dev_restore(dev);
		if (dev->subordinate)
			pci_bus_restore_locked(dev->subordinate);
	}
}

static int pci_slot_reset(struct pci_slot *slot, bool probe)
{
	int rc;

	if (!slot || !pci_slot_resettable(slot))
		return -ENOTTY;

	if (!probe)
		pci_slot_lock(slot);

	might_sleep();

	rc = pci_reset_hotplug_slot(slot->hotplug, probe);

	if (!probe)
		pci_slot_unlock(slot);

	return rc;
}

 
int pci_probe_reset_slot(struct pci_slot *slot)
{
	return pci_slot_reset(slot, PCI_RESET_PROBE);
}
EXPORT_SYMBOL_GPL(pci_probe_reset_slot);

 
static int __pci_reset_slot(struct pci_slot *slot)
{
	int rc;

	rc = pci_slot_reset(slot, PCI_RESET_PROBE);
	if (rc)
		return rc;

	if (pci_slot_trylock(slot)) {
		pci_slot_save_and_disable_locked(slot);
		might_sleep();
		rc = pci_reset_hotplug_slot(slot->hotplug, PCI_RESET_DO_RESET);
		pci_slot_restore_locked(slot);
		pci_slot_unlock(slot);
	} else
		rc = -EAGAIN;

	return rc;
}

static int pci_bus_reset(struct pci_bus *bus, bool probe)
{
	int ret;

	if (!bus->self || !pci_bus_resettable(bus))
		return -ENOTTY;

	if (probe)
		return 0;

	pci_bus_lock(bus);

	might_sleep();

	ret = pci_bridge_secondary_bus_reset(bus->self);

	pci_bus_unlock(bus);

	return ret;
}

 
int pci_bus_error_reset(struct pci_dev *bridge)
{
	struct pci_bus *bus = bridge->subordinate;
	struct pci_slot *slot;

	if (!bus)
		return -ENOTTY;

	mutex_lock(&pci_slot_mutex);
	if (list_empty(&bus->slots))
		goto bus_reset;

	list_for_each_entry(slot, &bus->slots, list)
		if (pci_probe_reset_slot(slot))
			goto bus_reset;

	list_for_each_entry(slot, &bus->slots, list)
		if (pci_slot_reset(slot, PCI_RESET_DO_RESET))
			goto bus_reset;

	mutex_unlock(&pci_slot_mutex);
	return 0;
bus_reset:
	mutex_unlock(&pci_slot_mutex);
	return pci_bus_reset(bridge->subordinate, PCI_RESET_DO_RESET);
}

 
int pci_probe_reset_bus(struct pci_bus *bus)
{
	return pci_bus_reset(bus, PCI_RESET_PROBE);
}
EXPORT_SYMBOL_GPL(pci_probe_reset_bus);

 
static int __pci_reset_bus(struct pci_bus *bus)
{
	int rc;

	rc = pci_bus_reset(bus, PCI_RESET_PROBE);
	if (rc)
		return rc;

	if (pci_bus_trylock(bus)) {
		pci_bus_save_and_disable_locked(bus);
		might_sleep();
		rc = pci_bridge_secondary_bus_reset(bus->self);
		pci_bus_restore_locked(bus);
		pci_bus_unlock(bus);
	} else
		rc = -EAGAIN;

	return rc;
}

 
int pci_reset_bus(struct pci_dev *pdev)
{
	return (!pci_probe_reset_slot(pdev->slot)) ?
	    __pci_reset_slot(pdev->slot) : __pci_reset_bus(pdev->bus);
}
EXPORT_SYMBOL_GPL(pci_reset_bus);

 
int pcix_get_max_mmrbc(struct pci_dev *dev)
{
	int cap;
	u32 stat;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_dword(dev, cap + PCI_X_STATUS, &stat))
		return -EINVAL;

	return 512 << ((stat & PCI_X_STATUS_MAX_READ) >> 21);
}
EXPORT_SYMBOL(pcix_get_max_mmrbc);

 
int pcix_get_mmrbc(struct pci_dev *dev)
{
	int cap;
	u16 cmd;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_word(dev, cap + PCI_X_CMD, &cmd))
		return -EINVAL;

	return 512 << ((cmd & PCI_X_CMD_MAX_READ) >> 2);
}
EXPORT_SYMBOL(pcix_get_mmrbc);

 
int pcix_set_mmrbc(struct pci_dev *dev, int mmrbc)
{
	int cap;
	u32 stat, v, o;
	u16 cmd;

	if (mmrbc < 512 || mmrbc > 4096 || !is_power_of_2(mmrbc))
		return -EINVAL;

	v = ffs(mmrbc) - 10;

	cap = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!cap)
		return -EINVAL;

	if (pci_read_config_dword(dev, cap + PCI_X_STATUS, &stat))
		return -EINVAL;

	if (v > (stat & PCI_X_STATUS_MAX_READ) >> 21)
		return -E2BIG;

	if (pci_read_config_word(dev, cap + PCI_X_CMD, &cmd))
		return -EINVAL;

	o = (cmd & PCI_X_CMD_MAX_READ) >> 2;
	if (o != v) {
		if (v > o && (dev->bus->bus_flags & PCI_BUS_FLAGS_NO_MMRBC))
			return -EIO;

		cmd &= ~PCI_X_CMD_MAX_READ;
		cmd |= v << 2;
		if (pci_write_config_word(dev, cap + PCI_X_CMD, cmd))
			return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL(pcix_set_mmrbc);

 
int pcie_get_readrq(struct pci_dev *dev)
{
	u16 ctl;

	pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &ctl);

	return 128 << ((ctl & PCI_EXP_DEVCTL_READRQ) >> 12);
}
EXPORT_SYMBOL(pcie_get_readrq);

 
int pcie_set_readrq(struct pci_dev *dev, int rq)
{
	u16 v;
	int ret;
	struct pci_host_bridge *bridge = pci_find_host_bridge(dev->bus);

	if (rq < 128 || rq > 4096 || !is_power_of_2(rq))
		return -EINVAL;

	 
	if (pcie_bus_config == PCIE_BUS_PERFORMANCE) {
		int mps = pcie_get_mps(dev);

		if (mps < rq)
			rq = mps;
	}

	v = (ffs(rq) - 8) << 12;

	if (bridge->no_inc_mrrs) {
		int max_mrrs = pcie_get_readrq(dev);

		if (rq > max_mrrs) {
			pci_info(dev, "can't set Max_Read_Request_Size to %d; max is %d\n", rq, max_mrrs);
			return -EINVAL;
		}
	}

	ret = pcie_capability_clear_and_set_word(dev, PCI_EXP_DEVCTL,
						  PCI_EXP_DEVCTL_READRQ, v);

	return pcibios_err_to_errno(ret);
}
EXPORT_SYMBOL(pcie_set_readrq);

 
int pcie_get_mps(struct pci_dev *dev)
{
	u16 ctl;

	pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &ctl);

	return 128 << ((ctl & PCI_EXP_DEVCTL_PAYLOAD) >> 5);
}
EXPORT_SYMBOL(pcie_get_mps);

 
int pcie_set_mps(struct pci_dev *dev, int mps)
{
	u16 v;
	int ret;

	if (mps < 128 || mps > 4096 || !is_power_of_2(mps))
		return -EINVAL;

	v = ffs(mps) - 8;
	if (v > dev->pcie_mpss)
		return -EINVAL;
	v <<= 5;

	ret = pcie_capability_clear_and_set_word(dev, PCI_EXP_DEVCTL,
						  PCI_EXP_DEVCTL_PAYLOAD, v);

	return pcibios_err_to_errno(ret);
}
EXPORT_SYMBOL(pcie_set_mps);

 
u32 pcie_bandwidth_available(struct pci_dev *dev, struct pci_dev **limiting_dev,
			     enum pci_bus_speed *speed,
			     enum pcie_link_width *width)
{
	u16 lnksta;
	enum pci_bus_speed next_speed;
	enum pcie_link_width next_width;
	u32 bw, next_bw;

	if (speed)
		*speed = PCI_SPEED_UNKNOWN;
	if (width)
		*width = PCIE_LNK_WIDTH_UNKNOWN;

	bw = 0;

	while (dev) {
		pcie_capability_read_word(dev, PCI_EXP_LNKSTA, &lnksta);

		next_speed = pcie_link_speed[lnksta & PCI_EXP_LNKSTA_CLS];
		next_width = FIELD_GET(PCI_EXP_LNKSTA_NLW, lnksta);

		next_bw = next_width * PCIE_SPEED2MBS_ENC(next_speed);

		 
		if (!bw || next_bw <= bw) {
			bw = next_bw;

			if (limiting_dev)
				*limiting_dev = dev;
			if (speed)
				*speed = next_speed;
			if (width)
				*width = next_width;
		}

		dev = pci_upstream_bridge(dev);
	}

	return bw;
}
EXPORT_SYMBOL(pcie_bandwidth_available);

 
enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *dev)
{
	u32 lnkcap2, lnkcap;

	 
	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP2, &lnkcap2);

	 
	if (lnkcap2)
		return PCIE_LNKCAP2_SLS2SPEED(lnkcap2);

	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &lnkcap);
	if ((lnkcap & PCI_EXP_LNKCAP_SLS) == PCI_EXP_LNKCAP_SLS_5_0GB)
		return PCIE_SPEED_5_0GT;
	else if ((lnkcap & PCI_EXP_LNKCAP_SLS) == PCI_EXP_LNKCAP_SLS_2_5GB)
		return PCIE_SPEED_2_5GT;

	return PCI_SPEED_UNKNOWN;
}
EXPORT_SYMBOL(pcie_get_speed_cap);

 
enum pcie_link_width pcie_get_width_cap(struct pci_dev *dev)
{
	u32 lnkcap;

	pcie_capability_read_dword(dev, PCI_EXP_LNKCAP, &lnkcap);
	if (lnkcap)
		return FIELD_GET(PCI_EXP_LNKCAP_MLW, lnkcap);

	return PCIE_LNK_WIDTH_UNKNOWN;
}
EXPORT_SYMBOL(pcie_get_width_cap);

 
u32 pcie_bandwidth_capable(struct pci_dev *dev, enum pci_bus_speed *speed,
			   enum pcie_link_width *width)
{
	*speed = pcie_get_speed_cap(dev);
	*width = pcie_get_width_cap(dev);

	if (*speed == PCI_SPEED_UNKNOWN || *width == PCIE_LNK_WIDTH_UNKNOWN)
		return 0;

	return *width * PCIE_SPEED2MBS_ENC(*speed);
}

 
void __pcie_print_link_status(struct pci_dev *dev, bool verbose)
{
	enum pcie_link_width width, width_cap;
	enum pci_bus_speed speed, speed_cap;
	struct pci_dev *limiting_dev = NULL;
	u32 bw_avail, bw_cap;

	bw_cap = pcie_bandwidth_capable(dev, &speed_cap, &width_cap);
	bw_avail = pcie_bandwidth_available(dev, &limiting_dev, &speed, &width);

	if (bw_avail >= bw_cap && verbose)
		pci_info(dev, "%u.%03u Gb/s available PCIe bandwidth (%s x%d link)\n",
			 bw_cap / 1000, bw_cap % 1000,
			 pci_speed_string(speed_cap), width_cap);
	else if (bw_avail < bw_cap)
		pci_info(dev, "%u.%03u Gb/s available PCIe bandwidth, limited by %s x%d link at %s (capable of %u.%03u Gb/s with %s x%d link)\n",
			 bw_avail / 1000, bw_avail % 1000,
			 pci_speed_string(speed), width,
			 limiting_dev ? pci_name(limiting_dev) : "<unknown>",
			 bw_cap / 1000, bw_cap % 1000,
			 pci_speed_string(speed_cap), width_cap);
}

 
void pcie_print_link_status(struct pci_dev *dev)
{
	__pcie_print_link_status(dev, true);
}
EXPORT_SYMBOL(pcie_print_link_status);

 
int pci_select_bars(struct pci_dev *dev, unsigned long flags)
{
	int i, bars = 0;
	for (i = 0; i < PCI_NUM_RESOURCES; i++)
		if (pci_resource_flags(dev, i) & flags)
			bars |= (1 << i);
	return bars;
}
EXPORT_SYMBOL(pci_select_bars);

 
static arch_set_vga_state_t arch_set_vga_state;

void __init pci_register_set_vga_state(arch_set_vga_state_t func)
{
	arch_set_vga_state = func;	 
}

static int pci_set_vga_state_arch(struct pci_dev *dev, bool decode,
				  unsigned int command_bits, u32 flags)
{
	if (arch_set_vga_state)
		return arch_set_vga_state(dev, decode, command_bits,
						flags);
	return 0;
}

 
int pci_set_vga_state(struct pci_dev *dev, bool decode,
		      unsigned int command_bits, u32 flags)
{
	struct pci_bus *bus;
	struct pci_dev *bridge;
	u16 cmd;
	int rc;

	WARN_ON((flags & PCI_VGA_STATE_CHANGE_DECODES) && (command_bits & ~(PCI_COMMAND_IO|PCI_COMMAND_MEMORY)));

	 
	rc = pci_set_vga_state_arch(dev, decode, command_bits, flags);
	if (rc)
		return rc;

	if (flags & PCI_VGA_STATE_CHANGE_DECODES) {
		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		if (decode)
			cmd |= command_bits;
		else
			cmd &= ~command_bits;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}

	if (!(flags & PCI_VGA_STATE_CHANGE_BRIDGE))
		return 0;

	bus = dev->bus;
	while (bus) {
		bridge = bus->self;
		if (bridge) {
			pci_read_config_word(bridge, PCI_BRIDGE_CONTROL,
					     &cmd);
			if (decode)
				cmd |= PCI_BRIDGE_CTL_VGA;
			else
				cmd &= ~PCI_BRIDGE_CTL_VGA;
			pci_write_config_word(bridge, PCI_BRIDGE_CONTROL,
					      cmd);
		}
		bus = bus->parent;
	}
	return 0;
}

#ifdef CONFIG_ACPI
bool pci_pr3_present(struct pci_dev *pdev)
{
	struct acpi_device *adev;

	if (acpi_disabled)
		return false;

	adev = ACPI_COMPANION(&pdev->dev);
	if (!adev)
		return false;

	return adev->power.flags.power_resources &&
		acpi_has_method(adev->handle, "_PR3");
}
EXPORT_SYMBOL_GPL(pci_pr3_present);
#endif

 
void pci_add_dma_alias(struct pci_dev *dev, u8 devfn_from,
		       unsigned int nr_devfns)
{
	int devfn_to;

	nr_devfns = min(nr_devfns, (unsigned int)MAX_NR_DEVFNS - devfn_from);
	devfn_to = devfn_from + nr_devfns - 1;

	if (!dev->dma_alias_mask)
		dev->dma_alias_mask = bitmap_zalloc(MAX_NR_DEVFNS, GFP_KERNEL);
	if (!dev->dma_alias_mask) {
		pci_warn(dev, "Unable to allocate DMA alias mask\n");
		return;
	}

	bitmap_set(dev->dma_alias_mask, devfn_from, nr_devfns);

	if (nr_devfns == 1)
		pci_info(dev, "Enabling fixed DMA alias to %02x.%d\n",
				PCI_SLOT(devfn_from), PCI_FUNC(devfn_from));
	else if (nr_devfns > 1)
		pci_info(dev, "Enabling fixed DMA alias for devfn range from %02x.%d to %02x.%d\n",
				PCI_SLOT(devfn_from), PCI_FUNC(devfn_from),
				PCI_SLOT(devfn_to), PCI_FUNC(devfn_to));
}

bool pci_devs_are_dma_aliases(struct pci_dev *dev1, struct pci_dev *dev2)
{
	return (dev1->dma_alias_mask &&
		test_bit(dev2->devfn, dev1->dma_alias_mask)) ||
	       (dev2->dma_alias_mask &&
		test_bit(dev1->devfn, dev2->dma_alias_mask)) ||
	       pci_real_dma_dev(dev1) == dev2 ||
	       pci_real_dma_dev(dev2) == dev1;
}

bool pci_device_is_present(struct pci_dev *pdev)
{
	u32 v;

	 
	pdev = pci_physfn(pdev);
	if (pci_dev_is_disconnected(pdev))
		return false;
	return pci_bus_read_dev_vendor_id(pdev->bus, pdev->devfn, &v, 0);
}
EXPORT_SYMBOL_GPL(pci_device_is_present);

void pci_ignore_hotplug(struct pci_dev *dev)
{
	struct pci_dev *bridge = dev->bus->self;

	dev->ignore_hotplug = 1;
	 
	if (bridge)
		bridge->ignore_hotplug = 1;
}
EXPORT_SYMBOL_GPL(pci_ignore_hotplug);

 
struct pci_dev __weak *pci_real_dma_dev(struct pci_dev *dev)
{
	return dev;
}

resource_size_t __weak pcibios_default_alignment(void)
{
	return 0;
}

 
void __weak pci_resource_to_user(const struct pci_dev *dev, int bar,
				 const struct resource *rsrc,
				 resource_size_t *start, resource_size_t *end)
{
	*start = rsrc->start;
	*end = rsrc->end;
}

static char *resource_alignment_param;
static DEFINE_SPINLOCK(resource_alignment_lock);

 
static resource_size_t pci_specified_resource_alignment(struct pci_dev *dev,
							bool *resize)
{
	int align_order, count;
	resource_size_t align = pcibios_default_alignment();
	const char *p;
	int ret;

	spin_lock(&resource_alignment_lock);
	p = resource_alignment_param;
	if (!p || !*p)
		goto out;
	if (pci_has_flag(PCI_PROBE_ONLY)) {
		align = 0;
		pr_info_once("PCI: Ignoring requested alignments (PCI_PROBE_ONLY)\n");
		goto out;
	}

	while (*p) {
		count = 0;
		if (sscanf(p, "%d%n", &align_order, &count) == 1 &&
		    p[count] == '@') {
			p += count + 1;
			if (align_order > 63) {
				pr_err("PCI: Invalid requested alignment (order %d)\n",
				       align_order);
				align_order = PAGE_SHIFT;
			}
		} else {
			align_order = PAGE_SHIFT;
		}

		ret = pci_dev_str_match(dev, p, &p);
		if (ret == 1) {
			*resize = true;
			align = 1ULL << align_order;
			break;
		} else if (ret < 0) {
			pr_err("PCI: Can't parse resource_alignment parameter: %s\n",
			       p);
			break;
		}

		if (*p != ';' && *p != ',') {
			 
			break;
		}
		p++;
	}
out:
	spin_unlock(&resource_alignment_lock);
	return align;
}

static void pci_request_resource_alignment(struct pci_dev *dev, int bar,
					   resource_size_t align, bool resize)
{
	struct resource *r = &dev->resource[bar];
	resource_size_t size;

	if (!(r->flags & IORESOURCE_MEM))
		return;

	if (r->flags & IORESOURCE_PCI_FIXED) {
		pci_info(dev, "BAR%d %pR: ignoring requested alignment %#llx\n",
			 bar, r, (unsigned long long)align);
		return;
	}

	size = resource_size(r);
	if (size >= align)
		return;

	 

	pci_info(dev, "BAR%d %pR: requesting alignment to %#llx\n",
		 bar, r, (unsigned long long)align);

	if (resize) {
		r->start = 0;
		r->end = align - 1;
	} else {
		r->flags &= ~IORESOURCE_SIZEALIGN;
		r->flags |= IORESOURCE_STARTALIGN;
		r->start = align;
		r->end = r->start + size - 1;
	}
	r->flags |= IORESOURCE_UNSET;
}

 
void pci_reassigndev_resource_alignment(struct pci_dev *dev)
{
	int i;
	struct resource *r;
	resource_size_t align;
	u16 command;
	bool resize = false;

	 
	if (dev->is_virtfn)
		return;

	 
	align = pci_specified_resource_alignment(dev, &resize);
	if (!align)
		return;

	if (dev->hdr_type == PCI_HEADER_TYPE_NORMAL &&
	    (dev->class >> 8) == PCI_CLASS_BRIDGE_HOST) {
		pci_warn(dev, "Can't reassign resources to host bridge\n");
		return;
	}

	pci_read_config_word(dev, PCI_COMMAND, &command);
	command &= ~PCI_COMMAND_MEMORY;
	pci_write_config_word(dev, PCI_COMMAND, command);

	for (i = 0; i <= PCI_ROM_RESOURCE; i++)
		pci_request_resource_alignment(dev, i, align, resize);

	 
	if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
		for (i = PCI_BRIDGE_RESOURCES; i < PCI_NUM_RESOURCES; i++) {
			r = &dev->resource[i];
			if (!(r->flags & IORESOURCE_MEM))
				continue;
			r->flags |= IORESOURCE_UNSET;
			r->end = resource_size(r) - 1;
			r->start = 0;
		}
		pci_disable_bridge_window(dev);
	}
}

static ssize_t resource_alignment_show(const struct bus_type *bus, char *buf)
{
	size_t count = 0;

	spin_lock(&resource_alignment_lock);
	if (resource_alignment_param)
		count = sysfs_emit(buf, "%s\n", resource_alignment_param);
	spin_unlock(&resource_alignment_lock);

	return count;
}

static ssize_t resource_alignment_store(const struct bus_type *bus,
					const char *buf, size_t count)
{
	char *param, *old, *end;

	if (count >= (PAGE_SIZE - 1))
		return -EINVAL;

	param = kstrndup(buf, count, GFP_KERNEL);
	if (!param)
		return -ENOMEM;

	end = strchr(param, '\n');
	if (end)
		*end = '\0';

	spin_lock(&resource_alignment_lock);
	old = resource_alignment_param;
	if (strlen(param)) {
		resource_alignment_param = param;
	} else {
		kfree(param);
		resource_alignment_param = NULL;
	}
	spin_unlock(&resource_alignment_lock);

	kfree(old);

	return count;
}

static BUS_ATTR_RW(resource_alignment);

static int __init pci_resource_alignment_sysfs_init(void)
{
	return bus_create_file(&pci_bus_type,
					&bus_attr_resource_alignment);
}
late_initcall(pci_resource_alignment_sysfs_init);

static void pci_no_domains(void)
{
#ifdef CONFIG_PCI_DOMAINS
	pci_domains_supported = 0;
#endif
}

#ifdef CONFIG_PCI_DOMAINS_GENERIC
static DEFINE_IDA(pci_domain_nr_static_ida);
static DEFINE_IDA(pci_domain_nr_dynamic_ida);

static void of_pci_reserve_static_domain_nr(void)
{
	struct device_node *np;
	int domain_nr;

	for_each_node_by_type(np, "pci") {
		domain_nr = of_get_pci_domain_nr(np);
		if (domain_nr < 0)
			continue;
		 
		ida_alloc_range(&pci_domain_nr_dynamic_ida,
				domain_nr, domain_nr, GFP_KERNEL);
	}
}

static int of_pci_bus_find_domain_nr(struct device *parent)
{
	static bool static_domains_reserved = false;
	int domain_nr;

	 
	if (!static_domains_reserved) {
		of_pci_reserve_static_domain_nr();
		static_domains_reserved = true;
	}

	if (parent) {
		 
		domain_nr = of_get_pci_domain_nr(parent->of_node);
		if (domain_nr >= 0)
			return ida_alloc_range(&pci_domain_nr_static_ida,
					       domain_nr, domain_nr,
					       GFP_KERNEL);
	}

	 
	return ida_alloc(&pci_domain_nr_dynamic_ida, GFP_KERNEL);
}

static void of_pci_bus_release_domain_nr(struct pci_bus *bus, struct device *parent)
{
	if (bus->domain_nr < 0)
		return;

	 
	if (of_get_pci_domain_nr(parent->of_node) == bus->domain_nr)
		ida_free(&pci_domain_nr_static_ida, bus->domain_nr);
	else
		ida_free(&pci_domain_nr_dynamic_ida, bus->domain_nr);
}

int pci_bus_find_domain_nr(struct pci_bus *bus, struct device *parent)
{
	return acpi_disabled ? of_pci_bus_find_domain_nr(parent) :
			       acpi_pci_bus_find_domain_nr(bus);
}

void pci_bus_release_domain_nr(struct pci_bus *bus, struct device *parent)
{
	if (!acpi_disabled)
		return;
	of_pci_bus_release_domain_nr(bus, parent);
}
#endif

 
int __weak pci_ext_cfg_avail(void)
{
	return 1;
}

void __weak pci_fixup_cardbus(struct pci_bus *bus)
{
}
EXPORT_SYMBOL(pci_fixup_cardbus);

static int __init pci_setup(char *str)
{
	while (str) {
		char *k = strchr(str, ',');
		if (k)
			*k++ = 0;
		if (*str && (str = pcibios_setup(str)) && *str) {
			if (!strcmp(str, "nomsi")) {
				pci_no_msi();
			} else if (!strncmp(str, "noats", 5)) {
				pr_info("PCIe: ATS is disabled\n");
				pcie_ats_disabled = true;
			} else if (!strcmp(str, "noaer")) {
				pci_no_aer();
			} else if (!strcmp(str, "earlydump")) {
				pci_early_dump = true;
			} else if (!strncmp(str, "realloc=", 8)) {
				pci_realloc_get_opt(str + 8);
			} else if (!strncmp(str, "realloc", 7)) {
				pci_realloc_get_opt("on");
			} else if (!strcmp(str, "nodomains")) {
				pci_no_domains();
			} else if (!strncmp(str, "noari", 5)) {
				pcie_ari_disabled = true;
			} else if (!strncmp(str, "cbiosize=", 9)) {
				pci_cardbus_io_size = memparse(str + 9, &str);
			} else if (!strncmp(str, "cbmemsize=", 10)) {
				pci_cardbus_mem_size = memparse(str + 10, &str);
			} else if (!strncmp(str, "resource_alignment=", 19)) {
				resource_alignment_param = str + 19;
			} else if (!strncmp(str, "ecrc=", 5)) {
				pcie_ecrc_get_policy(str + 5);
			} else if (!strncmp(str, "hpiosize=", 9)) {
				pci_hotplug_io_size = memparse(str + 9, &str);
			} else if (!strncmp(str, "hpmmiosize=", 11)) {
				pci_hotplug_mmio_size = memparse(str + 11, &str);
			} else if (!strncmp(str, "hpmmioprefsize=", 15)) {
				pci_hotplug_mmio_pref_size = memparse(str + 15, &str);
			} else if (!strncmp(str, "hpmemsize=", 10)) {
				pci_hotplug_mmio_size = memparse(str + 10, &str);
				pci_hotplug_mmio_pref_size = pci_hotplug_mmio_size;
			} else if (!strncmp(str, "hpbussize=", 10)) {
				pci_hotplug_bus_size =
					simple_strtoul(str + 10, &str, 0);
				if (pci_hotplug_bus_size > 0xff)
					pci_hotplug_bus_size = DEFAULT_HOTPLUG_BUS_SIZE;
			} else if (!strncmp(str, "pcie_bus_tune_off", 17)) {
				pcie_bus_config = PCIE_BUS_TUNE_OFF;
			} else if (!strncmp(str, "pcie_bus_safe", 13)) {
				pcie_bus_config = PCIE_BUS_SAFE;
			} else if (!strncmp(str, "pcie_bus_perf", 13)) {
				pcie_bus_config = PCIE_BUS_PERFORMANCE;
			} else if (!strncmp(str, "pcie_bus_peer2peer", 18)) {
				pcie_bus_config = PCIE_BUS_PEER2PEER;
			} else if (!strncmp(str, "pcie_scan_all", 13)) {
				pci_add_flags(PCI_SCAN_ALL_PCIE_DEVS);
			} else if (!strncmp(str, "disable_acs_redir=", 18)) {
				disable_acs_redir_param = str + 18;
			} else {
				pr_err("PCI: Unknown option `%s'\n", str);
			}
		}
		str = k;
	}
	return 0;
}
early_param("pci", pci_setup);

 
static int __init pci_realloc_setup_params(void)
{
	resource_alignment_param = kstrdup(resource_alignment_param,
					   GFP_KERNEL);
	disable_acs_redir_param = kstrdup(disable_acs_redir_param, GFP_KERNEL);

	return 0;
}
pure_initcall(pci_realloc_setup_params);
