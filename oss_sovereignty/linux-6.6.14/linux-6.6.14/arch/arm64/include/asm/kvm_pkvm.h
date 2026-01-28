#ifndef __ARM64_KVM_PKVM_H__
#define __ARM64_KVM_PKVM_H__
#include <linux/arm_ffa.h>
#include <linux/memblock.h>
#include <linux/scatterlist.h>
#include <asm/kvm_pgtable.h>
#define KVM_MAX_PVMS 255
#define HYP_MEMBLOCK_REGIONS 128
int pkvm_init_host_vm(struct kvm *kvm);
int pkvm_create_hyp_vm(struct kvm *kvm);
void pkvm_destroy_hyp_vm(struct kvm *kvm);
extern struct memblock_region kvm_nvhe_sym(hyp_memory)[];
extern unsigned int kvm_nvhe_sym(hyp_memblock_nr);
static inline unsigned long
hyp_vmemmap_memblock_size(struct memblock_region *reg, size_t vmemmap_entry_size)
{
	unsigned long nr_pages = reg->size >> PAGE_SHIFT;
	unsigned long start, end;
	start = (reg->base >> PAGE_SHIFT) * vmemmap_entry_size;
	end = start + nr_pages * vmemmap_entry_size;
	start = ALIGN_DOWN(start, PAGE_SIZE);
	end = ALIGN(end, PAGE_SIZE);
	return end - start;
}
static inline unsigned long hyp_vmemmap_pages(size_t vmemmap_entry_size)
{
	unsigned long res = 0, i;
	for (i = 0; i < kvm_nvhe_sym(hyp_memblock_nr); i++) {
		res += hyp_vmemmap_memblock_size(&kvm_nvhe_sym(hyp_memory)[i],
						 vmemmap_entry_size);
	}
	return res >> PAGE_SHIFT;
}
static inline unsigned long hyp_vm_table_pages(void)
{
	return PAGE_ALIGN(KVM_MAX_PVMS * sizeof(void *)) >> PAGE_SHIFT;
}
static inline unsigned long __hyp_pgtable_max_pages(unsigned long nr_pages)
{
	unsigned long total = 0, i;
	for (i = 0; i < KVM_PGTABLE_MAX_LEVELS; i++) {
		nr_pages = DIV_ROUND_UP(nr_pages, PTRS_PER_PTE);
		total += nr_pages;
	}
	return total;
}
static inline unsigned long __hyp_pgtable_total_pages(void)
{
	unsigned long res = 0, i;
	for (i = 0; i < kvm_nvhe_sym(hyp_memblock_nr); i++) {
		struct memblock_region *reg = &kvm_nvhe_sym(hyp_memory)[i];
		res += __hyp_pgtable_max_pages(reg->size >> PAGE_SHIFT);
	}
	return res;
}
static inline unsigned long hyp_s1_pgtable_pages(void)
{
	unsigned long res;
	res = __hyp_pgtable_total_pages();
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);
	return res;
}
static inline unsigned long host_s2_pgtable_pages(void)
{
	unsigned long res;
	res = __hyp_pgtable_total_pages() + 16;
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);
	return res;
}
#define KVM_FFA_MBOX_NR_PAGES	1
static inline unsigned long hyp_ffa_proxy_pages(void)
{
	size_t desc_max;
	desc_max = sizeof(struct ffa_mem_region) +
		   sizeof(struct ffa_mem_region_attributes) +
		   sizeof(struct ffa_composite_mem_region) +
		   SG_MAX_SEGMENTS * sizeof(struct ffa_mem_region_addr_range);
	return (2 * KVM_FFA_MBOX_NR_PAGES) + DIV_ROUND_UP(desc_max, PAGE_SIZE);
}
#endif	 
