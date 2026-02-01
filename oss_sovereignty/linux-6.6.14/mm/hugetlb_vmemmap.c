
 
#define pr_fmt(fmt)	"HugeTLB: " fmt

#include <linux/pgtable.h>
#include <linux/moduleparam.h>
#include <linux/bootmem_info.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include "hugetlb_vmemmap.h"

 
struct vmemmap_remap_walk {
	void			(*remap_pte)(pte_t *pte, unsigned long addr,
					     struct vmemmap_remap_walk *walk);
	unsigned long		nr_walked;
	struct page		*reuse_page;
	unsigned long		reuse_addr;
	struct list_head	*vmemmap_pages;
};

static int split_vmemmap_huge_pmd(pmd_t *pmd, unsigned long start)
{
	pmd_t __pmd;
	int i;
	unsigned long addr = start;
	struct page *head;
	pte_t *pgtable;

	spin_lock(&init_mm.page_table_lock);
	head = pmd_leaf(*pmd) ? pmd_page(*pmd) : NULL;
	spin_unlock(&init_mm.page_table_lock);

	if (!head)
		return 0;

	pgtable = pte_alloc_one_kernel(&init_mm);
	if (!pgtable)
		return -ENOMEM;

	pmd_populate_kernel(&init_mm, &__pmd, pgtable);

	for (i = 0; i < PTRS_PER_PTE; i++, addr += PAGE_SIZE) {
		pte_t entry, *pte;
		pgprot_t pgprot = PAGE_KERNEL;

		entry = mk_pte(head + i, pgprot);
		pte = pte_offset_kernel(&__pmd, addr);
		set_pte_at(&init_mm, addr, pte, entry);
	}

	spin_lock(&init_mm.page_table_lock);
	if (likely(pmd_leaf(*pmd))) {
		 
		if (!PageReserved(head))
			split_page(head, get_order(PMD_SIZE));

		 
		smp_wmb();
		pmd_populate_kernel(&init_mm, pmd, pgtable);
		flush_tlb_kernel_range(start, start + PMD_SIZE);
	} else {
		pte_free_kernel(&init_mm, pgtable);
	}
	spin_unlock(&init_mm.page_table_lock);

	return 0;
}

static void vmemmap_pte_range(pmd_t *pmd, unsigned long addr,
			      unsigned long end,
			      struct vmemmap_remap_walk *walk)
{
	pte_t *pte = pte_offset_kernel(pmd, addr);

	 
	if (!walk->reuse_page) {
		walk->reuse_page = pte_page(ptep_get(pte));
		 
		addr += PAGE_SIZE;
		pte++;
		walk->nr_walked++;
	}

	for (; addr != end; addr += PAGE_SIZE, pte++) {
		walk->remap_pte(pte, addr, walk);
		walk->nr_walked++;
	}
}

static int vmemmap_pmd_range(pud_t *pud, unsigned long addr,
			     unsigned long end,
			     struct vmemmap_remap_walk *walk)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_offset(pud, addr);
	do {
		int ret;

		ret = split_vmemmap_huge_pmd(pmd, addr & PMD_MASK);
		if (ret)
			return ret;

		next = pmd_addr_end(addr, end);
		vmemmap_pte_range(pmd, addr, next, walk);
	} while (pmd++, addr = next, addr != end);

	return 0;
}

static int vmemmap_pud_range(p4d_t *p4d, unsigned long addr,
			     unsigned long end,
			     struct vmemmap_remap_walk *walk)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(p4d, addr);
	do {
		int ret;

		next = pud_addr_end(addr, end);
		ret = vmemmap_pmd_range(pud, addr, next, walk);
		if (ret)
			return ret;
	} while (pud++, addr = next, addr != end);

	return 0;
}

static int vmemmap_p4d_range(pgd_t *pgd, unsigned long addr,
			     unsigned long end,
			     struct vmemmap_remap_walk *walk)
{
	p4d_t *p4d;
	unsigned long next;

	p4d = p4d_offset(pgd, addr);
	do {
		int ret;

		next = p4d_addr_end(addr, end);
		ret = vmemmap_pud_range(p4d, addr, next, walk);
		if (ret)
			return ret;
	} while (p4d++, addr = next, addr != end);

	return 0;
}

static int vmemmap_remap_range(unsigned long start, unsigned long end,
			       struct vmemmap_remap_walk *walk)
{
	unsigned long addr = start;
	unsigned long next;
	pgd_t *pgd;

	VM_BUG_ON(!PAGE_ALIGNED(start));
	VM_BUG_ON(!PAGE_ALIGNED(end));

