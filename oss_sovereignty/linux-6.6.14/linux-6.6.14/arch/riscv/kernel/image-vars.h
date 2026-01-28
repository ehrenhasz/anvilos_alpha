#ifndef __RISCV_KERNEL_IMAGE_VARS_H
#define __RISCV_KERNEL_IMAGE_VARS_H
#ifndef LINKER_SCRIPT
#error This file should only be included in vmlinux.lds.S
#endif
#ifdef CONFIG_EFI
__efistub__start		= _start;
__efistub__start_kernel		= _start_kernel;
__efistub__end			= _end;
__efistub__edata		= _edata;
__efistub___init_text_end	= __init_text_end;
__efistub_screen_info		= screen_info;
#endif
#endif  
