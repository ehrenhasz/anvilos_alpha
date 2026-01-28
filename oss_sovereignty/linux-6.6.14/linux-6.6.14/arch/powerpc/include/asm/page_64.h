#ifndef _ASM_POWERPC_PAGE_64_H
#define _ASM_POWERPC_PAGE_64_H
#include <asm/asm-const.h>
#define HW_PAGE_SHIFT		12
#define HW_PAGE_SIZE		(ASM_CONST(1) << HW_PAGE_SHIFT)
#define HW_PAGE_MASK		(~(HW_PAGE_SIZE-1))
#define PAGE_FACTOR		(PAGE_SHIFT - HW_PAGE_SHIFT)
#define SID_SHIFT		28
#define SID_MASK		ASM_CONST(0xfffffffff)
#define ESID_MASK		0xfffffffff0000000UL
#define GET_ESID(x)		(((x) >> SID_SHIFT) & SID_MASK)
#define SID_SHIFT_1T		40
#define SID_MASK_1T		0xffffffUL
#define ESID_MASK_1T		0xffffff0000000000UL
#define GET_ESID_1T(x)		(((x) >> SID_SHIFT_1T) & SID_MASK_1T)
#ifndef __ASSEMBLY__
#include <asm/cache.h>
typedef unsigned long pte_basic_t;
static inline void clear_page(void *addr)
{
	unsigned long iterations;
	unsigned long onex, twox, fourx, eightx;
	iterations = ppc64_caches.l1d.blocks_per_page / 8;
	onex = ppc64_caches.l1d.block_size;
	twox = onex << 1;
	fourx = onex << 2;
	eightx = onex << 3;
	asm volatile(
	"mtctr	%1	# clear_page\n\
	.balign	16\n\
1:	dcbz	0,%0\n\
	dcbz	%3,%0\n\
	dcbz	%4,%0\n\
	dcbz	%5,%0\n\
	dcbz	%6,%0\n\
	dcbz	%7,%0\n\
	dcbz	%8,%0\n\
	dcbz	%9,%0\n\
	add	%0,%0,%10\n\
	bdnz+	1b"
	: "=&r" (addr)
	: "r" (iterations), "0" (addr), "b" (onex), "b" (twox),
		"b" (twox+onex), "b" (fourx), "b" (fourx+onex),
		"b" (twox+fourx), "b" (eightx-onex), "r" (eightx)
	: "ctr", "memory");
}
extern void copy_page(void *to, void *from);
extern u64 ppc64_pft_size;
#endif  
#define VM_DATA_DEFAULT_FLAGS \
	(is_32bit_task() ? \
	 VM_DATA_DEFAULT_FLAGS32 : VM_DATA_DEFAULT_FLAGS64)
#define VM_STACK_DEFAULT_FLAGS32	VM_DATA_FLAGS_EXEC
#define VM_STACK_DEFAULT_FLAGS64	VM_DATA_FLAGS_NON_EXEC
#define VM_STACK_DEFAULT_FLAGS \
	(is_32bit_task() ? \
	 VM_STACK_DEFAULT_FLAGS32 : VM_STACK_DEFAULT_FLAGS64)
#include <asm-generic/getorder.h>
#endif  
