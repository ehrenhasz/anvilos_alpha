 

#include <linux/types.h>  

#include <linux/stop_machine.h>
#include <linux/kvm_para.h>
#include <linux/uaccess.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/sort.h>
#include <linux/cpu.h>
#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/syscore_ops.h>
#include <linux/rcupdate.h>

#include <asm/cacheinfo.h>
#include <asm/cpufeature.h>
#include <asm/e820/api.h>
#include <asm/mtrr.h>
#include <asm/msr.h>
#include <asm/memtype.h>

#include "mtrr.h"

 
#define MTRR_TO_PHYS_WC_OFFSET 1000

u32 num_var_ranges;

unsigned int mtrr_usage_table[MTRR_MAX_VAR_RANGES];
DEFINE_MUTEX(mtrr_mutex);

const struct mtrr_ops *mtrr_if;

 
static int have_wrcomb(void)
{
	struct pci_dev *dev;

	dev = pci_get_class(PCI_CLASS_BRIDGE_HOST << 8, NULL);
	if (dev != NULL) {
		 
		if (dev->vendor == PCI_VENDOR_ID_SERVERWORKS &&
		    dev->device == PCI_DEVICE_ID_SERVERWORKS_LE &&
		    dev->revision <= 5) {
			pr_info("Serverworks LE rev < 6 detected. Write-combining disabled.\n");
			pci_dev_put(dev);
			return 0;
		}
		 
		if (dev->vendor == PCI_VENDOR_ID_INTEL &&
		    dev->device == PCI_DEVICE_ID_INTEL_82451NX) {
			pr_info("Intel 450NX MMC detected. Write-combining disabled.\n");
			pci_dev_put(dev);
			return 0;
		}
		pci_dev_put(dev);
	}
	return mtrr_if->have_wrcomb ? mtrr_if->have_wrcomb() : 0;
}

static void __init init_table(void)
{
	int i, max;

	max = num_var_ranges;
	for (i = 0; i < max; i++)
		mtrr_usage_table[i] = 1;
}

struct set_mtrr_data {
	unsigned long	smp_base;
	unsigned long	smp_size;
	unsigned int	smp_reg;
	mtrr_type	smp_type;
};

 
static int mtrr_rendezvous_handler(void *info)
{
	struct set_mtrr_data *data = info;

	mtrr_if->set(data->smp_reg, data->smp_base,
		     data->smp_size, data->smp_type);
	return 0;
}

