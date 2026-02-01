 

#include <drm/drm_managed.h>
#include <linux/pm_runtime.h>

#include "gt/intel_engine_regs.h"
#include "gt/intel_gt_regs.h"

#include "i915_drv.h"
#include "i915_iosf_mbi.h"
#include "i915_reg.h"
#include "i915_trace.h"
#include "i915_vgpu.h"

#define FORCEWAKE_ACK_TIMEOUT_MS 50
#define GT_FIFO_TIMEOUT_MS	 10

#define __raw_posting_read(...) ((void)__raw_uncore_read32(__VA_ARGS__))

static void
fw_domains_get(struct intel_uncore *uncore, enum forcewake_domains fw_domains)
{
	uncore->fw_get_funcs->force_wake_get(uncore, fw_domains);
}

void
intel_uncore_mmio_debug_init_early(struct drm_i915_private *i915)
{
	spin_lock_init(&i915->mmio_debug.lock);
	i915->mmio_debug.unclaimed_mmio_check = 1;

	i915->uncore.debug = &i915->mmio_debug;
}

static void mmio_debug_suspend(struct intel_uncore *uncore)
{
	if (!uncore->debug)
		return;

	spin_lock(&uncore->debug->lock);

	 
	if (!uncore->debug->suspend_count++) {
		uncore->debug->saved_mmio_check = uncore->debug->unclaimed_mmio_check;
		uncore->debug->unclaimed_mmio_check = 0;
	}

	spin_unlock(&uncore->debug->lock);
}

static bool check_for_unclaimed_mmio(struct intel_uncore *uncore);

static void mmio_debug_resume(struct intel_uncore *uncore)
{
	if (!uncore->debug)
		return;

	spin_lock(&uncore->debug->lock);

	if (!--uncore->debug->suspend_count)
		uncore->debug->unclaimed_mmio_check = uncore->debug->saved_mmio_check;

	if (check_for_unclaimed_mmio(uncore))
		drm_info(&uncore->i915->drm,
			 "Invalid mmio detected during user access\n");

	spin_unlock(&uncore->debug->lock);
}

static const char * const forcewake_domain_names[] = {
	"render",
	"gt",
	"media",
	"vdbox0",
	"vdbox1",
	"vdbox2",
	"vdbox3",
	"vdbox4",
	"vdbox5",
	"vdbox6",
	"vdbox7",
	"vebox0",
	"vebox1",
	"vebox2",
	"vebox3",
	"gsc",
};

const char *
intel_uncore_forcewake_domain_to_str(const enum forcewake_domain_id id)
{
	BUILD_BUG_ON(ARRAY_SIZE(forcewake_domain_names) != FW_DOMAIN_ID_COUNT);

	if (id >= 0 && id < FW_DOMAIN_ID_COUNT)
		return forcewake_domain_names[id];

	WARN_ON(id);

	return "unknown";
}

#define fw_ack(d) readl((d)->reg_ack)
#define fw_set(d, val) writel(_MASKED_BIT_ENABLE((val)), (d)->reg_set)
#define fw_clear(d, val) writel(_MASKED_BIT_DISABLE((val)), (d)->reg_set)

static inline void
fw_domain_reset(const struct intel_uncore_forcewake_domain *d)
{
	 
	 
	if (GRAPHICS_VER(d->uncore->i915) >= 12)
		fw_clear(d, 0xefff);
	else
		fw_clear(d, 0xffff);
}

static inline void
fw_domain_arm_timer(struct intel_uncore_forcewake_domain *d)
{
	GEM_BUG_ON(d->uncore->fw_domains_timer & d->mask);
	d->uncore->fw_domains_timer |= d->mask;
	d->wake_count++;
	hrtimer_start_range_ns(&d->timer,
			       NSEC_PER_MSEC,
			       NSEC_PER_MSEC,
			       HRTIMER_MODE_REL);
}

static inline int
__wait_for_ack(const struct intel_uncore_forcewake_domain *d,
	       const u32 ack,
	       const u32 value)
{
	return wait_for_atomic((fw_ack(d) & ack) == value,
			       FORCEWAKE_ACK_TIMEOUT_MS);
}

static inline int
wait_ack_clear(const struct intel_uncore_forcewake_domain *d,
	       const u32 ack)
{
	return __wait_for_ack(d, ack, 0);
}

static inline int
wait_ack_set(const struct intel_uncore_forcewake_domain *d,
	     const u32 ack)
{
	return __wait_for_ack(d, ack, ack);
}

static inline void
fw_domain_wait_ack_clear(const struct intel_uncore_forcewake_domain *d)
{
	if (!wait_ack_clear(d, FORCEWAKE_KERNEL))
		return;

	if (fw_ack(d) == ~0)
		drm_err(&d->uncore->i915->drm,
			"%s: MMIO unreliable (forcewake register returns 0xFFFFFFFF)!\n",
			intel_uncore_forcewake_domain_to_str(d->id));
	else
		drm_err(&d->uncore->i915->drm,
			"%s: timed out waiting for forcewake ack to clear.\n",
			intel_uncore_forcewake_domain_to_str(d->id));

	add_taint_for_CI(d->uncore->i915, TAINT_WARN);  
}

enum ack_type {
	ACK_CLEAR = 0,
	ACK_SET
};

static int
fw_domain_wait_ack_with_fallback(const struct intel_uncore_forcewake_domain *d,
				 const enum ack_type type)
{
	const u32 ack_bit = FORCEWAKE_KERNEL;
	const u32 value = type == ACK_SET ? ack_bit : 0;
	unsigned int pass;
	bool ack_detected;

	 

	pass = 1;
	do {
		wait_ack_clear(d, FORCEWAKE_KERNEL_FALLBACK);

		fw_set(d, FORCEWAKE_KERNEL_FALLBACK);
		 
		udelay(10 * pass);
		wait_ack_set(d, FORCEWAKE_KERNEL_FALLBACK);

		ack_detected = (fw_ack(d) & ack_bit) == value;

		fw_clear(d, FORCEWAKE_KERNEL_FALLBACK);
	} while (!ack_detected && pass++ < 10);

	drm_dbg(&d->uncore->i915->drm,
		"%s had to use fallback to %s ack, 0x%x (passes %u)\n",
		intel_uncore_forcewake_domain_to_str(d->id),
		type == ACK_SET ? "set" : "clear",
		fw_ack(d),
		pass);

	return ack_detected ? 0 : -ETIMEDOUT;
}

static inline void
fw_domain_wait_ack_clear_fallback(const struct intel_uncore_forcewake_domain *d)
{
	if (likely(!wait_ack_clear(d, FORCEWAKE_KERNEL)))
		return;

	if (fw_domain_wait_ack_with_fallback(d, ACK_CLEAR))
		fw_domain_wait_ack_clear(d);
}

static inline void
fw_domain_get(const struct intel_uncore_forcewake_domain *d)
{
	fw_set(d, FORCEWAKE_KERNEL);
}

static inline void
fw_domain_wait_ack_set(const struct intel_uncore_forcewake_domain *d)
{
	if (wait_ack_set(d, FORCEWAKE_KERNEL)) {
		drm_err(&d->uncore->i915->drm,
			"%s: timed out waiting for forcewake ack request.\n",
			intel_uncore_forcewake_domain_to_str(d->id));
		add_taint_for_CI(d->uncore->i915, TAINT_WARN);  
	}
}

static inline void
fw_domain_wait_ack_set_fallback(const struct intel_uncore_forcewake_domain *d)
{
	if (likely(!wait_ack_set(d, FORCEWAKE_KERNEL)))
		return;

	if (fw_domain_wait_ack_with_fallback(d, ACK_SET))
		fw_domain_wait_ack_set(d);
}

static inline void
fw_domain_put(const struct intel_uncore_forcewake_domain *d)
{
	fw_clear(d, FORCEWAKE_KERNEL);
}

static void
fw_domains_get_normal(struct intel_uncore *uncore, enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *d;
	unsigned int tmp;

	GEM_BUG_ON(fw_domains & ~uncore->fw_domains);

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp) {
		fw_domain_wait_ack_clear(d);
		fw_domain_get(d);
	}

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp)
		fw_domain_wait_ack_set(d);

	uncore->fw_domains_active |= fw_domains;
}

static void
fw_domains_get_with_fallback(struct intel_uncore *uncore,
			     enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *d;
	unsigned int tmp;

	GEM_BUG_ON(fw_domains & ~uncore->fw_domains);

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp) {
		fw_domain_wait_ack_clear_fallback(d);
		fw_domain_get(d);
	}

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp)
		fw_domain_wait_ack_set_fallback(d);

	uncore->fw_domains_active |= fw_domains;
}

static void
fw_domains_put(struct intel_uncore *uncore, enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *d;
	unsigned int tmp;

	GEM_BUG_ON(fw_domains & ~uncore->fw_domains);

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp)
		fw_domain_put(d);

	uncore->fw_domains_active &= ~fw_domains;
}

static void
fw_domains_reset(struct intel_uncore *uncore,
		 enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *d;
	unsigned int tmp;

	if (!fw_domains)
		return;

	GEM_BUG_ON(fw_domains & ~uncore->fw_domains);

	for_each_fw_domain_masked(d, fw_domains, uncore, tmp)
		fw_domain_reset(d);
}

static inline u32 gt_thread_status(struct intel_uncore *uncore)
{
	u32 val;

	val = __raw_uncore_read32(uncore, GEN6_GT_THREAD_STATUS_REG);
	val &= GEN6_GT_THREAD_STATUS_CORE_MASK;

	return val;
}

static void __gen6_gt_wait_for_thread_c0(struct intel_uncore *uncore)
{
	 
	drm_WARN_ONCE(&uncore->i915->drm,
		      wait_for_atomic_us(gt_thread_status(uncore) == 0, 5000),
		      "GT thread status wait timed out\n");
}

static void fw_domains_get_with_thread_status(struct intel_uncore *uncore,
					      enum forcewake_domains fw_domains)
{
	fw_domains_get_normal(uncore, fw_domains);

	 
	__gen6_gt_wait_for_thread_c0(uncore);
}

static inline u32 fifo_free_entries(struct intel_uncore *uncore)
{
	u32 count = __raw_uncore_read32(uncore, GTFIFOCTL);

	return count & GT_FIFO_FREE_ENTRIES_MASK;
}

static void __gen6_gt_wait_for_fifo(struct intel_uncore *uncore)
{
	u32 n;

	 
	if (IS_VALLEYVIEW(uncore->i915))
		n = fifo_free_entries(uncore);
	else
		n = uncore->fifo_count;

	if (n <= GT_FIFO_NUM_RESERVED_ENTRIES) {
		if (wait_for_atomic((n = fifo_free_entries(uncore)) >
				    GT_FIFO_NUM_RESERVED_ENTRIES,
				    GT_FIFO_TIMEOUT_MS)) {
			drm_dbg(&uncore->i915->drm,
				"GT_FIFO timeout, entries: %u\n", n);
			return;
		}
	}

	uncore->fifo_count = n - 1;
}

