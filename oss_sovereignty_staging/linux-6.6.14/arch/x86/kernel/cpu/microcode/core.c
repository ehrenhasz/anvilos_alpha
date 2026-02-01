
 

#define pr_fmt(fmt) "microcode: " fmt

#include <linux/platform_device.h>
#include <linux/stop_machine.h>
#include <linux/syscore_ops.h>
#include <linux/miscdevice.h>
#include <linux/capability.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include <asm/cpu_device_id.h>
#include <asm/perf_event.h>
#include <asm/processor.h>
#include <asm/cmdline.h>
#include <asm/setup.h>

#include "internal.h"

#define DRIVER_VERSION	"2.2"

static struct microcode_ops	*microcode_ops;
static bool dis_ucode_ldr = true;

bool initrd_gone;

LIST_HEAD(microcode_cache);

 
struct ucode_cpu_info		ucode_cpu_info[NR_CPUS];

struct cpu_info_ctx {
	struct cpu_signature	*cpu_sig;
	int			err;
};

 
static u32 final_levels[] = {
	0x01000098,
	0x0100009f,
	0x010000af,
	0,  
};

 
static bool amd_check_current_patch_level(void)
{
	u32 lvl, dummy, i;
	u32 *levels;

	native_rdmsr(MSR_AMD64_PATCH_LEVEL, lvl, dummy);

	if (IS_ENABLED(CONFIG_X86_32))
		levels = (u32 *)__pa_nodebug(&final_levels);
	else
		levels = final_levels;

	for (i = 0; levels[i]; i++) {
		if (lvl == levels[i])
			return true;
	}
	return false;
}

static bool __init check_loader_disabled_bsp(void)
{
	static const char *__dis_opt_str = "dis_ucode_ldr";

#ifdef CONFIG_X86_32
	const char *cmdline = (const char *)__pa_nodebug(boot_command_line);
	const char *option  = (const char *)__pa_nodebug(__dis_opt_str);
	bool *res = (bool *)__pa_nodebug(&dis_ucode_ldr);

#else  
	const char *cmdline = boot_command_line;
	const char *option  = __dis_opt_str;
	bool *res = &dis_ucode_ldr;
#endif

	 
	if (native_cpuid_ecx(1) & BIT(31))
		return *res;

	if (x86_cpuid_vendor() == X86_VENDOR_AMD) {
		if (amd_check_current_patch_level())
			return *res;
	}

	if (cmdline_find_option_bool(cmdline, option) <= 0)
		*res = false;

	return *res;
}

void __init load_ucode_bsp(void)
{
	unsigned int cpuid_1_eax;
	bool intel = true;

	if (!have_cpuid_p())
		return;

	cpuid_1_eax = native_cpuid_eax(1);

	switch (x86_cpuid_vendor()) {
	case X86_VENDOR_INTEL:
		if (x86_family(cpuid_1_eax) < 6)
			return;
		break;

	case X86_VENDOR_AMD:
		if (x86_family(cpuid_1_eax) < 0x10)
			return;
		intel = false;
		break;

	default:
		return;
	}

	if (check_loader_disabled_bsp())
		return;

	if (intel)
		load_ucode_intel_bsp();
	else
		load_ucode_amd_early(cpuid_1_eax);
}

static bool check_loader_disabled_ap(void)
{
#ifdef CONFIG_X86_32
	return *((bool *)__pa_nodebug(&dis_ucode_ldr));
#else
	return dis_ucode_ldr;
#endif
}

void load_ucode_ap(void)
{
	unsigned int cpuid_1_eax;

	if (check_loader_disabled_ap())
		return;

	cpuid_1_eax = native_cpuid_eax(1);

	switch (x86_cpuid_vendor()) {
	case X86_VENDOR_INTEL:
		if (x86_family(cpuid_1_eax) >= 6)
			load_ucode_intel_ap();
		break;
	case X86_VENDOR_AMD:
		if (x86_family(cpuid_1_eax) >= 0x10)
			load_ucode_amd_early(cpuid_1_eax);
		break;
	default:
		break;
	}
}

static int __init save_microcode_in_initrd(void)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	int ret = -EINVAL;

	if (dis_ucode_ldr) {
		ret = 0;
		goto out;
	}

	switch (c->x86_vendor) {
	case X86_VENDOR_INTEL:
		if (c->x86 >= 6)
			ret = save_microcode_in_initrd_intel();
		break;
	case X86_VENDOR_AMD:
		if (c->x86 >= 0x10)
			ret = save_microcode_in_initrd_amd(cpuid_eax(1));
		break;
	default:
		break;
	}

out:
	initrd_gone = true;

	return ret;
}

struct cpio_data find_microcode_in_initrd(const char *path, bool use_pa)
{
#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long start = 0;
	size_t size;

#ifdef CONFIG_X86_32
	struct boot_params *params;

	if (use_pa)
		params = (struct boot_params *)__pa_nodebug(&boot_params);
	else
		params = &boot_params;

