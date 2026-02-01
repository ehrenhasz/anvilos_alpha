
 

#include <linux/export.h>
#include <linux/cc_platform.h>

#include <asm/coco.h>
#include <asm/processor.h>

enum cc_vendor cc_vendor __ro_after_init = CC_VENDOR_NONE;
static u64 cc_mask __ro_after_init;

static bool noinstr intel_cc_platform_has(enum cc_attr attr)
{
	switch (attr) {
	case CC_ATTR_GUEST_UNROLL_STRING_IO:
	case CC_ATTR_HOTPLUG_DISABLED:
	case CC_ATTR_GUEST_MEM_ENCRYPT:
	case CC_ATTR_MEM_ENCRYPT:
		return true;
	default:
		return false;
	}
}

 
static __maybe_unused __always_inline bool amd_cc_platform_vtom(enum cc_attr attr)
{
	switch (attr) {
	case CC_ATTR_GUEST_MEM_ENCRYPT:
	case CC_ATTR_MEM_ENCRYPT:
		return true;
	default:
		return false;
	}
}

 

static bool noinstr amd_cc_platform_has(enum cc_attr attr)
{
#ifdef CONFIG_AMD_MEM_ENCRYPT

	if (sev_status & MSR_AMD64_SNP_VTOM)
		return amd_cc_platform_vtom(attr);

	switch (attr) {
	case CC_ATTR_MEM_ENCRYPT:
		return sme_me_mask;

	case CC_ATTR_HOST_MEM_ENCRYPT:
		return sme_me_mask && !(sev_status & MSR_AMD64_SEV_ENABLED);

	case CC_ATTR_GUEST_MEM_ENCRYPT:
		return sev_status & MSR_AMD64_SEV_ENABLED;

	case CC_ATTR_GUEST_STATE_ENCRYPT:
		return sev_status & MSR_AMD64_SEV_ES_ENABLED;

	 
	case CC_ATTR_GUEST_UNROLL_STRING_IO:
		return (sev_status & MSR_AMD64_SEV_ENABLED) &&
			!(sev_status & MSR_AMD64_SEV_ES_ENABLED);

	case CC_ATTR_GUEST_SEV_SNP:
		return sev_status & MSR_AMD64_SEV_SNP_ENABLED;

	default:
		return false;
	}
#else
	return false;
#endif
}

bool noinstr cc_platform_has(enum cc_attr attr)
{
	switch (cc_vendor) {
	case CC_VENDOR_AMD:
		return amd_cc_platform_has(attr);
	case CC_VENDOR_INTEL:
		return intel_cc_platform_has(attr);
	default:
		return false;
	}
}
EXPORT_SYMBOL_GPL(cc_platform_has);

u64 cc_mkenc(u64 val)
{
	 
	switch (cc_vendor) {
	case CC_VENDOR_AMD:
		if (sev_status & MSR_AMD64_SNP_VTOM)
			return val & ~cc_mask;
		else
			return val | cc_mask;
	case CC_VENDOR_INTEL:
		return val & ~cc_mask;
	default:
		return val;
	}
}

u64 cc_mkdec(u64 val)
{
	 
	switch (cc_vendor) {
	case CC_VENDOR_AMD:
		if (sev_status & MSR_AMD64_SNP_VTOM)
			return val | cc_mask;
		else
			return val & ~cc_mask;
	case CC_VENDOR_INTEL:
		return val | cc_mask;
	default:
		return val;
	}
}
EXPORT_SYMBOL_GPL(cc_mkdec);

__init void cc_set_mask(u64 mask)
{
	cc_mask = mask;
}
