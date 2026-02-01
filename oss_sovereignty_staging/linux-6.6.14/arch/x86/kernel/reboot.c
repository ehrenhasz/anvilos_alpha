
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/export.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/efi.h>
#include <linux/dmi.h>
#include <linux/sched.h>
#include <linux/tboot.h>
#include <linux/delay.h>
#include <linux/objtool.h>
#include <linux/pgtable.h>
#include <acpi/reboot.h>
#include <asm/io.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/desc.h>
#include <asm/hpet.h>
#include <asm/proto.h>
#include <asm/reboot_fixups.h>
#include <asm/reboot.h>
#include <asm/pci_x86.h>
#include <asm/cpu.h>
#include <asm/nmi.h>
#include <asm/smp.h>

#include <linux/ctype.h>
#include <linux/mc146818rtc.h>
#include <asm/realmode.h>
#include <asm/x86_init.h>
#include <asm/efi.h>

 
void (*pm_power_off)(void);
EXPORT_SYMBOL(pm_power_off);

 
static int reboot_emergency;

 
bool port_cf9_safe = false;

 

 
static int __init set_acpi_reboot(const struct dmi_system_id *d)
{
	if (reboot_type != BOOT_ACPI) {
		reboot_type = BOOT_ACPI;
		pr_info("%s series board detected. Selecting %s-method for reboots.\n",
			d->ident, "ACPI");
	}
	return 0;
}

 
static int __init set_bios_reboot(const struct dmi_system_id *d)
{
	if (reboot_type != BOOT_BIOS) {
		reboot_type = BOOT_BIOS;
		pr_info("%s series board detected. Selecting %s-method for reboots.\n",
			d->ident, "BIOS");
	}
	return 0;
}

 
static int __init set_efi_reboot(const struct dmi_system_id *d)
{
	if (reboot_type != BOOT_EFI && !efi_runtime_disabled()) {
		reboot_type = BOOT_EFI;
		pr_info("%s series board detected. Selecting EFI-method for reboot.\n", d->ident);
	}
	return 0;
}

void __noreturn machine_real_restart(unsigned int type)
{
	local_irq_disable();

	 
	spin_lock(&rtc_lock);
	CMOS_WRITE(0x00, 0x8f);
	spin_unlock(&rtc_lock);

	 
	load_trampoline_pgtable();

	 
#ifdef CONFIG_X86_32
	asm volatile("jmpl *%0" : :
		     "rm" (real_mode_header->machine_real_restart_asm),
		     "a" (type));
#else
	asm volatile("ljmpl *%0" : :
		     "m" (real_mode_header->machine_real_restart_asm),
		     "D" (type));
#endif
	unreachable();
}
#ifdef CONFIG_APM_MODULE
EXPORT_SYMBOL(machine_real_restart);
#endif
STACK_FRAME_NON_STANDARD(machine_real_restart);

 
static int __init set_pci_reboot(const struct dmi_system_id *d)
{
	if (reboot_type != BOOT_CF9_FORCE) {
		reboot_type = BOOT_CF9_FORCE;
		pr_info("%s series board detected. Selecting %s-method for reboots.\n",
			d->ident, "PCI");
	}
	return 0;
}

