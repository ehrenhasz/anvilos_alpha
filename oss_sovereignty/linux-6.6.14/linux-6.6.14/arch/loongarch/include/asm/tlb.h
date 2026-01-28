#ifndef __ASM_TLB_H
#define __ASM_TLB_H
#include <linux/mm_types.h>
#include <asm/cpu-features.h>
#include <asm/loongarch.h>
static inline void tlbclr(void)
{
	__asm__ __volatile__("tlbclr");
}
static inline void tlbflush(void)
{
	__asm__ __volatile__("tlbflush");
}
static inline void tlb_probe(void)
{
	__asm__ __volatile__("tlbsrch");
}
static inline void tlb_read(void)
{
	__asm__ __volatile__("tlbrd");
}
static inline void tlb_write_indexed(void)
{
	__asm__ __volatile__("tlbwr");
}
static inline void tlb_write_random(void)
{
	__asm__ __volatile__("tlbfill");
}
enum invtlb_ops {
	INVTLB_ALL = 0x0,
	INVTLB_CURRENT_ALL = 0x1,
	INVTLB_CURRENT_GTRUE = 0x2,
	INVTLB_CURRENT_GFALSE = 0x3,
	INVTLB_GFALSE_AND_ASID = 0x4,
	INVTLB_ADDR_GFALSE_AND_ASID = 0x5,
	INVTLB_ADDR_GTRUE_OR_ASID = 0x6,
	INVGTLB_GID = 0x9,
	INVGTLB_GID_GTRUE = 0xa,
	INVGTLB_GID_GFALSE = 0xb,
	INVGTLB_GID_GFALSE_ASID = 0xc,
	INVGTLB_GID_GFALSE_ASID_ADDR = 0xd,
	INVGTLB_GID_GTRUE_ASID_ADDR = 0xe,
	INVGTLB_ALLGID_GVA_TO_GPA = 0x10,
	INVTLB_ALLGID_GPA_TO_HPA = 0x11,
	INVTLB_ALLGID = 0x12,
	INVGTLB_GID_GVA_TO_GPA = 0x13,
	INVTLB_GID_GPA_TO_HPA = 0x14,
	INVTLB_GID_ALL = 0x15,
	INVTLB_GID_ADDR = 0x16,
};
static __always_inline void invtlb(u32 op, u32 info, u64 addr)
{
	__asm__ __volatile__(
		"invtlb %0, %1, %2\n\t"
		:
		: "i"(op), "r"(info), "r"(addr)
		: "memory"
		);
}
static __always_inline void invtlb_addr(u32 op, u32 info, u64 addr)
{
	BUILD_BUG_ON(!__builtin_constant_p(info) || info != 0);
	__asm__ __volatile__(
		"invtlb %0, $zero, %1\n\t"
		:
		: "i"(op), "r"(addr)
		: "memory"
		);
}
static __always_inline void invtlb_info(u32 op, u32 info, u64 addr)
{
	BUILD_BUG_ON(!__builtin_constant_p(addr) || addr != 0);
	__asm__ __volatile__(
		"invtlb %0, %1, $zero\n\t"
		:
		: "i"(op), "r"(info)
		: "memory"
		);
}
static __always_inline void invtlb_all(u32 op, u32 info, u64 addr)
{
	BUILD_BUG_ON(!__builtin_constant_p(info) || info != 0);
	BUILD_BUG_ON(!__builtin_constant_p(addr) || addr != 0);
	__asm__ __volatile__(
		"invtlb %0, $zero, $zero\n\t"
		:
		: "i"(op)
		: "memory"
		);
}
#define __tlb_remove_tlb_entry(tlb, ptep, address) do { } while (0)
static void tlb_flush(struct mmu_gather *tlb);
#define tlb_flush tlb_flush
#include <asm-generic/tlb.h>
static inline void tlb_flush(struct mmu_gather *tlb)
{
	struct vm_area_struct vma;
	vma.vm_mm = tlb->mm;
	vm_flags_init(&vma, 0);
	if (tlb->fullmm) {
		flush_tlb_mm(tlb->mm);
		return;
	}
	flush_tlb_range(&vma, tlb->start, tlb->end);
}
extern void handle_tlb_load(void);
extern void handle_tlb_store(void);
extern void handle_tlb_modify(void);
extern void handle_tlb_refill(void);
extern void handle_tlb_protect(void);
extern void handle_tlb_load_ptw(void);
extern void handle_tlb_store_ptw(void);
extern void handle_tlb_modify_ptw(void);
extern void dump_tlb_all(void);
extern void dump_tlb_regs(void);
#endif  
