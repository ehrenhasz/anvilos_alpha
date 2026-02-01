
 

 

 

 

 

#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/memremap.h>
#include <linux/kmsan.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/export.h>
#include <linux/delayacct.h>
#include <linux/init.h>
#include <linux/pfn_t.h>
#include <linux/writeback.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/swapops.h>
#include <linux/elf.h>
#include <linux/gfp.h>
#include <linux/migrate.h>
#include <linux/string.h>
#include <linux/memory-tiers.h>
#include <linux/debugfs.h>
#include <linux/userfaultfd_k.h>
#include <linux/dax.h>
#include <linux/oom.h>
#include <linux/numa.h>
#include <linux/perf_event.h>
#include <linux/ptrace.h>
#include <linux/vmalloc.h>
#include <linux/sched/sysctl.h>

#include <trace/events/kmem.h>

#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <linux/uaccess.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>

#include "pgalloc-track.h"
#include "internal.h"
#include "swap.h"

#if defined(LAST_CPUPID_NOT_IN_PAGE_FLAGS) && !defined(CONFIG_COMPILE_TEST)
#warning Unfortunate NUMA and NUMA Balancing config, growing page-frame for last_cpupid.
#endif

#ifndef CONFIG_NUMA
unsigned long max_mapnr;
EXPORT_SYMBOL(max_mapnr);

struct page *mem_map;
EXPORT_SYMBOL(mem_map);
#endif

static vm_fault_t do_fault(struct vm_fault *vmf);
static vm_fault_t do_anonymous_page(struct vm_fault *vmf);
static bool vmf_pte_changed(struct vm_fault *vmf);

 
static bool vmf_orig_pte_uffd_wp(struct vm_fault *vmf)
{
	if (!(vmf->flags & FAULT_FLAG_ORIG_PTE_VALID))
		return false;

	return pte_marker_uffd_wp(vmf->orig_pte);
}

 
void *high_memory;
EXPORT_SYMBOL(high_memory);

 
int randomize_va_space __read_mostly =
#ifdef CONFIG_COMPAT_BRK
					1;
#else
					2;
#endif

#ifndef arch_wants_old_prefaulted_pte
static inline bool arch_wants_old_prefaulted_pte(void)
{
	 
	return false;
}
#endif

static int __init disable_randmaps(char *s)
{
	randomize_va_space = 0;
	return 1;
}
__setup("norandmaps", disable_randmaps);

unsigned long zero_pfn __read_mostly;
EXPORT_SYMBOL(zero_pfn);

unsigned long highest_memmap_pfn __read_mostly;

 
static int __init init_zero_pfn(void)
{
	zero_pfn = page_to_pfn(ZERO_PAGE(0));
	return 0;
}
early_initcall(init_zero_pfn);

void mm_trace_rss_stat(struct mm_struct *mm, int member)
{
	trace_rss_stat(mm, member);
}

 
static void free_pte_range(struct mmu_gather *tlb, pmd_t *pmd,
			   unsigned long addr)
{
	pgtable_t token = pmd_pgtable(*pmd);
	pmd_clear(pmd);
	pte_free_tlb(tlb, token, addr);
	mm_dec_nr_ptes(tlb->mm);
}

static inline void free_pmd_range(struct mmu_gather *tlb, pud_t *pud,
				unsigned long addr, unsigned long end,
				unsigned long floor, unsigned long ceiling)
{
	pmd_t *pmd;
	unsigned long next;
	unsigned long start;

	start = addr;
	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none_or_clear_bad(pmd))
			continue;
		free_pte_range(tlb, pmd, addr);
	} while (pmd++, addr = next, addr != end);

	start &= PUD_MASK;
	if (start < floor)
		return;
	if (ceiling) {
		ceiling &= PUD_MASK;
		if (!ceiling)
			return;
	}
	if (end - 1 > ceiling - 1)
		return;

	pmd = pmd_offset(pud, start);
	pud_clear(pud);
	pmd_free_tlb(tlb, pmd, start);
	mm_dec_nr_pmds(tlb->mm);
}

static inline void free_pud_range(struct mmu_gather *tlb, p4d_t *p4d,
				unsigned long addr, unsigned long end,
				unsigned long floor, unsigned long ceiling)
{
	pud_t *pud;
	unsigned long next;
	unsigned long start;

	start = addr;
	pud = pud_offset(p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		free_pmd_range(tlb, pud, addr, next, floor, ceiling);
	} while (pud++, addr = next, addr != end);

	start &= P4D_MASK;
	if (start < floor)
		return;
	if (ceiling) {
		ceiling &= P4D_MASK;
		if (!ceiling)
			return;
	}
	if (end - 1 > ceiling - 1)
		return;

	pud = pud_offset(p4d, start);
	p4d_clear(p4d);
	pud_free_tlb(tlb, pud, start);
	mm_dec_nr_puds(tlb->mm);
}

static inline void free_p4d_range(struct mmu_gather *tlb, pgd_t *pgd,
				unsigned long addr, unsigned long end,
				unsigned long floor, unsigned long ceiling)
{
	p4d_t *p4d;
	unsigned long next;
	unsigned long start;

	start = addr;
	p4d = p4d_offset(pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(p4d))
			continue;
		free_pud_range(tlb, p4d, addr, next, floor, ceiling);
	} while (p4d++, addr = next, addr != end);

	start &= PGDIR_MASK;
	if (start < floor)
		return;
	if (ceiling) {
		ceiling &= PGDIR_MASK;
		if (!ceiling)
			return;
	}
	if (end - 1 > ceiling - 1)
		return;

	p4d = p4d_offset(pgd, start);
	pgd_clear(pgd);
	p4d_free_tlb(tlb, p4d, start);
}

 
void free_pgd_range(struct mmu_gather *tlb,
			unsigned long addr, unsigned long end,
			unsigned long floor, unsigned long ceiling)
{
	pgd_t *pgd;
	unsigned long next;

	 

	addr &= PMD_MASK;
	if (addr < floor) {
		addr += PMD_SIZE;
		if (!addr)
			return;
	}
	if (ceiling) {
		ceiling &= PMD_MASK;
		if (!ceiling)
			return;
	}
	if (end - 1 > ceiling - 1)
		end -= PMD_SIZE;
	if (addr > end - 1)
		return;
	 
	tlb_change_page_size(tlb, PAGE_SIZE);
	pgd = pgd_offset(tlb->mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		free_p4d_range(tlb, pgd, addr, next, floor, ceiling);
	} while (pgd++, addr = next, addr != end);
}

void free_pgtables(struct mmu_gather *tlb, struct ma_state *mas,
		   struct vm_area_struct *vma, unsigned long floor,
		   unsigned long ceiling, bool mm_wr_locked)
{
	do {
		unsigned long addr = vma->vm_start;
		struct vm_area_struct *next;

		 
		next = mas_find(mas, ceiling - 1);

		 
		if (mm_wr_locked)
			vma_start_write(vma);
		unlink_anon_vmas(vma);
		unlink_file_vma(vma);

		if (is_vm_hugetlb_page(vma)) {
			hugetlb_free_pgd_range(tlb, addr, vma->vm_end,
				floor, next ? next->vm_start : ceiling);
		} else {
			 
			while (next && next->vm_start <= vma->vm_end + PMD_SIZE
			       && !is_vm_hugetlb_page(next)) {
				vma = next;
				next = mas_find(mas, ceiling - 1);
				if (mm_wr_locked)
					vma_start_write(vma);
				unlink_anon_vmas(vma);
				unlink_file_vma(vma);
			}
			free_pgd_range(tlb, addr, vma->vm_end,
				floor, next ? next->vm_start : ceiling);
		}
		vma = next;
	} while (vma);
}

void pmd_install(struct mm_struct *mm, pmd_t *pmd, pgtable_t *pte)
{
	spinlock_t *ptl = pmd_lock(mm, pmd);

	if (likely(pmd_none(*pmd))) {	 
		mm_inc_nr_ptes(mm);
		 
		smp_wmb();  
		pmd_populate(mm, pmd, *pte);
		*pte = NULL;
	}
	spin_unlock(ptl);
}

int __pte_alloc(struct mm_struct *mm, pmd_t *pmd)
{
	pgtable_t new = pte_alloc_one(mm);
	if (!new)
		return -ENOMEM;

	pmd_install(mm, pmd, &new);
	if (new)
		pte_free(mm, new);
	return 0;
}

int __pte_alloc_kernel(pmd_t *pmd)
{
	pte_t *new = pte_alloc_one_kernel(&init_mm);
	if (!new)
		return -ENOMEM;

	spin_lock(&init_mm.page_table_lock);
	if (likely(pmd_none(*pmd))) {	 
		smp_wmb();  
		pmd_populate_kernel(&init_mm, pmd, new);
		new = NULL;
	}
	spin_unlock(&init_mm.page_table_lock);
	if (new)
		pte_free_kernel(&init_mm, new);
	return 0;
}

static inline void init_rss_vec(int *rss)
{
	memset(rss, 0, sizeof(int) * NR_MM_COUNTERS);
}

static inline void add_mm_rss_vec(struct mm_struct *mm, int *rss)
{
	int i;

	if (current->mm == mm)
		sync_mm_rss(mm);
	for (i = 0; i < NR_MM_COUNTERS; i++)
		if (rss[i])
			add_mm_counter(mm, i, rss[i]);
}

 
static void print_bad_pte(struct vm_area_struct *vma, unsigned long addr,
			  pte_t pte, struct page *page)
{
	pgd_t *pgd = pgd_offset(vma->vm_mm, addr);
	p4d_t *p4d = p4d_offset(pgd, addr);
	pud_t *pud = pud_offset(p4d, addr);
	pmd_t *pmd = pmd_offset(pud, addr);
	struct address_space *mapping;
	pgoff_t index;
	static unsigned long resume;
	static unsigned long nr_shown;
	static unsigned long nr_unshown;

	 
	if (nr_shown == 60) {
		if (time_before(jiffies, resume)) {
			nr_unshown++;
			return;
		}
		if (nr_unshown) {
			pr_alert("BUG: Bad page map: %lu messages suppressed\n",
				 nr_unshown);
			nr_unshown = 0;
		}
		nr_shown = 0;
	}
	if (nr_shown++ == 0)
		resume = jiffies + 60 * HZ;

	mapping = vma->vm_file ? vma->vm_file->f_mapping : NULL;
	index = linear_page_index(vma, addr);

	pr_alert("BUG: Bad page map in process %s  pte:%08llx pmd:%08llx\n",
		 current->comm,
		 (long long)pte_val(pte), (long long)pmd_val(*pmd));
	if (page)
		dump_page(page, "bad pte");
	pr_alert("addr:%px vm_flags:%08lx anon_vma:%px mapping:%px index:%lx\n",
		 (void *)addr, vma->vm_flags, vma->anon_vma, mapping, index);
	pr_alert("file:%pD fault:%ps mmap:%ps read_folio:%ps\n",
		 vma->vm_file,
		 vma->vm_ops ? vma->vm_ops->fault : NULL,
		 vma->vm_file ? vma->vm_file->f_op->mmap : NULL,
		 mapping ? mapping->a_ops->read_folio : NULL);
	dump_stack();
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
}

 
struct page *vm_normal_page(struct vm_area_struct *vma, unsigned long addr,
			    pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);

	if (IS_ENABLED(CONFIG_ARCH_HAS_PTE_SPECIAL)) {
		if (likely(!pte_special(pte)))
			goto check_pfn;
		if (vma->vm_ops && vma->vm_ops->find_special_page)
			return vma->vm_ops->find_special_page(vma, addr);
		if (vma->vm_flags & (VM_PFNMAP | VM_MIXEDMAP))
			return NULL;
		if (is_zero_pfn(pfn))
			return NULL;
		if (pte_devmap(pte))
		 
			return NULL;

		print_bad_pte(vma, addr, pte, NULL);
		return NULL;
	}

	 

	if (unlikely(vma->vm_flags & (VM_PFNMAP|VM_MIXEDMAP))) {
		if (vma->vm_flags & VM_MIXEDMAP) {
			if (!pfn_valid(pfn))
				return NULL;
			goto out;
		} else {
			unsigned long off;
			off = (addr - vma->vm_start) >> PAGE_SHIFT;
			if (pfn == vma->vm_pgoff + off)
				return NULL;
			if (!is_cow_mapping(vma->vm_flags))
				return NULL;
		}
	}

	if (is_zero_pfn(pfn))
		return NULL;

check_pfn:
	if (unlikely(pfn > highest_memmap_pfn)) {
		print_bad_pte(vma, addr, pte, NULL);
		return NULL;
	}

	 
out:
	return pfn_to_page(pfn);
}