	size = params->hdr.ramdisk_size;

	 
	if (size)
		start = params->hdr.ramdisk_image;

# else  
	size  = (unsigned long)boot_params.ext_ramdisk_size << 32;
	size |= boot_params.hdr.ramdisk_size;

	if (size) {
		start  = (unsigned long)boot_params.ext_ramdisk_image << 32;
		start |= boot_params.hdr.ramdisk_image;

		start += PAGE_OFFSET;
	}
# endif

	 
	if (!use_pa) {
		if (initrd_gone)
			return (struct cpio_data){ NULL, 0, "" };
		if (initrd_start)
			start = initrd_start;
	} else {
		 
		u64 *rr = (u64 *)__pa_nodebug(&relocated_ramdisk);
		if (*rr)
			start = *rr;
	}

	return find_cpio_data(path, (void *)start, size, NULL);
#else  
	return (struct cpio_data){ NULL, 0, "" };
#endif
}

static void reload_early_microcode(unsigned int cpu)
{
	int vendor, family;

	vendor = x86_cpuid_vendor();
	family = x86_cpuid_family();

	switch (vendor) {
	case X86_VENDOR_INTEL:
		if (family >= 6)
			reload_ucode_intel();
		break;
	case X86_VENDOR_AMD:
		if (family >= 0x10)
			reload_ucode_amd(cpu);
		break;
	default:
		break;
	}
}

 
static struct platform_device	*microcode_pdev;

#ifdef CONFIG_MICROCODE_LATE_LOADING
 
#define SPINUNIT 100  

static int check_online_cpus(void)
{
	unsigned int cpu;

	 
	for_each_present_cpu(cpu) {
		if (topology_is_primary_thread(cpu) && !cpu_online(cpu)) {
			pr_err("Not all CPUs online, aborting microcode update.\n");
			return -EINVAL;
		}
	}

	return 0;
}

static atomic_t late_cpus_in;
static atomic_t late_cpus_out;

static int __wait_for_cpus(atomic_t *t, long long timeout)
{
	int all_cpus = num_online_cpus();

	atomic_inc(t);

	while (atomic_read(t) < all_cpus) {
		if (timeout < SPINUNIT) {
			pr_err("Timeout while waiting for CPUs rendezvous, remaining: %d\n",
				all_cpus - atomic_read(t));
			return 1;
		}

		ndelay(SPINUNIT);
		timeout -= SPINUNIT;

		touch_nmi_watchdog();
	}
	return 0;
}

 
static int __reload_late(void *info)
{
	int cpu = smp_processor_id();
	enum ucode_state err;
	int ret = 0;

	 
	if (__wait_for_cpus(&late_cpus_in, NSEC_PER_SEC))
		return -1;

	 
	if (cpumask_first(topology_sibling_cpumask(cpu)) == cpu)
		err = microcode_ops->apply_microcode(cpu);
	else
		goto wait_for_siblings;

	if (err >= UCODE_NFOUND) {
		if (err == UCODE_ERROR) {
			pr_warn("Error reloading microcode on CPU %d\n", cpu);
			ret = -1;
		}
	}

wait_for_siblings:
	if (__wait_for_cpus(&late_cpus_out, NSEC_PER_SEC))
		panic("Timeout during microcode update!\n");

	 
	if (cpumask_first(topology_sibling_cpumask(cpu)) != cpu)
		err = microcode_ops->apply_microcode(cpu);

	return ret;
}

 
static int microcode_reload_late(void)
{
	int old = boot_cpu_data.microcode, ret;
	struct cpuinfo_x86 prev_info;

	pr_err("Attempting late microcode loading - it is dangerous and taints the kernel.\n");
	pr_err("You should switch to early loading, if possible.\n");

	atomic_set(&late_cpus_in,  0);
	atomic_set(&late_cpus_out, 0);

	 
	store_cpu_caps(&prev_info);

	ret = stop_machine_cpuslocked(__reload_late, NULL, cpu_online_mask);
	if (!ret) {
		pr_info("Reload succeeded, microcode revision: 0x%x -> 0x%x\n",
			old, boot_cpu_data.microcode);
		microcode_check(&prev_info);
	} else {
		pr_info("Reload failed, current microcode revision: 0x%x\n",
			boot_cpu_data.microcode);
	}

	return ret;
}

static ssize_t reload_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	enum ucode_state tmp_ret = UCODE_OK;
	int bsp = boot_cpu_data.cpu_index;
	unsigned long val;
	ssize_t ret = 0;

	ret = kstrtoul(buf, 0, &val);
	if (ret || val != 1)
		return -EINVAL;

	cpus_read_lock();

	ret = check_online_cpus();
	if (ret)
		goto put;

	tmp_ret = microcode_ops->request_microcode_fw(bsp, &microcode_pdev->dev);
	if (tmp_ret != UCODE_NEW)
		goto put;

	ret = microcode_reload_late();