	pgd = pgd_offset_k(addr);
	do {
		int ret;

		next = pgd_addr_end(addr, end);
		ret = vmemmap_p4d_range(pgd, addr, next, walk);
		if (ret)
			return ret;
	} while (pgd++, addr = next, addr != end);

	flush_tlb_kernel_range(start, end);

	return 0;
}

 
static inline void free_vmemmap_page(struct page *page)
{
	if (PageReserved(page))
		free_bootmem_page(page);
	else
		__free_page(page);
}

 
static void free_vmemmap_page_list(struct list_head *list)
{
	struct page *page, *next;

	list_for_each_entry_safe(page, next, list, lru)
		free_vmemmap_page(page);
}

static void vmemmap_remap_pte(pte_t *pte, unsigned long addr,
			      struct vmemmap_remap_walk *walk)
{
	 
	pgprot_t pgprot = PAGE_KERNEL_RO;
	struct page *page = pte_page(ptep_get(pte));
	pte_t entry;

	 
	if (unlikely(addr == walk->reuse_addr)) {
		pgprot = PAGE_KERNEL;
		list_del(&walk->reuse_page->lru);

		 
		smp_wmb();
	}

	entry = mk_pte(walk->reuse_page, pgprot);
	list_add_tail(&page->lru, walk->vmemmap_pages);
	set_pte_at(&init_mm, addr, pte, entry);
}

 
#define NR_RESET_STRUCT_PAGE		3

static inline void reset_struct_pages(struct page *start)
{
	struct page *from = start + NR_RESET_STRUCT_PAGE;

	BUILD_BUG_ON(NR_RESET_STRUCT_PAGE * 2 > PAGE_SIZE / sizeof(struct page));
	memcpy(start, from, sizeof(*from) * NR_RESET_STRUCT_PAGE);
}

static void vmemmap_restore_pte(pte_t *pte, unsigned long addr,
				struct vmemmap_remap_walk *walk)
{
	pgprot_t pgprot = PAGE_KERNEL;
	struct page *page;
	void *to;

	BUG_ON(pte_page(ptep_get(pte)) != walk->reuse_page);

	page = list_first_entry(walk->vmemmap_pages, struct page, lru);
	list_del(&page->lru);
	to = page_to_virt(page);
	copy_page(to, (void *)walk->reuse_addr);
	reset_struct_pages(to);

	 
	smp_wmb();
	set_pte_at(&init_mm, addr, pte, mk_pte(page, pgprot));
}

 
static int vmemmap_remap_free(unsigned long start, unsigned long end,
			      unsigned long reuse)
{
	int ret;
	LIST_HEAD(vmemmap_pages);
	struct vmemmap_remap_walk walk = {
		.remap_pte	= vmemmap_remap_pte,
		.reuse_addr	= reuse,
		.vmemmap_pages	= &vmemmap_pages,
	};
	int nid = page_to_nid((struct page *)start);
	gfp_t gfp_mask = GFP_KERNEL | __GFP_THISNODE | __GFP_NORETRY |
			__GFP_NOWARN;

	 
	walk.reuse_page = alloc_pages_node(nid, gfp_mask, 0);
	if (walk.reuse_page) {
		copy_page(page_to_virt(walk.reuse_page),
			  (void *)walk.reuse_addr);
		list_add(&walk.reuse_page->lru, &vmemmap_pages);
	}

	 
	BUG_ON(start - reuse != PAGE_SIZE);

	mmap_read_lock(&init_mm);
	ret = vmemmap_remap_range(reuse, end, &walk);
	if (ret && walk.nr_walked) {
		end = reuse + walk.nr_walked * PAGE_SIZE;
		 
		walk = (struct vmemmap_remap_walk) {
			.remap_pte	= vmemmap_restore_pte,
			.reuse_addr	= reuse,
			.vmemmap_pages	= &vmemmap_pages,
		};

		vmemmap_remap_range(reuse, end, &walk);
	}
	mmap_read_unlock(&init_mm);

	free_vmemmap_page_list(&vmemmap_pages);

	return ret;
}

static int alloc_vmemmap_page_list(unsigned long start, unsigned long end,
				   struct list_head *list)
{
	gfp_t gfp_mask = GFP_KERNEL | __GFP_RETRY_MAYFAIL | __GFP_THISNODE;
	unsigned long nr_pages = (end - start) >> PAGE_SHIFT;
	int nid = page_to_nid((struct page *)start);
	struct page *page, *next;

	while (nr_pages--) {
		page = alloc_pages_node(nid, gfp_mask, 0);
		if (!page)
			goto out;
		list_add_tail(&page->lru, list);
	}