struct folio *vm_normal_folio(struct vm_area_struct *vma, unsigned long addr,
			    pte_t pte)
{
	struct page *page = vm_normal_page(vma, addr, pte);

	if (page)
		return page_folio(page);
	return NULL;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
struct page *vm_normal_page_pmd(struct vm_area_struct *vma, unsigned long addr,
				pmd_t pmd)
{
	unsigned long pfn = pmd_pfn(pmd);

	 
	if (unlikely(vma->vm_flags & (VM_PFNMAP|VM_MIXEDMAP))) {
		if (vma->vm_flags & VM_MIXEDMAP) {
			if (!pfn_valid(pfn))
				return NULL;
			goto out;
		} else {
			unsigned long off;
			off = (addr - vma->vm_start) >> PAGE_SHIFT;
			if (pfn == vma->vm_pgoff + off)
				return NULL;
			if (!is_cow_mapping(vma->vm_flags))
				return NULL;
		}
	}

	if (pmd_devmap(pmd))
		return NULL;
	if (is_huge_zero_pmd(pmd))
		return NULL;
	if (unlikely(pfn > highest_memmap_pfn))
		return NULL;

	 
out:
	return pfn_to_page(pfn);
}
#endif

static void restore_exclusive_pte(struct vm_area_struct *vma,
				  struct page *page, unsigned long address,
				  pte_t *ptep)
{
	pte_t orig_pte;
	pte_t pte;
	swp_entry_t entry;

	orig_pte = ptep_get(ptep);
	pte = pte_mkold(mk_pte(page, READ_ONCE(vma->vm_page_prot)));
	if (pte_swp_soft_dirty(orig_pte))
		pte = pte_mksoft_dirty(pte);

	entry = pte_to_swp_entry(orig_pte);
	if (pte_swp_uffd_wp(orig_pte))
		pte = pte_mkuffd_wp(pte);
	else if (is_writable_device_exclusive_entry(entry))
		pte = maybe_mkwrite(pte_mkdirty(pte), vma);

	VM_BUG_ON(pte_write(pte) && !(PageAnon(page) && PageAnonExclusive(page)));

	 
	if (PageAnon(page))
		page_add_anon_rmap(page, vma, address, RMAP_NONE);
	else
		 
		WARN_ON_ONCE(1);

	set_pte_at(vma->vm_mm, address, ptep, pte);

	 
	update_mmu_cache(vma, address, ptep);
}

 
static int
try_restore_exclusive_pte(pte_t *src_pte, struct vm_area_struct *vma,
			unsigned long addr)
{
	swp_entry_t entry = pte_to_swp_entry(ptep_get(src_pte));
	struct page *page = pfn_swap_entry_to_page(entry);

	if (trylock_page(page)) {
		restore_exclusive_pte(vma, page, addr, src_pte);
		unlock_page(page);
		return 0;
	}

	return -EBUSY;
}

 

static unsigned long
copy_nonpresent_pte(struct mm_struct *dst_mm, struct mm_struct *src_mm,
		pte_t *dst_pte, pte_t *src_pte, struct vm_area_struct *dst_vma,
		struct vm_area_struct *src_vma, unsigned long addr, int *rss)
{
	unsigned long vm_flags = dst_vma->vm_flags;
	pte_t orig_pte = ptep_get(src_pte);
	pte_t pte = orig_pte;
	struct page *page;
	swp_entry_t entry = pte_to_swp_entry(orig_pte);

	if (likely(!non_swap_entry(entry))) {
		if (swap_duplicate(entry) < 0)
			return -EIO;

		 
		if (unlikely(list_empty(&dst_mm->mmlist))) {
			spin_lock(&mmlist_lock);
			if (list_empty(&dst_mm->mmlist))
				list_add(&dst_mm->mmlist,
						&src_mm->mmlist);
			spin_unlock(&mmlist_lock);
		}
		 
		if (pte_swp_exclusive(orig_pte)) {
			pte = pte_swp_clear_exclusive(orig_pte);
			set_pte_at(src_mm, addr, src_pte, pte);
		}
		rss[MM_SWAPENTS]++;
	} else if (is_migration_entry(entry)) {
		page = pfn_swap_entry_to_page(entry);

		rss[mm_counter(page)]++;

		if (!is_readable_migration_entry(entry) &&
				is_cow_mapping(vm_flags)) {
			 
			entry = make_readable_migration_entry(
							swp_offset(entry));
			pte = swp_entry_to_pte(entry);
			if (pte_swp_soft_dirty(orig_pte))
				pte = pte_swp_mksoft_dirty(pte);
			if (pte_swp_uffd_wp(orig_pte))
				pte = pte_swp_mkuffd_wp(pte);
			set_pte_at(src_mm, addr, src_pte, pte);
		}
	} else if (is_device_private_entry(entry)) {
		page = pfn_swap_entry_to_page(entry);

		 
		get_page(page);
		rss[mm_counter(page)]++;
		 
		BUG_ON(page_try_dup_anon_rmap(page, false, src_vma));

		 
		if (is_writable_device_private_entry(entry) &&
		    is_cow_mapping(vm_flags)) {
			entry = make_readable_device_private_entry(
							swp_offset(entry));
			pte = swp_entry_to_pte(entry);
			if (pte_swp_uffd_wp(orig_pte))
				pte = pte_swp_mkuffd_wp(pte);
			set_pte_at(src_mm, addr, src_pte, pte);
		}
	} else if (is_device_exclusive_entry(entry)) {
		 
		VM_BUG_ON(!is_cow_mapping(src_vma->vm_flags));
		if (try_restore_exclusive_pte(src_pte, src_vma, addr))
			return -EBUSY;
		return -ENOENT;
	} else if (is_pte_marker_entry(entry)) {
		pte_marker marker = copy_pte_marker(entry, dst_vma);

		if (marker)
			set_pte_at(dst_mm, addr, dst_pte,
				   make_pte_marker(marker));
		return 0;
	}
	if (!userfaultfd_wp(dst_vma))
		pte = pte_swp_clear_uffd_wp(pte);
	set_pte_at(dst_mm, addr, dst_pte, pte);
	return 0;
}

 
static inline int
copy_present_page(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
		  pte_t *dst_pte, pte_t *src_pte, unsigned long addr, int *rss,
		  struct folio **prealloc, struct page *page)
{
	struct folio *new_folio;
	pte_t pte;

	new_folio = *prealloc;
	if (!new_folio)
		return -EAGAIN;

	 
	*prealloc = NULL;
	copy_user_highpage(&new_folio->page, page, addr, src_vma);
	__folio_mark_uptodate(new_folio);
	folio_add_new_anon_rmap(new_folio, dst_vma, addr);
	folio_add_lru_vma(new_folio, dst_vma);
	rss[MM_ANONPAGES]++;

	 
	pte = mk_pte(&new_folio->page, dst_vma->vm_page_prot);
	pte = maybe_mkwrite(pte_mkdirty(pte), dst_vma);
	if (userfaultfd_pte_wp(dst_vma, ptep_get(src_pte)))
		 
		pte = pte_mkuffd_wp(pte);
	set_pte_at(dst_vma->vm_mm, addr, dst_pte, pte);
	return 0;
}

 
static inline int
copy_present_pte(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
		 pte_t *dst_pte, pte_t *src_pte, unsigned long addr, int *rss,
		 struct folio **prealloc)
{
	struct mm_struct *src_mm = src_vma->vm_mm;
	unsigned long vm_flags = src_vma->vm_flags;
	pte_t pte = ptep_get(src_pte);
	struct page *page;
	struct folio *folio;

	page = vm_normal_page(src_vma, addr, pte);
	if (page)
		folio = page_folio(page);
	if (page && folio_test_anon(folio)) {
		 
		folio_get(folio);
		if (unlikely(page_try_dup_anon_rmap(page, false, src_vma))) {
			 
			folio_put(folio);
			return copy_present_page(dst_vma, src_vma, dst_pte, src_pte,
						 addr, rss, prealloc, page);
		}
		rss[MM_ANONPAGES]++;
	} else if (page) {
		folio_get(folio);
		page_dup_file_rmap(page, false);
		rss[mm_counter_file(page)]++;
	}

	 
	if (is_cow_mapping(vm_flags) && pte_write(pte)) {
		ptep_set_wrprotect(src_mm, addr, src_pte);
		pte = pte_wrprotect(pte);
	}
	VM_BUG_ON(page && folio_test_anon(folio) && PageAnonExclusive(page));

	 
	if (vm_flags & VM_SHARED)
		pte = pte_mkclean(pte);
	pte = pte_mkold(pte);

	if (!userfaultfd_wp(dst_vma))
		pte = pte_clear_uffd_wp(pte);

	set_pte_at(dst_vma->vm_mm, addr, dst_pte, pte);
	return 0;
}

static inline struct folio *page_copy_prealloc(struct mm_struct *src_mm,
		struct vm_area_struct *vma, unsigned long addr)
{
	struct folio *new_folio;

	new_folio = vma_alloc_folio(GFP_HIGHUSER_MOVABLE, 0, vma, addr, false);
	if (!new_folio)
		return NULL;

	if (mem_cgroup_charge(new_folio, src_mm, GFP_KERNEL)) {
		folio_put(new_folio);
		return NULL;
	}
	folio_throttle_swaprate(new_folio, GFP_KERNEL);

	return new_folio;
}

static int
copy_pte_range(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
	       pmd_t *dst_pmd, pmd_t *src_pmd, unsigned long addr,
	       unsigned long end)
{
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	struct mm_struct *src_mm = src_vma->vm_mm;
	pte_t *orig_src_pte, *orig_dst_pte;
	pte_t *src_pte, *dst_pte;
	pte_t ptent;
	spinlock_t *src_ptl, *dst_ptl;
	int progress, ret = 0;
	int rss[NR_MM_COUNTERS];
	swp_entry_t entry = (swp_entry_t){0};
	struct folio *prealloc = NULL;

again:
	progress = 0;
	init_rss_vec(rss);

	 
	dst_pte = pte_alloc_map_lock(dst_mm, dst_pmd, addr, &dst_ptl);
	if (!dst_pte) {
		ret = -ENOMEM;
		goto out;
	}
	src_pte = pte_offset_map_nolock(src_mm, src_pmd, addr, &src_ptl);
	if (!src_pte) {
		pte_unmap_unlock(dst_pte, dst_ptl);
		 
		goto out;
	}
	spin_lock_nested(src_ptl, SINGLE_DEPTH_NESTING);
	orig_src_pte = src_pte;
	orig_dst_pte = dst_pte;
	arch_enter_lazy_mmu_mode();

	do {
		 
		if (progress >= 32) {
			progress = 0;
			if (need_resched() ||
			    spin_needbreak(src_ptl) || spin_needbreak(dst_ptl))
				break;
		}
		ptent = ptep_get(src_pte);
		if (pte_none(ptent)) {
			progress++;
			continue;
		}
		if (unlikely(!pte_present(ptent))) {
			ret = copy_nonpresent_pte(dst_mm, src_mm,
						  dst_pte, src_pte,
						  dst_vma, src_vma,
						  addr, rss);
			if (ret == -EIO) {
				entry = pte_to_swp_entry(ptep_get(src_pte));
				break;
			} else if (ret == -EBUSY) {
				break;
			} else if (!ret) {
				progress += 8;
				continue;
			}

			 
			WARN_ON_ONCE(ret != -ENOENT);
		}
		 
		ret = copy_present_pte(dst_vma, src_vma, dst_pte, src_pte,
				       addr, rss, &prealloc);
		 
		if (unlikely(ret == -EAGAIN))
			break;
		if (unlikely(prealloc)) {
			 
			folio_put(prealloc);
			prealloc = NULL;
		}
		progress += 8;
	} while (dst_pte++, src_pte++, addr += PAGE_SIZE, addr != end);

	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(orig_src_pte, src_ptl);
	add_mm_rss_vec(dst_mm, rss);
	pte_unmap_unlock(orig_dst_pte, dst_ptl);
	cond_resched();

	if (ret == -EIO) {
		VM_WARN_ON_ONCE(!entry.val);
		if (add_swap_count_continuation(entry, GFP_KERNEL) < 0) {
			ret = -ENOMEM;
			goto out;
		}
		entry.val = 0;
	} else if (ret == -EBUSY) {
		goto out;
	} else if (ret ==  -EAGAIN) {
		prealloc = page_copy_prealloc(src_mm, src_vma, addr);
		if (!prealloc)
			return -ENOMEM;
	} else if (ret) {
		VM_WARN_ON_ONCE(1);
	}

	 
	ret = 0;

	if (addr != end)
		goto again;
out:
	if (unlikely(prealloc))
		folio_put(prealloc);
	return ret;
}

static inline int
copy_pmd_range(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
	       pud_t *dst_pud, pud_t *src_pud, unsigned long addr,
	       unsigned long end)
{
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	struct mm_struct *src_mm = src_vma->vm_mm;
	pmd_t *src_pmd, *dst_pmd;
	unsigned long next;

	dst_pmd = pmd_alloc(dst_mm, dst_pud, addr);
	if (!dst_pmd)
		return -ENOMEM;
	src_pmd = pmd_offset(src_pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (is_swap_pmd(*src_pmd) || pmd_trans_huge(*src_pmd)
			|| pmd_devmap(*src_pmd)) {
			int err;
			VM_BUG_ON_VMA(next-addr != HPAGE_PMD_SIZE, src_vma);
			err = copy_huge_pmd(dst_mm, src_mm, dst_pmd, src_pmd,
					    addr, dst_vma, src_vma);
			if (err == -ENOMEM)
				return -ENOMEM;
			if (!err)
				continue;
			 
		}
		if (pmd_none_or_clear_bad(src_pmd))
			continue;
		if (copy_pte_range(dst_vma, src_vma, dst_pmd, src_pmd,
				   addr, next))
			return -ENOMEM;
	} while (dst_pmd++, src_pmd++, addr = next, addr != end);
	return 0;
}

static inline int
copy_pud_range(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
	       p4d_t *dst_p4d, p4d_t *src_p4d, unsigned long addr,
	       unsigned long end)
{
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	struct mm_struct *src_mm = src_vma->vm_mm;
	pud_t *src_pud, *dst_pud;
	unsigned long next;

	dst_pud = pud_alloc(dst_mm, dst_p4d, addr);
	if (!dst_pud)
		return -ENOMEM;
	src_pud = pud_offset(src_p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_trans_huge(*src_pud) || pud_devmap(*src_pud)) {
			int err;

			VM_BUG_ON_VMA(next-addr != HPAGE_PUD_SIZE, src_vma);
			err = copy_huge_pud(dst_mm, src_mm,
					    dst_pud, src_pud, addr, src_vma);
			if (err == -ENOMEM)
				return -ENOMEM;
			if (!err)
				continue;
			 
		}
		if (pud_none_or_clear_bad(src_pud))
			continue;
		if (copy_pmd_range(dst_vma, src_vma, dst_pud, src_pud,
				   addr, next))
			return -ENOMEM;
	} while (dst_pud++, src_pud++, addr = next, addr != end);
	return 0;
}

static inline int
copy_p4d_range(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma,
	       pgd_t *dst_pgd, pgd_t *src_pgd, unsigned long addr,
	       unsigned long end)
{
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	p4d_t *src_p4d, *dst_p4d;
	unsigned long next;

	dst_p4d = p4d_alloc(dst_mm, dst_pgd, addr);
	if (!dst_p4d)
		return -ENOMEM;
	src_p4d = p4d_offset(src_pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(src_p4d))
			continue;
		if (copy_pud_range(dst_vma, src_vma, dst_p4d, src_p4d,
				   addr, next))
			return -ENOMEM;
	} while (dst_p4d++, src_p4d++, addr = next, addr != end);
	return 0;
}

 
static bool
vma_needs_copy(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma)
{
	 
	if (userfaultfd_wp(dst_vma))
		return true;

	if (src_vma->vm_flags & (VM_PFNMAP | VM_MIXEDMAP))
		return true;

	if (src_vma->anon_vma)
		return true;

	 
	return false;
}

int
copy_page_range(struct vm_area_struct *dst_vma, struct vm_area_struct *src_vma)
{
	pgd_t *src_pgd, *dst_pgd;
	unsigned long next;
	unsigned long addr = src_vma->vm_start;
	unsigned long end = src_vma->vm_end;
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	struct mm_struct *src_mm = src_vma->vm_mm;
	struct mmu_notifier_range range;
	bool is_cow;
	int ret;

	if (!vma_needs_copy(dst_vma, src_vma))
		return 0;

	if (is_vm_hugetlb_page(src_vma))
		return copy_hugetlb_page_range(dst_mm, src_mm, dst_vma, src_vma);

	if (unlikely(src_vma->vm_flags & VM_PFNMAP)) {
		 
		ret = track_pfn_copy(src_vma);
		if (ret)
			return ret;
	}

	 
	is_cow = is_cow_mapping(src_vma->vm_flags);

	if (is_cow) {
		mmu_notifier_range_init(&range, MMU_NOTIFY_PROTECTION_PAGE,
					0, src_mm, addr, end);
		mmu_notifier_invalidate_range_start(&range);
		 
		vma_assert_write_locked(src_vma);
		raw_write_seqcount_begin(&src_mm->write_protect_seq);
	}

	ret = 0;
	dst_pgd = pgd_offset(dst_mm, addr);
	src_pgd = pgd_offset(src_mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(src_pgd))
			continue;
		if (unlikely(copy_p4d_range(dst_vma, src_vma, dst_pgd, src_pgd,
					    addr, next))) {
			untrack_pfn_clear(dst_vma);
			ret = -ENOMEM;
			break;
		}
	} while (dst_pgd++, src_pgd++, addr = next, addr != end);

	if (is_cow) {
		raw_write_seqcount_end(&src_mm->write_protect_seq);
		mmu_notifier_invalidate_range_end(&range);
	}
	return ret;
}

 
static inline bool should_zap_cows(struct zap_details *details)
{
	 
	if (!details)
		return true;

	 
	return details->even_cows;
}

 
static inline bool should_zap_page(struct zap_details *details, struct page *page)
{
	 
	if (should_zap_cows(details))
		return true;

	 
	if (!page)
		return true;

	 
	return !PageAnon(page);
}

static inline bool zap_drop_file_uffd_wp(struct zap_details *details)
{
	if (!details)
		return false;

	return details->zap_flags & ZAP_FLAG_DROP_MARKER;
}

 
static inline void
zap_install_uffd_wp_if_needed(struct vm_area_struct *vma,
			      unsigned long addr, pte_t *pte,
			      struct zap_details *details, pte_t pteval)
{
	 
	if (vma_is_anonymous(vma))
		return;

	if (zap_drop_file_uffd_wp(details))
		return;

	pte_install_uffd_wp_if_needed(vma, addr, pte, pteval);
}

static unsigned long zap_pte_range(struct mmu_gather *tlb,
				struct vm_area_struct *vma, pmd_t *pmd,
				unsigned long addr, unsigned long end,
				struct zap_details *details)
{
	struct mm_struct *mm = tlb->mm;
	int force_flush = 0;
	int rss[NR_MM_COUNTERS];
	spinlock_t *ptl;
	pte_t *start_pte;
	pte_t *pte;
	swp_entry_t entry;

	tlb_change_page_size(tlb, PAGE_SIZE);
	init_rss_vec(rss);
	start_pte = pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
	if (!pte)
		return addr;

	flush_tlb_batched_pending(mm);
	arch_enter_lazy_mmu_mode();
	do {
		pte_t ptent = ptep_get(pte);
		struct page *page;

		if (pte_none(ptent))
			continue;

		if (need_resched())
			break;

		if (pte_present(ptent)) {
			unsigned int delay_rmap;

			page = vm_normal_page(vma, addr, ptent);
			if (unlikely(!should_zap_page(details, page)))
				continue;
			ptent = ptep_get_and_clear_full(mm, addr, pte,
							tlb->fullmm);
			arch_check_zapped_pte(vma, ptent);
			tlb_remove_tlb_entry(tlb, pte, addr);
			zap_install_uffd_wp_if_needed(vma, addr, pte, details,
						      ptent);
			if (unlikely(!page)) {
				ksm_might_unmap_zero_page(mm, ptent);
				continue;
			}

			delay_rmap = 0;
			if (!PageAnon(page)) {
				if (pte_dirty(ptent)) {
					set_page_dirty(page);
					if (tlb_delay_rmap(tlb)) {
						delay_rmap = 1;
						force_flush = 1;
					}
				}
				if (pte_young(ptent) && likely(vma_has_recency(vma)))
					mark_page_accessed(page);
			}
			rss[mm_counter(page)]--;
			if (!delay_rmap) {
				page_remove_rmap(page, vma, false);
				if (unlikely(page_mapcount(page) < 0))
					print_bad_pte(vma, addr, ptent, page);
			}
			if (unlikely(__tlb_remove_page(tlb, page, delay_rmap))) {
				force_flush = 1;
				addr += PAGE_SIZE;
				break;
			}
			continue;
		}

		entry = pte_to_swp_entry(ptent);
		if (is_device_private_entry(entry) ||
		    is_device_exclusive_entry(entry)) {
			page = pfn_swap_entry_to_page(entry);
			if (unlikely(!should_zap_page(details, page)))
				continue;
			 
			WARN_ON_ONCE(!vma_is_anonymous(vma));
			rss[mm_counter(page)]--;
			if (is_device_private_entry(entry))
				page_remove_rmap(page, vma, false);
			put_page(page);
		} else if (!non_swap_entry(entry)) {
			 
			if (!should_zap_cows(details))
				continue;
			rss[MM_SWAPENTS]--;
			if (unlikely(!free_swap_and_cache(entry)))
				print_bad_pte(vma, addr, ptent, NULL);
		} else if (is_migration_entry(entry)) {
			page = pfn_swap_entry_to_page(entry);
			if (!should_zap_page(details, page))
				continue;
			rss[mm_counter(page)]--;
		} else if (pte_marker_entry_uffd_wp(entry)) {
			 
			if (!vma_is_anonymous(vma) &&
			    !zap_drop_file_uffd_wp(details))
				continue;
		} else if (is_hwpoison_entry(entry) ||
			   is_poisoned_swp_entry(entry)) {
			if (!should_zap_cows(details))
				continue;
		} else {
			 
			WARN_ON_ONCE(1);
		}
		pte_clear_not_present_full(mm, addr, pte, tlb->fullmm);
		zap_install_uffd_wp_if_needed(vma, addr, pte, details, ptent);
	} while (pte++, addr += PAGE_SIZE, addr != end);

	add_mm_rss_vec(mm, rss);
	arch_leave_lazy_mmu_mode();

	 
	if (force_flush) {
		tlb_flush_mmu_tlbonly(tlb);
		tlb_flush_rmaps(tlb, vma);
	}
	pte_unmap_unlock(start_pte, ptl);

	 
	if (force_flush)
		tlb_flush_mmu(tlb);

	return addr;
}