static int __init set_kbd_reboot(const struct dmi_system_id *d)
{
	if (reboot_type != BOOT_KBD) {
		reboot_type = BOOT_KBD;
		pr_info("%s series board detected. Selecting %s-method for reboot.\n",
			d->ident, "KBD");
	}
	return 0;
}

 
static const struct dmi_system_id reboot_dmi_table[] __initconst = {

	 
	{	 
		.callback = set_kbd_reboot,
		.ident = "Acer Aspire One A110",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AOA110"),
		},
	},
	{	 
		.callback = set_efi_reboot,
		.ident = "Acer TravelMate X514-51T",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate X514-51T"),
		},
	},

	 
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple MacBook5",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "MacBook5"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple MacBook6,1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "MacBook6,1"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple MacBookPro5",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "MacBookPro5"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple Macmini3,1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Macmini3,1"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple iMac9,1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "iMac9,1"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Apple iMac10,1",
		.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "iMac10,1"),
		},
	},

	 
	{	 
		.callback = set_pci_reboot,
		.ident = "ASRock Q1900DC-ITX",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASRock"),
			DMI_MATCH(DMI_BOARD_NAME, "Q1900DC-ITX"),
		},
	},

	 
	{	 
		.callback = set_bios_reboot,
		.ident = "ASUS P4S800",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_BOARD_NAME, "P4S800"),
		},
	},
	{	 
		.callback = set_acpi_reboot,
		.ident = "ASUS EeeBook X205TA",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X205TA"),
		},
	},
	{	 
		.callback = set_acpi_reboot,
		.ident = "ASUS EeeBook X205TAW",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X205TAW"),
		},
	},

	 
	{        
		.callback = set_pci_reboot,
		.ident = "Certec BPC600",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Certec"),
			DMI_MATCH(DMI_PRODUCT_NAME, "BPC600"),
		},
	},

	 
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell DXP061",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Dell DXP061"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell E520",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Dell DM061"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell Latitude E5410",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude E5410"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell Latitude E5420",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude E5420"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell Latitude E6320",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude E6320"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell Latitude E6420",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude E6420"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 330",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 330"),
			DMI_MATCH(DMI_BOARD_NAME, "0KP561"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 360",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 360"),
			DMI_MATCH(DMI_BOARD_NAME, "0T656F"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 745",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 745"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 745",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 745"),
			DMI_MATCH(DMI_BOARD_NAME, "0MM599"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 745",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 745"),
			DMI_MATCH(DMI_BOARD_NAME, "0KW626"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell OptiPlex 760",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 760"),
			DMI_MATCH(DMI_BOARD_NAME, "0G919G"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell OptiPlex 990 BIOS A0x",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 990"),
			DMI_MATCH(DMI_BIOS_VERSION, "A0"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell PowerEdge 300",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 300/"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell PowerEdge 1300",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 1300/"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell PowerEdge 2400",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 2400"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell PowerEdge C6100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "C6100"),
		},
	},
	{	 
		.callback = set_pci_reboot,
		.ident = "Dell Precision M6600",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Precision M6600"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell Precision T5400",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Precision WorkStation T5400"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell Precision T7400",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Precision WorkStation T7400"),
		},
	},
	{	 
		.callback = set_bios_reboot,
		.ident = "Dell XPS710",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Dell XPS710"),
		},
	},
	{	 
		.callback = set_acpi_reboot,
		.ident = "Dell OptiPlex 7450 AIO",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex 7450 AIO"),
		},
	},

	 
	{	 
		.callback = set_bios_reboot,
		.ident = "HP Compaq Laptop",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HP Compaq"),
		},
	},

	{	 
		.callback = set_pci_reboot,
		.ident = "Zotac ZBOX CI327 nano",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "NA"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ZBOX-CI327NANO-GS-01"),
		},
	},

	 
	{	 
		.callback = set_bios_reboot,
		.ident = "Sony VGN-Z540N",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VGN-Z540N"),
		},
	},

	{ }
};

static int __init reboot_init(void)
{
	int rv;

	 
	if (!reboot_default)
		return 0;

	 
	rv = dmi_check_system(reboot_dmi_table);

	if (!rv && efi_reboot_required() && !efi_runtime_disabled())
		reboot_type = BOOT_EFI;

	return 0;
}
core_initcall(reboot_init);

static inline void kb_wait(void)
{
	int i;

	for (i = 0; i < 0x10000; i++) {
		if ((inb(0x64) & 0x02) == 0)
			break;
		udelay(2);
	}
}

static inline void nmi_shootdown_cpus_on_restart(void);

#if IS_ENABLED(CONFIG_KVM_INTEL) || IS_ENABLED(CONFIG_KVM_AMD)
 
static cpu_emergency_virt_cb __rcu *cpu_emergency_virt_callback;