	return 0;
out:
	list_for_each_entry_safe(page, next, list, lru)
		__free_page(page);
	return -ENOMEM;
}

 
static int vmemmap_remap_alloc(unsigned long start, unsigned long end,
			       unsigned long reuse)
{
	LIST_HEAD(vmemmap_pages);
	struct vmemmap_remap_walk walk = {
		.remap_pte	= vmemmap_restore_pte,
		.reuse_addr	= reuse,
		.vmemmap_pages	= &vmemmap_pages,
	};

	 
	BUG_ON(start - reuse != PAGE_SIZE);

	if (alloc_vmemmap_page_list(start, end, &vmemmap_pages))
		return -ENOMEM;

	mmap_read_lock(&init_mm);
	vmemmap_remap_range(reuse, end, &walk);
	mmap_read_unlock(&init_mm);

	return 0;
}

DEFINE_STATIC_KEY_FALSE(hugetlb_optimize_vmemmap_key);
EXPORT_SYMBOL(hugetlb_optimize_vmemmap_key);

static bool vmemmap_optimize_enabled = IS_ENABLED(CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP_DEFAULT_ON);
core_param(hugetlb_free_vmemmap, vmemmap_optimize_enabled, bool, 0);

 
int hugetlb_vmemmap_restore(const struct hstate *h, struct page *head)
{
	int ret;
	unsigned long vmemmap_start = (unsigned long)head, vmemmap_end;
	unsigned long vmemmap_reuse;

	if (!HPageVmemmapOptimized(head))
		return 0;

	vmemmap_end	= vmemmap_start + hugetlb_vmemmap_size(h);
	vmemmap_reuse	= vmemmap_start;
	vmemmap_start	+= HUGETLB_VMEMMAP_RESERVE_SIZE;

	 
	ret = vmemmap_remap_alloc(vmemmap_start, vmemmap_end, vmemmap_reuse);
	if (!ret) {
		ClearHPageVmemmapOptimized(head);
		static_branch_dec(&hugetlb_optimize_vmemmap_key);
	}

	return ret;
}

 
static bool vmemmap_should_optimize(const struct hstate *h, const struct page *head)
{
	if (!READ_ONCE(vmemmap_optimize_enabled))
		return false;

	if (!hugetlb_vmemmap_optimizable(h))
		return false;

	if (IS_ENABLED(CONFIG_MEMORY_HOTPLUG)) {
		pmd_t *pmdp, pmd;
		struct page *vmemmap_page;
		unsigned long vaddr = (unsigned long)head;

		 
		pmdp = pmd_off_k(vaddr);
		 
		pmd = READ_ONCE(*pmdp);
		if (pmd_leaf(pmd))
			vmemmap_page = pmd_page(pmd) + pte_index(vaddr);
		else
			vmemmap_page = pte_page(*pte_offset_kernel(pmdp, vaddr));
		 
		if (PageVmemmapSelfHosted(vmemmap_page))
			return false;
	}

	return true;
}

 
void hugetlb_vmemmap_optimize(const struct hstate *h, struct page *head)
{
	unsigned long vmemmap_start = (unsigned long)head, vmemmap_end;
	unsigned long vmemmap_reuse;

	if (!vmemmap_should_optimize(h, head))
		return;

	static_branch_inc(&hugetlb_optimize_vmemmap_key);

	vmemmap_end	= vmemmap_start + hugetlb_vmemmap_size(h);
	vmemmap_reuse	= vmemmap_start;
	vmemmap_start	+= HUGETLB_VMEMMAP_RESERVE_SIZE;

	 
	if (vmemmap_remap_free(vmemmap_start, vmemmap_end, vmemmap_reuse))
		static_branch_dec(&hugetlb_optimize_vmemmap_key);
	else
		SetHPageVmemmapOptimized(head);
}

static struct ctl_table hugetlb_vmemmap_sysctls[] = {
	{
		.procname	= "hugetlb_optimize_vmemmap",
		.data		= &vmemmap_optimize_enabled,
		.maxlen		= sizeof(vmemmap_optimize_enabled),
		.mode		= 0644,
		.proc_handler	= proc_dobool,
	},
	{ }
};

static int __init hugetlb_vmemmap_init(void)
{
	const struct hstate *h;

	 
	BUILD_BUG_ON(__NR_USED_SUBPAGE * sizeof(struct page) > HUGETLB_VMEMMAP_RESERVE_SIZE);

	for_each_hstate(h) {
		if (hugetlb_vmemmap_optimizable(h)) {
			register_sysctl_init("vm", hugetlb_vmemmap_sysctls);
			break;
		}
	}
	return 0;
}
late_initcall(hugetlb_vmemmap_init);