static inline unsigned long zap_pmd_range(struct mmu_gather *tlb,
				struct vm_area_struct *vma, pud_t *pud,
				unsigned long addr, unsigned long end,
				struct zap_details *details)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (is_swap_pmd(*pmd) || pmd_trans_huge(*pmd) || pmd_devmap(*pmd)) {
			if (next - addr != HPAGE_PMD_SIZE)
				__split_huge_pmd(vma, pmd, addr, false, NULL);
			else if (zap_huge_pmd(tlb, vma, pmd, addr)) {
				addr = next;
				continue;
			}
			 
		} else if (details && details->single_folio &&
			   folio_test_pmd_mappable(details->single_folio) &&
			   next - addr == HPAGE_PMD_SIZE && pmd_none(*pmd)) {
			spinlock_t *ptl = pmd_lock(tlb->mm, pmd);
			 
			spin_unlock(ptl);
		}
		if (pmd_none(*pmd)) {
			addr = next;
			continue;
		}
		addr = zap_pte_range(tlb, vma, pmd, addr, next, details);
		if (addr != next)
			pmd--;
	} while (pmd++, cond_resched(), addr != end);

	return addr;
}

static inline unsigned long zap_pud_range(struct mmu_gather *tlb,
				struct vm_area_struct *vma, p4d_t *p4d,
				unsigned long addr, unsigned long end,
				struct zap_details *details)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_trans_huge(*pud) || pud_devmap(*pud)) {
			if (next - addr != HPAGE_PUD_SIZE) {
				mmap_assert_locked(tlb->mm);
				split_huge_pud(vma, pud, addr);
			} else if (zap_huge_pud(tlb, vma, pud, addr))
				goto next;
			 
		}
		if (pud_none_or_clear_bad(pud))
			continue;
		next = zap_pmd_range(tlb, vma, pud, addr, next, details);
next:
		cond_resched();
	} while (pud++, addr = next, addr != end);

	return addr;
}

static inline unsigned long zap_p4d_range(struct mmu_gather *tlb,
				struct vm_area_struct *vma, pgd_t *pgd,
				unsigned long addr, unsigned long end,
				struct zap_details *details)
{
	p4d_t *p4d;
	unsigned long next;

	p4d = p4d_offset(pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(p4d))
			continue;
		next = zap_pud_range(tlb, vma, p4d, addr, next, details);
	} while (p4d++, addr = next, addr != end);

	return addr;
}

void unmap_page_range(struct mmu_gather *tlb,
			     struct vm_area_struct *vma,
			     unsigned long addr, unsigned long end,
			     struct zap_details *details)
{
	pgd_t *pgd;
	unsigned long next;

	BUG_ON(addr >= end);
	tlb_start_vma(tlb, vma);
	pgd = pgd_offset(vma->vm_mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		next = zap_p4d_range(tlb, vma, pgd, addr, next, details);
	} while (pgd++, addr = next, addr != end);
	tlb_end_vma(tlb, vma);
}


static void unmap_single_vma(struct mmu_gather *tlb,
		struct vm_area_struct *vma, unsigned long start_addr,
		unsigned long end_addr,
		struct zap_details *details, bool mm_wr_locked)
{
	unsigned long start = max(vma->vm_start, start_addr);
	unsigned long end;

	if (start >= vma->vm_end)
		return;
	end = min(vma->vm_end, end_addr);
	if (end <= vma->vm_start)
		return;

	if (vma->vm_file)
		uprobe_munmap(vma, start, end);

	if (unlikely(vma->vm_flags & VM_PFNMAP))
		untrack_pfn(vma, 0, 0, mm_wr_locked);

	if (start != end) {
		if (unlikely(is_vm_hugetlb_page(vma))) {
			 
			if (vma->vm_file) {
				zap_flags_t zap_flags = details ?
				    details->zap_flags : 0;
				__unmap_hugepage_range(tlb, vma, start, end,
							     NULL, zap_flags);
			}
		} else
			unmap_page_range(tlb, vma, start, end, details);
	}
}

 
void unmap_vmas(struct mmu_gather *tlb, struct ma_state *mas,
		struct vm_area_struct *vma, unsigned long start_addr,
		unsigned long end_addr, unsigned long tree_end,
		bool mm_wr_locked)
{
	struct mmu_notifier_range range;
	struct zap_details details = {
		.zap_flags = ZAP_FLAG_DROP_MARKER | ZAP_FLAG_UNMAP,
		 
		.even_cows = true,
	};

	mmu_notifier_range_init(&range, MMU_NOTIFY_UNMAP, 0, vma->vm_mm,
				start_addr, end_addr);
	mmu_notifier_invalidate_range_start(&range);
	do {
		unsigned long start = start_addr;
		unsigned long end = end_addr;
		hugetlb_zap_begin(vma, &start, &end);
		unmap_single_vma(tlb, vma, start, end, &details,
				 mm_wr_locked);
		hugetlb_zap_end(vma, &details);
	} while ((vma = mas_find(mas, tree_end - 1)) != NULL);
	mmu_notifier_invalidate_range_end(&range);
}

 
void zap_page_range_single(struct vm_area_struct *vma, unsigned long address,
		unsigned long size, struct zap_details *details)
{
	const unsigned long end = address + size;
	struct mmu_notifier_range range;
	struct mmu_gather tlb;

	lru_add_drain();
	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, vma->vm_mm,
				address, end);
	hugetlb_zap_begin(vma, &range.start, &range.end);
	tlb_gather_mmu(&tlb, vma->vm_mm);
	update_hiwater_rss(vma->vm_mm);
	mmu_notifier_invalidate_range_start(&range);
	 
	unmap_single_vma(&tlb, vma, address, end, details, false);
	mmu_notifier_invalidate_range_end(&range);
	tlb_finish_mmu(&tlb);
	hugetlb_zap_end(vma, details);
}

 
void zap_vma_ptes(struct vm_area_struct *vma, unsigned long address,
		unsigned long size)
{
	if (!range_in_vma(vma, address, address + size) ||
	    		!(vma->vm_flags & VM_PFNMAP))
		return;

	zap_page_range_single(vma, address, size, NULL);
}
EXPORT_SYMBOL_GPL(zap_vma_ptes);

static pmd_t *walk_to_pmd(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	pgd = pgd_offset(mm, addr);
	p4d = p4d_alloc(mm, pgd, addr);
	if (!p4d)
		return NULL;
	pud = pud_alloc(mm, p4d, addr);
	if (!pud)
		return NULL;
	pmd = pmd_alloc(mm, pud, addr);
	if (!pmd)
		return NULL;

	VM_BUG_ON(pmd_trans_huge(*pmd));
	return pmd;
}

pte_t *__get_locked_pte(struct mm_struct *mm, unsigned long addr,
			spinlock_t **ptl)
{
	pmd_t *pmd = walk_to_pmd(mm, addr);

	if (!pmd)
		return NULL;
	return pte_alloc_map_lock(mm, pmd, addr, ptl);
}

static int validate_page_before_insert(struct page *page)
{
	if (PageAnon(page) || PageSlab(page) || page_has_type(page))
		return -EINVAL;
	flush_dcache_page(page);
	return 0;
}

static int insert_page_into_pte_locked(struct vm_area_struct *vma, pte_t *pte,
			unsigned long addr, struct page *page, pgprot_t prot)
{
	if (!pte_none(ptep_get(pte)))
		return -EBUSY;
	 
	get_page(page);
	inc_mm_counter(vma->vm_mm, mm_counter_file(page));
	page_add_file_rmap(page, vma, false);
	set_pte_at(vma->vm_mm, addr, pte, mk_pte(page, prot));
	return 0;
}

 
static int insert_page(struct vm_area_struct *vma, unsigned long addr,
			struct page *page, pgprot_t prot)
{
	int retval;
	pte_t *pte;
	spinlock_t *ptl;

	retval = validate_page_before_insert(page);
	if (retval)
		goto out;
	retval = -ENOMEM;
	pte = get_locked_pte(vma->vm_mm, addr, &ptl);
	if (!pte)
		goto out;
	retval = insert_page_into_pte_locked(vma, pte, addr, page, prot);
	pte_unmap_unlock(pte, ptl);
out:
	return retval;
}

static int insert_page_in_batch_locked(struct vm_area_struct *vma, pte_t *pte,
			unsigned long addr, struct page *page, pgprot_t prot)
{
	int err;

	if (!page_count(page))
		return -EINVAL;
	err = validate_page_before_insert(page);
	if (err)
		return err;
	return insert_page_into_pte_locked(vma, pte, addr, page, prot);
}

 
static int insert_pages(struct vm_area_struct *vma, unsigned long addr,
			struct page **pages, unsigned long *num, pgprot_t prot)
{
	pmd_t *pmd = NULL;
	pte_t *start_pte, *pte;
	spinlock_t *pte_lock;
	struct mm_struct *const mm = vma->vm_mm;
	unsigned long curr_page_idx = 0;
	unsigned long remaining_pages_total = *num;
	unsigned long pages_to_write_in_pmd;
	int ret;
more:
	ret = -EFAULT;
	pmd = walk_to_pmd(mm, addr);
	if (!pmd)
		goto out;

	pages_to_write_in_pmd = min_t(unsigned long,
		remaining_pages_total, PTRS_PER_PTE - pte_index(addr));

	 
	ret = -ENOMEM;
	if (pte_alloc(mm, pmd))
		goto out;

	while (pages_to_write_in_pmd) {
		int pte_idx = 0;
		const int batch_size = min_t(int, pages_to_write_in_pmd, 8);

		start_pte = pte_offset_map_lock(mm, pmd, addr, &pte_lock);
		if (!start_pte) {
			ret = -EFAULT;
			goto out;
		}
		for (pte = start_pte; pte_idx < batch_size; ++pte, ++pte_idx) {
			int err = insert_page_in_batch_locked(vma, pte,
				addr, pages[curr_page_idx], prot);
			if (unlikely(err)) {
				pte_unmap_unlock(start_pte, pte_lock);
				ret = err;
				remaining_pages_total -= pte_idx;
				goto out;
			}
			addr += PAGE_SIZE;
			++curr_page_idx;
		}
		pte_unmap_unlock(start_pte, pte_lock);
		pages_to_write_in_pmd -= batch_size;
		remaining_pages_total -= batch_size;
	}
	if (remaining_pages_total)
		goto more;
	ret = 0;
out:
	*num = remaining_pages_total;
	return ret;
}

 
int vm_insert_pages(struct vm_area_struct *vma, unsigned long addr,
			struct page **pages, unsigned long *num)
{
	const unsigned long end_addr = addr + (*num * PAGE_SIZE) - 1;

	if (addr < vma->vm_start || end_addr >= vma->vm_end)
		return -EFAULT;
	if (!(vma->vm_flags & VM_MIXEDMAP)) {
		BUG_ON(mmap_read_trylock(vma->vm_mm));
		BUG_ON(vma->vm_flags & VM_PFNMAP);
		vm_flags_set(vma, VM_MIXEDMAP);
	}
	 
	return insert_pages(vma, addr, pages, num, vma->vm_page_prot);
}
EXPORT_SYMBOL(vm_insert_pages);

 
int vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
			struct page *page)
{
	if (addr < vma->vm_start || addr >= vma->vm_end)
		return -EFAULT;
	if (!page_count(page))
		return -EINVAL;
	if (!(vma->vm_flags & VM_MIXEDMAP)) {
		BUG_ON(mmap_read_trylock(vma->vm_mm));
		BUG_ON(vma->vm_flags & VM_PFNMAP);
		vm_flags_set(vma, VM_MIXEDMAP);
	}
	return insert_page(vma, addr, page, vma->vm_page_prot);
}
EXPORT_SYMBOL(vm_insert_page);

 
static int __vm_map_pages(struct vm_area_struct *vma, struct page **pages,
				unsigned long num, unsigned long offset)
{
	unsigned long count = vma_pages(vma);
	unsigned long uaddr = vma->vm_start;
	int ret, i;

	 
	if (offset >= num)
		return -ENXIO;

	 
	if (count > num - offset)
		return -ENXIO;

	for (i = 0; i < count; i++) {
		ret = vm_insert_page(vma, uaddr, pages[offset + i]);
		if (ret < 0)
			return ret;
		uaddr += PAGE_SIZE;
	}

	return 0;
}

 
int vm_map_pages(struct vm_area_struct *vma, struct page **pages,
				unsigned long num)
{
	return __vm_map_pages(vma, pages, num, vma->vm_pgoff);
}
EXPORT_SYMBOL(vm_map_pages);

 
int vm_map_pages_zero(struct vm_area_struct *vma, struct page **pages,
				unsigned long num)
{
	return __vm_map_pages(vma, pages, num, 0);
}
EXPORT_SYMBOL(vm_map_pages_zero);

static vm_fault_t insert_pfn(struct vm_area_struct *vma, unsigned long addr,
			pfn_t pfn, pgprot_t prot, bool mkwrite)
{
	struct mm_struct *mm = vma->vm_mm;
	pte_t *pte, entry;
	spinlock_t *ptl;

	pte = get_locked_pte(mm, addr, &ptl);
	if (!pte)
		return VM_FAULT_OOM;
	entry = ptep_get(pte);
	if (!pte_none(entry)) {
		if (mkwrite) {
			 
			if (pte_pfn(entry) != pfn_t_to_pfn(pfn)) {
				WARN_ON_ONCE(!is_zero_pfn(pte_pfn(entry)));
				goto out_unlock;
			}
			entry = pte_mkyoung(entry);
			entry = maybe_mkwrite(pte_mkdirty(entry), vma);
			if (ptep_set_access_flags(vma, addr, pte, entry, 1))
				update_mmu_cache(vma, addr, pte);
		}
		goto out_unlock;
	}

	 
	if (pfn_t_devmap(pfn))
		entry = pte_mkdevmap(pfn_t_pte(pfn, prot));
	else
		entry = pte_mkspecial(pfn_t_pte(pfn, prot));

	if (mkwrite) {
		entry = pte_mkyoung(entry);
		entry = maybe_mkwrite(pte_mkdirty(entry), vma);
	}

	set_pte_at(mm, addr, pte, entry);
	update_mmu_cache(vma, addr, pte);  

out_unlock:
	pte_unmap_unlock(pte, ptl);
	return VM_FAULT_NOPAGE;
}

 
vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
			unsigned long pfn, pgprot_t pgprot)
{
	 
	BUG_ON(!(vma->vm_flags & (VM_PFNMAP|VM_MIXEDMAP)));
	BUG_ON((vma->vm_flags & (VM_PFNMAP|VM_MIXEDMAP)) ==
						(VM_PFNMAP|VM_MIXEDMAP));
	BUG_ON((vma->vm_flags & VM_PFNMAP) && is_cow_mapping(vma->vm_flags));
	BUG_ON((vma->vm_flags & VM_MIXEDMAP) && pfn_valid(pfn));

	if (addr < vma->vm_start || addr >= vma->vm_end)
		return VM_FAULT_SIGBUS;

	if (!pfn_modify_allowed(pfn, pgprot))
		return VM_FAULT_SIGBUS;

	track_pfn_insert(vma, &pgprot, __pfn_to_pfn_t(pfn, PFN_DEV));

	return insert_pfn(vma, addr, __pfn_to_pfn_t(pfn, PFN_DEV), pgprot,
			false);
}
EXPORT_SYMBOL(vmf_insert_pfn_prot);

 
vm_fault_t vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr,
			unsigned long pfn)
{
	return vmf_insert_pfn_prot(vma, addr, pfn, vma->vm_page_prot);
}
EXPORT_SYMBOL(vmf_insert_pfn);

static bool vm_mixed_ok(struct vm_area_struct *vma, pfn_t pfn)
{
	 
	if (vma->vm_flags & VM_MIXEDMAP)
		return true;
	if (pfn_t_devmap(pfn))
		return true;
	if (pfn_t_special(pfn))
		return true;
	if (is_zero_pfn(pfn_t_to_pfn(pfn)))
		return true;
	return false;
}

