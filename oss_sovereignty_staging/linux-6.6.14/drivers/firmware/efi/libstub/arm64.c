
 


#include <linux/efi.h>
#include <asm/efi.h>
#include <asm/image.h>
#include <asm/memory.h>
#include <asm/sysreg.h>

#include "efistub.h"

static bool system_needs_vamap(void)
{
	const struct efi_smbios_type4_record *record;
	const u32 __aligned(1) *socid;
	const u8 *version;

	 
	record = (struct efi_smbios_type4_record *)efi_get_smbios_record(4);
	if (!record)
		return false;

	socid = (u32 *)record->processor_id;
	switch (*socid & 0xffff000f) {
		static char const altra[] = "Ampere(TM) Altra(TM) Processor";
		static char const emag[] = "eMAG";

	default:
		version = efi_get_smbios_string(&record->header, 4,
						processor_version);
		if (!version || (strncmp(version, altra, sizeof(altra) - 1) &&
				 strncmp(version, emag, sizeof(emag) - 1)))
			break;

		fallthrough;

	case 0x0a160001:	
	case 0x0a160002:	
		efi_warn("Working around broken SetVirtualAddressMap()\n");
		return true;
	}

	return false;
}

efi_status_t check_platform_features(void)
{
	u64 tg;

	 
	if (VA_BITS_MIN >= 48 && !system_needs_vamap())
		efi_novamap = true;

	 
	if (IS_ENABLED(CONFIG_ARM64_4K_PAGES))
		return EFI_SUCCESS;

	tg = (read_cpuid(ID_AA64MMFR0_EL1) >> ID_AA64MMFR0_EL1_TGRAN_SHIFT) & 0xf;
	if (tg < ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MIN || tg > ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MAX) {
		if (IS_ENABLED(CONFIG_ARM64_64K_PAGES))
			efi_err("This 64 KB granular kernel is not supported by your CPU\n");
		else
			efi_err("This 16 KB granular kernel is not supported by your CPU\n");
		return EFI_UNSUPPORTED;
	}
	return EFI_SUCCESS;
}

#ifdef CONFIG_ARM64_WORKAROUND_CLEAN_CACHE
#define DCTYPE	"civac"
#else
#define DCTYPE	"cvau"
#endif

u32 __weak code_size;

void efi_cache_sync_image(unsigned long image_base,
			  unsigned long alloc_size)
{
	u32 ctr = read_cpuid_effective_cachetype();
	u64 lsize = 4 << cpuid_feature_extract_unsigned_field(ctr,
						CTR_EL0_DminLine_SHIFT);

	 
	if (!(ctr & BIT(CTR_EL0_IDC_SHIFT))) {
		unsigned long base = image_base;
		unsigned long size = code_size;

		do {
			asm("dc " DCTYPE ", %0" :: "r"(base));
			base += lsize;
			size -= lsize;
		} while (size >= lsize);
	}

	asm("ic ialluis");
	dsb(ish);
	isb();

	efi_remap_image(image_base, alloc_size, code_size);
}

unsigned long __weak primary_entry_offset(void)
{
	 
       return 0;
}

void __noreturn efi_enter_kernel(unsigned long entrypoint,
				 unsigned long fdt_addr,
				 unsigned long fdt_size)
{
	void (* __noreturn enter_kernel)(u64, u64, u64, u64);

	enter_kernel = (void *)entrypoint + primary_entry_offset();
	enter_kernel(fdt_addr, 0, 0, 0);
}