static enum hrtimer_restart
intel_uncore_fw_release_timer(struct hrtimer *timer)
{
	struct intel_uncore_forcewake_domain *domain =
	       container_of(timer, struct intel_uncore_forcewake_domain, timer);
	struct intel_uncore *uncore = domain->uncore;
	unsigned long irqflags;

	assert_rpm_device_not_suspended(uncore->rpm);

	if (xchg(&domain->active, false))
		return HRTIMER_RESTART;

	spin_lock_irqsave(&uncore->lock, irqflags);

	uncore->fw_domains_timer &= ~domain->mask;

	GEM_BUG_ON(!domain->wake_count);
	if (--domain->wake_count == 0)
		fw_domains_put(uncore, domain->mask);

	spin_unlock_irqrestore(&uncore->lock, irqflags);

	return HRTIMER_NORESTART;
}

 
static unsigned int
intel_uncore_forcewake_reset(struct intel_uncore *uncore)
{
	unsigned long irqflags;
	struct intel_uncore_forcewake_domain *domain;
	int retry_count = 100;
	enum forcewake_domains fw, active_domains;

	iosf_mbi_assert_punit_acquired();

	 
	while (1) {
		unsigned int tmp;

		active_domains = 0;

		for_each_fw_domain(domain, uncore, tmp) {
			smp_store_mb(domain->active, false);
			if (hrtimer_cancel(&domain->timer) == 0)
				continue;

			intel_uncore_fw_release_timer(&domain->timer);
		}

		spin_lock_irqsave(&uncore->lock, irqflags);

		for_each_fw_domain(domain, uncore, tmp) {
			if (hrtimer_active(&domain->timer))
				active_domains |= domain->mask;
		}

		if (active_domains == 0)
			break;

		if (--retry_count == 0) {
			drm_err(&uncore->i915->drm, "Timed out waiting for forcewake timers to finish\n");
			break;
		}

		spin_unlock_irqrestore(&uncore->lock, irqflags);
		cond_resched();
	}

	drm_WARN_ON(&uncore->i915->drm, active_domains);

	fw = uncore->fw_domains_active;
	if (fw)
		fw_domains_put(uncore, fw);

	fw_domains_reset(uncore, uncore->fw_domains);
	assert_forcewakes_inactive(uncore);

	spin_unlock_irqrestore(&uncore->lock, irqflags);

	return fw;  
}

static bool
fpga_check_for_unclaimed_mmio(struct intel_uncore *uncore)
{
	u32 dbg;

	dbg = __raw_uncore_read32(uncore, FPGA_DBG);
	if (likely(!(dbg & FPGA_DBG_RM_NOCLAIM)))
		return false;

	 
	if (unlikely(dbg == ~0))
		drm_err(&uncore->i915->drm,
			"Lost access to MMIO BAR; all registers now read back as 0xFFFFFFFF!\n");

	__raw_uncore_write32(uncore, FPGA_DBG, FPGA_DBG_RM_NOCLAIM);

	return true;
}

static bool
vlv_check_for_unclaimed_mmio(struct intel_uncore *uncore)
{
	u32 cer;

	cer = __raw_uncore_read32(uncore, CLAIM_ER);
	if (likely(!(cer & (CLAIM_ER_OVERFLOW | CLAIM_ER_CTR_MASK))))
		return false;

	__raw_uncore_write32(uncore, CLAIM_ER, CLAIM_ER_CLR);

	return true;
}

static bool
gen6_check_for_fifo_debug(struct intel_uncore *uncore)
{
	u32 fifodbg;

	fifodbg = __raw_uncore_read32(uncore, GTFIFODBG);

	if (unlikely(fifodbg)) {
		drm_dbg(&uncore->i915->drm, "GTFIFODBG = 0x08%x\n", fifodbg);
		__raw_uncore_write32(uncore, GTFIFODBG, fifodbg);
	}

	return fifodbg;
}

static bool
check_for_unclaimed_mmio(struct intel_uncore *uncore)
{
	bool ret = false;

	lockdep_assert_held(&uncore->debug->lock);

	if (uncore->debug->suspend_count)
		return false;

	if (intel_uncore_has_fpga_dbg_unclaimed(uncore))
		ret |= fpga_check_for_unclaimed_mmio(uncore);

	if (intel_uncore_has_dbg_unclaimed(uncore))
		ret |= vlv_check_for_unclaimed_mmio(uncore);

	if (intel_uncore_has_fifo(uncore))
		ret |= gen6_check_for_fifo_debug(uncore);

	return ret;
}

static void forcewake_early_sanitize(struct intel_uncore *uncore,
				     unsigned int restore_forcewake)
{
	GEM_BUG_ON(!intel_uncore_has_forcewake(uncore));

	 
	if (IS_CHERRYVIEW(uncore->i915)) {
		__raw_uncore_write32(uncore, GTFIFOCTL,
				     __raw_uncore_read32(uncore, GTFIFOCTL) |
				     GT_FIFO_CTL_BLOCK_ALL_POLICY_STALL |
				     GT_FIFO_CTL_RC6_POLICY_STALL);
	}

	iosf_mbi_punit_acquire();
	intel_uncore_forcewake_reset(uncore);
	if (restore_forcewake) {
		spin_lock_irq(&uncore->lock);
		fw_domains_get(uncore, restore_forcewake);

		if (intel_uncore_has_fifo(uncore))
			uncore->fifo_count = fifo_free_entries(uncore);
		spin_unlock_irq(&uncore->lock);
	}
	iosf_mbi_punit_release();
}

void intel_uncore_suspend(struct intel_uncore *uncore)
{
	if (!intel_uncore_has_forcewake(uncore))
		return;

	iosf_mbi_punit_acquire();
	iosf_mbi_unregister_pmic_bus_access_notifier_unlocked(
		&uncore->pmic_bus_access_nb);
	uncore->fw_domains_saved = intel_uncore_forcewake_reset(uncore);
	iosf_mbi_punit_release();
}

void intel_uncore_resume_early(struct intel_uncore *uncore)
{
	unsigned int restore_forcewake;

	if (intel_uncore_unclaimed_mmio(uncore))
		drm_dbg(&uncore->i915->drm, "unclaimed mmio detected on resume, clearing\n");

	if (!intel_uncore_has_forcewake(uncore))
		return;

	restore_forcewake = fetch_and_zero(&uncore->fw_domains_saved);
	forcewake_early_sanitize(uncore, restore_forcewake);

	iosf_mbi_register_pmic_bus_access_notifier(&uncore->pmic_bus_access_nb);
}

void intel_uncore_runtime_resume(struct intel_uncore *uncore)
{
	if (!intel_uncore_has_forcewake(uncore))
		return;

	iosf_mbi_register_pmic_bus_access_notifier(&uncore->pmic_bus_access_nb);
}

static void __intel_uncore_forcewake_get(struct intel_uncore *uncore,
					 enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *domain;
	unsigned int tmp;

	fw_domains &= uncore->fw_domains;

	for_each_fw_domain_masked(domain, fw_domains, uncore, tmp) {
		if (domain->wake_count++) {
			fw_domains &= ~domain->mask;
			domain->active = true;
		}
	}

	if (fw_domains)
		fw_domains_get(uncore, fw_domains);
}

 
void intel_uncore_forcewake_get(struct intel_uncore *uncore,
				enum forcewake_domains fw_domains)
{
	unsigned long irqflags;

	if (!uncore->fw_get_funcs)
		return;

	assert_rpm_wakelock_held(uncore->rpm);

	spin_lock_irqsave(&uncore->lock, irqflags);
	__intel_uncore_forcewake_get(uncore, fw_domains);
	spin_unlock_irqrestore(&uncore->lock, irqflags);
}

 
void intel_uncore_forcewake_user_get(struct intel_uncore *uncore)
{
	spin_lock_irq(&uncore->lock);
	if (!uncore->user_forcewake_count++) {
		intel_uncore_forcewake_get__locked(uncore, FORCEWAKE_ALL);
		mmio_debug_suspend(uncore);
	}
	spin_unlock_irq(&uncore->lock);
}

 
void intel_uncore_forcewake_user_put(struct intel_uncore *uncore)
{
	spin_lock_irq(&uncore->lock);
	if (!--uncore->user_forcewake_count) {
		mmio_debug_resume(uncore);
		intel_uncore_forcewake_put__locked(uncore, FORCEWAKE_ALL);
	}
	spin_unlock_irq(&uncore->lock);
}

 
void intel_uncore_forcewake_get__locked(struct intel_uncore *uncore,
					enum forcewake_domains fw_domains)
{
	lockdep_assert_held(&uncore->lock);

	if (!uncore->fw_get_funcs)
		return;

	__intel_uncore_forcewake_get(uncore, fw_domains);
}

static void __intel_uncore_forcewake_put(struct intel_uncore *uncore,
					 enum forcewake_domains fw_domains,
					 bool delayed)
{
	struct intel_uncore_forcewake_domain *domain;
	unsigned int tmp;

	fw_domains &= uncore->fw_domains;

	for_each_fw_domain_masked(domain, fw_domains, uncore, tmp) {
		GEM_BUG_ON(!domain->wake_count);

		if (--domain->wake_count) {
			domain->active = true;
			continue;
		}

		if (delayed &&
		    !(domain->uncore->fw_domains_timer & domain->mask))
			fw_domain_arm_timer(domain);
		else
			fw_domains_put(uncore, domain->mask);
	}
}

 
void intel_uncore_forcewake_put(struct intel_uncore *uncore,
				enum forcewake_domains fw_domains)
{
	unsigned long irqflags;

	if (!uncore->fw_get_funcs)
		return;

	spin_lock_irqsave(&uncore->lock, irqflags);
	__intel_uncore_forcewake_put(uncore, fw_domains, false);
	spin_unlock_irqrestore(&uncore->lock, irqflags);
}

void intel_uncore_forcewake_put_delayed(struct intel_uncore *uncore,
					enum forcewake_domains fw_domains)
{
	unsigned long irqflags;

	if (!uncore->fw_get_funcs)
		return;

	spin_lock_irqsave(&uncore->lock, irqflags);
	__intel_uncore_forcewake_put(uncore, fw_domains, true);
	spin_unlock_irqrestore(&uncore->lock, irqflags);
}

 
void intel_uncore_forcewake_flush(struct intel_uncore *uncore,
				  enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *domain;
	unsigned int tmp;

	if (!uncore->fw_get_funcs)
		return;

	fw_domains &= uncore->fw_domains;
	for_each_fw_domain_masked(domain, fw_domains, uncore, tmp) {
		WRITE_ONCE(domain->active, false);
		if (hrtimer_cancel(&domain->timer))
			intel_uncore_fw_release_timer(&domain->timer);
	}
}

 
void intel_uncore_forcewake_put__locked(struct intel_uncore *uncore,
					enum forcewake_domains fw_domains)
{
	lockdep_assert_held(&uncore->lock);

	if (!uncore->fw_get_funcs)
		return;

	__intel_uncore_forcewake_put(uncore, fw_domains, false);
}

void assert_forcewakes_inactive(struct intel_uncore *uncore)
{
	if (!uncore->fw_get_funcs)
		return;

	drm_WARN(&uncore->i915->drm, uncore->fw_domains_active,
		 "Expected all fw_domains to be inactive, but %08x are still on\n",
		 uncore->fw_domains_active);
}