static vm_fault_t __vm_insert_mixed(struct vm_area_struct *vma,
		unsigned long addr, pfn_t pfn, bool mkwrite)
{
	pgprot_t pgprot = vma->vm_page_prot;
	int err;

	BUG_ON(!vm_mixed_ok(vma, pfn));

	if (addr < vma->vm_start || addr >= vma->vm_end)
		return VM_FAULT_SIGBUS;

	track_pfn_insert(vma, &pgprot, pfn);

	if (!pfn_modify_allowed(pfn_t_to_pfn(pfn), pgprot))
		return VM_FAULT_SIGBUS;

	 
	if (!IS_ENABLED(CONFIG_ARCH_HAS_PTE_SPECIAL) &&
	    !pfn_t_devmap(pfn) && pfn_t_valid(pfn)) {
		struct page *page;

		 
		page = pfn_to_page(pfn_t_to_pfn(pfn));
		err = insert_page(vma, addr, page, pgprot);
	} else {
		return insert_pfn(vma, addr, pfn, pgprot, mkwrite);
	}

	if (err == -ENOMEM)
		return VM_FAULT_OOM;
	if (err < 0 && err != -EBUSY)
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

vm_fault_t vmf_insert_mixed(struct vm_area_struct *vma, unsigned long addr,
		pfn_t pfn)
{
	return __vm_insert_mixed(vma, addr, pfn, false);
}
EXPORT_SYMBOL(vmf_insert_mixed);

 
vm_fault_t vmf_insert_mixed_mkwrite(struct vm_area_struct *vma,
		unsigned long addr, pfn_t pfn)
{
	return __vm_insert_mixed(vma, addr, pfn, true);
}
EXPORT_SYMBOL(vmf_insert_mixed_mkwrite);

 
static int remap_pte_range(struct mm_struct *mm, pmd_t *pmd,
			unsigned long addr, unsigned long end,
			unsigned long pfn, pgprot_t prot)
{
	pte_t *pte, *mapped_pte;
	spinlock_t *ptl;
	int err = 0;

	mapped_pte = pte = pte_alloc_map_lock(mm, pmd, addr, &ptl);
	if (!pte)
		return -ENOMEM;
	arch_enter_lazy_mmu_mode();
	do {
		BUG_ON(!pte_none(ptep_get(pte)));
		if (!pfn_modify_allowed(pfn, prot)) {
			err = -EACCES;
			break;
		}
		set_pte_at(mm, addr, pte, pte_mkspecial(pfn_pte(pfn, prot)));
		pfn++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(mapped_pte, ptl);
	return err;
}

static inline int remap_pmd_range(struct mm_struct *mm, pud_t *pud,
			unsigned long addr, unsigned long end,
			unsigned long pfn, pgprot_t prot)
{
	pmd_t *pmd;
	unsigned long next;
	int err;

	pfn -= addr >> PAGE_SHIFT;
	pmd = pmd_alloc(mm, pud, addr);
	if (!pmd)
		return -ENOMEM;
	VM_BUG_ON(pmd_trans_huge(*pmd));
	do {
		next = pmd_addr_end(addr, end);
		err = remap_pte_range(mm, pmd, addr, next,
				pfn + (addr >> PAGE_SHIFT), prot);
		if (err)
			return err;
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static inline int remap_pud_range(struct mm_struct *mm, p4d_t *p4d,
			unsigned long addr, unsigned long end,
			unsigned long pfn, pgprot_t prot)
{
	pud_t *pud;
	unsigned long next;
	int err;

	pfn -= addr >> PAGE_SHIFT;
	pud = pud_alloc(mm, p4d, addr);
	if (!pud)
		return -ENOMEM;
	do {
		next = pud_addr_end(addr, end);
		err = remap_pmd_range(mm, pud, addr, next,
				pfn + (addr >> PAGE_SHIFT), prot);
		if (err)
			return err;
	} while (pud++, addr = next, addr != end);
	return 0;
}

static inline int remap_p4d_range(struct mm_struct *mm, pgd_t *pgd,
			unsigned long addr, unsigned long end,
			unsigned long pfn, pgprot_t prot)
{
	p4d_t *p4d;
	unsigned long next;
	int err;

	pfn -= addr >> PAGE_SHIFT;
	p4d = p4d_alloc(mm, pgd, addr);
	if (!p4d)
		return -ENOMEM;
	do {
		next = p4d_addr_end(addr, end);
		err = remap_pud_range(mm, p4d, addr, next,
				pfn + (addr >> PAGE_SHIFT), prot);
		if (err)
			return err;
	} while (p4d++, addr = next, addr != end);
	return 0;
}

 
int remap_pfn_range_notrack(struct vm_area_struct *vma, unsigned long addr,
		unsigned long pfn, unsigned long size, pgprot_t prot)
{
	pgd_t *pgd;
	unsigned long next;
	unsigned long end = addr + PAGE_ALIGN(size);
	struct mm_struct *mm = vma->vm_mm;
	int err;

	if (WARN_ON_ONCE(!PAGE_ALIGNED(addr)))
		return -EINVAL;

	 
	if (is_cow_mapping(vma->vm_flags)) {
		if (addr != vma->vm_start || end != vma->vm_end)
			return -EINVAL;
		vma->vm_pgoff = pfn;
	}

	vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);

	BUG_ON(addr >= end);
	pfn -= addr >> PAGE_SHIFT;
	pgd = pgd_offset(mm, addr);
	flush_cache_range(vma, addr, end);
	do {
		next = pgd_addr_end(addr, end);
		err = remap_p4d_range(mm, pgd, addr, next,
				pfn + (addr >> PAGE_SHIFT), prot);
		if (err)
			return err;
	} while (pgd++, addr = next, addr != end);

	return 0;
}

 
int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
		    unsigned long pfn, unsigned long size, pgprot_t prot)
{
	int err;

	err = track_pfn_remap(vma, &prot, pfn, addr, PAGE_ALIGN(size));
	if (err)
		return -EINVAL;

	err = remap_pfn_range_notrack(vma, addr, pfn, size, prot);
	if (err)
		untrack_pfn(vma, pfn, PAGE_ALIGN(size), true);
	return err;
}
EXPORT_SYMBOL(remap_pfn_range);

 
int vm_iomap_memory(struct vm_area_struct *vma, phys_addr_t start, unsigned long len)
{
	unsigned long vm_len, pfn, pages;

	 
	if (start + len < start)
		return -EINVAL;
	 
	len += start & ~PAGE_MASK;
	pfn = start >> PAGE_SHIFT;
	pages = (len + ~PAGE_MASK) >> PAGE_SHIFT;
	if (pfn + pages < pfn)
		return -EINVAL;

	 
	if (vma->vm_pgoff > pages)
		return -EINVAL;
	pfn += vma->vm_pgoff;
	pages -= vma->vm_pgoff;

	 
	vm_len = vma->vm_end - vma->vm_start;
	if (vm_len >> PAGE_SHIFT > pages)
		return -EINVAL;

	 
	return io_remap_pfn_range(vma, vma->vm_start, pfn, vm_len, vma->vm_page_prot);
}
EXPORT_SYMBOL(vm_iomap_memory);

static int apply_to_pte_range(struct mm_struct *mm, pmd_t *pmd,
				     unsigned long addr, unsigned long end,
				     pte_fn_t fn, void *data, bool create,
				     pgtbl_mod_mask *mask)
{
	pte_t *pte, *mapped_pte;
	int err = 0;
	spinlock_t *ptl;

	if (create) {
		mapped_pte = pte = (mm == &init_mm) ?
			pte_alloc_kernel_track(pmd, addr, mask) :
			pte_alloc_map_lock(mm, pmd, addr, &ptl);
		if (!pte)
			return -ENOMEM;
	} else {
		mapped_pte = pte = (mm == &init_mm) ?
			pte_offset_kernel(pmd, addr) :
			pte_offset_map_lock(mm, pmd, addr, &ptl);
		if (!pte)
			return -EINVAL;
	}

	arch_enter_lazy_mmu_mode();

	if (fn) {
		do {
			if (create || !pte_none(ptep_get(pte))) {
				err = fn(pte++, addr, data);
				if (err)
					break;
			}
		} while (addr += PAGE_SIZE, addr != end);
	}
	*mask |= PGTBL_PTE_MODIFIED;

	arch_leave_lazy_mmu_mode();

	if (mm != &init_mm)
		pte_unmap_unlock(mapped_pte, ptl);
	return err;
}

static int apply_to_pmd_range(struct mm_struct *mm, pud_t *pud,
				     unsigned long addr, unsigned long end,
				     pte_fn_t fn, void *data, bool create,
				     pgtbl_mod_mask *mask)
{
	pmd_t *pmd;
	unsigned long next;
	int err = 0;

	BUG_ON(pud_huge(*pud));

	if (create) {
		pmd = pmd_alloc_track(mm, pud, addr, mask);
		if (!pmd)
			return -ENOMEM;
	} else {
		pmd = pmd_offset(pud, addr);
	}
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none(*pmd) && !create)
			continue;
		if (WARN_ON_ONCE(pmd_leaf(*pmd)))
			return -EINVAL;
		if (!pmd_none(*pmd) && WARN_ON_ONCE(pmd_bad(*pmd))) {
			if (!create)
				continue;
			pmd_clear_bad(pmd);
		}
		err = apply_to_pte_range(mm, pmd, addr, next,
					 fn, data, create, mask);
		if (err)
			break;
	} while (pmd++, addr = next, addr != end);

	return err;
}

static int apply_to_pud_range(struct mm_struct *mm, p4d_t *p4d,
				     unsigned long addr, unsigned long end,
				     pte_fn_t fn, void *data, bool create,
				     pgtbl_mod_mask *mask)
{
	pud_t *pud;
	unsigned long next;
	int err = 0;

	if (create) {
		pud = pud_alloc_track(mm, p4d, addr, mask);
		if (!pud)
			return -ENOMEM;
	} else {
		pud = pud_offset(p4d, addr);
	}
	do {
		next = pud_addr_end(addr, end);
		if (pud_none(*pud) && !create)
			continue;
		if (WARN_ON_ONCE(pud_leaf(*pud)))
			return -EINVAL;
		if (!pud_none(*pud) && WARN_ON_ONCE(pud_bad(*pud))) {
			if (!create)
				continue;
			pud_clear_bad(pud);
		}
		err = apply_to_pmd_range(mm, pud, addr, next,
					 fn, data, create, mask);
		if (err)
			break;
	} while (pud++, addr = next, addr != end);

	return err;
}

static int apply_to_p4d_range(struct mm_struct *mm, pgd_t *pgd,
				     unsigned long addr, unsigned long end,
				     pte_fn_t fn, void *data, bool create,
				     pgtbl_mod_mask *mask)
{
	p4d_t *p4d;
	unsigned long next;
	int err = 0;

	if (create) {
		p4d = p4d_alloc_track(mm, pgd, addr, mask);
		if (!p4d)
			return -ENOMEM;
	} else {
		p4d = p4d_offset(pgd, addr);
	}
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none(*p4d) && !create)
			continue;
		if (WARN_ON_ONCE(p4d_leaf(*p4d)))
			return -EINVAL;
		if (!p4d_none(*p4d) && WARN_ON_ONCE(p4d_bad(*p4d))) {
			if (!create)
				continue;
			p4d_clear_bad(p4d);
		}
		err = apply_to_pud_range(mm, p4d, addr, next,
					 fn, data, create, mask);
		if (err)
			break;
	} while (p4d++, addr = next, addr != end);

	return err;
}

static int __apply_to_page_range(struct mm_struct *mm, unsigned long addr,
				 unsigned long size, pte_fn_t fn,
				 void *data, bool create)
{
	pgd_t *pgd;
	unsigned long start = addr, next;
	unsigned long end = addr + size;
	pgtbl_mod_mask mask = 0;
	int err = 0;

	if (WARN_ON(addr >= end))
		return -EINVAL;

	pgd = pgd_offset(mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none(*pgd) && !create)
			continue;
		if (WARN_ON_ONCE(pgd_leaf(*pgd)))
			return -EINVAL;
		if (!pgd_none(*pgd) && WARN_ON_ONCE(pgd_bad(*pgd))) {
			if (!create)
				continue;
			pgd_clear_bad(pgd);
		}
		err = apply_to_p4d_range(mm, pgd, addr, next,
					 fn, data, create, &mask);
		if (err)
			break;
	} while (pgd++, addr = next, addr != end);

	if (mask & ARCH_PAGE_TABLE_SYNC_MASK)
		arch_sync_kernel_mappings(start, start + size);

	return err;
}

 
int apply_to_page_range(struct mm_struct *mm, unsigned long addr,
			unsigned long size, pte_fn_t fn, void *data)
{
	return __apply_to_page_range(mm, addr, size, fn, data, true);
}
EXPORT_SYMBOL_GPL(apply_to_page_range);

 
int apply_to_existing_page_range(struct mm_struct *mm, unsigned long addr,
				 unsigned long size, pte_fn_t fn, void *data)
{
	return __apply_to_page_range(mm, addr, size, fn, data, false);
}
EXPORT_SYMBOL_GPL(apply_to_existing_page_range);

 
static inline int pte_unmap_same(struct vm_fault *vmf)
{
	int same = 1;
#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPTION)
	if (sizeof(pte_t) > sizeof(unsigned long)) {
		spin_lock(vmf->ptl);
		same = pte_same(ptep_get(vmf->pte), vmf->orig_pte);
		spin_unlock(vmf->ptl);
	}
#endif
	pte_unmap(vmf->pte);
	vmf->pte = NULL;
	return same;
}

 
static inline int __wp_page_copy_user(struct page *dst, struct page *src,
				      struct vm_fault *vmf)
{
	int ret;
	void *kaddr;
	void __user *uaddr;
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	unsigned long addr = vmf->address;

	if (likely(src)) {
		if (copy_mc_user_highpage(dst, src, addr, vma)) {
			memory_failure_queue(page_to_pfn(src), 0);
			return -EHWPOISON;
		}
		return 0;
	}

	 
	kaddr = kmap_atomic(dst);
	uaddr = (void __user *)(addr & PAGE_MASK);

	 
	vmf->pte = NULL;
	if (!arch_has_hw_pte_young() && !pte_young(vmf->orig_pte)) {
		pte_t entry;

		vmf->pte = pte_offset_map_lock(mm, vmf->pmd, addr, &vmf->ptl);
		if (unlikely(!vmf->pte || !pte_same(ptep_get(vmf->pte), vmf->orig_pte))) {
			 
			if (vmf->pte)
				update_mmu_tlb(vma, addr, vmf->pte);
			ret = -EAGAIN;
			goto pte_unlock;
		}

		entry = pte_mkyoung(vmf->orig_pte);
		if (ptep_set_access_flags(vma, addr, vmf->pte, entry, 0))
			update_mmu_cache_range(vmf, vma, addr, vmf->pte, 1);
	}

	 
	if (__copy_from_user_inatomic(kaddr, uaddr, PAGE_SIZE)) {
		if (vmf->pte)
			goto warn;

		 
		vmf->pte = pte_offset_map_lock(mm, vmf->pmd, addr, &vmf->ptl);
		if (unlikely(!vmf->pte || !pte_same(ptep_get(vmf->pte), vmf->orig_pte))) {
			 
			if (vmf->pte)
				update_mmu_tlb(vma, addr, vmf->pte);
			ret = -EAGAIN;
			goto pte_unlock;
		}

		 
		if (__copy_from_user_inatomic(kaddr, uaddr, PAGE_SIZE)) {
			 
warn:
			WARN_ON_ONCE(1);
			clear_page(kaddr);
		}
	}

	ret = 0;

pte_unlock:
	if (vmf->pte)
		pte_unmap_unlock(vmf->pte, vmf->ptl);
	kunmap_atomic(kaddr);
	flush_dcache_page(dst);

	return ret;
}

static gfp_t __get_fault_gfp_mask(struct vm_area_struct *vma)
{
	struct file *vm_file = vma->vm_file;

	if (vm_file)
		return mapping_gfp_mask(vm_file->f_mapping) | __GFP_FS | __GFP_IO;

	 
	return GFP_KERNEL;
}

 
static vm_fault_t do_page_mkwrite(struct vm_fault *vmf, struct folio *folio)
{
	vm_fault_t ret;
	unsigned int old_flags = vmf->flags;

	vmf->flags = FAULT_FLAG_WRITE|FAULT_FLAG_MKWRITE;

	if (vmf->vma->vm_file &&
	    IS_SWAPFILE(vmf->vma->vm_file->f_mapping->host))
		return VM_FAULT_SIGBUS;

	ret = vmf->vma->vm_ops->page_mkwrite(vmf);
	 
	vmf->flags = old_flags;
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE)))
		return ret;
	if (unlikely(!(ret & VM_FAULT_LOCKED))) {
		folio_lock(folio);
		if (!folio->mapping) {
			folio_unlock(folio);
			return 0;  
		}
		ret |= VM_FAULT_LOCKED;
	} else
		VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	return ret;
}

 
static vm_fault_t fault_dirty_shared_page(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct address_space *mapping;
	struct folio *folio = page_folio(vmf->page);
	bool dirtied;
	bool page_mkwrite = vma->vm_ops && vma->vm_ops->page_mkwrite;

	dirtied = folio_mark_dirty(folio);
	VM_BUG_ON_FOLIO(folio_test_anon(folio), folio);
	 
	mapping = folio_raw_mapping(folio);
	folio_unlock(folio);

	if (!page_mkwrite)
		file_update_time(vma->vm_file);

	 
	if ((dirtied || page_mkwrite) && mapping) {
		struct file *fpin;

		fpin = maybe_unlock_mmap_for_io(vmf, NULL);
		balance_dirty_pages_ratelimited(mapping);
		if (fpin) {
			fput(fpin);
			return VM_FAULT_COMPLETED;
		}
	}

	return 0;
}

 
static inline void wp_page_reuse(struct vm_fault *vmf)
	__releases(vmf->ptl)
{
	struct vm_area_struct *vma = vmf->vma;
	struct page *page = vmf->page;
	pte_t entry;

	VM_BUG_ON(!(vmf->flags & FAULT_FLAG_WRITE));
	VM_BUG_ON(page && PageAnon(page) && !PageAnonExclusive(page));

	 
	if (page)
		page_cpupid_xchg_last(page, (1 << LAST_CPUPID_SHIFT) - 1);

	flush_cache_page(vma, vmf->address, pte_pfn(vmf->orig_pte));
	entry = pte_mkyoung(vmf->orig_pte);
	entry = maybe_mkwrite(pte_mkdirty(entry), vma);
	if (ptep_set_access_flags(vma, vmf->address, vmf->pte, entry, 1))
		update_mmu_cache_range(vmf, vma, vmf->address, vmf->pte, 1);
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	count_vm_event(PGREUSE);
}

 
static vm_fault_t wp_page_copy(struct vm_fault *vmf)
{
	const bool unshare = vmf->flags & FAULT_FLAG_UNSHARE;
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	struct folio *old_folio = NULL;
	struct folio *new_folio = NULL;
	pte_t entry;
	int page_copied = 0;
	struct mmu_notifier_range range;
	int ret;

	delayacct_wpcopy_start();

	if (vmf->page)
		old_folio = page_folio(vmf->page);
	if (unlikely(anon_vma_prepare(vma)))
		goto oom;

	if (is_zero_pfn(pte_pfn(vmf->orig_pte))) {
		new_folio = vma_alloc_zeroed_movable_folio(vma, vmf->address);
		if (!new_folio)
			goto oom;
	} else {
		new_folio = vma_alloc_folio(GFP_HIGHUSER_MOVABLE, 0, vma,
				vmf->address, false);
		if (!new_folio)
			goto oom;

		ret = __wp_page_copy_user(&new_folio->page, vmf->page, vmf);
		if (ret) {
			 
			folio_put(new_folio);
			if (old_folio)
				folio_put(old_folio);

			delayacct_wpcopy_end();
			return ret == -EHWPOISON ? VM_FAULT_HWPOISON : 0;
		}
		kmsan_copy_page_meta(&new_folio->page, vmf->page);
	}

	if (mem_cgroup_charge(new_folio, mm, GFP_KERNEL))
		goto oom_free_new;
	folio_throttle_swaprate(new_folio, GFP_KERNEL);

	__folio_mark_uptodate(new_folio);

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm,
				vmf->address & PAGE_MASK,
				(vmf->address & PAGE_MASK) + PAGE_SIZE);
	mmu_notifier_invalidate_range_start(&range);

	 
	vmf->pte = pte_offset_map_lock(mm, vmf->pmd, vmf->address, &vmf->ptl);
	if (likely(vmf->pte && pte_same(ptep_get(vmf->pte), vmf->orig_pte))) {
		if (old_folio) {
			if (!folio_test_anon(old_folio)) {
				dec_mm_counter(mm, mm_counter_file(&old_folio->page));
				inc_mm_counter(mm, MM_ANONPAGES);
			}
		} else {
			ksm_might_unmap_zero_page(mm, vmf->orig_pte);
			inc_mm_counter(mm, MM_ANONPAGES);
		}
		flush_cache_page(vma, vmf->address, pte_pfn(vmf->orig_pte));
		entry = mk_pte(&new_folio->page, vma->vm_page_prot);
		entry = pte_sw_mkyoung(entry);
		if (unlikely(unshare)) {
			if (pte_soft_dirty(vmf->orig_pte))
				entry = pte_mksoft_dirty(entry);
			if (pte_uffd_wp(vmf->orig_pte))
				entry = pte_mkuffd_wp(entry);
		} else {
			entry = maybe_mkwrite(pte_mkdirty(entry), vma);
		}

		 
		ptep_clear_flush(vma, vmf->address, vmf->pte);
		folio_add_new_anon_rmap(new_folio, vma, vmf->address);
		folio_add_lru_vma(new_folio, vma);
		 
		BUG_ON(unshare && pte_write(entry));
		set_pte_at_notify(mm, vmf->address, vmf->pte, entry);
		update_mmu_cache_range(vmf, vma, vmf->address, vmf->pte, 1);
		if (old_folio) {
			 
			page_remove_rmap(vmf->page, vma, false);
		}

		 
		new_folio = old_folio;
		page_copied = 1;
		pte_unmap_unlock(vmf->pte, vmf->ptl);
	} else if (vmf->pte) {
		update_mmu_tlb(vma, vmf->address, vmf->pte);
		pte_unmap_unlock(vmf->pte, vmf->ptl);
	}

	mmu_notifier_invalidate_range_end(&range);

	if (new_folio)
		folio_put(new_folio);
	if (old_folio) {
		if (page_copied)
			free_swap_cache(&old_folio->page);
		folio_put(old_folio);
	}

	delayacct_wpcopy_end();
	return 0;
