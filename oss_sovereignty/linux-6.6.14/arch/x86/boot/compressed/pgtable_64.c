
#include "misc.h"
#include <asm/e820/types.h>
#include <asm/processor.h>
#include "pgtable.h"
#include "../string.h"
#include "efi.h"

#define BIOS_START_MIN		0x20000U	 
#define BIOS_START_MAX		0x9f000U	 

#ifdef CONFIG_X86_5LEVEL
 
unsigned int __section(".data") __pgtable_l5_enabled;
unsigned int __section(".data") pgdir_shift = 39;
unsigned int __section(".data") ptrs_per_p4d = 1;
#endif

 
static char trampoline_save[TRAMPOLINE_32BIT_SIZE];

 
unsigned long *trampoline_32bit __section(".data");

extern struct boot_params *boot_params;
int cmdline_find_option_bool(const char *option);

static unsigned long find_trampoline_placement(void)
{
	unsigned long bios_start = 0, ebda_start = 0;
	struct boot_e820_entry *entry;
	char *signature;
	int i;

	 

	 
	signature = (char *)&boot_params->efi_info.efi_loader_signature;
	if (strncmp(signature, EFI32_LOADER_SIGNATURE, 4) &&
	    strncmp(signature, EFI64_LOADER_SIGNATURE, 4)) {
		ebda_start = *(unsigned short *)0x40e << 4;
		bios_start = *(unsigned short *)0x413 << 10;
	}

	if (bios_start < BIOS_START_MIN || bios_start > BIOS_START_MAX)
		bios_start = BIOS_START_MAX;

	if (ebda_start > BIOS_START_MIN && ebda_start < bios_start)
		bios_start = ebda_start;

	bios_start = round_down(bios_start, PAGE_SIZE);

	 
	for (i = boot_params->e820_entries - 1; i >= 0; i--) {
		unsigned long new = bios_start;

		entry = &boot_params->e820_table[i];

		 
		if (bios_start <= entry->addr)
			continue;

		 
		if (entry->type != E820_TYPE_RAM)
			continue;

		 
		if (bios_start > entry->addr + entry->size)
			new = entry->addr + entry->size;

		 
		new = round_down(new, PAGE_SIZE);

		 
		if (new - TRAMPOLINE_32BIT_SIZE < entry->addr)
			continue;

		 
		if (new - TRAMPOLINE_32BIT_SIZE > bios_start)
			break;

		bios_start = new;
		break;
	}

	 
	return bios_start - TRAMPOLINE_32BIT_SIZE;
}

asmlinkage void configure_5level_paging(struct boot_params *bp, void *pgtable)
{
	void (*toggle_la57)(void *cr3);
	bool l5_required = false;

	 
	boot_params = bp;

	 
	if (IS_ENABLED(CONFIG_X86_5LEVEL) &&
			!cmdline_find_option_bool("no5lvl") &&
			native_cpuid_eax(0) >= 7 &&
			(native_cpuid_ecx(7) & (1 << (X86_FEATURE_LA57 & 31)))) {
		l5_required = true;

		 
		__pgtable_l5_enabled = 1;
		pgdir_shift = 48;
		ptrs_per_p4d = 512;
	}

	 
	if (l5_required == !!(native_read_cr4() & X86_CR4_LA57))
		return;

	trampoline_32bit = (unsigned long *)find_trampoline_placement();

	 
	memcpy(trampoline_save, trampoline_32bit, TRAMPOLINE_32BIT_SIZE);

	 
	memset(trampoline_32bit, 0, TRAMPOLINE_32BIT_SIZE);

	 
	toggle_la57 = memcpy(trampoline_32bit +
			TRAMPOLINE_32BIT_CODE_OFFSET / sizeof(unsigned long),
			&trampoline_32bit_src, TRAMPOLINE_32BIT_CODE_SIZE);

	 
	*(u32 *)((u8 *)toggle_la57 + trampoline_ljmp_imm_offset) +=
						(unsigned long)toggle_la57;

	 

	if (l5_required) {
		 
		*trampoline_32bit = __native_read_cr3() | _PAGE_TABLE_NOENC;
	} else {
		unsigned long src;

		 
		src = *(unsigned long *)__native_read_cr3() & PAGE_MASK;
		memcpy(trampoline_32bit, (void *)src, PAGE_SIZE);
	}

	toggle_la57(trampoline_32bit);

	 
	memcpy(pgtable, trampoline_32bit, PAGE_SIZE);
	native_write_cr3((unsigned long)pgtable);

	 
	memcpy(trampoline_32bit, trampoline_save, TRAMPOLINE_32BIT_SIZE);
}