static inline int types_compatible(mtrr_type type1, mtrr_type type2)
{
	return type1 == MTRR_TYPE_UNCACHABLE ||
	       type2 == MTRR_TYPE_UNCACHABLE ||
	       (type1 == MTRR_TYPE_WRTHROUGH && type2 == MTRR_TYPE_WRBACK) ||
	       (type1 == MTRR_TYPE_WRBACK && type2 == MTRR_TYPE_WRTHROUGH);
}

 
static void set_mtrr(unsigned int reg, unsigned long base, unsigned long size,
		     mtrr_type type)
{
	struct set_mtrr_data data = { .smp_reg = reg,
				      .smp_base = base,
				      .smp_size = size,
				      .smp_type = type
				    };

	stop_machine_cpuslocked(mtrr_rendezvous_handler, &data, cpu_online_mask);

	generic_rebuild_map();
}

 
int mtrr_add_page(unsigned long base, unsigned long size,
		  unsigned int type, bool increment)
{
	unsigned long lbase, lsize;
	int i, replace, error;
	mtrr_type ltype;

	if (!mtrr_enabled())
		return -ENXIO;

	error = mtrr_if->validate_add_page(base, size, type);
	if (error)
		return error;

	if (type >= MTRR_NUM_TYPES) {
		pr_warn("type: %u invalid\n", type);
		return -EINVAL;
	}

	 
	if ((type == MTRR_TYPE_WRCOMB) && !have_wrcomb()) {
		pr_warn("your processor doesn't support write-combining\n");
		return -ENOSYS;
	}

	if (!size) {
		pr_warn("zero sized request\n");
		return -EINVAL;
	}

	if ((base | (base + size - 1)) >>
	    (boot_cpu_data.x86_phys_bits - PAGE_SHIFT)) {
		pr_warn("base or size exceeds the MTRR width\n");
		return -EINVAL;
	}

	error = -EINVAL;
	replace = -1;

	 
	cpus_read_lock();

	 
	mutex_lock(&mtrr_mutex);
	for (i = 0; i < num_var_ranges; ++i) {
		mtrr_if->get(i, &lbase, &lsize, &ltype);
		if (!lsize || base > lbase + lsize - 1 ||
		    base + size - 1 < lbase)
			continue;
		 
		if (base < lbase || base + size - 1 > lbase + lsize - 1) {
			if (base <= lbase &&
			    base + size - 1 >= lbase + lsize - 1) {
				 
				if (type == ltype) {
					replace = replace == -1 ? i : -2;
					continue;
				} else if (types_compatible(type, ltype))
					continue;
			}
			pr_warn("0x%lx000,0x%lx000 overlaps existing 0x%lx000,0x%lx000\n", base, size, lbase,
				lsize);
			goto out;
		}
		 
		if (ltype != type) {
			if (types_compatible(type, ltype))
				continue;
			pr_warn("type mismatch for %lx000,%lx000 old: %s new: %s\n",
				base, size, mtrr_attrib_to_str(ltype),
				mtrr_attrib_to_str(type));
			goto out;
		}
		if (increment)
			++mtrr_usage_table[i];
		error = i;
		goto out;
	}
	 
	i = mtrr_if->get_free_region(base, size, replace);
	if (i >= 0) {
		set_mtrr(i, base, size, type);
		if (likely(replace < 0)) {
			mtrr_usage_table[i] = 1;
		} else {
			mtrr_usage_table[i] = mtrr_usage_table[replace];
			if (increment)
				mtrr_usage_table[i]++;
			if (unlikely(replace != i)) {
				set_mtrr(replace, 0, 0, 0);
				mtrr_usage_table[replace] = 0;
			}
		}
	} else {
		pr_info("no more MTRRs available\n");
	}
	error = i;
 out:
	mutex_unlock(&mtrr_mutex);
	cpus_read_unlock();
	return error;
}