oom_free_new:
	folio_put(new_folio);
oom:
	if (old_folio)
		folio_put(old_folio);

	delayacct_wpcopy_end();
	return VM_FAULT_OOM;
}

 
vm_fault_t finish_mkwrite_fault(struct vm_fault *vmf)
{
	WARN_ON_ONCE(!(vmf->vma->vm_flags & VM_SHARED));
	vmf->pte = pte_offset_map_lock(vmf->vma->vm_mm, vmf->pmd, vmf->address,
				       &vmf->ptl);
	if (!vmf->pte)
		return VM_FAULT_NOPAGE;
	 
	if (!pte_same(ptep_get(vmf->pte), vmf->orig_pte)) {
		update_mmu_tlb(vmf->vma, vmf->address, vmf->pte);
		pte_unmap_unlock(vmf->pte, vmf->ptl);
		return VM_FAULT_NOPAGE;
	}
	wp_page_reuse(vmf);
	return 0;
}

 
static vm_fault_t wp_pfn_shared(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;

	if (vma->vm_ops && vma->vm_ops->pfn_mkwrite) {
		vm_fault_t ret;

		pte_unmap_unlock(vmf->pte, vmf->ptl);
		if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
			vma_end_read(vmf->vma);
			return VM_FAULT_RETRY;
		}

		vmf->flags |= FAULT_FLAG_MKWRITE;
		ret = vma->vm_ops->pfn_mkwrite(vmf);
		if (ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE))
			return ret;
		return finish_mkwrite_fault(vmf);
	}
	wp_page_reuse(vmf);
	return 0;
}

static vm_fault_t wp_page_shared(struct vm_fault *vmf, struct folio *folio)
	__releases(vmf->ptl)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret = 0;

	folio_get(folio);

	if (vma->vm_ops && vma->vm_ops->page_mkwrite) {
		vm_fault_t tmp;

		pte_unmap_unlock(vmf->pte, vmf->ptl);
		if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
			folio_put(folio);
			vma_end_read(vmf->vma);
			return VM_FAULT_RETRY;
		}

		tmp = do_page_mkwrite(vmf, folio);
		if (unlikely(!tmp || (tmp &
				      (VM_FAULT_ERROR | VM_FAULT_NOPAGE)))) {
			folio_put(folio);
			return tmp;
		}
		tmp = finish_mkwrite_fault(vmf);
		if (unlikely(tmp & (VM_FAULT_ERROR | VM_FAULT_NOPAGE))) {
			folio_unlock(folio);
			folio_put(folio);
			return tmp;
		}
	} else {
		wp_page_reuse(vmf);
		folio_lock(folio);
	}
	ret |= fault_dirty_shared_page(vmf);
	folio_put(folio);

	return ret;
}

 
static vm_fault_t do_wp_page(struct vm_fault *vmf)
	__releases(vmf->ptl)
{
	const bool unshare = vmf->flags & FAULT_FLAG_UNSHARE;
	struct vm_area_struct *vma = vmf->vma;
	struct folio *folio = NULL;

	if (likely(!unshare)) {
		if (userfaultfd_pte_wp(vma, ptep_get(vmf->pte))) {
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			return handle_userfault(vmf, VM_UFFD_WP);
		}

		 
		if (unlikely(userfaultfd_wp(vmf->vma) &&
			     mm_tlb_flush_pending(vmf->vma->vm_mm)))
			flush_tlb_page(vmf->vma, vmf->address);
	}

	vmf->page = vm_normal_page(vma, vmf->address, vmf->orig_pte);

	if (vmf->page)
		folio = page_folio(vmf->page);

	 
	if (vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) {
		 
		if (!vmf->page)
			return wp_pfn_shared(vmf);
		return wp_page_shared(vmf, folio);
	}

	 
	if (folio && folio_test_anon(folio)) {
		 
		if (PageAnonExclusive(vmf->page))
			goto reuse;

		 
		if (folio_test_ksm(folio) || folio_ref_count(folio) > 3)
			goto copy;
		if (!folio_test_lru(folio))
			 
			lru_add_drain();
		if (folio_ref_count(folio) > 1 + folio_test_swapcache(folio))
			goto copy;
		if (!folio_trylock(folio))
			goto copy;
		if (folio_test_swapcache(folio))
			folio_free_swap(folio);
		if (folio_test_ksm(folio) || folio_ref_count(folio) != 1) {
			folio_unlock(folio);
			goto copy;
		}
		 
		page_move_anon_rmap(vmf->page, vma);
		folio_unlock(folio);
reuse:
		if (unlikely(unshare)) {
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			return 0;
		}
		wp_page_reuse(vmf);
		return 0;
	}
copy:
	if ((vmf->flags & FAULT_FLAG_VMA_LOCK) && !vma->anon_vma) {
		pte_unmap_unlock(vmf->pte, vmf->ptl);
		vma_end_read(vmf->vma);
		return VM_FAULT_RETRY;
	}

	 
	if (folio)
		folio_get(folio);

	pte_unmap_unlock(vmf->pte, vmf->ptl);
#ifdef CONFIG_KSM
	if (folio && folio_test_ksm(folio))
		count_vm_event(COW_KSM);
#endif
	return wp_page_copy(vmf);
}

static void unmap_mapping_range_vma(struct vm_area_struct *vma,
		unsigned long start_addr, unsigned long end_addr,
		struct zap_details *details)
{
	zap_page_range_single(vma, start_addr, end_addr - start_addr, details);
}

static inline void unmap_mapping_range_tree(struct rb_root_cached *root,
					    pgoff_t first_index,
					    pgoff_t last_index,
					    struct zap_details *details)
{
	struct vm_area_struct *vma;
	pgoff_t vba, vea, zba, zea;

	vma_interval_tree_foreach(vma, root, first_index, last_index) {
		vba = vma->vm_pgoff;
		vea = vba + vma_pages(vma) - 1;
		zba = max(first_index, vba);
		zea = min(last_index, vea);

		unmap_mapping_range_vma(vma,
			((zba - vba) << PAGE_SHIFT) + vma->vm_start,
			((zea - vba + 1) << PAGE_SHIFT) + vma->vm_start,
				details);
	}
}

 
void unmap_mapping_folio(struct folio *folio)
{
	struct address_space *mapping = folio->mapping;
	struct zap_details details = { };
	pgoff_t	first_index;
	pgoff_t	last_index;

	VM_BUG_ON(!folio_test_locked(folio));

	first_index = folio->index;
	last_index = folio_next_index(folio) - 1;

	details.even_cows = false;
	details.single_folio = folio;
	details.zap_flags = ZAP_FLAG_DROP_MARKER;

	i_mmap_lock_read(mapping);
	if (unlikely(!RB_EMPTY_ROOT(&mapping->i_mmap.rb_root)))
		unmap_mapping_range_tree(&mapping->i_mmap, first_index,
					 last_index, &details);
	i_mmap_unlock_read(mapping);
}

 
void unmap_mapping_pages(struct address_space *mapping, pgoff_t start,
		pgoff_t nr, bool even_cows)
{
	struct zap_details details = { };
	pgoff_t	first_index = start;
	pgoff_t	last_index = start + nr - 1;

	details.even_cows = even_cows;
	if (last_index < first_index)
		last_index = ULONG_MAX;

	i_mmap_lock_read(mapping);
	if (unlikely(!RB_EMPTY_ROOT(&mapping->i_mmap.rb_root)))
		unmap_mapping_range_tree(&mapping->i_mmap, first_index,
					 last_index, &details);
	i_mmap_unlock_read(mapping);
}
EXPORT_SYMBOL_GPL(unmap_mapping_pages);

 
void unmap_mapping_range(struct address_space *mapping,
		loff_t const holebegin, loff_t const holelen, int even_cows)
{
	pgoff_t hba = (pgoff_t)(holebegin) >> PAGE_SHIFT;
	pgoff_t hlen = ((pgoff_t)(holelen) + PAGE_SIZE - 1) >> PAGE_SHIFT;

	 
	if (sizeof(holelen) > sizeof(hlen)) {
		long long holeend =
			(holebegin + holelen + PAGE_SIZE - 1) >> PAGE_SHIFT;
		if (holeend & ~(long long)ULONG_MAX)
			hlen = ULONG_MAX - hba + 1;
	}

	unmap_mapping_pages(mapping, hba, hlen, even_cows);
}
EXPORT_SYMBOL(unmap_mapping_range);

 
static vm_fault_t remove_device_exclusive_entry(struct vm_fault *vmf)
{
	struct folio *folio = page_folio(vmf->page);
	struct vm_area_struct *vma = vmf->vma;
	struct mmu_notifier_range range;
	vm_fault_t ret;

	 
	if (!folio_try_get(folio))
		return 0;

	ret = folio_lock_or_retry(folio, vmf);
	if (ret) {
		folio_put(folio);
		return ret;
	}
	mmu_notifier_range_init_owner(&range, MMU_NOTIFY_EXCLUSIVE, 0,
				vma->vm_mm, vmf->address & PAGE_MASK,
				(vmf->address & PAGE_MASK) + PAGE_SIZE, NULL);
	mmu_notifier_invalidate_range_start(&range);

	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address,
				&vmf->ptl);
	if (likely(vmf->pte && pte_same(ptep_get(vmf->pte), vmf->orig_pte)))
		restore_exclusive_pte(vma, vmf->page, vmf->address, vmf->pte);

	if (vmf->pte)
		pte_unmap_unlock(vmf->pte, vmf->ptl);
	folio_unlock(folio);
	folio_put(folio);

	mmu_notifier_invalidate_range_end(&range);
	return 0;
}

static inline bool should_try_to_free_swap(struct folio *folio,
					   struct vm_area_struct *vma,
					   unsigned int fault_flags)
{
	if (!folio_test_swapcache(folio))
		return false;
	if (mem_cgroup_swap_full(folio) || (vma->vm_flags & VM_LOCKED) ||
	    folio_test_mlocked(folio))
		return true;
	 
	return (fault_flags & FAULT_FLAG_WRITE) && !folio_test_ksm(folio) &&
		folio_ref_count(folio) == 2;
}

static vm_fault_t pte_marker_clear(struct vm_fault *vmf)
{
	vmf->pte = pte_offset_map_lock(vmf->vma->vm_mm, vmf->pmd,
				       vmf->address, &vmf->ptl);
	if (!vmf->pte)
		return 0;
	 
	if (pte_same(vmf->orig_pte, ptep_get(vmf->pte)))
		pte_clear(vmf->vma->vm_mm, vmf->address, vmf->pte);
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	return 0;
}

static vm_fault_t do_pte_missing(struct vm_fault *vmf)
{
	if (vma_is_anonymous(vmf->vma))
		return do_anonymous_page(vmf);
	else
		return do_fault(vmf);
}

 
static vm_fault_t pte_marker_handle_uffd_wp(struct vm_fault *vmf)
{
	 
	if (unlikely(!userfaultfd_wp(vmf->vma)))
		return pte_marker_clear(vmf);

	return do_pte_missing(vmf);
}

static vm_fault_t handle_pte_marker(struct vm_fault *vmf)
{
	swp_entry_t entry = pte_to_swp_entry(vmf->orig_pte);
	unsigned long marker = pte_marker_get(entry);

	 
	if (WARN_ON_ONCE(!marker))
		return VM_FAULT_SIGBUS;

	 
	if (marker & PTE_MARKER_POISONED)
		return VM_FAULT_HWPOISON;

	if (pte_marker_entry_uffd_wp(entry))
		return pte_marker_handle_uffd_wp(vmf);

	 
	return VM_FAULT_SIGBUS;
}

 
vm_fault_t do_swap_page(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct folio *swapcache, *folio = NULL;
	struct page *page;
	struct swap_info_struct *si = NULL;
	rmap_t rmap_flags = RMAP_NONE;
	bool exclusive = false;
	swp_entry_t entry;
	pte_t pte;
	vm_fault_t ret = 0;
	void *shadow = NULL;

	if (!pte_unmap_same(vmf))
		goto out;

	entry = pte_to_swp_entry(vmf->orig_pte);
	if (unlikely(non_swap_entry(entry))) {
		if (is_migration_entry(entry)) {
			migration_entry_wait(vma->vm_mm, vmf->pmd,
					     vmf->address);
		} else if (is_device_exclusive_entry(entry)) {
			vmf->page = pfn_swap_entry_to_page(entry);
			ret = remove_device_exclusive_entry(vmf);
		} else if (is_device_private_entry(entry)) {
			if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
				 
				vma_end_read(vma);
				ret = VM_FAULT_RETRY;
				goto out;
			}

			vmf->page = pfn_swap_entry_to_page(entry);
			vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
					vmf->address, &vmf->ptl);
			if (unlikely(!vmf->pte ||
				     !pte_same(ptep_get(vmf->pte),
							vmf->orig_pte)))
				goto unlock;

			 
			get_page(vmf->page);
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			ret = vmf->page->pgmap->ops->migrate_to_ram(vmf);
			put_page(vmf->page);
		} else if (is_hwpoison_entry(entry)) {
			ret = VM_FAULT_HWPOISON;
		} else if (is_pte_marker_entry(entry)) {
			ret = handle_pte_marker(vmf);
		} else {
			print_bad_pte(vma, vmf->address, vmf->orig_pte, NULL);
			ret = VM_FAULT_SIGBUS;
		}
		goto out;
	}

	 
	si = get_swap_device(entry);
	if (unlikely(!si))
		goto out;

	folio = swap_cache_get_folio(entry, vma, vmf->address);
	if (folio)
		page = folio_file_page(folio, swp_offset(entry));
	swapcache = folio;

	if (!folio) {
		if (data_race(si->flags & SWP_SYNCHRONOUS_IO) &&
		    __swap_count(entry) == 1) {
			 
			folio = vma_alloc_folio(GFP_HIGHUSER_MOVABLE, 0,
						vma, vmf->address, false);
			page = &folio->page;
			if (folio) {
				__folio_set_locked(folio);
				__folio_set_swapbacked(folio);

				if (mem_cgroup_swapin_charge_folio(folio,
							vma->vm_mm, GFP_KERNEL,
							entry)) {
					ret = VM_FAULT_OOM;
					goto out_page;
				}
				mem_cgroup_swapin_uncharge_swap(entry);

				shadow = get_shadow_from_swap_cache(entry);
				if (shadow)
					workingset_refault(folio, shadow);

				folio_add_lru(folio);

				 
				folio->swap = entry;
				swap_readpage(page, true, NULL);
				folio->private = NULL;
			}
		} else {
			page = swapin_readahead(entry, GFP_HIGHUSER_MOVABLE,
						vmf);
			if (page)
				folio = page_folio(page);
			swapcache = folio;
		}

		if (!folio) {
			 
			vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
					vmf->address, &vmf->ptl);
			if (likely(vmf->pte &&
				   pte_same(ptep_get(vmf->pte), vmf->orig_pte)))
				ret = VM_FAULT_OOM;
			goto unlock;
		}

		 
		ret = VM_FAULT_MAJOR;
		count_vm_event(PGMAJFAULT);
		count_memcg_event_mm(vma->vm_mm, PGMAJFAULT);
	} else if (PageHWPoison(page)) {
		 
		ret = VM_FAULT_HWPOISON;
		goto out_release;
	}

	ret |= folio_lock_or_retry(folio, vmf);
	if (ret & VM_FAULT_RETRY)
		goto out_release;

	if (swapcache) {
		 
		if (unlikely(!folio_test_swapcache(folio) ||
			     page_swap_entry(page).val != entry.val))
			goto out_page;

		 
		page = ksm_might_need_to_copy(page, vma, vmf->address);
		if (unlikely(!page)) {
			ret = VM_FAULT_OOM;
			goto out_page;
		} else if (unlikely(PTR_ERR(page) == -EHWPOISON)) {
			ret = VM_FAULT_HWPOISON;
			goto out_page;
		}
		folio = page_folio(page);

		 
		if ((vmf->flags & FAULT_FLAG_WRITE) && folio == swapcache &&
		    !folio_test_ksm(folio) && !folio_test_lru(folio))
			lru_add_drain();
	}

	folio_throttle_swaprate(folio, GFP_KERNEL);

	 
	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address,
			&vmf->ptl);
	if (unlikely(!vmf->pte || !pte_same(ptep_get(vmf->pte), vmf->orig_pte)))
		goto out_nomap;

	if (unlikely(!folio_test_uptodate(folio))) {
		ret = VM_FAULT_SIGBUS;
		goto out_nomap;
	}

	 
	BUG_ON(!folio_test_anon(folio) && folio_test_mappedtodisk(folio));
	BUG_ON(folio_test_anon(folio) && PageAnonExclusive(page));

	 
	if (!folio_test_ksm(folio)) {
		exclusive = pte_swp_exclusive(vmf->orig_pte);
		if (folio != swapcache) {
			 
			exclusive = true;
		} else if (exclusive && folio_test_writeback(folio) &&
			  data_race(si->flags & SWP_STABLE_WRITES)) {
			 
			exclusive = false;
		}
	}

	 
	arch_swap_restore(entry, folio);

	 
	swap_free(entry);
	if (should_try_to_free_swap(folio, vma, vmf->flags))
		folio_free_swap(folio);

	inc_mm_counter(vma->vm_mm, MM_ANONPAGES);
	dec_mm_counter(vma->vm_mm, MM_SWAPENTS);
	pte = mk_pte(page, vma->vm_page_prot);

	 
	if (!folio_test_ksm(folio) &&
	    (exclusive || folio_ref_count(folio) == 1)) {
		if (vmf->flags & FAULT_FLAG_WRITE) {
			pte = maybe_mkwrite(pte_mkdirty(pte), vma);
			vmf->flags &= ~FAULT_FLAG_WRITE;
		}
		rmap_flags |= RMAP_EXCLUSIVE;
	}
	flush_icache_page(vma, page);
	if (pte_swp_soft_dirty(vmf->orig_pte))
		pte = pte_mksoft_dirty(pte);
	if (pte_swp_uffd_wp(vmf->orig_pte))
		pte = pte_mkuffd_wp(pte);
	vmf->orig_pte = pte;

	 
	if (unlikely(folio != swapcache && swapcache)) {
		page_add_new_anon_rmap(page, vma, vmf->address);
		folio_add_lru_vma(folio, vma);
	} else {
		page_add_anon_rmap(page, vma, vmf->address, rmap_flags);
	}

	VM_BUG_ON(!folio_test_anon(folio) ||
			(pte_write(pte) && !PageAnonExclusive(page)));
	set_pte_at(vma->vm_mm, vmf->address, vmf->pte, pte);
	arch_do_swap_page(vma->vm_mm, vma, vmf->address, pte, vmf->orig_pte);

	folio_unlock(folio);
	if (folio != swapcache && swapcache) {
		 
		folio_unlock(swapcache);
		folio_put(swapcache);
	}

	if (vmf->flags & FAULT_FLAG_WRITE) {
		ret |= do_wp_page(vmf);
		if (ret & VM_FAULT_ERROR)
			ret &= VM_FAULT_ERROR;
		goto out;
	}

	 
	update_mmu_cache_range(vmf, vma, vmf->address, vmf->pte, 1);