void assert_forcewakes_active(struct intel_uncore *uncore,
			      enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *domain;
	unsigned int tmp;

	if (!IS_ENABLED(CONFIG_DRM_I915_DEBUG_RUNTIME_PM))
		return;

	if (!uncore->fw_get_funcs)
		return;

	spin_lock_irq(&uncore->lock);

	assert_rpm_wakelock_held(uncore->rpm);

	fw_domains &= uncore->fw_domains;
	drm_WARN(&uncore->i915->drm, fw_domains & ~uncore->fw_domains_active,
		 "Expected %08x fw_domains to be active, but %08x are off\n",
		 fw_domains, fw_domains & ~uncore->fw_domains_active);

	 
	for_each_fw_domain_masked(domain, fw_domains, uncore, tmp) {
		unsigned int actual = READ_ONCE(domain->wake_count);
		unsigned int expect = 1;

		if (uncore->fw_domains_timer & domain->mask)
			expect++;  

		if (drm_WARN(&uncore->i915->drm, actual < expect,
			     "Expected domain %d to be held awake by caller, count=%d\n",
			     domain->id, actual))
			break;
	}

	spin_unlock_irq(&uncore->lock);
}

 
#define NEEDS_FORCE_WAKE(reg) ({ \
	u32 __reg = (reg); \
	__reg < 0x40000 || __reg >= 0x116000; \
})

static int fw_range_cmp(u32 offset, const struct intel_forcewake_range *entry)
{
	if (offset < entry->start)
		return -1;
	else if (offset > entry->end)
		return 1;
	else
		return 0;
}

 
#define BSEARCH(key, base, num, cmp) ({                                 \
	unsigned int start__ = 0, end__ = (num);                        \
	typeof(base) result__ = NULL;                                   \
	while (start__ < end__) {                                       \
		unsigned int mid__ = start__ + (end__ - start__) / 2;   \
		int ret__ = (cmp)((key), (base) + mid__);               \
		if (ret__ < 0) {                                        \
			end__ = mid__;                                  \
		} else if (ret__ > 0) {                                 \
			start__ = mid__ + 1;                            \
		} else {                                                \
			result__ = (base) + mid__;                      \
			break;                                          \
		}                                                       \
	}                                                               \
	result__;                                                       \
})

static enum forcewake_domains
find_fw_domain(struct intel_uncore *uncore, u32 offset)
{
	const struct intel_forcewake_range *entry;

	if (IS_GSI_REG(offset))
		offset += uncore->gsi_offset;

	entry = BSEARCH(offset,
			uncore->fw_domains_table,
			uncore->fw_domains_table_entries,
			fw_range_cmp);

	if (!entry)
		return 0;

	 
	if (entry->domains == FORCEWAKE_ALL)
		return uncore->fw_domains;

	drm_WARN(&uncore->i915->drm, entry->domains & ~uncore->fw_domains,
		 "Uninitialized forcewake domain(s) 0x%x accessed at 0x%x\n",
		 entry->domains & ~uncore->fw_domains, offset);

	return entry->domains;
}

 

static const struct i915_range gen8_shadowed_regs[] = {
	{ .start =  0x2030, .end =  0x2030 },
	{ .start =  0xA008, .end =  0xA00C },
	{ .start = 0x12030, .end = 0x12030 },
	{ .start = 0x1a030, .end = 0x1a030 },
	{ .start = 0x22030, .end = 0x22030 },
};

static const struct i915_range gen11_shadowed_regs[] = {
	{ .start =   0x2030, .end =   0x2030 },
	{ .start =   0x2550, .end =   0x2550 },
	{ .start =   0xA008, .end =   0xA00C },
	{ .start =  0x22030, .end =  0x22030 },
	{ .start =  0x22230, .end =  0x22230 },
	{ .start =  0x22510, .end =  0x22550 },
	{ .start = 0x1C0030, .end = 0x1C0030 },
	{ .start = 0x1C0230, .end = 0x1C0230 },
	{ .start = 0x1C0510, .end = 0x1C0550 },
	{ .start = 0x1C4030, .end = 0x1C4030 },
	{ .start = 0x1C4230, .end = 0x1C4230 },
	{ .start = 0x1C4510, .end = 0x1C4550 },
	{ .start = 0x1C8030, .end = 0x1C8030 },
	{ .start = 0x1C8230, .end = 0x1C8230 },
	{ .start = 0x1C8510, .end = 0x1C8550 },
	{ .start = 0x1D0030, .end = 0x1D0030 },
	{ .start = 0x1D0230, .end = 0x1D0230 },
	{ .start = 0x1D0510, .end = 0x1D0550 },
	{ .start = 0x1D4030, .end = 0x1D4030 },
	{ .start = 0x1D4230, .end = 0x1D4230 },
	{ .start = 0x1D4510, .end = 0x1D4550 },
	{ .start = 0x1D8030, .end = 0x1D8030 },
	{ .start = 0x1D8230, .end = 0x1D8230 },
	{ .start = 0x1D8510, .end = 0x1D8550 },
};

static const struct i915_range gen12_shadowed_regs[] = {
	{ .start =   0x2030, .end =   0x2030 },
	{ .start =   0x2510, .end =   0x2550 },
	{ .start =   0xA008, .end =   0xA00C },
	{ .start =   0xA188, .end =   0xA188 },
	{ .start =   0xA278, .end =   0xA278 },
	{ .start =   0xA540, .end =   0xA56C },
	{ .start =   0xC4C8, .end =   0xC4C8 },
	{ .start =   0xC4D4, .end =   0xC4D4 },
	{ .start =   0xC600, .end =   0xC600 },
	{ .start =  0x22030, .end =  0x22030 },
	{ .start =  0x22510, .end =  0x22550 },
	{ .start = 0x1C0030, .end = 0x1C0030 },
	{ .start = 0x1C0510, .end = 0x1C0550 },
	{ .start = 0x1C4030, .end = 0x1C4030 },
	{ .start = 0x1C4510, .end = 0x1C4550 },
	{ .start = 0x1C8030, .end = 0x1C8030 },
	{ .start = 0x1C8510, .end = 0x1C8550 },
	{ .start = 0x1D0030, .end = 0x1D0030 },
	{ .start = 0x1D0510, .end = 0x1D0550 },
	{ .start = 0x1D4030, .end = 0x1D4030 },
	{ .start = 0x1D4510, .end = 0x1D4550 },
	{ .start = 0x1D8030, .end = 0x1D8030 },
	{ .start = 0x1D8510, .end = 0x1D8550 },

	 
	{ .start = 0x1E0030, .end = 0x1E0030 },
	{ .start = 0x1E0510, .end = 0x1E0550 },
	{ .start = 0x1E4030, .end = 0x1E4030 },
	{ .start = 0x1E4510, .end = 0x1E4550 },
	{ .start = 0x1E8030, .end = 0x1E8030 },
	{ .start = 0x1E8510, .end = 0x1E8550 },
	{ .start = 0x1F0030, .end = 0x1F0030 },
	{ .start = 0x1F0510, .end = 0x1F0550 },
	{ .start = 0x1F4030, .end = 0x1F4030 },
	{ .start = 0x1F4510, .end = 0x1F4550 },
	{ .start = 0x1F8030, .end = 0x1F8030 },
	{ .start = 0x1F8510, .end = 0x1F8550 },
};

static const struct i915_range dg2_shadowed_regs[] = {
	{ .start =   0x2030, .end =   0x2030 },
	{ .start =   0x2510, .end =   0x2550 },
	{ .start =   0xA008, .end =   0xA00C },
	{ .start =   0xA188, .end =   0xA188 },
	{ .start =   0xA278, .end =   0xA278 },
	{ .start =   0xA540, .end =   0xA56C },
	{ .start =   0xC4C8, .end =   0xC4C8 },
	{ .start =   0xC4E0, .end =   0xC4E0 },
	{ .start =   0xC600, .end =   0xC600 },
	{ .start =   0xC658, .end =   0xC658 },
	{ .start =  0x22030, .end =  0x22030 },
	{ .start =  0x22510, .end =  0x22550 },
	{ .start = 0x1C0030, .end = 0x1C0030 },
	{ .start = 0x1C0510, .end = 0x1C0550 },
	{ .start = 0x1C4030, .end = 0x1C4030 },
	{ .start = 0x1C4510, .end = 0x1C4550 },
	{ .start = 0x1C8030, .end = 0x1C8030 },
	{ .start = 0x1C8510, .end = 0x1C8550 },
	{ .start = 0x1D0030, .end = 0x1D0030 },
	{ .start = 0x1D0510, .end = 0x1D0550 },
	{ .start = 0x1D4030, .end = 0x1D4030 },
	{ .start = 0x1D4510, .end = 0x1D4550 },
	{ .start = 0x1D8030, .end = 0x1D8030 },
	{ .start = 0x1D8510, .end = 0x1D8550 },
	{ .start = 0x1E0030, .end = 0x1E0030 },
	{ .start = 0x1E0510, .end = 0x1E0550 },
	{ .start = 0x1E4030, .end = 0x1E4030 },
	{ .start = 0x1E4510, .end = 0x1E4550 },
	{ .start = 0x1E8030, .end = 0x1E8030 },
	{ .start = 0x1E8510, .end = 0x1E8550 },
	{ .start = 0x1F0030, .end = 0x1F0030 },
	{ .start = 0x1F0510, .end = 0x1F0550 },
	{ .start = 0x1F4030, .end = 0x1F4030 },
	{ .start = 0x1F4510, .end = 0x1F4550 },
	{ .start = 0x1F8030, .end = 0x1F8030 },
	{ .start = 0x1F8510, .end = 0x1F8550 },
};

static const struct i915_range pvc_shadowed_regs[] = {
	{ .start =   0x2030, .end =   0x2030 },
	{ .start =   0x2510, .end =   0x2550 },
	{ .start =   0xA008, .end =   0xA00C },
	{ .start =   0xA188, .end =   0xA188 },
	{ .start =   0xA278, .end =   0xA278 },
	{ .start =   0xA540, .end =   0xA56C },
	{ .start =   0xC4C8, .end =   0xC4C8 },
	{ .start =   0xC4E0, .end =   0xC4E0 },
	{ .start =   0xC600, .end =   0xC600 },
	{ .start =   0xC658, .end =   0xC658 },
	{ .start =  0x22030, .end =  0x22030 },
	{ .start =  0x22510, .end =  0x22550 },
	{ .start = 0x1C0030, .end = 0x1C0030 },
	{ .start = 0x1C0510, .end = 0x1C0550 },
	{ .start = 0x1C4030, .end = 0x1C4030 },
	{ .start = 0x1C4510, .end = 0x1C4550 },
	{ .start = 0x1C8030, .end = 0x1C8030 },
	{ .start = 0x1C8510, .end = 0x1C8550 },
	{ .start = 0x1D0030, .end = 0x1D0030 },
	{ .start = 0x1D0510, .end = 0x1D0550 },
	{ .start = 0x1D4030, .end = 0x1D4030 },
	{ .start = 0x1D4510, .end = 0x1D4550 },
	{ .start = 0x1D8030, .end = 0x1D8030 },
	{ .start = 0x1D8510, .end = 0x1D8550 },
	{ .start = 0x1E0030, .end = 0x1E0030 },
	{ .start = 0x1E0510, .end = 0x1E0550 },
	{ .start = 0x1E4030, .end = 0x1E4030 },
	{ .start = 0x1E4510, .end = 0x1E4550 },
	{ .start = 0x1E8030, .end = 0x1E8030 },
	{ .start = 0x1E8510, .end = 0x1E8550 },
	{ .start = 0x1F0030, .end = 0x1F0030 },
	{ .start = 0x1F0510, .end = 0x1F0550 },
	{ .start = 0x1F4030, .end = 0x1F4030 },
	{ .start = 0x1F4510, .end = 0x1F4550 },
	{ .start = 0x1F8030, .end = 0x1F8030 },
	{ .start = 0x1F8510, .end = 0x1F8550 },
};

