
#include <linux/efi.h>

#include <asm/boot.h>
#include <asm/desc.h>
#include <asm/efi.h>

#include "efistub.h"
#include "x86-stub.h"

bool efi_no5lvl;

static void (*la57_toggle)(void *cr3);

static const struct desc_struct gdt[] = {
	[GDT_ENTRY_KERNEL32_CS] = GDT_ENTRY_INIT(0xc09b, 0, 0xfffff),
	[GDT_ENTRY_KERNEL_CS]   = GDT_ENTRY_INIT(0xa09b, 0, 0xfffff),
};

 
efi_status_t efi_setup_5level_paging(void)
{
	u8 tmpl_size = (u8 *)&trampoline_ljmp_imm_offset - (u8 *)&trampoline_32bit_src;
	efi_status_t status;
	u8 *la57_code;

	if (!efi_is_64bit())
		return EFI_SUCCESS;

	 
	if (native_cpuid_eax(0) < 7 ||
	    !(native_cpuid_ecx(7) & (1 << (X86_FEATURE_LA57 & 31))))
		return EFI_SUCCESS;

	 
	status = efi_allocate_pages(2 * PAGE_SIZE, (unsigned long *)&la57_code,
				    U32_MAX);
	if (status != EFI_SUCCESS)
		return status;

	la57_toggle = memcpy(la57_code, trampoline_32bit_src, tmpl_size);
	memset(la57_code + tmpl_size, 0x90, PAGE_SIZE - tmpl_size);

	 
	*(u32 *)&la57_code[trampoline_ljmp_imm_offset] += (unsigned long)la57_code;

	efi_adjust_memory_range_protection((unsigned long)la57_toggle, PAGE_SIZE);

	return EFI_SUCCESS;
}

void efi_5level_switch(void)
{
	bool want_la57 = IS_ENABLED(CONFIG_X86_5LEVEL) && !efi_no5lvl;
	bool have_la57 = native_read_cr4() & X86_CR4_LA57;
	bool need_toggle = want_la57 ^ have_la57;
	u64 *pgt = (void *)la57_toggle + PAGE_SIZE;
	u64 *cr3 = (u64 *)__native_read_cr3();
	u64 *new_cr3;

	if (!la57_toggle || !need_toggle)
		return;

	if (!have_la57) {
		 
		new_cr3 = memset(pgt, 0, PAGE_SIZE);
		new_cr3[0] = (u64)cr3 | _PAGE_TABLE_NOENC;
	} else {
		 
		new_cr3 = (u64 *)(cr3[0] & PAGE_MASK);

		 
		if ((u64)new_cr3 > U32_MAX)
			new_cr3 = memcpy(pgt, new_cr3, PAGE_SIZE);
	}

	native_load_gdt(&(struct desc_ptr){ sizeof(gdt) - 1, (u64)gdt });

	la57_toggle(new_cr3);
}