unlock:
	if (vmf->pte)
		pte_unmap_unlock(vmf->pte, vmf->ptl);
out:
	if (si)
		put_swap_device(si);
	return ret;
out_nomap:
	if (vmf->pte)
		pte_unmap_unlock(vmf->pte, vmf->ptl);
out_page:
	folio_unlock(folio);
out_release:
	folio_put(folio);
	if (folio != swapcache && swapcache) {
		folio_unlock(swapcache);
		folio_put(swapcache);
	}
	if (si)
		put_swap_device(si);
	return ret;
}

 
static vm_fault_t do_anonymous_page(struct vm_fault *vmf)
{
	bool uffd_wp = vmf_orig_pte_uffd_wp(vmf);
	struct vm_area_struct *vma = vmf->vma;
	struct folio *folio;
	vm_fault_t ret = 0;
	pte_t entry;

	 
	if (vma->vm_flags & VM_SHARED)
		return VM_FAULT_SIGBUS;

	 
	if (pte_alloc(vma->vm_mm, vmf->pmd))
		return VM_FAULT_OOM;

	 
	if (!(vmf->flags & FAULT_FLAG_WRITE) &&
			!mm_forbids_zeropage(vma->vm_mm)) {
		entry = pte_mkspecial(pfn_pte(my_zero_pfn(vmf->address),
						vma->vm_page_prot));
		vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
				vmf->address, &vmf->ptl);
		if (!vmf->pte)
			goto unlock;
		if (vmf_pte_changed(vmf)) {
			update_mmu_tlb(vma, vmf->address, vmf->pte);
			goto unlock;
		}
		ret = check_stable_address_space(vma->vm_mm);
		if (ret)
			goto unlock;
		 
		if (userfaultfd_missing(vma)) {
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			return handle_userfault(vmf, VM_UFFD_MISSING);
		}
		goto setpte;
	}

	 
	if (unlikely(anon_vma_prepare(vma)))
		goto oom;
	folio = vma_alloc_zeroed_movable_folio(vma, vmf->address);
	if (!folio)
		goto oom;

	if (mem_cgroup_charge(folio, vma->vm_mm, GFP_KERNEL))
		goto oom_free_page;
	folio_throttle_swaprate(folio, GFP_KERNEL);

	 
	__folio_mark_uptodate(folio);

	entry = mk_pte(&folio->page, vma->vm_page_prot);
	entry = pte_sw_mkyoung(entry);
	if (vma->vm_flags & VM_WRITE)
		entry = pte_mkwrite(pte_mkdirty(entry), vma);

	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, vmf->address,
			&vmf->ptl);
	if (!vmf->pte)
		goto release;
	if (vmf_pte_changed(vmf)) {
		update_mmu_tlb(vma, vmf->address, vmf->pte);
		goto release;
	}

	ret = check_stable_address_space(vma->vm_mm);
	if (ret)
		goto release;

	 
	if (userfaultfd_missing(vma)) {
		pte_unmap_unlock(vmf->pte, vmf->ptl);
		folio_put(folio);
		return handle_userfault(vmf, VM_UFFD_MISSING);
	}

	inc_mm_counter(vma->vm_mm, MM_ANONPAGES);
	folio_add_new_anon_rmap(folio, vma, vmf->address);
	folio_add_lru_vma(folio, vma);
setpte:
	if (uffd_wp)
		entry = pte_mkuffd_wp(entry);
	set_pte_at(vma->vm_mm, vmf->address, vmf->pte, entry);

	 
	update_mmu_cache_range(vmf, vma, vmf->address, vmf->pte, 1);
unlock:
	if (vmf->pte)
		pte_unmap_unlock(vmf->pte, vmf->ptl);
	return ret;
release:
	folio_put(folio);
	goto unlock;
oom_free_page:
	folio_put(folio);
oom:
	return VM_FAULT_OOM;
}

 
static vm_fault_t __do_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret;

	 
	if (pmd_none(*vmf->pmd) && !vmf->prealloc_pte) {
		vmf->prealloc_pte = pte_alloc_one(vma->vm_mm);
		if (!vmf->prealloc_pte)
			return VM_FAULT_OOM;
	}

	ret = vma->vm_ops->fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY |
			    VM_FAULT_DONE_COW)))
		return ret;

	if (unlikely(PageHWPoison(vmf->page))) {
		struct page *page = vmf->page;
		vm_fault_t poisonret = VM_FAULT_HWPOISON;
		if (ret & VM_FAULT_LOCKED) {
			if (page_mapped(page))
				unmap_mapping_pages(page_mapping(page),
						    page->index, 1, false);
			 
			if (invalidate_inode_page(page))
				poisonret = VM_FAULT_NOPAGE;
			unlock_page(page);
		}
		put_page(page);
		vmf->page = NULL;
		return poisonret;
	}

	if (unlikely(!(ret & VM_FAULT_LOCKED)))
		lock_page(vmf->page);
	else
		VM_BUG_ON_PAGE(!PageLocked(vmf->page), vmf->page);

	return ret;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static void deposit_prealloc_pte(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;

	pgtable_trans_huge_deposit(vma->vm_mm, vmf->pmd, vmf->prealloc_pte);
	 
	mm_inc_nr_ptes(vma->vm_mm);
	vmf->prealloc_pte = NULL;
}

vm_fault_t do_set_pmd(struct vm_fault *vmf, struct page *page)
{
	struct vm_area_struct *vma = vmf->vma;
	bool write = vmf->flags & FAULT_FLAG_WRITE;
	unsigned long haddr = vmf->address & HPAGE_PMD_MASK;
	pmd_t entry;
	vm_fault_t ret = VM_FAULT_FALLBACK;

	if (!transhuge_vma_suitable(vma, haddr))
		return ret;

	page = compound_head(page);
	if (compound_order(page) != HPAGE_PMD_ORDER)
		return ret;

	 
	if (unlikely(PageHasHWPoisoned(page)))
		return ret;

	 
	if (arch_needs_pgtable_deposit() && !vmf->prealloc_pte) {
		vmf->prealloc_pte = pte_alloc_one(vma->vm_mm);
		if (!vmf->prealloc_pte)
			return VM_FAULT_OOM;
	}

	vmf->ptl = pmd_lock(vma->vm_mm, vmf->pmd);
	if (unlikely(!pmd_none(*vmf->pmd)))
		goto out;

	flush_icache_pages(vma, page, HPAGE_PMD_NR);

	entry = mk_huge_pmd(page, vma->vm_page_prot);
	if (write)
		entry = maybe_pmd_mkwrite(pmd_mkdirty(entry), vma);

	add_mm_counter(vma->vm_mm, mm_counter_file(page), HPAGE_PMD_NR);
	page_add_file_rmap(page, vma, true);

	 
	if (arch_needs_pgtable_deposit())
		deposit_prealloc_pte(vmf);

	set_pmd_at(vma->vm_mm, haddr, vmf->pmd, entry);

	update_mmu_cache_pmd(vma, haddr, vmf->pmd);

	 
	ret = 0;
	count_vm_event(THP_FILE_MAPPED);
out:
	spin_unlock(vmf->ptl);
	return ret;
}
#else
vm_fault_t do_set_pmd(struct vm_fault *vmf, struct page *page)
{
	return VM_FAULT_FALLBACK;
}
#endif

 
void set_pte_range(struct vm_fault *vmf, struct folio *folio,
		struct page *page, unsigned int nr, unsigned long addr)
{
	struct vm_area_struct *vma = vmf->vma;
	bool uffd_wp = vmf_orig_pte_uffd_wp(vmf);
	bool write = vmf->flags & FAULT_FLAG_WRITE;
	bool prefault = in_range(vmf->address, addr, nr * PAGE_SIZE);
	pte_t entry;

	flush_icache_pages(vma, page, nr);
	entry = mk_pte(page, vma->vm_page_prot);

	if (prefault && arch_wants_old_prefaulted_pte())
		entry = pte_mkold(entry);
	else
		entry = pte_sw_mkyoung(entry);

	if (write)
		entry = maybe_mkwrite(pte_mkdirty(entry), vma);
	if (unlikely(uffd_wp))
		entry = pte_mkuffd_wp(entry);
	 
	if (write && !(vma->vm_flags & VM_SHARED)) {
		add_mm_counter(vma->vm_mm, MM_ANONPAGES, nr);
		VM_BUG_ON_FOLIO(nr != 1, folio);
		folio_add_new_anon_rmap(folio, vma, addr);
		folio_add_lru_vma(folio, vma);
	} else {
		add_mm_counter(vma->vm_mm, mm_counter_file(page), nr);
		folio_add_file_rmap_range(folio, page, nr, vma, false);
	}
	set_ptes(vma->vm_mm, addr, vmf->pte, entry, nr);

	 
	update_mmu_cache_range(vmf, vma, addr, vmf->pte, nr);
}

static bool vmf_pte_changed(struct vm_fault *vmf)
{
	if (vmf->flags & FAULT_FLAG_ORIG_PTE_VALID)
		return !pte_same(ptep_get(vmf->pte), vmf->orig_pte);

	return !pte_none(ptep_get(vmf->pte));
}

 
vm_fault_t finish_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct page *page;
	vm_fault_t ret;

	 
	if ((vmf->flags & FAULT_FLAG_WRITE) && !(vma->vm_flags & VM_SHARED))
		page = vmf->cow_page;
	else
		page = vmf->page;

	 
	if (!(vma->vm_flags & VM_SHARED)) {
		ret = check_stable_address_space(vma->vm_mm);
		if (ret)
			return ret;
	}

	if (pmd_none(*vmf->pmd)) {
		if (PageTransCompound(page)) {
			ret = do_set_pmd(vmf, page);
			if (ret != VM_FAULT_FALLBACK)
				return ret;
		}

		if (vmf->prealloc_pte)
			pmd_install(vma->vm_mm, vmf->pmd, &vmf->prealloc_pte);
		else if (unlikely(pte_alloc(vma->vm_mm, vmf->pmd)))
			return VM_FAULT_OOM;
	}

	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
				      vmf->address, &vmf->ptl);
	if (!vmf->pte)
		return VM_FAULT_NOPAGE;

	 
	if (likely(!vmf_pte_changed(vmf))) {
		struct folio *folio = page_folio(page);

		set_pte_range(vmf, folio, page, 1, vmf->address);
		ret = 0;
	} else {
		update_mmu_tlb(vma, vmf->address, vmf->pte);
		ret = VM_FAULT_NOPAGE;
	}

	pte_unmap_unlock(vmf->pte, vmf->ptl);
	return ret;
}

static unsigned long fault_around_pages __read_mostly =
	65536 >> PAGE_SHIFT;

#ifdef CONFIG_DEBUG_FS
static int fault_around_bytes_get(void *data, u64 *val)
{
	*val = fault_around_pages << PAGE_SHIFT;
	return 0;
}

 
static int fault_around_bytes_set(void *data, u64 val)
{
	if (val / PAGE_SIZE > PTRS_PER_PTE)
		return -EINVAL;

	 
	fault_around_pages = max(rounddown_pow_of_two(val) >> PAGE_SHIFT, 1UL);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fault_around_bytes_fops,
		fault_around_bytes_get, fault_around_bytes_set, "%llu\n");

static int __init fault_around_debugfs(void)
{
	debugfs_create_file_unsafe("fault_around_bytes", 0644, NULL, NULL,
				   &fault_around_bytes_fops);
	return 0;
}
late_initcall(fault_around_debugfs);
#endif

 
static vm_fault_t do_fault_around(struct vm_fault *vmf)
{
	pgoff_t nr_pages = READ_ONCE(fault_around_pages);
	pgoff_t pte_off = pte_index(vmf->address);
	 
	pgoff_t vma_off = vmf->pgoff - vmf->vma->vm_pgoff;
	pgoff_t from_pte, to_pte;
	vm_fault_t ret;

	 
	from_pte = max(ALIGN_DOWN(pte_off, nr_pages),
		       pte_off - min(pte_off, vma_off));

	 
	to_pte = min3(from_pte + nr_pages, (pgoff_t)PTRS_PER_PTE,
		      pte_off + vma_pages(vmf->vma) - vma_off) - 1;

	if (pmd_none(*vmf->pmd)) {
		vmf->prealloc_pte = pte_alloc_one(vmf->vma->vm_mm);
		if (!vmf->prealloc_pte)
			return VM_FAULT_OOM;
	}

	rcu_read_lock();
	ret = vmf->vma->vm_ops->map_pages(vmf,
			vmf->pgoff + from_pte - pte_off,
			vmf->pgoff + to_pte - pte_off);
	rcu_read_unlock();

	return ret;
}

 
static inline bool should_fault_around(struct vm_fault *vmf)
{
	 
	if (!vmf->vma->vm_ops->map_pages)
		return false;

	if (uffd_disable_fault_around(vmf->vma))
		return false;

	 
	return fault_around_pages > 1;
}

static vm_fault_t do_read_fault(struct vm_fault *vmf)
{
	vm_fault_t ret = 0;
	struct folio *folio;

	 
	if (should_fault_around(vmf)) {
		ret = do_fault_around(vmf);
		if (ret)
			return ret;
	}

	if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
		vma_end_read(vmf->vma);
		return VM_FAULT_RETRY;
	}

	ret = __do_fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		return ret;

	ret |= finish_fault(vmf);
	folio = page_folio(vmf->page);
	folio_unlock(folio);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		folio_put(folio);
	return ret;
}

