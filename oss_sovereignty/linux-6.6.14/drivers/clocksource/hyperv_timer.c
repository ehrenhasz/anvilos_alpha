

 

#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/sched_clock.h>
#include <linux/mm.h>
#include <linux/cpuhotplug.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/hyperv.h>
#include <clocksource/hyperv_timer.h>
#include <asm/hyperv-tlfs.h>
#include <asm/mshyperv.h>

static struct clock_event_device __percpu *hv_clock_event;
static u64 hv_sched_clock_offset __ro_after_init;

 
static bool direct_mode_enabled;

static int stimer0_irq = -1;
static int stimer0_message_sint;
static __maybe_unused DEFINE_PER_CPU(long, stimer0_evt);

 
void hv_stimer0_isr(void)
{
	struct clock_event_device *ce;

	ce = this_cpu_ptr(hv_clock_event);
	ce->event_handler(ce);
}
EXPORT_SYMBOL_GPL(hv_stimer0_isr);

 
static irqreturn_t __maybe_unused hv_stimer0_percpu_isr(int irq, void *dev_id)
{
	hv_stimer0_isr();
	return IRQ_HANDLED;
}

static int hv_ce_set_next_event(unsigned long delta,
				struct clock_event_device *evt)
{
	u64 current_tick;

	current_tick = hv_read_reference_counter();
	current_tick += delta;
	hv_set_register(HV_REGISTER_STIMER0_COUNT, current_tick);
	return 0;
}

static int hv_ce_shutdown(struct clock_event_device *evt)
{
	hv_set_register(HV_REGISTER_STIMER0_COUNT, 0);
	hv_set_register(HV_REGISTER_STIMER0_CONFIG, 0);
	if (direct_mode_enabled && stimer0_irq >= 0)
		disable_percpu_irq(stimer0_irq);

	return 0;
}

static int hv_ce_set_oneshot(struct clock_event_device *evt)
{
	union hv_stimer_config timer_cfg;

	timer_cfg.as_uint64 = 0;
	timer_cfg.enable = 1;
	timer_cfg.auto_enable = 1;
	if (direct_mode_enabled) {
		 
		timer_cfg.direct_mode = 1;
		timer_cfg.apic_vector = HYPERV_STIMER0_VECTOR;
		if (stimer0_irq >= 0)
			enable_percpu_irq(stimer0_irq, IRQ_TYPE_NONE);
	} else {
		 
		timer_cfg.direct_mode = 0;
		timer_cfg.sintx = stimer0_message_sint;
	}
	hv_set_register(HV_REGISTER_STIMER0_CONFIG, timer_cfg.as_uint64);
	return 0;
}

 
static int hv_stimer_init(unsigned int cpu)
{
	struct clock_event_device *ce;

	if (!hv_clock_event)
		return 0;

	ce = per_cpu_ptr(hv_clock_event, cpu);
	ce->name = "Hyper-V clockevent";
	ce->features = CLOCK_EVT_FEAT_ONESHOT;
	ce->cpumask = cpumask_of(cpu);
	ce->rating = 1000;
	ce->set_state_shutdown = hv_ce_shutdown;
	ce->set_state_oneshot = hv_ce_set_oneshot;
	ce->set_next_event = hv_ce_set_next_event;

	clockevents_config_and_register(ce,
					HV_CLOCK_HZ,
					HV_MIN_DELTA_TICKS,
					HV_MAX_MAX_DELTA_TICKS);
	return 0;
}

 
int hv_stimer_cleanup(unsigned int cpu)
{
	struct clock_event_device *ce;

	if (!hv_clock_event)
		return 0;

	 
	ce = per_cpu_ptr(hv_clock_event, cpu);
	if (direct_mode_enabled)
		hv_ce_shutdown(ce);
	else
		clockevents_unbind_device(ce, cpu);

	return 0;
}
EXPORT_SYMBOL_GPL(hv_stimer_cleanup);

 
void __weak hv_setup_stimer0_handler(void (*handler)(void))
{
};

void __weak hv_remove_stimer0_handler(void)
{
};

#ifdef CONFIG_ACPI
 
static int hv_setup_stimer0_irq(void)
{
	int ret;

	ret = acpi_register_gsi(NULL, HYPERV_STIMER0_VECTOR,
			ACPI_EDGE_SENSITIVE, ACPI_ACTIVE_HIGH);
	if (ret < 0) {
		pr_err("Can't register Hyper-V stimer0 GSI. Error %d", ret);
		return ret;
	}
	stimer0_irq = ret;

	ret = request_percpu_irq(stimer0_irq, hv_stimer0_percpu_isr,
		"Hyper-V stimer0", &stimer0_evt);
	if (ret) {
		pr_err("Can't request Hyper-V stimer0 IRQ %d. Error %d",
			stimer0_irq, ret);
		acpi_unregister_gsi(stimer0_irq);
		stimer0_irq = -1;
	}
	return ret;
}