static const struct i915_range mtl_shadowed_regs[] = {
	{ .start =   0x2030, .end =   0x2030 },
	{ .start =   0x2510, .end =   0x2550 },
	{ .start =   0xA008, .end =   0xA00C },
	{ .start =   0xA188, .end =   0xA188 },
	{ .start =   0xA278, .end =   0xA278 },
	{ .start =   0xA540, .end =   0xA56C },
	{ .start =   0xC050, .end =   0xC050 },
	{ .start =   0xC340, .end =   0xC340 },
	{ .start =   0xC4C8, .end =   0xC4C8 },
	{ .start =   0xC4E0, .end =   0xC4E0 },
	{ .start =   0xC600, .end =   0xC600 },
	{ .start =   0xC658, .end =   0xC658 },
	{ .start =   0xCFD4, .end =   0xCFDC },
	{ .start =  0x22030, .end =  0x22030 },
	{ .start =  0x22510, .end =  0x22550 },
};

static const struct i915_range xelpmp_shadowed_regs[] = {
	{ .start = 0x1C0030, .end = 0x1C0030 },
	{ .start = 0x1C0510, .end = 0x1C0550 },
	{ .start = 0x1C8030, .end = 0x1C8030 },
	{ .start = 0x1C8510, .end = 0x1C8550 },
	{ .start = 0x1D0030, .end = 0x1D0030 },
	{ .start = 0x1D0510, .end = 0x1D0550 },
	{ .start = 0x38A008, .end = 0x38A00C },
	{ .start = 0x38A188, .end = 0x38A188 },
	{ .start = 0x38A278, .end = 0x38A278 },
	{ .start = 0x38A540, .end = 0x38A56C },
	{ .start = 0x38A618, .end = 0x38A618 },
	{ .start = 0x38C050, .end = 0x38C050 },
	{ .start = 0x38C340, .end = 0x38C340 },
	{ .start = 0x38C4C8, .end = 0x38C4C8 },
	{ .start = 0x38C4E0, .end = 0x38C4E4 },
	{ .start = 0x38C600, .end = 0x38C600 },
	{ .start = 0x38C658, .end = 0x38C658 },
	{ .start = 0x38CFD4, .end = 0x38CFDC },
};

static int mmio_range_cmp(u32 key, const struct i915_range *range)
{
	if (key < range->start)
		return -1;
	else if (key > range->end)
		return 1;
	else
		return 0;
}

static bool is_shadowed(struct intel_uncore *uncore, u32 offset)
{
	if (drm_WARN_ON(&uncore->i915->drm, !uncore->shadowed_reg_table))
		return false;

	if (IS_GSI_REG(offset))
		offset += uncore->gsi_offset;

	return BSEARCH(offset,
		       uncore->shadowed_reg_table,
		       uncore->shadowed_reg_table_entries,
		       mmio_range_cmp);
}

static enum forcewake_domains
gen6_reg_write_fw_domains(struct intel_uncore *uncore, i915_reg_t reg)
{
	return FORCEWAKE_RENDER;
}

#define __fwtable_reg_read_fw_domains(uncore, offset) \
({ \
	enum forcewake_domains __fwd = 0; \
	if (NEEDS_FORCE_WAKE((offset))) \
		__fwd = find_fw_domain(uncore, offset); \
	__fwd; \
})

#define __fwtable_reg_write_fw_domains(uncore, offset) \
({ \
	enum forcewake_domains __fwd = 0; \
	const u32 __offset = (offset); \
	if (NEEDS_FORCE_WAKE((__offset)) && !is_shadowed(uncore, __offset)) \
		__fwd = find_fw_domain(uncore, __offset); \
	__fwd; \
})

#define GEN_FW_RANGE(s, e, d) \
	{ .start = (s), .end = (e), .domains = (d) }

 

static const struct intel_forcewake_range __gen6_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0x3ffff, FORCEWAKE_RENDER),
};

static const struct intel_forcewake_range __vlv_fw_ranges[] = {
	GEN_FW_RANGE(0x2000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x5000, 0x7fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xb000, 0x11fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x12000, 0x13fff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x22000, 0x23fff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x2e000, 0x2ffff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_MEDIA),
};

static const struct intel_forcewake_range __chv_fw_ranges[] = {
	GEN_FW_RANGE(0x2000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x4fff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8000, 0x82ff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x8300, 0x84ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8500, 0x85ff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x8800, 0x88ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x9000, 0xafff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0xb000, 0xb47f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xd000, 0xd7ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0xe000, 0xe7ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xf000, 0xffff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x12000, 0x13fff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x1a000, 0x1bfff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x1e800, 0x1e9ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x30000, 0x37fff, FORCEWAKE_MEDIA),
};

static const struct intel_forcewake_range __gen9_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0xaff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xb00, 0x1fff, 0),  
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2700, 0x2fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x3000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x51ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8000, 0x812f, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8130, 0x813f, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x8140, 0x815f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8160, 0x82ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8300, 0x84ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8500, 0x87ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8800, 0x89ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x8a00, 0x8bff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8c00, 0x8cff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8d00, 0x93ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x9400, 0x97ff, FORCEWAKE_RENDER | FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x9800, 0xafff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xb000, 0xb47f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xb480, 0xcfff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xd000, 0xd7ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0xd800, 0xdfff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xe000, 0xe8ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xe900, 0x11fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x12000, 0x13fff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x14000, 0x19fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x1a000, 0x1e9ff, FORCEWAKE_MEDIA),
	GEN_FW_RANGE(0x1ea00, 0x243ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24400, 0x247ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x24800, 0x2ffff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_MEDIA),
};

static const struct intel_forcewake_range __gen11_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0x1fff, 0),  
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2700, 0x2fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x3000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x51ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8000, 0x813f, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8140, 0x815f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8160, 0x82ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8300, 0x84ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8500, 0x87ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8800, 0x8bff, 0),
	GEN_FW_RANGE(0x8c00, 0x8cff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8d00, 0x94cf, FORCEWAKE_GT),
	GEN_FW_RANGE(0x94d0, 0x955f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x9560, 0x95ff, 0),
	GEN_FW_RANGE(0x9600, 0xafff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xb000, 0xb47f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xb480, 0xdeff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xdf00, 0xe8ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xe900, 0x16dff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x16e00, 0x19fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x1a000, 0x23fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24000, 0x2407f, 0),
	GEN_FW_RANGE(0x24080, 0x2417f, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24180, 0x242ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x24300, 0x243ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24400, 0x24fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x25000, 0x3ffff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x40000, 0x1bffff, 0),
	GEN_FW_RANGE(0x1c0000, 0x1c3fff, FORCEWAKE_MEDIA_VDBOX0),
	GEN_FW_RANGE(0x1c4000, 0x1c7fff, 0),
	GEN_FW_RANGE(0x1c8000, 0x1cffff, FORCEWAKE_MEDIA_VEBOX0),
	GEN_FW_RANGE(0x1d0000, 0x1d3fff, FORCEWAKE_MEDIA_VDBOX2),
	GEN_FW_RANGE(0x1d4000, 0x1dbfff, 0)
};