static vm_fault_t do_cow_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret;

	if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
		vma_end_read(vma);
		return VM_FAULT_RETRY;
	}

	if (unlikely(anon_vma_prepare(vma)))
		return VM_FAULT_OOM;

	vmf->cow_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, vmf->address);
	if (!vmf->cow_page)
		return VM_FAULT_OOM;

	if (mem_cgroup_charge(page_folio(vmf->cow_page), vma->vm_mm,
				GFP_KERNEL)) {
		put_page(vmf->cow_page);
		return VM_FAULT_OOM;
	}
	folio_throttle_swaprate(page_folio(vmf->cow_page), GFP_KERNEL);

	ret = __do_fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		goto uncharge_out;
	if (ret & VM_FAULT_DONE_COW)
		return ret;

	copy_user_highpage(vmf->cow_page, vmf->page, vmf->address, vma);
	__SetPageUptodate(vmf->cow_page);

	ret |= finish_fault(vmf);
	unlock_page(vmf->page);
	put_page(vmf->page);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		goto uncharge_out;
	return ret;
uncharge_out:
	put_page(vmf->cow_page);
	return ret;
}

static vm_fault_t do_shared_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret, tmp;
	struct folio *folio;

	if (vmf->flags & FAULT_FLAG_VMA_LOCK) {
		vma_end_read(vma);
		return VM_FAULT_RETRY;
	}

	ret = __do_fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		return ret;

	folio = page_folio(vmf->page);

	 
	if (vma->vm_ops->page_mkwrite) {
		folio_unlock(folio);
		tmp = do_page_mkwrite(vmf, folio);
		if (unlikely(!tmp ||
				(tmp & (VM_FAULT_ERROR | VM_FAULT_NOPAGE)))) {
			folio_put(folio);
			return tmp;
		}
	}

	ret |= finish_fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE |
					VM_FAULT_RETRY))) {
		folio_unlock(folio);
		folio_put(folio);
		return ret;
	}

	ret |= fault_dirty_shared_page(vmf);
	return ret;
}

 
static vm_fault_t do_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *vm_mm = vma->vm_mm;
	vm_fault_t ret;

	 
	if (!vma->vm_ops->fault) {
		vmf->pte = pte_offset_map_lock(vmf->vma->vm_mm, vmf->pmd,
					       vmf->address, &vmf->ptl);
		if (unlikely(!vmf->pte))
			ret = VM_FAULT_SIGBUS;
		else {
			 
			if (unlikely(pte_none(ptep_get(vmf->pte))))
				ret = VM_FAULT_SIGBUS;
			else
				ret = VM_FAULT_NOPAGE;

			pte_unmap_unlock(vmf->pte, vmf->ptl);
		}
	} else if (!(vmf->flags & FAULT_FLAG_WRITE))
		ret = do_read_fault(vmf);
	else if (!(vma->vm_flags & VM_SHARED))
		ret = do_cow_fault(vmf);
	else
		ret = do_shared_fault(vmf);

	 
	if (vmf->prealloc_pte) {
		pte_free(vm_mm, vmf->prealloc_pte);
		vmf->prealloc_pte = NULL;
	}
	return ret;
}

int numa_migrate_prep(struct page *page, struct vm_area_struct *vma,
		      unsigned long addr, int page_nid, int *flags)
{
	get_page(page);

	 
	vma_set_access_pid_bit(vma);

	count_vm_numa_event(NUMA_HINT_FAULTS);
	if (page_nid == numa_node_id()) {
		count_vm_numa_event(NUMA_HINT_FAULTS_LOCAL);
		*flags |= TNF_FAULT_LOCAL;
	}

	return mpol_misplaced(page, vma, addr);
}

static vm_fault_t do_numa_page(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct page *page = NULL;
	int page_nid = NUMA_NO_NODE;
	bool writable = false;
	int last_cpupid;
	int target_nid;
	pte_t pte, old_pte;
	int flags = 0;

	 
	spin_lock(vmf->ptl);
	if (unlikely(!pte_same(ptep_get(vmf->pte), vmf->orig_pte))) {
		pte_unmap_unlock(vmf->pte, vmf->ptl);
		goto out;
	}

	 
	old_pte = ptep_get(vmf->pte);
	pte = pte_modify(old_pte, vma->vm_page_prot);

	 
	writable = pte_write(pte);
	if (!writable && vma_wants_manual_pte_write_upgrade(vma) &&
	    can_change_pte_writable(vma, vmf->address, pte))
		writable = true;

	page = vm_normal_page(vma, vmf->address, pte);
	if (!page || is_zone_device_page(page))
		goto out_map;

	 
	if (PageCompound(page))
		goto out_map;

	 
	if (!writable)
		flags |= TNF_NO_GROUP;

	 
	if (page_mapcount(page) > 1 && (vma->vm_flags & VM_SHARED))
		flags |= TNF_SHARED;

	page_nid = page_to_nid(page);
	 
	if ((sysctl_numa_balancing_mode & NUMA_BALANCING_MEMORY_TIERING) &&
	    !node_is_toptier(page_nid))
		last_cpupid = (-1 & LAST_CPUPID_MASK);
	else
		last_cpupid = page_cpupid_last(page);
	target_nid = numa_migrate_prep(page, vma, vmf->address, page_nid,
			&flags);
	if (target_nid == NUMA_NO_NODE) {
		put_page(page);
		goto out_map;
	}
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	writable = false;

	 
	if (migrate_misplaced_page(page, vma, target_nid)) {
		page_nid = target_nid;
		flags |= TNF_MIGRATED;
	} else {
		flags |= TNF_MIGRATE_FAIL;
		vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd,
					       vmf->address, &vmf->ptl);
		if (unlikely(!vmf->pte))
			goto out;
		if (unlikely(!pte_same(ptep_get(vmf->pte), vmf->orig_pte))) {
			pte_unmap_unlock(vmf->pte, vmf->ptl);
			goto out;
		}
		goto out_map;
	}

out:
	if (page_nid != NUMA_NO_NODE)
		task_numa_fault(last_cpupid, page_nid, 1, flags);
	return 0;
out_map:
	 
	old_pte = ptep_modify_prot_start(vma, vmf->address, vmf->pte);
	pte = pte_modify(old_pte, vma->vm_page_prot);
	pte = pte_mkyoung(pte);
	if (writable)
		pte = pte_mkwrite(pte, vma);
	ptep_modify_prot_commit(vma, vmf->address, vmf->pte, old_pte, pte);
	update_mmu_cache_range(vmf, vma, vmf->address, vmf->pte, 1);
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	goto out;
}

static inline vm_fault_t create_huge_pmd(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	if (vma_is_anonymous(vma))
		return do_huge_pmd_anonymous_page(vmf);
	if (vma->vm_ops->huge_fault)
		return vma->vm_ops->huge_fault(vmf, PMD_ORDER);
	return VM_FAULT_FALLBACK;
}

 
static inline vm_fault_t wp_huge_pmd(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	const bool unshare = vmf->flags & FAULT_FLAG_UNSHARE;
	vm_fault_t ret;

	if (vma_is_anonymous(vma)) {
		if (likely(!unshare) &&
		    userfaultfd_huge_pmd_wp(vma, vmf->orig_pmd))
			return handle_userfault(vmf, VM_UFFD_WP);
		return do_huge_pmd_wp_page(vmf);
	}

	if (vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) {
		if (vma->vm_ops->huge_fault) {
			ret = vma->vm_ops->huge_fault(vmf, PMD_ORDER);
			if (!(ret & VM_FAULT_FALLBACK))
				return ret;
		}
	}

	 
	__split_huge_pmd(vma, vmf->pmd, vmf->address, false, NULL);

	return VM_FAULT_FALLBACK;
}

static vm_fault_t create_huge_pud(struct vm_fault *vmf)
{
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) &&			\
	defined(CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD)
	struct vm_area_struct *vma = vmf->vma;
	 
	if (vma_is_anonymous(vma))
		return VM_FAULT_FALLBACK;
	if (vma->vm_ops->huge_fault)
		return vma->vm_ops->huge_fault(vmf, PUD_ORDER);
#endif  
	return VM_FAULT_FALLBACK;
}

static vm_fault_t wp_huge_pud(struct vm_fault *vmf, pud_t orig_pud)
{
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) &&			\
	defined(CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD)
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret;

	 
	if (vma_is_anonymous(vma))
		goto split;
	if (vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) {
		if (vma->vm_ops->huge_fault) {
			ret = vma->vm_ops->huge_fault(vmf, PUD_ORDER);
			if (!(ret & VM_FAULT_FALLBACK))
				return ret;
		}
	}
split:
	 
	__split_huge_pud(vma, vmf->pud, vmf->address);
#endif  
	return VM_FAULT_FALLBACK;
}

 
static vm_fault_t handle_pte_fault(struct vm_fault *vmf)
{
	pte_t entry;

	if (unlikely(pmd_none(*vmf->pmd))) {
		 
		vmf->pte = NULL;
		vmf->flags &= ~FAULT_FLAG_ORIG_PTE_VALID;
	} else {
		 
		vmf->pte = pte_offset_map_nolock(vmf->vma->vm_mm, vmf->pmd,
						 vmf->address, &vmf->ptl);
		if (unlikely(!vmf->pte))
			return 0;
		vmf->orig_pte = ptep_get_lockless(vmf->pte);
		vmf->flags |= FAULT_FLAG_ORIG_PTE_VALID;

		if (pte_none(vmf->orig_pte)) {
			pte_unmap(vmf->pte);
			vmf->pte = NULL;
		}
	}

	if (!vmf->pte)
		return do_pte_missing(vmf);

	if (!pte_present(vmf->orig_pte))
		return do_swap_page(vmf);

	if (pte_protnone(vmf->orig_pte) && vma_is_accessible(vmf->vma))
		return do_numa_page(vmf);

	spin_lock(vmf->ptl);
	entry = vmf->orig_pte;
	if (unlikely(!pte_same(ptep_get(vmf->pte), entry))) {
		update_mmu_tlb(vmf->vma, vmf->address, vmf->pte);
		goto unlock;
	}
	if (vmf->flags & (FAULT_FLAG_WRITE|FAULT_FLAG_UNSHARE)) {
		if (!pte_write(entry))
			return do_wp_page(vmf);
		else if (likely(vmf->flags & FAULT_FLAG_WRITE))
			entry = pte_mkdirty(entry);
	}
	entry = pte_mkyoung(entry);
	if (ptep_set_access_flags(vmf->vma, vmf->address, vmf->pte, entry,
				vmf->flags & FAULT_FLAG_WRITE)) {
		update_mmu_cache_range(vmf, vmf->vma, vmf->address,
				vmf->pte, 1);
	} else {
		 
		if (vmf->flags & FAULT_FLAG_TRIED)
			goto unlock;
		 
		if (vmf->flags & FAULT_FLAG_WRITE)
			flush_tlb_fix_spurious_fault(vmf->vma, vmf->address,
						     vmf->pte);
	}
unlock:
	pte_unmap_unlock(vmf->pte, vmf->ptl);
	return 0;
}

 
static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
		unsigned long address, unsigned int flags)
{
	struct vm_fault vmf = {
		.vma = vma,
		.address = address & PAGE_MASK,
		.real_address = address,
		.flags = flags,
		.pgoff = linear_page_index(vma, address),
		.gfp_mask = __get_fault_gfp_mask(vma),
	};
	struct mm_struct *mm = vma->vm_mm;
	unsigned long vm_flags = vma->vm_flags;
	pgd_t *pgd;
	p4d_t *p4d;
	vm_fault_t ret;

	pgd = pgd_offset(mm, address);
	p4d = p4d_alloc(mm, pgd, address);
	if (!p4d)
		return VM_FAULT_OOM;

	vmf.pud = pud_alloc(mm, p4d, address);
	if (!vmf.pud)
		return VM_FAULT_OOM;
retry_pud:
	if (pud_none(*vmf.pud) &&
	    hugepage_vma_check(vma, vm_flags, false, true, true)) {
		ret = create_huge_pud(&vmf);
		if (!(ret & VM_FAULT_FALLBACK))
			return ret;
	} else {
		pud_t orig_pud = *vmf.pud;

		barrier();
		if (pud_trans_huge(orig_pud) || pud_devmap(orig_pud)) {

			 
			if ((flags & FAULT_FLAG_WRITE) && !pud_write(orig_pud)) {
				ret = wp_huge_pud(&vmf, orig_pud);
				if (!(ret & VM_FAULT_FALLBACK))
					return ret;
			} else {
				huge_pud_set_accessed(&vmf, orig_pud);
				return 0;
			}
		}
	}

	vmf.pmd = pmd_alloc(mm, vmf.pud, address);
	if (!vmf.pmd)
		return VM_FAULT_OOM;

	 
	if (pud_trans_unstable(vmf.pud))
		goto retry_pud;

	if (pmd_none(*vmf.pmd) &&
	    hugepage_vma_check(vma, vm_flags, false, true, true)) {
		ret = create_huge_pmd(&vmf);
		if (!(ret & VM_FAULT_FALLBACK))
			return ret;
	} else {
		vmf.orig_pmd = pmdp_get_lockless(vmf.pmd);

		if (unlikely(is_swap_pmd(vmf.orig_pmd))) {
			VM_BUG_ON(thp_migration_supported() &&
					  !is_pmd_migration_entry(vmf.orig_pmd));
			if (is_pmd_migration_entry(vmf.orig_pmd))
				pmd_migration_entry_wait(mm, vmf.pmd);
			return 0;
		}
		if (pmd_trans_huge(vmf.orig_pmd) || pmd_devmap(vmf.orig_pmd)) {
			if (pmd_protnone(vmf.orig_pmd) && vma_is_accessible(vma))
				return do_huge_pmd_numa_page(&vmf);

			if ((flags & (FAULT_FLAG_WRITE|FAULT_FLAG_UNSHARE)) &&
			    !pmd_write(vmf.orig_pmd)) {
				ret = wp_huge_pmd(&vmf);
				if (!(ret & VM_FAULT_FALLBACK))
					return ret;
			} else {
				huge_pmd_set_accessed(&vmf);
				return 0;
			}
		}
	}

	return handle_pte_fault(&vmf);
}

 
static inline void mm_account_fault(struct mm_struct *mm, struct pt_regs *regs,
				    unsigned long address, unsigned int flags,
				    vm_fault_t ret)
{
	bool major;

	 
	if (ret & VM_FAULT_RETRY)
		return;

	 
	count_vm_event(PGFAULT);
	count_memcg_event_mm(mm, PGFAULT);

	 
	if (ret & VM_FAULT_ERROR)
		return;

	 
	major = (ret & VM_FAULT_MAJOR) || (flags & FAULT_FLAG_TRIED);

	if (major)
		current->maj_flt++;
	else
		current->min_flt++;

	 
	if (!regs)
		return;

	if (major)
		perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MAJ, 1, regs, address);
	else
		perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS_MIN, 1, regs, address);
}

#ifdef CONFIG_LRU_GEN
static void lru_gen_enter_fault(struct vm_area_struct *vma)
{
	 
	current->in_lru_fault = vma_has_recency(vma);
}

static void lru_gen_exit_fault(void)
{
	current->in_lru_fault = false;
}
#else
static void lru_gen_enter_fault(struct vm_area_struct *vma)
{
}

static void lru_gen_exit_fault(void)
{
}
#endif  