static void hv_remove_stimer0_irq(void)
{
	if (stimer0_irq == -1) {
		hv_remove_stimer0_handler();
	} else {
		free_percpu_irq(stimer0_irq, &stimer0_evt);
		acpi_unregister_gsi(stimer0_irq);
		stimer0_irq = -1;
	}
}
#else
static int hv_setup_stimer0_irq(void)
{
	return 0;
}

static void hv_remove_stimer0_irq(void)
{
}
#endif

 
int hv_stimer_alloc(bool have_percpu_irqs)
{
	int ret;

	 
	if (!(ms_hyperv.features & HV_MSR_SYNTIMER_AVAILABLE))
		return -EINVAL;

	hv_clock_event = alloc_percpu(struct clock_event_device);
	if (!hv_clock_event)
		return -ENOMEM;

	direct_mode_enabled = ms_hyperv.misc_features &
			HV_STIMER_DIRECT_MODE_AVAILABLE;

	 
	if (!direct_mode_enabled)
		return 0;

	if (have_percpu_irqs) {
		ret = hv_setup_stimer0_irq();
		if (ret)
			goto free_clock_event;
	} else {
		hv_setup_stimer0_handler(hv_stimer0_isr);
	}

	 
	ret = cpuhp_setup_state(CPUHP_AP_HYPERV_TIMER_STARTING,
			"clockevents/hyperv/stimer:starting",
			hv_stimer_init, hv_stimer_cleanup);
	if (ret < 0) {
		hv_remove_stimer0_irq();
		goto free_clock_event;
	}
	return ret;

free_clock_event:
	free_percpu(hv_clock_event);
	hv_clock_event = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(hv_stimer_alloc);

 
void hv_stimer_legacy_init(unsigned int cpu, int sint)
{
	if (direct_mode_enabled)
		return;

	 
	stimer0_message_sint = sint;
	(void)hv_stimer_init(cpu);
}
EXPORT_SYMBOL_GPL(hv_stimer_legacy_init);

 
void hv_stimer_legacy_cleanup(unsigned int cpu)
{
	if (direct_mode_enabled)
		return;
	(void)hv_stimer_cleanup(cpu);
}
EXPORT_SYMBOL_GPL(hv_stimer_legacy_cleanup);

 
void hv_stimer_global_cleanup(void)
{
	int	cpu;

	 
	for_each_present_cpu(cpu) {
		hv_stimer_legacy_cleanup(cpu);
	}

	if (!hv_clock_event)
		return;

	if (direct_mode_enabled) {
		cpuhp_remove_state(CPUHP_AP_HYPERV_TIMER_STARTING);
		hv_remove_stimer0_irq();
		stimer0_irq = -1;
	}
	free_percpu(hv_clock_event);
	hv_clock_event = NULL;

}
EXPORT_SYMBOL_GPL(hv_stimer_global_cleanup);

static __always_inline u64 read_hv_clock_msr(void)
{
	 
	return hv_raw_get_register(HV_REGISTER_TIME_REF_COUNT);
}

 

static union {
	struct ms_hyperv_tsc_page page;
	u8 reserved[PAGE_SIZE];
} tsc_pg __bss_decrypted __aligned(PAGE_SIZE);

static struct ms_hyperv_tsc_page *tsc_page = &tsc_pg.page;
static unsigned long tsc_pfn;

unsigned long hv_get_tsc_pfn(void)
{
	return tsc_pfn;
}
EXPORT_SYMBOL_GPL(hv_get_tsc_pfn);

struct ms_hyperv_tsc_page *hv_get_tsc_page(void)
{
	return tsc_page;
}
EXPORT_SYMBOL_GPL(hv_get_tsc_page);

static __always_inline u64 read_hv_clock_tsc(void)
{
	u64 cur_tsc, time;

	 
	if (!hv_read_tsc_page_tsc(tsc_page, &cur_tsc, &time))
		time = read_hv_clock_msr();

	return time;
}

static u64 notrace read_hv_clock_tsc_cs(struct clocksource *arg)
{
	return read_hv_clock_tsc();
}

static u64 noinstr read_hv_sched_clock_tsc(void)
{
	return (read_hv_clock_tsc() - hv_sched_clock_offset) *
		(NSEC_PER_SEC / HV_CLOCK_HZ);
}

static void suspend_hv_clock_tsc(struct clocksource *arg)
{
	union hv_reference_tsc_msr tsc_msr;

	 
	tsc_msr.as_uint64 = hv_get_register(HV_REGISTER_REFERENCE_TSC);
	tsc_msr.enable = 0;
	hv_set_register(HV_REGISTER_REFERENCE_TSC, tsc_msr.as_uint64);
}


static void resume_hv_clock_tsc(struct clocksource *arg)
{
	union hv_reference_tsc_msr tsc_msr;

	 
	tsc_msr.as_uint64 = hv_get_register(HV_REGISTER_REFERENCE_TSC);
	tsc_msr.enable = 1;
	tsc_msr.pfn = tsc_pfn;
	hv_set_register(HV_REGISTER_REFERENCE_TSC, tsc_msr.as_uint64);
}

#ifdef HAVE_VDSO_CLOCKMODE_HVCLOCK
static int hv_cs_enable(struct clocksource *cs)
{
	vclocks_set_used(VDSO_CLOCKMODE_HVCLOCK);
	return 0;
}
#endif

static struct clocksource hyperv_cs_tsc = {
	.name	= "hyperv_clocksource_tsc_page",
	.rating	= 500,
	.read	= read_hv_clock_tsc_cs,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
	.suspend= suspend_hv_clock_tsc,
	.resume	= resume_hv_clock_tsc,
#ifdef HAVE_VDSO_CLOCKMODE_HVCLOCK
	.enable = hv_cs_enable,
	.vdso_clock_mode = VDSO_CLOCKMODE_HVCLOCK,
#else
	.vdso_clock_mode = VDSO_CLOCKMODE_NONE,
#endif
};

static u64 notrace read_hv_clock_msr_cs(struct clocksource *arg)
{
	return read_hv_clock_msr();
}

static struct clocksource hyperv_cs_msr = {
	.name	= "hyperv_clocksource_msr",
	.rating	= 495,
	.read	= read_hv_clock_msr_cs,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

 
#ifdef CONFIG_GENERIC_SCHED_CLOCK
static __always_inline void hv_setup_sched_clock(void *sched_clock)
{
	 
	sched_clock_register(sched_clock, 64, NSEC_PER_SEC);
}
#elif defined CONFIG_PARAVIRT
static __always_inline void hv_setup_sched_clock(void *sched_clock)
{
	 
	paravirt_set_sched_clock(sched_clock);
}
#else  
static __always_inline void hv_setup_sched_clock(void *sched_clock) {}
#endif  

static void __init hv_init_tsc_clocksource(void)
{
	union hv_reference_tsc_msr tsc_msr;

	 
	if (ms_hyperv.features & HV_ACCESS_TSC_INVARIANT) {
		hyperv_cs_tsc.rating = 250;
		hyperv_cs_msr.rating = 245;
	}

	if (!(ms_hyperv.features & HV_MSR_REFERENCE_TSC_AVAILABLE))
		return;

	hv_read_reference_counter = read_hv_clock_tsc;

	 
	tsc_msr.as_uint64 = hv_get_register(HV_REGISTER_REFERENCE_TSC);
	if (hv_root_partition)
		tsc_pfn = tsc_msr.pfn;
	else
		tsc_pfn = HVPFN_DOWN(virt_to_phys(tsc_page));
	tsc_msr.enable = 1;
	tsc_msr.pfn = tsc_pfn;
	hv_set_register(HV_REGISTER_REFERENCE_TSC, tsc_msr.as_uint64);

	clocksource_register_hz(&hyperv_cs_tsc, NSEC_PER_SEC/100);

	 
	if (!(ms_hyperv.features & HV_ACCESS_TSC_INVARIANT)) {
		hv_sched_clock_offset = hv_read_reference_counter();
		hv_setup_sched_clock(read_hv_sched_clock_tsc);
	}
}

void __init hv_init_clocksource(void)
{
	 
	hv_init_tsc_clocksource();

	if (ms_hyperv.features & HV_MSR_TIME_REF_COUNT_AVAILABLE)
		clocksource_register_hz(&hyperv_cs_msr, NSEC_PER_SEC/100);
}

void __init hv_remap_tsc_clocksource(void)
{
	if (!(ms_hyperv.features & HV_MSR_REFERENCE_TSC_AVAILABLE))
		return;

	if (!hv_root_partition) {
		WARN(1, "%s: attempt to remap TSC page in guest partition\n",
		     __func__);
		return;
	}

	tsc_page = memremap(tsc_pfn << HV_HYP_PAGE_SHIFT, sizeof(tsc_pg),
			    MEMREMAP_WB);
	if (!tsc_page)
		pr_err("Failed to remap Hyper-V TSC page.\n");
}