static const struct intel_forcewake_range __gen12_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0x1fff, 0),  
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2700, 0x27ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x2800, 0x2aff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2b00, 0x2fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x3000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x51ff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x8000, 0x813f, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8140, 0x815f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8160, 0x81ff, 0),  
	GEN_FW_RANGE(0x8200, 0x82ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8300, 0x84ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8500, 0x94cf, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x94d0, 0x955f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x9560, 0x97ff, 0),  
	GEN_FW_RANGE(0x9800, 0xafff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xb000, 0xb3ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xb400, 0xcfff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0xd000, 0xd7ff, 0),
	GEN_FW_RANGE(0xd800, 0xd8ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xd900, 0xdbff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xdc00, 0xefff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0xf000, 0x147ff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x14800, 0x1ffff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x20000, 0x20fff, FORCEWAKE_MEDIA_VDBOX0),
	GEN_FW_RANGE(0x21000, 0x21fff, FORCEWAKE_MEDIA_VDBOX2),
	GEN_FW_RANGE(0x22000, 0x23fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24000, 0x2417f, 0),  
	GEN_FW_RANGE(0x24180, 0x249ff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x24a00, 0x251ff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x25200, 0x255ff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x25600, 0x2567f, FORCEWAKE_MEDIA_VDBOX0),
	GEN_FW_RANGE(0x25680, 0x259ff, FORCEWAKE_MEDIA_VDBOX2),  
	GEN_FW_RANGE(0x25a00, 0x25a7f, FORCEWAKE_MEDIA_VDBOX0),
	GEN_FW_RANGE(0x25a80, 0x2ffff, FORCEWAKE_MEDIA_VDBOX2),  
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x40000, 0x1bffff, 0),
	GEN_FW_RANGE(0x1c0000, 0x1c3fff, FORCEWAKE_MEDIA_VDBOX0),  
	GEN_FW_RANGE(0x1c4000, 0x1c7fff, 0),
	GEN_FW_RANGE(0x1c8000, 0x1cbfff, FORCEWAKE_MEDIA_VEBOX0),  
	GEN_FW_RANGE(0x1cc000, 0x1cffff, FORCEWAKE_MEDIA_VDBOX0),  
	GEN_FW_RANGE(0x1d0000, 0x1d3fff, FORCEWAKE_MEDIA_VDBOX2),  
};

 
#define XEHP_FWRANGES(FW_RANGE_D800)					\
	GEN_FW_RANGE(0x0, 0x1fff, 0),  					\
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0x2700, 0x4aff, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0x4b00, 0x51ff, 0),  					\
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0x8000, 0x813f, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0x8140, 0x815f, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0x8160, 0x81ff, 0),  					\
	GEN_FW_RANGE(0x8200, 0x82ff, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0x8300, 0x84ff, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0x8500, 0x8cff, FORCEWAKE_GT),  				\
	GEN_FW_RANGE(0x8d00, 0x8fff, FORCEWAKE_RENDER),  					\
	GEN_FW_RANGE(0x9000, 0x94cf, FORCEWAKE_GT),  					\
	GEN_FW_RANGE(0x94d0, 0x955f, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0x9560, 0x967f, 0),  					\
	GEN_FW_RANGE(0x9680, 0x97ff, FORCEWAKE_RENDER),  					\
	GEN_FW_RANGE(0x9800, 0xcfff, FORCEWAKE_GT),  						\
	GEN_FW_RANGE(0xd000, 0xd7ff, 0),					\
	GEN_FW_RANGE(0xd800, 0xd87f, FW_RANGE_D800),			\
	GEN_FW_RANGE(0xd880, 0xdbff, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0xdc00, 0xdcff, FORCEWAKE_RENDER),				\
	GEN_FW_RANGE(0xdd00, 0xde7f, FORCEWAKE_GT),  					\
	GEN_FW_RANGE(0xde80, 0xe8ff, FORCEWAKE_RENDER),  					\
	GEN_FW_RANGE(0xe900, 0xffff, FORCEWAKE_GT),  						\
	GEN_FW_RANGE(0x10000, 0x12fff, 0),  					\
	GEN_FW_RANGE(0x13000, 0x131ff, FORCEWAKE_MEDIA_VDBOX0),  	\
	GEN_FW_RANGE(0x13200, 0x13fff, FORCEWAKE_MEDIA_VDBOX2),  					\
	GEN_FW_RANGE(0x14000, 0x141ff, FORCEWAKE_MEDIA_VDBOX0),  	\
	GEN_FW_RANGE(0x14200, 0x143ff, FORCEWAKE_MEDIA_VDBOX2),  	\
	GEN_FW_RANGE(0x14400, 0x145ff, FORCEWAKE_MEDIA_VDBOX4),  	\
	GEN_FW_RANGE(0x14600, 0x147ff, FORCEWAKE_MEDIA_VDBOX6),  	\
	GEN_FW_RANGE(0x14800, 0x14fff, FORCEWAKE_RENDER),			\
	GEN_FW_RANGE(0x15000, 0x16dff, FORCEWAKE_GT),  					\
	GEN_FW_RANGE(0x16e00, 0x1ffff, FORCEWAKE_RENDER),			\
	GEN_FW_RANGE(0x20000, 0x21fff, FORCEWAKE_MEDIA_VDBOX0),  					\
	GEN_FW_RANGE(0x22000, 0x23fff, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0x24000, 0x2417f, 0),  					\
	GEN_FW_RANGE(0x24180, 0x249ff, FORCEWAKE_GT),  					\
	GEN_FW_RANGE(0x24a00, 0x251ff, FORCEWAKE_RENDER),  					\
	GEN_FW_RANGE(0x25200, 0x25fff, FORCEWAKE_GT),  					\
	GEN_FW_RANGE(0x26000, 0x2ffff, FORCEWAKE_RENDER),  				\
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_GT),				\
	GEN_FW_RANGE(0x40000, 0x1bffff, 0),					\
	GEN_FW_RANGE(0x1c0000, 0x1c3fff, FORCEWAKE_MEDIA_VDBOX0),  					\
	GEN_FW_RANGE(0x1c4000, 0x1c7fff, FORCEWAKE_MEDIA_VDBOX1),  				\
	GEN_FW_RANGE(0x1c8000, 0x1cbfff, FORCEWAKE_MEDIA_VEBOX0),  				\
	GEN_FW_RANGE(0x1cc000, 0x1ccfff, FORCEWAKE_MEDIA_VDBOX0),		\
	GEN_FW_RANGE(0x1cd000, 0x1cdfff, FORCEWAKE_MEDIA_VDBOX2),		\
	GEN_FW_RANGE(0x1ce000, 0x1cefff, FORCEWAKE_MEDIA_VDBOX4),		\
	GEN_FW_RANGE(0x1cf000, 0x1cffff, FORCEWAKE_MEDIA_VDBOX6),		\
	GEN_FW_RANGE(0x1d0000, 0x1d3fff, FORCEWAKE_MEDIA_VDBOX2),  					\
	GEN_FW_RANGE(0x1d4000, 0x1d7fff, FORCEWAKE_MEDIA_VDBOX3),  				\
	GEN_FW_RANGE(0x1d8000, 0x1dffff, FORCEWAKE_MEDIA_VEBOX1),  				\
	GEN_FW_RANGE(0x1e0000, 0x1e3fff, FORCEWAKE_MEDIA_VDBOX4),  					\
	GEN_FW_RANGE(0x1e4000, 0x1e7fff, FORCEWAKE_MEDIA_VDBOX5),  				\
	GEN_FW_RANGE(0x1e8000, 0x1effff, FORCEWAKE_MEDIA_VEBOX2),  				\
	GEN_FW_RANGE(0x1f0000, 0x1f3fff, FORCEWAKE_MEDIA_VDBOX6),  					\
	GEN_FW_RANGE(0x1f4000, 0x1f7fff, FORCEWAKE_MEDIA_VDBOX7),  				\
	GEN_FW_RANGE(0x1f8000, 0x1fa0ff, FORCEWAKE_MEDIA_VEBOX3),

static const struct intel_forcewake_range __xehp_fw_ranges[] = {
	XEHP_FWRANGES(FORCEWAKE_GT)
};

static const struct intel_forcewake_range __dg2_fw_ranges[] = {
	XEHP_FWRANGES(FORCEWAKE_RENDER)
};

static const struct intel_forcewake_range __pvc_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0xaff, 0),
	GEN_FW_RANGE(0xb00, 0xbff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xc00, 0xfff, 0),
	GEN_FW_RANGE(0x1000, 0x1fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2700, 0x2fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x3000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x813f, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x8140, 0x817f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x8180, 0x81ff, 0),
	GEN_FW_RANGE(0x8200, 0x94cf, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x94d0, 0x955f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x9560, 0x967f, 0),  
	GEN_FW_RANGE(0x9680, 0x97ff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x9800, 0xcfff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0xd000, 0xd3ff, 0),
	GEN_FW_RANGE(0xd400, 0xdbff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xdc00, 0xdcff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xdd00, 0xde7f, FORCEWAKE_GT),  
	GEN_FW_RANGE(0xde80, 0xe8ff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0xe900, 0x11fff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x12000, 0x12fff, 0),  
	GEN_FW_RANGE(0x13000, 0x19fff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x1a000, 0x21fff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x22000, 0x23fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24000, 0x2417f, 0),  
	GEN_FW_RANGE(0x24180, 0x25fff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x26000, 0x2ffff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x40000, 0x1bffff, 0),
	GEN_FW_RANGE(0x1c0000, 0x1c3fff, FORCEWAKE_MEDIA_VDBOX0),  
	GEN_FW_RANGE(0x1c4000, 0x1cffff, FORCEWAKE_MEDIA_VDBOX1),  
	GEN_FW_RANGE(0x1d0000, 0x23ffff, FORCEWAKE_MEDIA_VDBOX2),  
	GEN_FW_RANGE(0x240000, 0x3dffff, 0),
	GEN_FW_RANGE(0x3e0000, 0x3effff, FORCEWAKE_GT),
};

static const struct intel_forcewake_range __mtl_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0xaff, 0),
	GEN_FW_RANGE(0xb00, 0xbff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xc00, 0xfff, 0),
	GEN_FW_RANGE(0x1000, 0x1fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x2000, 0x26ff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x2700, 0x2fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x3000, 0x3fff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x4000, 0x51ff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x5200, 0x7fff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x8000, 0x813f, FORCEWAKE_GT),
	GEN_FW_RANGE(0x8140, 0x817f, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x8180, 0x81ff, 0),
	GEN_FW_RANGE(0x8200, 0x94cf, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x94d0, 0x955f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0x9560, 0x967f, 0),  
	GEN_FW_RANGE(0x9680, 0x97ff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x9800, 0xcfff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0xd000, 0xd7ff, 0),  
	GEN_FW_RANGE(0xd800, 0xd87f, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xd880, 0xdbff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xdc00, 0xdcff, FORCEWAKE_RENDER),
	GEN_FW_RANGE(0xdd00, 0xde7f, FORCEWAKE_GT),  
	GEN_FW_RANGE(0xde80, 0xe8ff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0xe900, 0xe9ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0xea00, 0x147ff, 0),  
	GEN_FW_RANGE(0x14800, 0x19fff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x1a000, 0x21fff, FORCEWAKE_RENDER),  
	GEN_FW_RANGE(0x22000, 0x23fff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x24000, 0x2ffff, 0),  
	GEN_FW_RANGE(0x30000, 0x3ffff, FORCEWAKE_GT)
};

 
static const struct intel_forcewake_range __xelpmp_fw_ranges[] = {
	GEN_FW_RANGE(0x0, 0x115fff, 0),  
	GEN_FW_RANGE(0x116000, 0x11ffff, FORCEWAKE_GSC),  
	GEN_FW_RANGE(0x120000, 0x1bffff, 0),  
	GEN_FW_RANGE(0x1c0000, 0x1c7fff, FORCEWAKE_MEDIA_VDBOX0),  
	GEN_FW_RANGE(0x1c8000, 0x1cbfff, FORCEWAKE_MEDIA_VEBOX0),  
	GEN_FW_RANGE(0x1cc000, 0x1cffff, FORCEWAKE_MEDIA_VDBOX0),  
	GEN_FW_RANGE(0x1d0000, 0x1d7fff, FORCEWAKE_MEDIA_VDBOX2),  
	GEN_FW_RANGE(0x1d8000, 0x1da0ff, FORCEWAKE_MEDIA_VEBOX1),
	GEN_FW_RANGE(0x1da100, 0x380aff, 0),  
	GEN_FW_RANGE(0x380b00, 0x380bff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x380c00, 0x380fff, 0),
	GEN_FW_RANGE(0x381000, 0x38817f, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x388180, 0x3882ff, 0),  
	GEN_FW_RANGE(0x388300, 0x38955f, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x389560, 0x389fff, 0),  
	GEN_FW_RANGE(0x38a000, 0x38cfff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x38d000, 0x38d11f, 0),
	GEN_FW_RANGE(0x38d120, 0x391fff, FORCEWAKE_GT),  
	GEN_FW_RANGE(0x392000, 0x392fff, 0),  
	GEN_FW_RANGE(0x393000, 0x3931ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x393200, 0x39323f, FORCEWAKE_ALL),  
	GEN_FW_RANGE(0x393240, 0x3933ff, FORCEWAKE_GT),
	GEN_FW_RANGE(0x393400, 0x3934ff, FORCEWAKE_ALL),  
	GEN_FW_RANGE(0x393500, 0x393c7f, 0),  
	GEN_FW_RANGE(0x393c80, 0x393dff, FORCEWAKE_GT),
};

static void
ilk_dummy_write(struct intel_uncore *uncore)
{
	 
	__raw_uncore_write32(uncore, RING_MI_MODE(RENDER_RING_BASE), 0);
}

static void
__unclaimed_reg_debug(struct intel_uncore *uncore,
		      const i915_reg_t reg,
		      const bool read)
{
	if (drm_WARN(&uncore->i915->drm,
		     check_for_unclaimed_mmio(uncore),
		     "Unclaimed %s register 0x%x\n",
		     read ? "read from" : "write to",
		     i915_mmio_reg_offset(reg)))
		 
		uncore->i915->params.mmio_debug--;
}

static void
__unclaimed_previous_reg_debug(struct intel_uncore *uncore,
			       const i915_reg_t reg,
			       const bool read)
{
	if (check_for_unclaimed_mmio(uncore))
		drm_dbg(&uncore->i915->drm,
			"Unclaimed access detected before %s register 0x%x\n",
			read ? "read from" : "write to",
			i915_mmio_reg_offset(reg));
}