static int mtrr_check(unsigned long base, unsigned long size)
{
	if ((base & (PAGE_SIZE - 1)) || (size & (PAGE_SIZE - 1))) {
		pr_warn("size and base must be multiples of 4 kiB\n");
		Dprintk("size: 0x%lx  base: 0x%lx\n", size, base);
		dump_stack();
		return -1;
	}
	return 0;
}

 
int mtrr_add(unsigned long base, unsigned long size, unsigned int type,
	     bool increment)
{
	if (!mtrr_enabled())
		return -ENODEV;
	if (mtrr_check(base, size))
		return -EINVAL;
	return mtrr_add_page(base >> PAGE_SHIFT, size >> PAGE_SHIFT, type,
			     increment);
}

 
int mtrr_del_page(int reg, unsigned long base, unsigned long size)
{
	int i, max;
	mtrr_type ltype;
	unsigned long lbase, lsize;
	int error = -EINVAL;

	if (!mtrr_enabled())
		return -ENODEV;

	max = num_var_ranges;
	 
	cpus_read_lock();
	mutex_lock(&mtrr_mutex);
	if (reg < 0) {
		 
		for (i = 0; i < max; ++i) {
			mtrr_if->get(i, &lbase, &lsize, &ltype);
			if (lbase == base && lsize == size) {
				reg = i;
				break;
			}
		}
		if (reg < 0) {
			Dprintk("no MTRR for %lx000,%lx000 found\n", base, size);
			goto out;
		}
	}
	if (reg >= max) {
		pr_warn("register: %d too big\n", reg);
		goto out;
	}
	mtrr_if->get(reg, &lbase, &lsize, &ltype);
	if (lsize < 1) {
		pr_warn("MTRR %d not used\n", reg);
		goto out;
	}
	if (mtrr_usage_table[reg] < 1) {
		pr_warn("reg: %d has count=0\n", reg);
		goto out;
	}
	if (--mtrr_usage_table[reg] < 1)
		set_mtrr(reg, 0, 0, 0);
	error = reg;
 out:
	mutex_unlock(&mtrr_mutex);
	cpus_read_unlock();
	return error;
}

 
int mtrr_del(int reg, unsigned long base, unsigned long size)
{
	if (!mtrr_enabled())
		return -ENODEV;
	if (mtrr_check(base, size))
		return -EINVAL;
	return mtrr_del_page(reg, base >> PAGE_SHIFT, size >> PAGE_SHIFT);
}

 
int arch_phys_wc_add(unsigned long base, unsigned long size)
{
	int ret;

	if (pat_enabled() || !mtrr_enabled())
		return 0;   

	ret = mtrr_add(base, size, MTRR_TYPE_WRCOMB, true);
	if (ret < 0) {
		pr_warn("Failed to add WC MTRR for [%p-%p]; performance may suffer.",
			(void *)base, (void *)(base + size - 1));
		return ret;
	}
	return ret + MTRR_TO_PHYS_WC_OFFSET;
}
EXPORT_SYMBOL(arch_phys_wc_add);

 
void arch_phys_wc_del(int handle)
{
	if (handle >= 1) {
		WARN_ON(handle < MTRR_TO_PHYS_WC_OFFSET);
		mtrr_del(handle - MTRR_TO_PHYS_WC_OFFSET, 0, 0);
	}
}
EXPORT_SYMBOL(arch_phys_wc_del);

 
int arch_phys_wc_index(int handle)
{
	if (handle < MTRR_TO_PHYS_WC_OFFSET)
		return -1;
	else
		return handle - MTRR_TO_PHYS_WC_OFFSET;
}
EXPORT_SYMBOL_GPL(arch_phys_wc_index);

int __initdata changed_by_mtrr_cleanup;

 
void __init mtrr_bp_init(void)
{
	bool generic_mtrrs = cpu_feature_enabled(X86_FEATURE_MTRR);
	const char *why = "(not available)";
	unsigned long config, dummy;

	phys_hi_rsvd = GENMASK(31, boot_cpu_data.x86_phys_bits - 32);

	if (!generic_mtrrs && mtrr_state.enabled) {
		 
		init_table();
		mtrr_build_map();
		pr_info("MTRRs set to read-only\n");

		return;
	}

	if (generic_mtrrs)
		mtrr_if = &generic_mtrr_ops;
	else
		mtrr_set_if();

	if (mtrr_enabled()) {
		 
		if (mtrr_if == &generic_mtrr_ops)
			rdmsr(MSR_MTRRcap, config, dummy);
		else
			config = mtrr_if->var_regs;
		num_var_ranges = config & MTRR_CAP_VCNT;

		init_table();
		if (mtrr_if == &generic_mtrr_ops) {
			 
			if (get_mtrr_state()) {
				memory_caching_control |= CACHE_MTRR;
				changed_by_mtrr_cleanup = mtrr_cleanup();
				mtrr_build_map();
			} else {
				mtrr_if = NULL;
				why = "by BIOS";
			}
		}
	}

	if (!mtrr_enabled())
		pr_info("MTRRs disabled %s\n", why);
}

 
void mtrr_save_state(void)
{
	int first_cpu;

	if (!mtrr_enabled())
		return;

	first_cpu = cpumask_first(cpu_online_mask);
	smp_call_function_single(first_cpu, mtrr_save_fixed_ranges, NULL, 1);
}

static int __init mtrr_init_finalize(void)
{
	 
	mtrr_copy_map();

	if (!mtrr_enabled())
		return 0;

	if (memory_caching_control & CACHE_MTRR) {
		if (!changed_by_mtrr_cleanup)
			mtrr_state_warn();
		return 0;
	}

	mtrr_register_syscore();

	return 0;
}
subsys_initcall(mtrr_init_finalize);