void cpu_emergency_register_virt_callback(cpu_emergency_virt_cb *callback)
{
	if (WARN_ON_ONCE(rcu_access_pointer(cpu_emergency_virt_callback)))
		return;

	rcu_assign_pointer(cpu_emergency_virt_callback, callback);
}
EXPORT_SYMBOL_GPL(cpu_emergency_register_virt_callback);

void cpu_emergency_unregister_virt_callback(cpu_emergency_virt_cb *callback)
{
	if (WARN_ON_ONCE(rcu_access_pointer(cpu_emergency_virt_callback) != callback))
		return;

	rcu_assign_pointer(cpu_emergency_virt_callback, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(cpu_emergency_unregister_virt_callback);

 
void cpu_emergency_disable_virtualization(void)
{
	cpu_emergency_virt_cb *callback;

	 
	lockdep_assert_irqs_disabled();

	rcu_read_lock();
	callback = rcu_dereference(cpu_emergency_virt_callback);
	if (callback)
		callback();
	rcu_read_unlock();
}

static void emergency_reboot_disable_virtualization(void)
{
	local_irq_disable();

	 
	if (rcu_access_pointer(cpu_emergency_virt_callback)) {
		 
		cpu_emergency_disable_virtualization();

		 
		nmi_shootdown_cpus_on_restart();
	}
}
#else
static void emergency_reboot_disable_virtualization(void) { }
#endif  

void __attribute__((weak)) mach_reboot_fixups(void)
{
}

 
static void native_machine_emergency_restart(void)
{
	int i;
	int attempt = 0;
	int orig_reboot_type = reboot_type;
	unsigned short mode;

	if (reboot_emergency)
		emergency_reboot_disable_virtualization();

	tboot_shutdown(TB_SHUTDOWN_REBOOT);

	 
	mode = reboot_mode == REBOOT_WARM ? 0x1234 : 0;
	*((unsigned short *)__va(0x472)) = mode;

	 
	if (efi_capsule_pending(NULL)) {
		pr_info("EFI capsule is pending, forcing EFI reboot.\n");
		reboot_type = BOOT_EFI;
	}

	for (;;) {
		 
		switch (reboot_type) {
		case BOOT_ACPI:
			acpi_reboot();
			reboot_type = BOOT_KBD;
			break;

		case BOOT_KBD:
			mach_reboot_fixups();  

			for (i = 0; i < 10; i++) {
				kb_wait();
				udelay(50);
				outb(0xfe, 0x64);  
				udelay(50);
			}
			if (attempt == 0 && orig_reboot_type == BOOT_ACPI) {
				attempt = 1;
				reboot_type = BOOT_ACPI;
			} else {
				reboot_type = BOOT_EFI;
			}
			break;

		case BOOT_EFI:
			efi_reboot(reboot_mode, NULL);
			reboot_type = BOOT_BIOS;
			break;

		case BOOT_BIOS:
			machine_real_restart(MRR_BIOS);

			 
			reboot_type = BOOT_CF9_SAFE;
			break;

		case BOOT_CF9_FORCE:
			port_cf9_safe = true;
			fallthrough;

		case BOOT_CF9_SAFE:
			if (port_cf9_safe) {
				u8 reboot_code = reboot_mode == REBOOT_WARM ?  0x06 : 0x0E;
				u8 cf9 = inb(0xcf9) & ~reboot_code;
				outb(cf9|2, 0xcf9);  
				udelay(50);
				 
				outb(cf9|reboot_code, 0xcf9);
				udelay(50);
			}
			reboot_type = BOOT_TRIPLE;
			break;

		case BOOT_TRIPLE:
			idt_invalidate();
			__asm__ __volatile__("int3");

			 
			reboot_type = BOOT_KBD;
			break;
		}
	}
}

void native_machine_shutdown(void)
{
	 
#ifdef CONFIG_X86_IO_APIC
	 
	clear_IO_APIC();
#endif

#ifdef CONFIG_SMP
	 
	local_irq_disable();
	stop_other_cpus();
#endif

	lapic_shutdown();
	restore_boot_irq_mode();

#ifdef CONFIG_HPET_TIMER
	hpet_disable();
#endif

#ifdef CONFIG_X86_64
	x86_platform.iommu_shutdown();
#endif
}

static void __machine_emergency_restart(int emergency)
{
	reboot_emergency = emergency;
	machine_ops.emergency_restart();
}

static void native_machine_restart(char *__unused)
{
	pr_notice("machine restart\n");

	if (!reboot_force)
		machine_shutdown();
	__machine_emergency_restart(0);
}

static void native_machine_halt(void)
{
	 
	machine_shutdown();

	tboot_shutdown(TB_SHUTDOWN_HALT);

	stop_this_cpu(NULL);
}

static void native_machine_power_off(void)
{
	if (kernel_can_power_off()) {
		if (!reboot_force)
			machine_shutdown();
		do_kernel_power_off();
	}
	 
	tboot_shutdown(TB_SHUTDOWN_HALT);
}

struct machine_ops machine_ops __ro_after_init = {
	.power_off = native_machine_power_off,
	.shutdown = native_machine_shutdown,
	.emergency_restart = native_machine_emergency_restart,
	.restart = native_machine_restart,
	.halt = native_machine_halt,
#ifdef CONFIG_KEXEC_CORE
	.crash_shutdown = native_machine_crash_shutdown,
#endif
};

void machine_power_off(void)
{
	machine_ops.power_off();
}

void machine_shutdown(void)
{
	machine_ops.shutdown();
}

void machine_emergency_restart(void)
{
	__machine_emergency_restart(1);
}

void machine_restart(char *cmd)
{
	machine_ops.restart(cmd);
}

void machine_halt(void)
{
	machine_ops.halt();
}

#ifdef CONFIG_KEXEC_CORE
void machine_crash_shutdown(struct pt_regs *regs)
{
	machine_ops.crash_shutdown(regs);
}
#endif

 
int crashing_cpu = -1;

#if defined(CONFIG_SMP)

static nmi_shootdown_cb shootdown_callback;

static atomic_t waiting_for_crash_ipi;
static int crash_ipi_issued;

static int crash_nmi_callback(unsigned int val, struct pt_regs *regs)
{
	int cpu;

	cpu = raw_smp_processor_id();

	 
	if (cpu == crashing_cpu)
		return NMI_HANDLED;
	local_irq_disable();

	if (shootdown_callback)
		shootdown_callback(cpu, regs);

	 
	cpu_emergency_disable_virtualization();

	atomic_dec(&waiting_for_crash_ipi);
	 
	halt();
	for (;;)
		cpu_relax();

	return NMI_HANDLED;
}

 
void nmi_shootdown_cpus(nmi_shootdown_cb callback)
{
	unsigned long msecs;

	local_irq_disable();

	 
	if (WARN_ON_ONCE(crash_ipi_issued))
		return;

	 
	crashing_cpu = safe_smp_processor_id();

	shootdown_callback = callback;

	atomic_set(&waiting_for_crash_ipi, num_online_cpus() - 1);
	 
	if (register_nmi_handler(NMI_LOCAL, crash_nmi_callback,
				 NMI_FLAG_FIRST, "crash"))
		return;		 
	 
	wmb();

	apic_send_IPI_allbutself(NMI_VECTOR);

	 
	WRITE_ONCE(crash_ipi_issued, 1);

	msecs = 1000;  
	while ((atomic_read(&waiting_for_crash_ipi) > 0) && msecs) {
		mdelay(1);
		msecs--;
	}

	 
}

static inline void nmi_shootdown_cpus_on_restart(void)
{
	if (!crash_ipi_issued)
		nmi_shootdown_cpus(NULL);
}

 
void run_crash_ipi_callback(struct pt_regs *regs)
{
	if (crash_ipi_issued)
		crash_nmi_callback(0, regs);
}

 
void __noreturn nmi_panic_self_stop(struct pt_regs *regs)
{
	while (1) {
		 
		run_crash_ipi_callback(regs);
		cpu_relax();
	}
}

#else  
void nmi_shootdown_cpus(nmi_shootdown_cb callback)
{
	 
}

static inline void nmi_shootdown_cpus_on_restart(void) { }

void run_crash_ipi_callback(struct pt_regs *regs)
{
}
#endif
