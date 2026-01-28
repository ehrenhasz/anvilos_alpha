#ifndef _ASM_X86_BOOT_H
#define _ASM_X86_BOOT_H
#include <asm/pgtable_types.h>
#include <uapi/asm/boot.h>
#define LOAD_PHYSICAL_ADDR ((CONFIG_PHYSICAL_START \
				+ (CONFIG_PHYSICAL_ALIGN - 1)) \
				& ~(CONFIG_PHYSICAL_ALIGN - 1))
#ifdef CONFIG_X86_64
# define MIN_KERNEL_ALIGN_LG2	PMD_SHIFT
#else
# define MIN_KERNEL_ALIGN_LG2	(PAGE_SHIFT + THREAD_SIZE_ORDER)
#endif
#define MIN_KERNEL_ALIGN	(_AC(1, UL) << MIN_KERNEL_ALIGN_LG2)
#if (CONFIG_PHYSICAL_ALIGN & (CONFIG_PHYSICAL_ALIGN-1)) || \
	(CONFIG_PHYSICAL_ALIGN < MIN_KERNEL_ALIGN)
# error "Invalid value for CONFIG_PHYSICAL_ALIGN"
#endif
#if defined(CONFIG_KERNEL_BZIP2)
# define BOOT_HEAP_SIZE		0x400000
#elif defined(CONFIG_KERNEL_ZSTD)
# define BOOT_HEAP_SIZE		 0x30000
#else
# define BOOT_HEAP_SIZE		 0x10000
#endif
#ifdef CONFIG_X86_64
# define BOOT_STACK_SIZE	0x4000
# define BOOT_INIT_PGT_SIZE	(6*4096)
# define BOOT_PGT_SIZE_WARN	(28*4096)
# define BOOT_PGT_SIZE		(32*4096)
#else  
# define BOOT_STACK_SIZE	0x1000
#endif
#ifndef __ASSEMBLY__
extern unsigned int output_len;
extern const unsigned long kernel_total_size;
unsigned long decompress_kernel(unsigned char *outbuf, unsigned long virt_addr,
				void (*error)(char *x));
#endif
#endif  