static inline bool __must_check
unclaimed_reg_debug_header(struct intel_uncore *uncore,
			   const i915_reg_t reg, const bool read)
{
	if (likely(!uncore->i915->params.mmio_debug) || !uncore->debug)
		return false;

	 
	lockdep_assert_held(&uncore->lock);

	spin_lock(&uncore->debug->lock);
	__unclaimed_previous_reg_debug(uncore, reg, read);

	return true;
}

static inline void
unclaimed_reg_debug_footer(struct intel_uncore *uncore,
			   const i915_reg_t reg, const bool read)
{
	 
	lockdep_assert_held(&uncore->lock);

	__unclaimed_reg_debug(uncore, reg, read);
	spin_unlock(&uncore->debug->lock);
}

#define __vgpu_read(x) \
static u##x \
vgpu_read##x(struct intel_uncore *uncore, i915_reg_t reg, bool trace) { \
	u##x val = __raw_uncore_read##x(uncore, reg); \
	trace_i915_reg_rw(false, reg, val, sizeof(val), trace); \
	return val; \
}
__vgpu_read(8)
__vgpu_read(16)
__vgpu_read(32)
__vgpu_read(64)

#define GEN2_READ_HEADER(x) \
	u##x val = 0; \
	assert_rpm_wakelock_held(uncore->rpm);

#define GEN2_READ_FOOTER \
	trace_i915_reg_rw(false, reg, val, sizeof(val), trace); \
	return val

#define __gen2_read(x) \
static u##x \
gen2_read##x(struct intel_uncore *uncore, i915_reg_t reg, bool trace) { \
	GEN2_READ_HEADER(x); \
	val = __raw_uncore_read##x(uncore, reg); \
	GEN2_READ_FOOTER; \
}

#define __gen5_read(x) \
static u##x \
gen5_read##x(struct intel_uncore *uncore, i915_reg_t reg, bool trace) { \
	GEN2_READ_HEADER(x); \
	ilk_dummy_write(uncore); \
	val = __raw_uncore_read##x(uncore, reg); \
	GEN2_READ_FOOTER; \
}

__gen5_read(8)
__gen5_read(16)
__gen5_read(32)
__gen5_read(64)
__gen2_read(8)
__gen2_read(16)
__gen2_read(32)
__gen2_read(64)

#undef __gen5_read
#undef __gen2_read

#undef GEN2_READ_FOOTER
#undef GEN2_READ_HEADER

#define GEN6_READ_HEADER(x) \
	u32 offset = i915_mmio_reg_offset(reg); \
	unsigned long irqflags; \
	bool unclaimed_reg_debug; \
	u##x val = 0; \
	assert_rpm_wakelock_held(uncore->rpm); \
	spin_lock_irqsave(&uncore->lock, irqflags); \
	unclaimed_reg_debug = unclaimed_reg_debug_header(uncore, reg, true)

#define GEN6_READ_FOOTER \
	if (unclaimed_reg_debug) \
		unclaimed_reg_debug_footer(uncore, reg, true);	\
	spin_unlock_irqrestore(&uncore->lock, irqflags); \
	trace_i915_reg_rw(false, reg, val, sizeof(val), trace); \
	return val

static noinline void ___force_wake_auto(struct intel_uncore *uncore,
					enum forcewake_domains fw_domains)
{
	struct intel_uncore_forcewake_domain *domain;
	unsigned int tmp;

	GEM_BUG_ON(fw_domains & ~uncore->fw_domains);

	for_each_fw_domain_masked(domain, fw_domains, uncore, tmp)
		fw_domain_arm_timer(domain);

	fw_domains_get(uncore, fw_domains);
}

static inline void __force_wake_auto(struct intel_uncore *uncore,
				     enum forcewake_domains fw_domains)
{
	GEM_BUG_ON(!fw_domains);

	 
	fw_domains &= uncore->fw_domains;
	fw_domains &= ~uncore->fw_domains_active;

	if (fw_domains)
		___force_wake_auto(uncore, fw_domains);
}

#define __gen_fwtable_read(x) \
static u##x \
fwtable_read##x(struct intel_uncore *uncore, i915_reg_t reg, bool trace) \
{ \
	enum forcewake_domains fw_engine; \
	GEN6_READ_HEADER(x); \
	fw_engine = __fwtable_reg_read_fw_domains(uncore, offset); \
	if (fw_engine) \
		__force_wake_auto(uncore, fw_engine); \
	val = __raw_uncore_read##x(uncore, reg); \
	GEN6_READ_FOOTER; \
}

static enum forcewake_domains
fwtable_reg_read_fw_domains(struct intel_uncore *uncore, i915_reg_t reg) {
	return __fwtable_reg_read_fw_domains(uncore, i915_mmio_reg_offset(reg));
}

__gen_fwtable_read(8)
__gen_fwtable_read(16)
__gen_fwtable_read(32)
__gen_fwtable_read(64)

#undef __gen_fwtable_read
#undef GEN6_READ_FOOTER
#undef GEN6_READ_HEADER

#define GEN2_WRITE_HEADER \
	trace_i915_reg_rw(true, reg, val, sizeof(val), trace); \
	assert_rpm_wakelock_held(uncore->rpm); \

#define GEN2_WRITE_FOOTER

#define __gen2_write(x) \
static void \
gen2_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { \
	GEN2_WRITE_HEADER; \
	__raw_uncore_write##x(uncore, reg, val); \
	GEN2_WRITE_FOOTER; \
}

#define __gen5_write(x) \
static void \
gen5_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { \
	GEN2_WRITE_HEADER; \
	ilk_dummy_write(uncore); \
	__raw_uncore_write##x(uncore, reg, val); \
	GEN2_WRITE_FOOTER; \
}

__gen5_write(8)
__gen5_write(16)
__gen5_write(32)
__gen2_write(8)
__gen2_write(16)
__gen2_write(32)

#undef __gen5_write
#undef __gen2_write

#undef GEN2_WRITE_FOOTER
#undef GEN2_WRITE_HEADER

#define GEN6_WRITE_HEADER \
	u32 offset = i915_mmio_reg_offset(reg); \
	unsigned long irqflags; \
	bool unclaimed_reg_debug; \
	trace_i915_reg_rw(true, reg, val, sizeof(val), trace); \
	assert_rpm_wakelock_held(uncore->rpm); \
	spin_lock_irqsave(&uncore->lock, irqflags); \
	unclaimed_reg_debug = unclaimed_reg_debug_header(uncore, reg, false)

#define GEN6_WRITE_FOOTER \
	if (unclaimed_reg_debug) \
		unclaimed_reg_debug_footer(uncore, reg, false); \
	spin_unlock_irqrestore(&uncore->lock, irqflags)

#define __gen6_write(x) \
static void \
gen6_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { \
	GEN6_WRITE_HEADER; \
	if (NEEDS_FORCE_WAKE(offset)) \
		__gen6_gt_wait_for_fifo(uncore); \
	__raw_uncore_write##x(uncore, reg, val); \
	GEN6_WRITE_FOOTER; \
}
__gen6_write(8)
__gen6_write(16)
__gen6_write(32)

#define __gen_fwtable_write(x) \
static void \
fwtable_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { \
	enum forcewake_domains fw_engine; \
	GEN6_WRITE_HEADER; \
	fw_engine = __fwtable_reg_write_fw_domains(uncore, offset); \
	if (fw_engine) \
		__force_wake_auto(uncore, fw_engine); \
	__raw_uncore_write##x(uncore, reg, val); \
	GEN6_WRITE_FOOTER; \
}

static enum forcewake_domains
fwtable_reg_write_fw_domains(struct intel_uncore *uncore, i915_reg_t reg)
{
	return __fwtable_reg_write_fw_domains(uncore, i915_mmio_reg_offset(reg));
}

__gen_fwtable_write(8)
__gen_fwtable_write(16)
__gen_fwtable_write(32)

#undef __gen_fwtable_write
#undef GEN6_WRITE_FOOTER
#undef GEN6_WRITE_HEADER

#define __vgpu_write(x) \
static void \
vgpu_write##x(struct intel_uncore *uncore, i915_reg_t reg, u##x val, bool trace) { \
	trace_i915_reg_rw(true, reg, val, sizeof(val), trace); \
	__raw_uncore_write##x(uncore, reg, val); \
}
__vgpu_write(8)
__vgpu_write(16)
__vgpu_write(32)

#define ASSIGN_RAW_WRITE_MMIO_VFUNCS(uncore, x) \
do { \
	(uncore)->funcs.mmio_writeb = x##_write8; \
	(uncore)->funcs.mmio_writew = x##_write16; \
	(uncore)->funcs.mmio_writel = x##_write32; \
} while (0)

#define ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, x) \
do { \
	(uncore)->funcs.mmio_readb = x##_read8; \
	(uncore)->funcs.mmio_readw = x##_read16; \
	(uncore)->funcs.mmio_readl = x##_read32; \
	(uncore)->funcs.mmio_readq = x##_read64; \
} while (0)

#define ASSIGN_WRITE_MMIO_VFUNCS(uncore, x) \
do { \
	ASSIGN_RAW_WRITE_MMIO_VFUNCS((uncore), x); \
	(uncore)->funcs.write_fw_domains = x##_reg_write_fw_domains; \
} while (0)

#define ASSIGN_READ_MMIO_VFUNCS(uncore, x) \
do { \
	ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, x); \
	(uncore)->funcs.read_fw_domains = x##_reg_read_fw_domains; \
} while (0)

static int __fw_domain_init(struct intel_uncore *uncore,
			    enum forcewake_domain_id domain_id,
			    i915_reg_t reg_set,
			    i915_reg_t reg_ack)
{
	struct intel_uncore_forcewake_domain *d;

	GEM_BUG_ON(domain_id >= FW_DOMAIN_ID_COUNT);
	GEM_BUG_ON(uncore->fw_domain[domain_id]);

	if (i915_inject_probe_failure(uncore->i915))
		return -ENOMEM;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	drm_WARN_ON(&uncore->i915->drm, !i915_mmio_reg_valid(reg_set));
	drm_WARN_ON(&uncore->i915->drm, !i915_mmio_reg_valid(reg_ack));

	d->uncore = uncore;
	d->wake_count = 0;
	d->reg_set = uncore->regs + i915_mmio_reg_offset(reg_set) + uncore->gsi_offset;
	d->reg_ack = uncore->regs + i915_mmio_reg_offset(reg_ack) + uncore->gsi_offset;

	d->id = domain_id;

	BUILD_BUG_ON(FORCEWAKE_RENDER != (1 << FW_DOMAIN_ID_RENDER));
	BUILD_BUG_ON(FORCEWAKE_GT != (1 << FW_DOMAIN_ID_GT));
	BUILD_BUG_ON(FORCEWAKE_MEDIA != (1 << FW_DOMAIN_ID_MEDIA));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX0 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX0));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX1 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX1));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX2 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX2));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX3 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX3));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX4 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX4));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX5 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX5));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX6 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX6));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VDBOX7 != (1 << FW_DOMAIN_ID_MEDIA_VDBOX7));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VEBOX0 != (1 << FW_DOMAIN_ID_MEDIA_VEBOX0));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VEBOX1 != (1 << FW_DOMAIN_ID_MEDIA_VEBOX1));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VEBOX2 != (1 << FW_DOMAIN_ID_MEDIA_VEBOX2));
	BUILD_BUG_ON(FORCEWAKE_MEDIA_VEBOX3 != (1 << FW_DOMAIN_ID_MEDIA_VEBOX3));
	BUILD_BUG_ON(FORCEWAKE_GSC != (1 << FW_DOMAIN_ID_GSC));

	d->mask = BIT(domain_id);

	hrtimer_init(&d->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	d->timer.function = intel_uncore_fw_release_timer;

	uncore->fw_domains |= BIT(domain_id);

	fw_domain_reset(d);

	uncore->fw_domain[domain_id] = d;

	return 0;
}

