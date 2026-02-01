
 

#include <linux/efi.h>

#include <asm/efi.h>
#include <asm/sections.h>
#include <asm/unaligned.h>

#include "efistub.h"

unsigned long stext_offset(void)
{
	 
	return _start_kernel - _start;
}

efi_status_t handle_kernel_image(unsigned long *image_addr,
				 unsigned long *image_size,
				 unsigned long *reserve_addr,
				 unsigned long *reserve_size,
				 efi_loaded_image_t *image,
				 efi_handle_t image_handle)
{
	unsigned long kernel_size, kernel_codesize, kernel_memsize;
	efi_status_t status;

	kernel_size = _edata - _start;
	kernel_codesize = __init_text_end - _start;
	kernel_memsize = kernel_size + (_end - _edata);
	*image_addr = (unsigned long)_start;
	*image_size = kernel_memsize;
	*reserve_size = *image_size;

	status = efi_kaslr_relocate_kernel(image_addr,
					   reserve_addr, reserve_size,
					   kernel_size, kernel_codesize, kernel_memsize,
					   efi_kaslr_get_phys_seed(image_handle));
	if (status != EFI_SUCCESS) {
		efi_err("Failed to relocate kernel\n");
		*image_size = 0;
	}

	return status;
}

void efi_icache_sync(unsigned long start, unsigned long end)
{
	asm volatile ("fence.i" ::: "memory");
}