put:
	cpus_read_unlock();

	if (ret == 0)
		ret = size;

	add_taint(TAINT_CPU_OUT_OF_SPEC, LOCKDEP_STILL_OK);

	return ret;
}

static DEVICE_ATTR_WO(reload);
#endif

static ssize_t version_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ucode_cpu_info *uci = ucode_cpu_info + dev->id;

	return sprintf(buf, "0x%x\n", uci->cpu_sig.rev);
}

static ssize_t processor_flags_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ucode_cpu_info *uci = ucode_cpu_info + dev->id;

	return sprintf(buf, "0x%x\n", uci->cpu_sig.pf);
}

static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(processor_flags);

static struct attribute *mc_default_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_processor_flags.attr,
	NULL
};

static const struct attribute_group mc_attr_group = {
	.attrs			= mc_default_attrs,
	.name			= "microcode",
};

static void microcode_fini_cpu(int cpu)
{
	if (microcode_ops->microcode_fini_cpu)
		microcode_ops->microcode_fini_cpu(cpu);
}

static enum ucode_state microcode_init_cpu(int cpu)
{
	struct ucode_cpu_info *uci = ucode_cpu_info + cpu;

	memset(uci, 0, sizeof(*uci));

	microcode_ops->collect_cpu_info(cpu, &uci->cpu_sig);

	return microcode_ops->apply_microcode(cpu);
}

 
void microcode_bsp_resume(void)
{
	int cpu = smp_processor_id();
	struct ucode_cpu_info *uci = ucode_cpu_info + cpu;

	if (uci->mc)
		microcode_ops->apply_microcode(cpu);
	else
		reload_early_microcode(cpu);
}

static struct syscore_ops mc_syscore_ops = {
	.resume	= microcode_bsp_resume,
};

static int mc_cpu_starting(unsigned int cpu)
{
	enum ucode_state err = microcode_ops->apply_microcode(cpu);

	pr_debug("%s: CPU%d, err: %d\n", __func__, cpu, err);

	return err == UCODE_ERROR;
}

static int mc_cpu_online(unsigned int cpu)
{
	struct device *dev = get_cpu_device(cpu);

	if (sysfs_create_group(&dev->kobj, &mc_attr_group))
		pr_err("Failed to create group for CPU%d\n", cpu);
	return 0;
}

static int mc_cpu_down_prep(unsigned int cpu)
{
	struct device *dev;

	dev = get_cpu_device(cpu);

	microcode_fini_cpu(cpu);

	 
	sysfs_remove_group(&dev->kobj, &mc_attr_group);
	pr_debug("%s: CPU%d\n", __func__, cpu);

	return 0;
}

static void setup_online_cpu(struct work_struct *work)
{
	int cpu = smp_processor_id();
	enum ucode_state err;

	err = microcode_init_cpu(cpu);
	if (err == UCODE_ERROR) {
		pr_err("Error applying microcode on CPU%d\n", cpu);
		return;
	}

	mc_cpu_online(cpu);
}

static struct attribute *cpu_root_microcode_attrs[] = {
#ifdef CONFIG_MICROCODE_LATE_LOADING
	&dev_attr_reload.attr,
#endif
	NULL
};

static const struct attribute_group cpu_root_microcode_group = {
	.name  = "microcode",
	.attrs = cpu_root_microcode_attrs,
};

static int __init microcode_init(void)
{
	struct device *dev_root;
	struct cpuinfo_x86 *c = &boot_cpu_data;
	int error;

	if (dis_ucode_ldr)
		return -EINVAL;

	if (c->x86_vendor == X86_VENDOR_INTEL)
		microcode_ops = init_intel_microcode();
	else if (c->x86_vendor == X86_VENDOR_AMD)
		microcode_ops = init_amd_microcode();
	else
		pr_err("no support for this CPU vendor\n");

	if (!microcode_ops)
		return -ENODEV;

	microcode_pdev = platform_device_register_simple("microcode", -1, NULL, 0);
	if (IS_ERR(microcode_pdev))
		return PTR_ERR(microcode_pdev);

	dev_root = bus_get_dev_root(&cpu_subsys);
	if (dev_root) {
		error = sysfs_create_group(&dev_root->kobj, &cpu_root_microcode_group);
		put_device(dev_root);
		if (error) {
			pr_err("Error creating microcode group!\n");
			goto out_pdev;
		}
	}

	 
	schedule_on_each_cpu(setup_online_cpu);

	register_syscore_ops(&mc_syscore_ops);
	cpuhp_setup_state_nocalls(CPUHP_AP_MICROCODE_LOADER, "x86/microcode:starting",
				  mc_cpu_starting, NULL);
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "x86/microcode:online",
				  mc_cpu_online, mc_cpu_down_prep);

	pr_info("Microcode Update Driver: v%s.", DRIVER_VERSION);

	return 0;

 out_pdev:
	platform_device_unregister(microcode_pdev);
	return error;

}
fs_initcall(save_microcode_in_initrd);
late_initcall(microcode_init);