static void fw_domain_fini(struct intel_uncore *uncore,
			   enum forcewake_domain_id domain_id)
{
	struct intel_uncore_forcewake_domain *d;

	GEM_BUG_ON(domain_id >= FW_DOMAIN_ID_COUNT);

	d = fetch_and_zero(&uncore->fw_domain[domain_id]);
	if (!d)
		return;

	uncore->fw_domains &= ~BIT(domain_id);
	drm_WARN_ON(&uncore->i915->drm, d->wake_count);
	drm_WARN_ON(&uncore->i915->drm, hrtimer_cancel(&d->timer));
	kfree(d);
}

static void intel_uncore_fw_domains_fini(struct intel_uncore *uncore)
{
	struct intel_uncore_forcewake_domain *d;
	int tmp;

	for_each_fw_domain(d, uncore, tmp)
		fw_domain_fini(uncore, d->id);
}

static const struct intel_uncore_fw_get uncore_get_fallback = {
	.force_wake_get = fw_domains_get_with_fallback
};

static const struct intel_uncore_fw_get uncore_get_normal = {
	.force_wake_get = fw_domains_get_normal,
};

static const struct intel_uncore_fw_get uncore_get_thread_status = {
	.force_wake_get = fw_domains_get_with_thread_status
};

static int intel_uncore_fw_domains_init(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;
	int ret = 0;

	GEM_BUG_ON(!intel_uncore_has_forcewake(uncore));

#define fw_domain_init(uncore__, id__, set__, ack__) \
	(ret ?: (ret = __fw_domain_init((uncore__), (id__), (set__), (ack__))))

	if (GRAPHICS_VER(i915) >= 11) {
		intel_engine_mask_t emask;
		int i;

		 
		emask = uncore->gt->info.engine_mask;

		uncore->fw_get_funcs = &uncore_get_fallback;
		if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70))
			fw_domain_init(uncore, FW_DOMAIN_ID_GT,
				       FORCEWAKE_GT_GEN9,
				       FORCEWAKE_ACK_GT_MTL);
		else
			fw_domain_init(uncore, FW_DOMAIN_ID_GT,
				       FORCEWAKE_GT_GEN9,
				       FORCEWAKE_ACK_GT_GEN9);

		if (RCS_MASK(uncore->gt) || CCS_MASK(uncore->gt))
			fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
				       FORCEWAKE_RENDER_GEN9,
				       FORCEWAKE_ACK_RENDER_GEN9);

		for (i = 0; i < I915_MAX_VCS; i++) {
			if (!__HAS_ENGINE(emask, _VCS(i)))
				continue;

			fw_domain_init(uncore, FW_DOMAIN_ID_MEDIA_VDBOX0 + i,
				       FORCEWAKE_MEDIA_VDBOX_GEN11(i),
				       FORCEWAKE_ACK_MEDIA_VDBOX_GEN11(i));
		}
		for (i = 0; i < I915_MAX_VECS; i++) {
			if (!__HAS_ENGINE(emask, _VECS(i)))
				continue;

			fw_domain_init(uncore, FW_DOMAIN_ID_MEDIA_VEBOX0 + i,
				       FORCEWAKE_MEDIA_VEBOX_GEN11(i),
				       FORCEWAKE_ACK_MEDIA_VEBOX_GEN11(i));
		}

		if (uncore->gt->type == GT_MEDIA)
			fw_domain_init(uncore, FW_DOMAIN_ID_GSC,
				       FORCEWAKE_REQ_GSC, FORCEWAKE_ACK_GSC);
	} else if (IS_GRAPHICS_VER(i915, 9, 10)) {
		uncore->fw_get_funcs = &uncore_get_fallback;
		fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
			       FORCEWAKE_RENDER_GEN9,
			       FORCEWAKE_ACK_RENDER_GEN9);
		fw_domain_init(uncore, FW_DOMAIN_ID_GT,
			       FORCEWAKE_GT_GEN9,
			       FORCEWAKE_ACK_GT_GEN9);
		fw_domain_init(uncore, FW_DOMAIN_ID_MEDIA,
			       FORCEWAKE_MEDIA_GEN9, FORCEWAKE_ACK_MEDIA_GEN9);
	} else if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)) {
		uncore->fw_get_funcs = &uncore_get_normal;
		fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
			       FORCEWAKE_VLV, FORCEWAKE_ACK_VLV);
		fw_domain_init(uncore, FW_DOMAIN_ID_MEDIA,
			       FORCEWAKE_MEDIA_VLV, FORCEWAKE_ACK_MEDIA_VLV);
	} else if (IS_HASWELL(i915) || IS_BROADWELL(i915)) {
		uncore->fw_get_funcs = &uncore_get_thread_status;
		fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
			       FORCEWAKE_MT, FORCEWAKE_ACK_HSW);
	} else if (IS_IVYBRIDGE(i915)) {
		u32 ecobus;

		 

		 
		uncore->fw_get_funcs = &uncore_get_thread_status;

		 

		__raw_uncore_write32(uncore, FORCEWAKE, 0);
		__raw_posting_read(uncore, ECOBUS);

		ret = __fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
				       FORCEWAKE_MT, FORCEWAKE_MT_ACK);
		if (ret)
			goto out;

		spin_lock_irq(&uncore->lock);
		fw_domains_get_with_thread_status(uncore, FORCEWAKE_RENDER);
		ecobus = __raw_uncore_read32(uncore, ECOBUS);
		fw_domains_put(uncore, FORCEWAKE_RENDER);
		spin_unlock_irq(&uncore->lock);

		if (!(ecobus & FORCEWAKE_MT_ENABLE)) {
			drm_info(&i915->drm, "No MT forcewake available on Ivybridge, this can result in issues\n");
			drm_info(&i915->drm, "when using vblank-synced partial screen updates.\n");
			fw_domain_fini(uncore, FW_DOMAIN_ID_RENDER);
			fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
				       FORCEWAKE, FORCEWAKE_ACK);
		}
	} else if (GRAPHICS_VER(i915) == 6) {
		uncore->fw_get_funcs = &uncore_get_thread_status;
		fw_domain_init(uncore, FW_DOMAIN_ID_RENDER,
			       FORCEWAKE, FORCEWAKE_ACK);
	}

#undef fw_domain_init

	 
	drm_WARN_ON(&i915->drm, !ret && uncore->fw_domains == 0);

out:
	if (ret)
		intel_uncore_fw_domains_fini(uncore);

	return ret;
}

#define ASSIGN_FW_DOMAINS_TABLE(uncore, d) \
{ \
	(uncore)->fw_domains_table = \
			(struct intel_forcewake_range *)(d); \
	(uncore)->fw_domains_table_entries = ARRAY_SIZE((d)); \
}

#define ASSIGN_SHADOW_TABLE(uncore, d) \
{ \
	(uncore)->shadowed_reg_table = d; \
	(uncore)->shadowed_reg_table_entries = ARRAY_SIZE((d)); \
}

static int i915_pmic_bus_access_notifier(struct notifier_block *nb,
					 unsigned long action, void *data)
{
	struct intel_uncore *uncore = container_of(nb,
			struct intel_uncore, pmic_bus_access_nb);

	switch (action) {
	case MBI_PMIC_BUS_ACCESS_BEGIN:
		 
		disable_rpm_wakeref_asserts(uncore->rpm);
		intel_uncore_forcewake_get(uncore, FORCEWAKE_ALL);
		enable_rpm_wakeref_asserts(uncore->rpm);
		break;
	case MBI_PMIC_BUS_ACCESS_END:
		intel_uncore_forcewake_put(uncore, FORCEWAKE_ALL);
		break;
	}

	return NOTIFY_OK;
}

static void uncore_unmap_mmio(struct drm_device *drm, void *regs)
{
	iounmap((void __iomem *)regs);
}

int intel_uncore_setup_mmio(struct intel_uncore *uncore, phys_addr_t phys_addr)
{
	struct drm_i915_private *i915 = uncore->i915;
	int mmio_size;

	 
	if (IS_DGFX(i915) || GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70))
		mmio_size = 4 * 1024 * 1024;
	else if (GRAPHICS_VER(i915) >= 5)
		mmio_size = 2 * 1024 * 1024;
	else
		mmio_size = 512 * 1024;

	uncore->regs = ioremap(phys_addr, mmio_size);
	if (uncore->regs == NULL) {
		drm_err(&i915->drm, "failed to map registers\n");
		return -EIO;
	}

	return drmm_add_action_or_reset(&i915->drm, uncore_unmap_mmio,
					(void __force *)uncore->regs);
}

void intel_uncore_init_early(struct intel_uncore *uncore,
			     struct intel_gt *gt)
{
	spin_lock_init(&uncore->lock);
	uncore->i915 = gt->i915;
	uncore->gt = gt;
	uncore->rpm = &gt->i915->runtime_pm;
}

static void uncore_raw_init(struct intel_uncore *uncore)
{
	GEM_BUG_ON(intel_uncore_has_forcewake(uncore));

	if (intel_vgpu_active(uncore->i915)) {
		ASSIGN_RAW_WRITE_MMIO_VFUNCS(uncore, vgpu);
		ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, vgpu);
	} else if (GRAPHICS_VER(uncore->i915) == 5) {
		ASSIGN_RAW_WRITE_MMIO_VFUNCS(uncore, gen5);
		ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, gen5);
	} else {
		ASSIGN_RAW_WRITE_MMIO_VFUNCS(uncore, gen2);
		ASSIGN_RAW_READ_MMIO_VFUNCS(uncore, gen2);
	}
}

static int uncore_media_forcewake_init(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;

	if (MEDIA_VER(i915) >= 13) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __xelpmp_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, xelpmp_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else {
		MISSING_CASE(MEDIA_VER(i915));
		return -ENODEV;
	}

	return 0;
}