static vm_fault_t sanitize_fault_flags(struct vm_area_struct *vma,
				       unsigned int *flags)
{
	if (unlikely(*flags & FAULT_FLAG_UNSHARE)) {
		if (WARN_ON_ONCE(*flags & FAULT_FLAG_WRITE))
			return VM_FAULT_SIGSEGV;
		 
		if (!is_cow_mapping(vma->vm_flags))
			*flags &= ~FAULT_FLAG_UNSHARE;
	} else if (*flags & FAULT_FLAG_WRITE) {
		 
		if (WARN_ON_ONCE(!(vma->vm_flags & VM_MAYWRITE)))
			return VM_FAULT_SIGSEGV;
		 
		if (WARN_ON_ONCE(!(vma->vm_flags & VM_WRITE) &&
				 !is_cow_mapping(vma->vm_flags)))
			return VM_FAULT_SIGSEGV;
	}
#ifdef CONFIG_PER_VMA_LOCK
	 
	if (WARN_ON_ONCE((*flags &
			(FAULT_FLAG_VMA_LOCK | FAULT_FLAG_RETRY_NOWAIT)) ==
			(FAULT_FLAG_VMA_LOCK | FAULT_FLAG_RETRY_NOWAIT)))
		return VM_FAULT_SIGSEGV;
#endif

	return 0;
}

 
vm_fault_t handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
			   unsigned int flags, struct pt_regs *regs)
{
	 
	struct mm_struct *mm = vma->vm_mm;
	vm_fault_t ret;

	__set_current_state(TASK_RUNNING);

	ret = sanitize_fault_flags(vma, &flags);
	if (ret)
		goto out;

	if (!arch_vma_access_permitted(vma, flags & FAULT_FLAG_WRITE,
					    flags & FAULT_FLAG_INSTRUCTION,
					    flags & FAULT_FLAG_REMOTE)) {
		ret = VM_FAULT_SIGSEGV;
		goto out;
	}

	 
	if (flags & FAULT_FLAG_USER)
		mem_cgroup_enter_user_fault();

	lru_gen_enter_fault(vma);

	if (unlikely(is_vm_hugetlb_page(vma)))
		ret = hugetlb_fault(vma->vm_mm, vma, address, flags);
	else
		ret = __handle_mm_fault(vma, address, flags);

	lru_gen_exit_fault();

	if (flags & FAULT_FLAG_USER) {
		mem_cgroup_exit_user_fault();
		 
		if (task_in_memcg_oom(current) && !(ret & VM_FAULT_OOM))
			mem_cgroup_oom_synchronize(false);
	}
out:
	mm_account_fault(mm, regs, address, flags, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(handle_mm_fault);

#ifdef CONFIG_LOCK_MM_AND_FIND_VMA
#include <linux/extable.h>

static inline bool get_mmap_lock_carefully(struct mm_struct *mm, struct pt_regs *regs)
{
	if (likely(mmap_read_trylock(mm)))
		return true;

	if (regs && !user_mode(regs)) {
		unsigned long ip = instruction_pointer(regs);
		if (!search_exception_tables(ip))
			return false;
	}

	return !mmap_read_lock_killable(mm);
}

static inline bool mmap_upgrade_trylock(struct mm_struct *mm)
{
	 
	return false;
}

static inline bool upgrade_mmap_lock_carefully(struct mm_struct *mm, struct pt_regs *regs)
{
	mmap_read_unlock(mm);
	if (regs && !user_mode(regs)) {
		unsigned long ip = instruction_pointer(regs);
		if (!search_exception_tables(ip))
			return false;
	}
	return !mmap_write_lock_killable(mm);
}

 
struct vm_area_struct *lock_mm_and_find_vma(struct mm_struct *mm,
			unsigned long addr, struct pt_regs *regs)
{
	struct vm_area_struct *vma;

	if (!get_mmap_lock_carefully(mm, regs))
		return NULL;

	vma = find_vma(mm, addr);
	if (likely(vma && (vma->vm_start <= addr)))
		return vma;

	 
	if (!vma || !(vma->vm_flags & VM_GROWSDOWN)) {
		mmap_read_unlock(mm);
		return NULL;
	}

	 
	if (!mmap_upgrade_trylock(mm)) {
		if (!upgrade_mmap_lock_carefully(mm, regs))
			return NULL;

		vma = find_vma(mm, addr);
		if (!vma)
			goto fail;
		if (vma->vm_start <= addr)
			goto success;
		if (!(vma->vm_flags & VM_GROWSDOWN))
			goto fail;
	}

	if (expand_stack_locked(vma, addr))
		goto fail;

success:
	mmap_write_downgrade(mm);
	return vma;

fail:
	mmap_write_unlock(mm);
	return NULL;
}
#endif

#ifdef CONFIG_PER_VMA_LOCK
 
struct vm_area_struct *lock_vma_under_rcu(struct mm_struct *mm,
					  unsigned long address)
{
	MA_STATE(mas, &mm->mm_mt, address, address);
	struct vm_area_struct *vma;

	rcu_read_lock();
retry:
	vma = mas_walk(&mas);
	if (!vma)
		goto inval;

	if (!vma_start_read(vma))
		goto inval;

	 
	if (unlikely(vma_is_anonymous(vma) && !vma->anon_vma))
		goto inval_end_read;

	 
	if (unlikely(address < vma->vm_start || address >= vma->vm_end))
		goto inval_end_read;

	 
	if (vma->detached) {
		vma_end_read(vma);
		count_vm_vma_lock_event(VMA_LOCK_MISS);
		 
		goto retry;
	}

	rcu_read_unlock();
	return vma;

inval_end_read:
	vma_end_read(vma);
inval:
	rcu_read_unlock();
	count_vm_vma_lock_event(VMA_LOCK_ABORT);
	return NULL;
}
#endif  

#ifndef __PAGETABLE_P4D_FOLDED
 
int __p4d_alloc(struct mm_struct *mm, pgd_t *pgd, unsigned long address)
{
	p4d_t *new = p4d_alloc_one(mm, address);
	if (!new)
		return -ENOMEM;

	spin_lock(&mm->page_table_lock);
	if (pgd_present(*pgd)) {	 
		p4d_free(mm, new);
	} else {
		smp_wmb();  
		pgd_populate(mm, pgd, new);
	}
	spin_unlock(&mm->page_table_lock);
	return 0;
}
#endif  

#ifndef __PAGETABLE_PUD_FOLDED
 
int __pud_alloc(struct mm_struct *mm, p4d_t *p4d, unsigned long address)
{
	pud_t *new = pud_alloc_one(mm, address);
	if (!new)
		return -ENOMEM;

	spin_lock(&mm->page_table_lock);
	if (!p4d_present(*p4d)) {
		mm_inc_nr_puds(mm);
		smp_wmb();  
		p4d_populate(mm, p4d, new);
	} else	 
		pud_free(mm, new);
	spin_unlock(&mm->page_table_lock);
	return 0;
}
#endif  

#ifndef __PAGETABLE_PMD_FOLDED
 
int __pmd_alloc(struct mm_struct *mm, pud_t *pud, unsigned long address)
{
	spinlock_t *ptl;
	pmd_t *new = pmd_alloc_one(mm, address);
	if (!new)
		return -ENOMEM;

	ptl = pud_lock(mm, pud);
	if (!pud_present(*pud)) {
		mm_inc_nr_pmds(mm);
		smp_wmb();  
		pud_populate(mm, pud, new);
	} else {	 
		pmd_free(mm, new);
	}
	spin_unlock(ptl);
	return 0;
}
#endif  

 
int follow_pte(struct mm_struct *mm, unsigned long address,
	       pte_t **ptepp, spinlock_t **ptlp)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep;

	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;

	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
		goto out;

	pud = pud_offset(p4d, address);
	if (pud_none(*pud) || unlikely(pud_bad(*pud)))
		goto out;

	pmd = pmd_offset(pud, address);
	VM_BUG_ON(pmd_trans_huge(*pmd));

	ptep = pte_offset_map_lock(mm, pmd, address, ptlp);
	if (!ptep)
		goto out;
	if (!pte_present(ptep_get(ptep)))
		goto unlock;
	*ptepp = ptep;
	return 0;
unlock:
	pte_unmap_unlock(ptep, *ptlp);
out:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(follow_pte);

 
int follow_pfn(struct vm_area_struct *vma, unsigned long address,
	unsigned long *pfn)
{
	int ret = -EINVAL;
	spinlock_t *ptl;
	pte_t *ptep;

	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
		return ret;

	ret = follow_pte(vma->vm_mm, address, &ptep, &ptl);
	if (ret)
		return ret;
	*pfn = pte_pfn(ptep_get(ptep));
	pte_unmap_unlock(ptep, ptl);
	return 0;
}
EXPORT_SYMBOL(follow_pfn);

#ifdef CONFIG_HAVE_IOREMAP_PROT
int follow_phys(struct vm_area_struct *vma,
		unsigned long address, unsigned int flags,
		unsigned long *prot, resource_size_t *phys)
{
	int ret = -EINVAL;
	pte_t *ptep, pte;
	spinlock_t *ptl;

	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
		goto out;

	if (follow_pte(vma->vm_mm, address, &ptep, &ptl))
		goto out;
	pte = ptep_get(ptep);

	if ((flags & FOLL_WRITE) && !pte_write(pte))
		goto unlock;

	*prot = pgprot_val(pte_pgprot(pte));
	*phys = (resource_size_t)pte_pfn(pte) << PAGE_SHIFT;

	ret = 0;
unlock:
	pte_unmap_unlock(ptep, ptl);
out:
	return ret;
}

 
int generic_access_phys(struct vm_area_struct *vma, unsigned long addr,
			void *buf, int len, int write)
{
	resource_size_t phys_addr;
	unsigned long prot = 0;
	void __iomem *maddr;
	pte_t *ptep, pte;
	spinlock_t *ptl;
	int offset = offset_in_page(addr);
	int ret = -EINVAL;

	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
		return -EINVAL;

retry:
	if (follow_pte(vma->vm_mm, addr, &ptep, &ptl))
		return -EINVAL;
	pte = ptep_get(ptep);
	pte_unmap_unlock(ptep, ptl);

	prot = pgprot_val(pte_pgprot(pte));
	phys_addr = (resource_size_t)pte_pfn(pte) << PAGE_SHIFT;

	if ((write & FOLL_WRITE) && !pte_write(pte))
		return -EINVAL;

	maddr = ioremap_prot(phys_addr, PAGE_ALIGN(len + offset), prot);
	if (!maddr)
		return -ENOMEM;

	if (follow_pte(vma->vm_mm, addr, &ptep, &ptl))
		goto out_unmap;

	if (!pte_same(pte, ptep_get(ptep))) {
		pte_unmap_unlock(ptep, ptl);
		iounmap(maddr);

		goto retry;
	}

	if (write)
		memcpy_toio(maddr + offset, buf, len);
	else
		memcpy_fromio(buf, maddr + offset, len);
	ret = len;
	pte_unmap_unlock(ptep, ptl);
out_unmap:
	iounmap(maddr);

	return ret;
}
EXPORT_SYMBOL_GPL(generic_access_phys);
#endif

 
int __access_remote_vm(struct mm_struct *mm, unsigned long addr, void *buf,
		       int len, unsigned int gup_flags)
{
	void *old_buf = buf;
	int write = gup_flags & FOLL_WRITE;

	if (mmap_read_lock_killable(mm))
		return 0;

	 
	addr = untagged_addr_remote(mm, addr);

	 
	if (!vma_lookup(mm, addr) && !expand_stack(mm, addr))
		return 0;

	 
	while (len) {
		int bytes, offset;
		void *maddr;
		struct vm_area_struct *vma = NULL;
		struct page *page = get_user_page_vma_remote(mm, addr,
							     gup_flags, &vma);

		if (IS_ERR_OR_NULL(page)) {
			 
			vma = vma_lookup(mm, addr);
			if (!vma) {
				vma = expand_stack(mm, addr);

				 
				if (!vma)
					return buf - old_buf;

				 
				continue;
			}


			 
			bytes = 0;
#ifdef CONFIG_HAVE_IOREMAP_PROT
			if (vma->vm_ops && vma->vm_ops->access)
				bytes = vma->vm_ops->access(vma, addr, buf,
							    len, write);
#endif
			if (bytes <= 0)
				break;
		} else {
			bytes = len;
			offset = addr & (PAGE_SIZE-1);
			if (bytes > PAGE_SIZE-offset)
				bytes = PAGE_SIZE-offset;

			maddr = kmap(page);
			if (write) {
				copy_to_user_page(vma, page, addr,
						  maddr + offset, buf, bytes);
				set_page_dirty_lock(page);
			} else {
				copy_from_user_page(vma, page, addr,
						    buf, maddr + offset, bytes);
			}
			kunmap(page);
			put_page(page);
		}
		len -= bytes;
		buf += bytes;
		addr += bytes;
	}
	mmap_read_unlock(mm);

	return buf - old_buf;
}

 
int access_remote_vm(struct mm_struct *mm, unsigned long addr,
		void *buf, int len, unsigned int gup_flags)
{
	return __access_remote_vm(mm, addr, buf, len, gup_flags);
}

 
int access_process_vm(struct task_struct *tsk, unsigned long addr,
		void *buf, int len, unsigned int gup_flags)
{
	struct mm_struct *mm;
	int ret;

	mm = get_task_mm(tsk);
	if (!mm)
		return 0;

	ret = __access_remote_vm(mm, addr, buf, len, gup_flags);

	mmput(mm);

	return ret;
}
EXPORT_SYMBOL_GPL(access_process_vm);

 
void print_vma_addr(char *prefix, unsigned long ip)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;

	 
	if (!mmap_read_trylock(mm))
		return;

	vma = find_vma(mm, ip);
	if (vma && vma->vm_file) {
		struct file *f = vma->vm_file;
		char *buf = (char *)__get_free_page(GFP_NOWAIT);
		if (buf) {
			char *p;

			p = file_path(f, buf, PAGE_SIZE);
			if (IS_ERR(p))
				p = "?";
			printk("%s%s[%lx+%lx]", prefix, kbasename(p),
					vma->vm_start,
					vma->vm_end - vma->vm_start);
			free_page((unsigned long)buf);
		}
	}
	mmap_read_unlock(mm);
}

#if defined(CONFIG_PROVE_LOCKING) || defined(CONFIG_DEBUG_ATOMIC_SLEEP)
void __might_fault(const char *file, int line)
{
	if (pagefault_disabled())
		return;
	__might_sleep(file, line);
#if defined(CONFIG_DEBUG_ATOMIC_SLEEP)
	if (current->mm)
		might_lock_read(&current->mm->mmap_lock);
#endif
}
EXPORT_SYMBOL(__might_fault);
#endif

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || defined(CONFIG_HUGETLBFS)
 
static inline int process_huge_page(
	unsigned long addr_hint, unsigned int pages_per_huge_page,
	int (*process_subpage)(unsigned long addr, int idx, void *arg),
	void *arg)
{
	int i, n, base, l, ret;
	unsigned long addr = addr_hint &
		~(((unsigned long)pages_per_huge_page << PAGE_SHIFT) - 1);

	 
	might_sleep();
	n = (addr_hint - addr) / PAGE_SIZE;
	if (2 * n <= pages_per_huge_page) {
		 
		base = 0;
		l = n;
		 
		for (i = pages_per_huge_page - 1; i >= 2 * n; i--) {
			cond_resched();
			ret = process_subpage(addr + i * PAGE_SIZE, i, arg);
			if (ret)
				return ret;
		}
	} else {
		 
		base = pages_per_huge_page - 2 * (pages_per_huge_page - n);
		l = pages_per_huge_page - n;
		 
		for (i = 0; i < base; i++) {
			cond_resched();
			ret = process_subpage(addr + i * PAGE_SIZE, i, arg);
			if (ret)
				return ret;
		}
	}
	 
	for (i = 0; i < l; i++) {
		int left_idx = base + i;
		int right_idx = base + 2 * l - 1 - i;

		cond_resched();
		ret = process_subpage(addr + left_idx * PAGE_SIZE, left_idx, arg);
		if (ret)
			return ret;
		cond_resched();
		ret = process_subpage(addr + right_idx * PAGE_SIZE, right_idx, arg);
		if (ret)
			return ret;
	}
	return 0;
}

static void clear_gigantic_page(struct page *page,
				unsigned long addr,
				unsigned int pages_per_huge_page)
{
	int i;
	struct page *p;

	might_sleep();
	for (i = 0; i < pages_per_huge_page; i++) {
		p = nth_page(page, i);
		cond_resched();
		clear_user_highpage(p, addr + i * PAGE_SIZE);
	}
}

static int clear_subpage(unsigned long addr, int idx, void *arg)
{
	struct page *page = arg;

	clear_user_highpage(page + idx, addr);
	return 0;
}

void clear_huge_page(struct page *page,
		     unsigned long addr_hint, unsigned int pages_per_huge_page)
{
	unsigned long addr = addr_hint &
		~(((unsigned long)pages_per_huge_page << PAGE_SHIFT) - 1);

	if (unlikely(pages_per_huge_page > MAX_ORDER_NR_PAGES)) {
		clear_gigantic_page(page, addr, pages_per_huge_page);
		return;
	}

	process_huge_page(addr_hint, pages_per_huge_page, clear_subpage, page);
}

static int copy_user_gigantic_page(struct folio *dst, struct folio *src,
				     unsigned long addr,
				     struct vm_area_struct *vma,
				     unsigned int pages_per_huge_page)
{
	int i;
	struct page *dst_page;
	struct page *src_page;

	for (i = 0; i < pages_per_huge_page; i++) {
		dst_page = folio_page(dst, i);
		src_page = folio_page(src, i);

		cond_resched();
		if (copy_mc_user_highpage(dst_page, src_page,
					  addr + i*PAGE_SIZE, vma)) {
			memory_failure_queue(page_to_pfn(src_page), 0);
			return -EHWPOISON;
		}
	}
	return 0;
}

struct copy_subpage_arg {
	struct page *dst;
	struct page *src;
	struct vm_area_struct *vma;
};

static int copy_subpage(unsigned long addr, int idx, void *arg)
{
	struct copy_subpage_arg *copy_arg = arg;

	if (copy_mc_user_highpage(copy_arg->dst + idx, copy_arg->src + idx,
				  addr, copy_arg->vma)) {
		memory_failure_queue(page_to_pfn(copy_arg->src + idx), 0);
		return -EHWPOISON;
	}
	return 0;
}

int copy_user_large_folio(struct folio *dst, struct folio *src,
			  unsigned long addr_hint, struct vm_area_struct *vma)
{
	unsigned int pages_per_huge_page = folio_nr_pages(dst);
	unsigned long addr = addr_hint &
		~(((unsigned long)pages_per_huge_page << PAGE_SHIFT) - 1);
	struct copy_subpage_arg arg = {
		.dst = &dst->page,
		.src = &src->page,
		.vma = vma,
	};

	if (unlikely(pages_per_huge_page > MAX_ORDER_NR_PAGES))
		return copy_user_gigantic_page(dst, src, addr, vma,
					       pages_per_huge_page);

	return process_huge_page(addr_hint, pages_per_huge_page, copy_subpage, &arg);
}

long copy_folio_from_user(struct folio *dst_folio,
			   const void __user *usr_src,
			   bool allow_pagefault)
{
	void *kaddr;
	unsigned long i, rc = 0;
	unsigned int nr_pages = folio_nr_pages(dst_folio);
	unsigned long ret_val = nr_pages * PAGE_SIZE;
	struct page *subpage;

	for (i = 0; i < nr_pages; i++) {
		subpage = folio_page(dst_folio, i);
		kaddr = kmap_local_page(subpage);
		if (!allow_pagefault)
			pagefault_disable();
		rc = copy_from_user(kaddr, usr_src + i * PAGE_SIZE, PAGE_SIZE);
		if (!allow_pagefault)
			pagefault_enable();
		kunmap_local(kaddr);

		ret_val -= (PAGE_SIZE - rc);
		if (rc)
			break;

		flush_dcache_page(subpage);

		cond_resched();
	}
	return ret_val;
}
#endif  

#if USE_SPLIT_PTE_PTLOCKS && ALLOC_SPLIT_PTLOCKS

static struct kmem_cache *page_ptl_cachep;

void __init ptlock_cache_init(void)
{
	page_ptl_cachep = kmem_cache_create("page->ptl", sizeof(spinlock_t), 0,
			SLAB_PANIC, NULL);
}

bool ptlock_alloc(struct ptdesc *ptdesc)
{
	spinlock_t *ptl;

	ptl = kmem_cache_alloc(page_ptl_cachep, GFP_KERNEL);
	if (!ptl)
		return false;
	ptdesc->ptl = ptl;
	return true;
}

void ptlock_free(struct ptdesc *ptdesc)
{
	kmem_cache_free(page_ptl_cachep, ptdesc->ptl);
}
#endif