static int uncore_forcewake_init(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;
	int ret;

	GEM_BUG_ON(!intel_uncore_has_forcewake(uncore));

	ret = intel_uncore_fw_domains_init(uncore);
	if (ret)
		return ret;
	forcewake_early_sanitize(uncore, 0);

	ASSIGN_READ_MMIO_VFUNCS(uncore, fwtable);

	if (uncore->gt->type == GT_MEDIA)
		return uncore_media_forcewake_init(uncore);

	if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __mtl_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, mtl_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 60)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __pvc_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, pvc_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 55)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __dg2_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, dg2_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 50)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __xehp_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen12_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER(i915) >= 12) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __gen12_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen12_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER(i915) == 11) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __gen11_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen11_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (IS_GRAPHICS_VER(i915, 9, 10)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __gen9_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen8_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (IS_CHERRYVIEW(i915)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __chv_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen8_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (GRAPHICS_VER(i915) == 8) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __gen6_fw_ranges);
		ASSIGN_SHADOW_TABLE(uncore, gen8_shadowed_regs);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, fwtable);
	} else if (IS_VALLEYVIEW(i915)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __vlv_fw_ranges);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, gen6);
	} else if (IS_GRAPHICS_VER(i915, 6, 7)) {
		ASSIGN_FW_DOMAINS_TABLE(uncore, __gen6_fw_ranges);
		ASSIGN_WRITE_MMIO_VFUNCS(uncore, gen6);
	}

	uncore->pmic_bus_access_nb.notifier_call = i915_pmic_bus_access_notifier;
	iosf_mbi_register_pmic_bus_access_notifier(&uncore->pmic_bus_access_nb);

	return 0;
}

static int sanity_check_mmio_access(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;

	if (GRAPHICS_VER(i915) < 8)
		return 0;

	 
#define COND (__raw_uncore_read32(uncore, FORCEWAKE_MT) != ~0)
	if (wait_for(COND, 2000) == -ETIMEDOUT) {
		drm_err(&i915->drm, "Device is non-operational; MMIO access returns 0xFFFFFFFF!\n");
		return -EIO;
	}

	return 0;
}

int intel_uncore_init_mmio(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;
	int ret;

	ret = sanity_check_mmio_access(uncore);
	if (ret)
		return ret;

	 
	if (IS_DGFX(i915) &&
	    !(__raw_uncore_read32(uncore, GU_CNTL) & LMEM_INIT)) {
		drm_err(&i915->drm, "LMEM not initialized by firmware\n");
		return -ENODEV;
	}

	if (GRAPHICS_VER(i915) > 5 && !intel_vgpu_active(i915))
		uncore->flags |= UNCORE_HAS_FORCEWAKE;

	if (!intel_uncore_has_forcewake(uncore)) {
		uncore_raw_init(uncore);
	} else {
		ret = uncore_forcewake_init(uncore);
		if (ret)
			return ret;
	}

	 
	GEM_BUG_ON(intel_uncore_has_forcewake(uncore) != !!uncore->fw_get_funcs);
	GEM_BUG_ON(intel_uncore_has_forcewake(uncore) != !!uncore->funcs.read_fw_domains);
	GEM_BUG_ON(intel_uncore_has_forcewake(uncore) != !!uncore->funcs.write_fw_domains);

	if (HAS_FPGA_DBG_UNCLAIMED(i915))
		uncore->flags |= UNCORE_HAS_FPGA_DBG_UNCLAIMED;

	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915))
		uncore->flags |= UNCORE_HAS_DBG_UNCLAIMED;

	if (IS_GRAPHICS_VER(i915, 6, 7))
		uncore->flags |= UNCORE_HAS_FIFO;

	 
	if (intel_uncore_unclaimed_mmio(uncore))
		drm_dbg(&i915->drm, "unclaimed mmio detected on uncore init, clearing\n");

	return 0;
}

 
void intel_uncore_prune_engine_fw_domains(struct intel_uncore *uncore,
					  struct intel_gt *gt)
{
	enum forcewake_domains fw_domains = uncore->fw_domains;
	enum forcewake_domain_id domain_id;
	int i;

	if (!intel_uncore_has_forcewake(uncore) || GRAPHICS_VER(uncore->i915) < 11)
		return;

	for (i = 0; i < I915_MAX_VCS; i++) {
		domain_id = FW_DOMAIN_ID_MEDIA_VDBOX0 + i;

		if (HAS_ENGINE(gt, _VCS(i)))
			continue;

		 
		if (GRAPHICS_VER_FULL(uncore->i915) >= IP_VER(12, 50) && i % 2 == 0) {
			if ((i + 1 < I915_MAX_VCS) && HAS_ENGINE(gt, _VCS(i + 1)))
				continue;

			if (HAS_ENGINE(gt, _VECS(i / 2)))
				continue;
		}

		if (fw_domains & BIT(domain_id))
			fw_domain_fini(uncore, domain_id);
	}

	for (i = 0; i < I915_MAX_VECS; i++) {
		domain_id = FW_DOMAIN_ID_MEDIA_VEBOX0 + i;

		if (HAS_ENGINE(gt, _VECS(i)))
			continue;

		if (fw_domains & BIT(domain_id))
			fw_domain_fini(uncore, domain_id);
	}

	if ((fw_domains & BIT(FW_DOMAIN_ID_GSC)) && !HAS_ENGINE(gt, GSC0))
		fw_domain_fini(uncore, FW_DOMAIN_ID_GSC);
}

 
static void driver_initiated_flr(struct intel_uncore *uncore)
{
	struct drm_i915_private *i915 = uncore->i915;
	const unsigned int flr_timeout_ms = 3000;  
	int ret;

	drm_dbg(&i915->drm, "Triggering Driver-FLR\n");

	 
	ret = intel_wait_for_register_fw(uncore, GU_CNTL, DRIVERFLR, 0, flr_timeout_ms);
	if (ret) {
		drm_err(&i915->drm,
			"Failed to wait for Driver-FLR bit to clear! %d\n",
			ret);
		return;
	}
	intel_uncore_write_fw(uncore, GU_DEBUG, DRIVERFLR_STATUS);

	 
	intel_uncore_rmw_fw(uncore, GU_CNTL, 0, DRIVERFLR);

	 
	ret = intel_wait_for_register_fw(uncore, GU_CNTL,
					 DRIVERFLR, 0,
					 flr_timeout_ms);
	if (ret) {
		drm_err(&i915->drm, "Driver-FLR-teardown wait completion failed! %d\n", ret);
		return;
	}

	 
	ret = intel_wait_for_register_fw(uncore, GU_DEBUG,
					 DRIVERFLR_STATUS, DRIVERFLR_STATUS,
					 flr_timeout_ms);
	if (ret) {
		drm_err(&i915->drm, "Driver-FLR-reinit wait completion failed! %d\n", ret);
		return;
	}

	 
	intel_uncore_write_fw(uncore, GU_DEBUG, DRIVERFLR_STATUS);
}

 
void intel_uncore_fini_mmio(struct drm_device *dev, void *data)
{
	struct intel_uncore *uncore = data;

	if (intel_uncore_has_forcewake(uncore)) {
		iosf_mbi_punit_acquire();
		iosf_mbi_unregister_pmic_bus_access_notifier_unlocked(
			&uncore->pmic_bus_access_nb);
		intel_uncore_forcewake_reset(uncore);
		intel_uncore_fw_domains_fini(uncore);
		iosf_mbi_punit_release();
	}

	if (intel_uncore_needs_flr_on_fini(uncore))
		driver_initiated_flr(uncore);
}

 
int __intel_wait_for_register_fw(struct intel_uncore *uncore,
				 i915_reg_t reg,
				 u32 mask,
				 u32 value,
				 unsigned int fast_timeout_us,
				 unsigned int slow_timeout_ms,
				 u32 *out_value)
{
	u32 reg_value = 0;
#define done (((reg_value = intel_uncore_read_fw(uncore, reg)) & mask) == value)
	int ret;

	 
	might_sleep_if(slow_timeout_ms);
	GEM_BUG_ON(fast_timeout_us > 20000);
	GEM_BUG_ON(!fast_timeout_us && !slow_timeout_ms);

	ret = -ETIMEDOUT;
	if (fast_timeout_us && fast_timeout_us <= 20000)
		ret = _wait_for_atomic(done, fast_timeout_us, 0);
	if (ret && slow_timeout_ms)
		ret = wait_for(done, slow_timeout_ms);

	if (out_value)
		*out_value = reg_value;

	return ret;
#undef done
}

 
int __intel_wait_for_register(struct intel_uncore *uncore,
			      i915_reg_t reg,
			      u32 mask,
			      u32 value,
			      unsigned int fast_timeout_us,
			      unsigned int slow_timeout_ms,
			      u32 *out_value)
{
	unsigned fw =
		intel_uncore_forcewake_for_reg(uncore, reg, FW_REG_READ);
	u32 reg_value;
	int ret;

	might_sleep_if(slow_timeout_ms);

	spin_lock_irq(&uncore->lock);
	intel_uncore_forcewake_get__locked(uncore, fw);

	ret = __intel_wait_for_register_fw(uncore,
					   reg, mask, value,
					   fast_timeout_us, 0, &reg_value);

	intel_uncore_forcewake_put__locked(uncore, fw);
	spin_unlock_irq(&uncore->lock);

	if (ret && slow_timeout_ms)
		ret = __wait_for(reg_value = intel_uncore_read_notrace(uncore,
								       reg),
				 (reg_value & mask) == value,
				 slow_timeout_ms * 1000, 10, 1000);

	 
	trace_i915_reg_rw(false, reg, reg_value, sizeof(reg_value), true);

	if (out_value)
		*out_value = reg_value;

	return ret;
}

bool intel_uncore_unclaimed_mmio(struct intel_uncore *uncore)
{
	bool ret;

	if (!uncore->debug)
		return false;

	spin_lock_irq(&uncore->debug->lock);
	ret = check_for_unclaimed_mmio(uncore);
	spin_unlock_irq(&uncore->debug->lock);

	return ret;
}

bool
intel_uncore_arm_unclaimed_mmio_detection(struct intel_uncore *uncore)
{
	bool ret = false;

	if (drm_WARN_ON(&uncore->i915->drm, !uncore->debug))
		return false;

	spin_lock_irq(&uncore->debug->lock);

	if (unlikely(uncore->debug->unclaimed_mmio_check <= 0))
		goto out;

	if (unlikely(check_for_unclaimed_mmio(uncore))) {
		if (!uncore->i915->params.mmio_debug) {
			drm_dbg(&uncore->i915->drm,
				"Unclaimed register detected, "
				"enabling oneshot unclaimed register reporting. "
				"Please use i915.mmio_debug=N for more information.\n");
			uncore->i915->params.mmio_debug++;
		}
		uncore->debug->unclaimed_mmio_check--;
		ret = true;
	}

out:
	spin_unlock_irq(&uncore->debug->lock);

	return ret;
}

 
enum forcewake_domains
intel_uncore_forcewake_for_reg(struct intel_uncore *uncore,
			       i915_reg_t reg, unsigned int op)
{
	enum forcewake_domains fw_domains = 0;

	drm_WARN_ON(&uncore->i915->drm, !op);

	if (!intel_uncore_has_forcewake(uncore))
		return 0;

	if (op & FW_REG_READ)
		fw_domains = uncore->funcs.read_fw_domains(uncore, reg);

	if (op & FW_REG_WRITE)
		fw_domains |= uncore->funcs.write_fw_domains(uncore, reg);

	drm_WARN_ON(&uncore->i915->drm, fw_domains & ~uncore->fw_domains);

	return fw_domains;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/mock_uncore.c"
#include "selftests/intel_uncore.c"
#endif
