
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/export.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/file.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/backing-dev.h>
#include <linux/compiler.h>
#include <linux/mount.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/audit.h>
#include <linux/printk.h>

#include <linux/uaccess.h>
#include <linux/uio.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include "internal.h"

void *high_memory;
EXPORT_SYMBOL(high_memory);
struct page *mem_map;
unsigned long max_mapnr;
EXPORT_SYMBOL(max_mapnr);
unsigned long highest_memmap_pfn;
int sysctl_nr_trim_pages = CONFIG_NOMMU_INITIAL_TRIM_EXCESS;
int heap_stack_gap = 0;

atomic_long_t mmap_pages_allocated;

EXPORT_SYMBOL(mem_map);

 
static struct kmem_cache *vm_region_jar;
struct rb_root nommu_region_tree = RB_ROOT;
DECLARE_RWSEM(nommu_region_sem);

const struct vm_operations_struct generic_file_vm_ops = {
};

 
unsigned int kobjsize(const void *objp)
{
	struct page *page;

	 
	if (!objp || !virt_addr_valid(objp))
		return 0;

	page = virt_to_head_page(objp);

	 
	if (PageSlab(page))
		return ksize(objp);

	 
	if (!PageCompound(page)) {
		struct vm_area_struct *vma;

		vma = find_vma(current->mm, (unsigned long)objp);
		if (vma)
			return vma->vm_end - vma->vm_start;
	}

	 
	return page_size(page);
}

 
int follow_pfn(struct vm_area_struct *vma, unsigned long address,
	unsigned long *pfn)
{
	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP)))
		return -EINVAL;

	*pfn = address >> PAGE_SHIFT;
	return 0;
}
EXPORT_SYMBOL(follow_pfn);

LIST_HEAD(vmap_area_list);

void vfree(const void *addr)
{
	kfree(addr);
}
EXPORT_SYMBOL(vfree);

void *__vmalloc(unsigned long size, gfp_t gfp_mask)
{
	 
	return kmalloc(size, (gfp_mask | __GFP_COMP) & ~__GFP_HIGHMEM);
}
EXPORT_SYMBOL(__vmalloc);

void *__vmalloc_node_range(unsigned long size, unsigned long align,
		unsigned long start, unsigned long end, gfp_t gfp_mask,
		pgprot_t prot, unsigned long vm_flags, int node,
		const void *caller)
{
	return __vmalloc(size, gfp_mask);
}

void *__vmalloc_node(unsigned long size, unsigned long align, gfp_t gfp_mask,
		int node, const void *caller)
{
	return __vmalloc(size, gfp_mask);
}

static void *__vmalloc_user_flags(unsigned long size, gfp_t flags)
{
	void *ret;

	ret = __vmalloc(size, flags);
	if (ret) {
		struct vm_area_struct *vma;

		mmap_write_lock(current->mm);
		vma = find_vma(current->mm, (unsigned long)ret);
		if (vma)
			vm_flags_set(vma, VM_USERMAP);
		mmap_write_unlock(current->mm);
	}

	return ret;
}

void *vmalloc_user(unsigned long size)
{
	return __vmalloc_user_flags(size, GFP_KERNEL | __GFP_ZERO);
}
EXPORT_SYMBOL(vmalloc_user);

struct page *vmalloc_to_page(const void *addr)
{
	return virt_to_page(addr);
}
EXPORT_SYMBOL(vmalloc_to_page);

unsigned long vmalloc_to_pfn(const void *addr)
{
	return page_to_pfn(virt_to_page(addr));
}
EXPORT_SYMBOL(vmalloc_to_pfn);

long vread_iter(struct iov_iter *iter, const char *addr, size_t count)
{
	 
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;

	return copy_to_iter(addr, count, iter);
}

 
void *vmalloc(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(vmalloc);

void *vmalloc_huge(unsigned long size, gfp_t gfp_mask) __weak __alias(__vmalloc);

 
void *vzalloc(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL | __GFP_ZERO);
}
EXPORT_SYMBOL(vzalloc);

 
void *vmalloc_node(unsigned long size, int node)
{
	return vmalloc(size);
}
EXPORT_SYMBOL(vmalloc_node);

 
void *vzalloc_node(unsigned long size, int node)
{
	return vzalloc(size);
}
EXPORT_SYMBOL(vzalloc_node);

 
void *vmalloc_32(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL);
}
EXPORT_SYMBOL(vmalloc_32);

 
void *vmalloc_32_user(unsigned long size)
{
	 
	return vmalloc_user(size);
}
EXPORT_SYMBOL(vmalloc_32_user);

void *vmap(struct page **pages, unsigned int count, unsigned long flags, pgprot_t prot)
{
	BUG();
	return NULL;
}
EXPORT_SYMBOL(vmap);

void vunmap(const void *addr)
{
	BUG();
}
EXPORT_SYMBOL(vunmap);

void *vm_map_ram(struct page **pages, unsigned int count, int node)
{
	BUG();
	return NULL;
}
EXPORT_SYMBOL(vm_map_ram);

void vm_unmap_ram(const void *mem, unsigned int count)
{
	BUG();
}
EXPORT_SYMBOL(vm_unmap_ram);

void vm_unmap_aliases(void)
{
}
EXPORT_SYMBOL_GPL(vm_unmap_aliases);

void free_vm_area(struct vm_struct *area)
{
	BUG();
}
EXPORT_SYMBOL_GPL(free_vm_area);

int vm_insert_page(struct vm_area_struct *vma, unsigned long addr,
		   struct page *page)
{
	return -EINVAL;
}
EXPORT_SYMBOL(vm_insert_page);

int vm_map_pages(struct vm_area_struct *vma, struct page **pages,
			unsigned long num)
{
	return -EINVAL;
}
EXPORT_SYMBOL(vm_map_pages);

int vm_map_pages_zero(struct vm_area_struct *vma, struct page **pages,
				unsigned long num)
{
	return -EINVAL;
}
EXPORT_SYMBOL(vm_map_pages_zero);

 
SYSCALL_DEFINE1(brk, unsigned long, brk)
{
	struct mm_struct *mm = current->mm;

	if (brk < mm->start_brk || brk > mm->context.end_brk)
		return mm->brk;

	if (mm->brk == brk)
		return mm->brk;

	 
	if (brk <= mm->brk) {
		mm->brk = brk;
		return brk;
	}

	 
	flush_icache_user_range(mm->brk, brk);
	return mm->brk = brk;
}

 
void __init mmap_init(void)
{
	int ret;

	ret = percpu_counter_init(&vm_committed_as, 0, GFP_KERNEL);
	VM_BUG_ON(ret);
	vm_region_jar = KMEM_CACHE(vm_region, SLAB_PANIC|SLAB_ACCOUNT);
}

 
#ifdef CONFIG_DEBUG_NOMMU_REGIONS
static noinline void validate_nommu_regions(void)
{
	struct vm_region *region, *last;
	struct rb_node *p, *lastp;

	lastp = rb_first(&nommu_region_tree);
	if (!lastp)
		return;

	last = rb_entry(lastp, struct vm_region, vm_rb);
	BUG_ON(last->vm_end <= last->vm_start);
	BUG_ON(last->vm_top < last->vm_end);

	while ((p = rb_next(lastp))) {
		region = rb_entry(p, struct vm_region, vm_rb);
		last = rb_entry(lastp, struct vm_region, vm_rb);

		BUG_ON(region->vm_end <= region->vm_start);
		BUG_ON(region->vm_top < region->vm_end);
		BUG_ON(region->vm_start < last->vm_top);

		lastp = p;
	}
}
#else
static void validate_nommu_regions(void)
{
}
#endif

 
static void add_nommu_region(struct vm_region *region)
{
	struct vm_region *pregion;
	struct rb_node **p, *parent;

	validate_nommu_regions();

	parent = NULL;
	p = &nommu_region_tree.rb_node;
	while (*p) {
		parent = *p;
		pregion = rb_entry(parent, struct vm_region, vm_rb);
		if (region->vm_start < pregion->vm_start)
			p = &(*p)->rb_left;
		else if (region->vm_start > pregion->vm_start)
			p = &(*p)->rb_right;
		else if (pregion == region)
			return;
		else
			BUG();
	}

	rb_link_node(&region->vm_rb, parent, p);
	rb_insert_color(&region->vm_rb, &nommu_region_tree);

	validate_nommu_regions();
}

 
static void delete_nommu_region(struct vm_region *region)
{
	BUG_ON(!nommu_region_tree.rb_node);

	validate_nommu_regions();
	rb_erase(&region->vm_rb, &nommu_region_tree);
	validate_nommu_regions();
}

 
static void free_page_series(unsigned long from, unsigned long to)
{
	for (; from < to; from += PAGE_SIZE) {
		struct page *page = virt_to_page((void *)from);

		atomic_long_dec(&mmap_pages_allocated);
		put_page(page);
	}
}

 
static void __put_nommu_region(struct vm_region *region)
	__releases(nommu_region_sem)
{
	BUG_ON(!nommu_region_tree.rb_node);

	if (--region->vm_usage == 0) {
		if (region->vm_top > region->vm_start)
			delete_nommu_region(region);
		up_write(&nommu_region_sem);

		if (region->vm_file)
			fput(region->vm_file);

		 
		if (region->vm_flags & VM_MAPPED_COPY)
			free_page_series(region->vm_start, region->vm_top);
		kmem_cache_free(vm_region_jar, region);
	} else {
		up_write(&nommu_region_sem);
	}
}

 
static void put_nommu_region(struct vm_region *region)
{
	down_write(&nommu_region_sem);
	__put_nommu_region(region);
}

static void setup_vma_to_mm(struct vm_area_struct *vma, struct mm_struct *mm)
{
	vma->vm_mm = mm;

	 
	if (vma->vm_file) {
		struct address_space *mapping = vma->vm_file->f_mapping;

		i_mmap_lock_write(mapping);
		flush_dcache_mmap_lock(mapping);
		vma_interval_tree_insert(vma, &mapping->i_mmap);
		flush_dcache_mmap_unlock(mapping);
		i_mmap_unlock_write(mapping);
	}
}

static void cleanup_vma_from_mm(struct vm_area_struct *vma)
{
	vma->vm_mm->map_count--;
	 
	if (vma->vm_file) {
		struct address_space *mapping;
		mapping = vma->vm_file->f_mapping;

		i_mmap_lock_write(mapping);
		flush_dcache_mmap_lock(mapping);
		vma_interval_tree_remove(vma, &mapping->i_mmap);
		flush_dcache_mmap_unlock(mapping);
		i_mmap_unlock_write(mapping);
	}
}

 
static int delete_vma_from_mm(struct vm_area_struct *vma)
{
	VMA_ITERATOR(vmi, vma->vm_mm, vma->vm_start);

	vma_iter_config(&vmi, vma->vm_start, vma->vm_end);
	if (vma_iter_prealloc(&vmi, vma)) {
		pr_warn("Allocation of vma tree for process %d failed\n",
		       current->pid);
		return -ENOMEM;
	}
	cleanup_vma_from_mm(vma);

	 
	vma_iter_clear(&vmi);
	return 0;
}
 
static void delete_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
	if (vma->vm_ops && vma->vm_ops->close)
		vma->vm_ops->close(vma);
	if (vma->vm_file)
		fput(vma->vm_file);
	put_nommu_region(vma->vm_region);
	vm_area_free(vma);
}

struct vm_area_struct *find_vma_intersection(struct mm_struct *mm,
					     unsigned long start_addr,
					     unsigned long end_addr)
{
	unsigned long index = start_addr;

	mmap_assert_locked(mm);
	return mt_find(&mm->mm_mt, &index, end_addr - 1);
}
EXPORT_SYMBOL(find_vma_intersection);

 
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
	VMA_ITERATOR(vmi, mm, addr);

	return vma_iter_load(&vmi);
}
EXPORT_SYMBOL(find_vma);

 
struct vm_area_struct *lock_mm_and_find_vma(struct mm_struct *mm,
			unsigned long addr, struct pt_regs *regs)
{
	struct vm_area_struct *vma;

	mmap_read_lock(mm);
	vma = vma_lookup(mm, addr);
	if (!vma)
		mmap_read_unlock(mm);
	return vma;
}

 
int expand_stack_locked(struct vm_area_struct *vma, unsigned long addr)
{
	return -ENOMEM;
}

struct vm_area_struct *expand_stack(struct mm_struct *mm, unsigned long addr)
{
	mmap_read_unlock(mm);
	return NULL;
}

 
static struct vm_area_struct *find_vma_exact(struct mm_struct *mm,
					     unsigned long addr,
					     unsigned long len)
{
	struct vm_area_struct *vma;
	unsigned long end = addr + len;
	VMA_ITERATOR(vmi, mm, addr);

	vma = vma_iter_load(&vmi);
	if (!vma)
		return NULL;
	if (vma->vm_start != addr)
		return NULL;
	if (vma->vm_end != end)
		return NULL;

	return vma;
}

 
static int validate_mmap_request(struct file *file,
				 unsigned long addr,
				 unsigned long len,
				 unsigned long prot,
				 unsigned long flags,
				 unsigned long pgoff,
				 unsigned long *_capabilities)
{
	unsigned long capabilities, rlen;
	int ret;

	 
	if (flags & MAP_FIXED)
		return -EINVAL;

	if ((flags & MAP_TYPE) != MAP_PRIVATE &&
	    (flags & MAP_TYPE) != MAP_SHARED)
		return -EINVAL;

	if (!len)
		return -EINVAL;

	 
	rlen = PAGE_ALIGN(len);
	if (!rlen || rlen > TASK_SIZE)
		return -ENOMEM;

	 
	if ((pgoff + (rlen >> PAGE_SHIFT)) < pgoff)
		return -EOVERFLOW;

	if (file) {
		 
		if (!file->f_op->mmap)
			return -ENODEV;

		 
		if (file->f_op->mmap_capabilities) {
			capabilities = file->f_op->mmap_capabilities(file);
		} else {
			 
			switch (file_inode(file)->i_mode & S_IFMT) {
			case S_IFREG:
			case S_IFBLK:
				capabilities = NOMMU_MAP_COPY;
				break;

			case S_IFCHR:
				capabilities =
					NOMMU_MAP_DIRECT |
					NOMMU_MAP_READ |
					NOMMU_MAP_WRITE;
				break;

			default:
				return -EINVAL;
			}
		}

		 
		if (!file->f_op->get_unmapped_area)
			capabilities &= ~NOMMU_MAP_DIRECT;
		if (!(file->f_mode & FMODE_CAN_READ))
			capabilities &= ~NOMMU_MAP_COPY;

		 
		if (!(file->f_mode & FMODE_READ))
			return -EACCES;

		if (flags & MAP_SHARED) {
			 
			if ((prot & PROT_WRITE) &&
			    !(file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (IS_APPEND(file_inode(file)) &&
			    (file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (!(capabilities & NOMMU_MAP_DIRECT))
				return -ENODEV;

			 
			capabilities &= ~NOMMU_MAP_COPY;
		} else {
			 
			if (!(capabilities & NOMMU_MAP_COPY))
				return -ENODEV;

			 
			if (prot & PROT_WRITE)
				capabilities &= ~NOMMU_MAP_DIRECT;
		}

		if (capabilities & NOMMU_MAP_DIRECT) {
			if (((prot & PROT_READ)  && !(capabilities & NOMMU_MAP_READ))  ||
			    ((prot & PROT_WRITE) && !(capabilities & NOMMU_MAP_WRITE)) ||
			    ((prot & PROT_EXEC)  && !(capabilities & NOMMU_MAP_EXEC))
			    ) {
				capabilities &= ~NOMMU_MAP_DIRECT;
				if (flags & MAP_SHARED) {
					pr_warn("MAP_SHARED not completely supported on !MMU\n");
					return -EINVAL;
				}
			}
		}

		 
		if (path_noexec(&file->f_path)) {
			if (prot & PROT_EXEC)
				return -EPERM;
		} else if ((prot & PROT_READ) && !(prot & PROT_EXEC)) {
			 
			if (current->personality & READ_IMPLIES_EXEC) {
				if (capabilities & NOMMU_MAP_EXEC)
					prot |= PROT_EXEC;
			}
		} else if ((prot & PROT_READ) &&
			 (prot & PROT_EXEC) &&
			 !(capabilities & NOMMU_MAP_EXEC)
			 ) {
			 
			capabilities &= ~NOMMU_MAP_DIRECT;
		}
	} else {
		 
		capabilities = NOMMU_MAP_COPY;

		 
		if ((prot & PROT_READ) &&
		    (current->personality & READ_IMPLIES_EXEC))
			prot |= PROT_EXEC;
	}

	 
	ret = security_mmap_addr(addr);
	if (ret < 0)
		return ret;

	 
	*_capabilities = capabilities;
	return 0;
}

 
static unsigned long determine_vm_flags(struct file *file,
					unsigned long prot,
					unsigned long flags,
					unsigned long capabilities)
{
	unsigned long vm_flags;

	vm_flags = calc_vm_prot_bits(prot, 0) | calc_vm_flag_bits(flags);

	if (!file) {
		 
		vm_flags |= VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;
	} else if (flags & MAP_PRIVATE) {
		 
		if (capabilities & NOMMU_MAP_DIRECT)
			vm_flags |= (capabilities & NOMMU_VMFLAGS);
		else
			vm_flags |= VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;

		if (!(prot & PROT_WRITE) && !current->ptrace)
			 
			vm_flags |= VM_MAYOVERLAY;
	} else {
		 
		vm_flags |= VM_SHARED | VM_MAYSHARE |
			    (capabilities & NOMMU_VMFLAGS);
	}

	return vm_flags;
}

 
static int do_mmap_shared_file(struct vm_area_struct *vma)
{
	int ret;

	ret = call_mmap(vma->vm_file, vma);
	if (ret == 0) {
		vma->vm_region->vm_top = vma->vm_region->vm_end;
		return 0;
	}
	if (ret != -ENOSYS)
		return ret;

	 
	return -ENODEV;
}

 
static int do_mmap_private(struct vm_area_struct *vma,
			   struct vm_region *region,
			   unsigned long len,
			   unsigned long capabilities)
{
	unsigned long total, point;
	void *base;
	int ret, order;

	 
	if (capabilities & NOMMU_MAP_DIRECT) {
		ret = call_mmap(vma->vm_file, vma);
		 
		if (WARN_ON_ONCE(!is_nommu_shared_mapping(vma->vm_flags)))
			ret = -ENOSYS;
		if (ret == 0) {
			vma->vm_region->vm_top = vma->vm_region->vm_end;
			return 0;
		}
		if (ret != -ENOSYS)
			return ret;

		 
	}


	 
	order = get_order(len);
	total = 1 << order;
	point = len >> PAGE_SHIFT;

	 
	if (sysctl_nr_trim_pages && total - point >= sysctl_nr_trim_pages)
		total = point;

	base = alloc_pages_exact(total << PAGE_SHIFT, GFP_KERNEL);
	if (!base)
		goto enomem;

	atomic_long_add(total, &mmap_pages_allocated);

	vm_flags_set(vma, VM_MAPPED_COPY);
	region->vm_flags = vma->vm_flags;
	region->vm_start = (unsigned long) base;
	region->vm_end   = region->vm_start + len;
	region->vm_top   = region->vm_start + (total << PAGE_SHIFT);

	vma->vm_start = region->vm_start;
	vma->vm_end   = region->vm_start + len;

	if (vma->vm_file) {
		 
		loff_t fpos;

		fpos = vma->vm_pgoff;
		fpos <<= PAGE_SHIFT;

		ret = kernel_read(vma->vm_file, base, len, &fpos);
		if (ret < 0)
			goto error_free;

		 
		if (ret < len)
			memset(base + ret, 0, len - ret);

	} else {
		vma_set_anonymous(vma);
	}

	return 0;

error_free:
	free_page_series(region->vm_start, region->vm_top);
	region->vm_start = vma->vm_start = 0;
	region->vm_end   = vma->vm_end = 0;
	region->vm_top   = 0;
	return ret;

enomem:
	pr_err("Allocation of length %lu from process %d (%s) failed\n",
	       len, current->pid, current->comm);
	show_mem();
	return -ENOMEM;
}

 
unsigned long do_mmap(struct file *file,
			unsigned long addr,
			unsigned long len,
			unsigned long prot,
			unsigned long flags,
			vm_flags_t vm_flags,
			unsigned long pgoff,
			unsigned long *populate,
			struct list_head *uf)
{
	struct vm_area_struct *vma;
	struct vm_region *region;
	struct rb_node *rb;
	unsigned long capabilities, result;
	int ret;
	VMA_ITERATOR(vmi, current->mm, 0);

	*populate = 0;

	 
	ret = validate_mmap_request(file, addr, len, prot, flags, pgoff,
				    &capabilities);
	if (ret < 0)
		return ret;

	 
	addr = 0;
	len = PAGE_ALIGN(len);

	 
	vm_flags |= determine_vm_flags(file, prot, flags, capabilities);


	 
	region = kmem_cache_zalloc(vm_region_jar, GFP_KERNEL);
	if (!region)
		goto error_getting_region;

	vma = vm_area_alloc(current->mm);
	if (!vma)
		goto error_getting_vma;

	region->vm_usage = 1;
	region->vm_flags = vm_flags;
	region->vm_pgoff = pgoff;

	vm_flags_init(vma, vm_flags);
	vma->vm_pgoff = pgoff;

	if (file) {
		region->vm_file = get_file(file);
		vma->vm_file = get_file(file);
	}

	down_write(&nommu_region_sem);

	 
	if (is_nommu_shared_mapping(vm_flags)) {
		struct vm_region *pregion;
		unsigned long pglen, rpglen, pgend, rpgend, start;

		pglen = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
		pgend = pgoff + pglen;

		for (rb = rb_first(&nommu_region_tree); rb; rb = rb_next(rb)) {
			pregion = rb_entry(rb, struct vm_region, vm_rb);

			if (!is_nommu_shared_mapping(pregion->vm_flags))
				continue;

			 
			if (file_inode(pregion->vm_file) !=
			    file_inode(file))
				continue;

			if (pregion->vm_pgoff >= pgend)
				continue;

			rpglen = pregion->vm_end - pregion->vm_start;
			rpglen = (rpglen + PAGE_SIZE - 1) >> PAGE_SHIFT;
			rpgend = pregion->vm_pgoff + rpglen;
			if (pgoff >= rpgend)
				continue;

			 
			if ((pregion->vm_pgoff != pgoff || rpglen != pglen) &&
			    !(pgoff >= pregion->vm_pgoff && pgend <= rpgend)) {
				 
				if (!(capabilities & NOMMU_MAP_DIRECT))
					goto sharing_violation;
				continue;
			}

			 
			pregion->vm_usage++;
			vma->vm_region = pregion;
			start = pregion->vm_start;
			start += (pgoff - pregion->vm_pgoff) << PAGE_SHIFT;
			vma->vm_start = start;
			vma->vm_end = start + len;

			if (pregion->vm_flags & VM_MAPPED_COPY)
				vm_flags_set(vma, VM_MAPPED_COPY);
			else {
				ret = do_mmap_shared_file(vma);
				if (ret < 0) {
					vma->vm_region = NULL;
					vma->vm_start = 0;
					vma->vm_end = 0;
					pregion->vm_usage--;
					pregion = NULL;
					goto error_just_free;
				}
			}
			fput(region->vm_file);
			kmem_cache_free(vm_region_jar, region);
			region = pregion;
			result = start;
			goto share;
		}

		 
		if (capabilities & NOMMU_MAP_DIRECT) {
			addr = file->f_op->get_unmapped_area(file, addr, len,
							     pgoff, flags);
			if (IS_ERR_VALUE(addr)) {
				ret = addr;
				if (ret != -ENOSYS)
					goto error_just_free;

				 
				ret = -ENODEV;
				if (!(capabilities & NOMMU_MAP_COPY))
					goto error_just_free;

				capabilities &= ~NOMMU_MAP_DIRECT;
			} else {
				vma->vm_start = region->vm_start = addr;
				vma->vm_end = region->vm_end = addr + len;
			}
		}
	}

	vma->vm_region = region;

	 
	if (file && vma->vm_flags & VM_SHARED)
		ret = do_mmap_shared_file(vma);
	else
		ret = do_mmap_private(vma, region, len, capabilities);
	if (ret < 0)
		goto error_just_free;
	add_nommu_region(region);

	 
	if (!vma->vm_file &&
	    (!IS_ENABLED(CONFIG_MMAP_ALLOW_UNINITIALIZED) ||
	     !(flags & MAP_UNINITIALIZED)))
		memset((void *)region->vm_start, 0,
		       region->vm_end - region->vm_start);

	 
	result = vma->vm_start;

	current->mm->total_vm += len >> PAGE_SHIFT;

share:
	BUG_ON(!vma->vm_region);
	vma_iter_config(&vmi, vma->vm_start, vma->vm_end);
	if (vma_iter_prealloc(&vmi, vma))
		goto error_just_free;

	setup_vma_to_mm(vma, current->mm);
	current->mm->map_count++;
	 
	vma_iter_store(&vmi, vma);

	 
	if (vma->vm_flags & VM_EXEC && !region->vm_icache_flushed) {
		flush_icache_user_range(region->vm_start, region->vm_end);
		region->vm_icache_flushed = true;
	}

	up_write(&nommu_region_sem);

	return result;

error_just_free:
	up_write(&nommu_region_sem);
error:
	vma_iter_free(&vmi);
	if (region->vm_file)
		fput(region->vm_file);
	kmem_cache_free(vm_region_jar, region);
	if (vma->vm_file)
		fput(vma->vm_file);
	vm_area_free(vma);
	return ret;

sharing_violation:
	up_write(&nommu_region_sem);
	pr_warn("Attempt to share mismatched mappings\n");
	ret = -EINVAL;
	goto error;

error_getting_vma:
	kmem_cache_free(vm_region_jar, region);
	pr_warn("Allocation of vma for %lu byte allocation from process %d failed\n",
			len, current->pid);
	show_mem();
	return -ENOMEM;

error_getting_region:
	pr_warn("Allocation of vm region for %lu byte allocation from process %d failed\n",
			len, current->pid);
	show_mem();
	return -ENOMEM;
}

unsigned long ksys_mmap_pgoff(unsigned long addr, unsigned long len,
			      unsigned long prot, unsigned long flags,
			      unsigned long fd, unsigned long pgoff)
{
	struct file *file = NULL;
	unsigned long retval = -EBADF;

	audit_mmap_fd(fd, flags);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	retval = vm_mmap_pgoff(file, addr, len, prot, flags, pgoff);

	if (file)
		fput(file);
out:
	return retval;
}

SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, pgoff)
{
	return ksys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}

#ifdef __ARCH_WANT_SYS_OLD_MMAP
struct mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

SYSCALL_DEFINE1(old_mmap, struct mmap_arg_struct __user *, arg)
{
	struct mmap_arg_struct a;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;
	if (offset_in_page(a.offset))
		return -EINVAL;

	return ksys_mmap_pgoff(a.addr, a.len, a.prot, a.flags, a.fd,
			       a.offset >> PAGE_SHIFT);
}
#endif  

 
int split_vma(struct vma_iterator *vmi, struct vm_area_struct *vma,
	      unsigned long addr, int new_below)
{
	struct vm_area_struct *new;
	struct vm_region *region;
	unsigned long npages;
	struct mm_struct *mm;

	 
	if (vma->vm_file)
		return -ENOMEM;

	mm = vma->vm_mm;
	if (mm->map_count >= sysctl_max_map_count)
		return -ENOMEM;

	region = kmem_cache_alloc(vm_region_jar, GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	new = vm_area_dup(vma);
	if (!new)
		goto err_vma_dup;

	 
	*region = *vma->vm_region;
	new->vm_region = region;

	npages = (addr - vma->vm_start) >> PAGE_SHIFT;

	if (new_below) {
		region->vm_top = region->vm_end = new->vm_end = addr;
	} else {
		region->vm_start = new->vm_start = addr;
		region->vm_pgoff = new->vm_pgoff += npages;
	}

	vma_iter_config(vmi, new->vm_start, new->vm_end);
	if (vma_iter_prealloc(vmi, vma)) {
		pr_warn("Allocation of vma tree for process %d failed\n",
			current->pid);
		goto err_vmi_preallocate;
	}

	if (new->vm_ops && new->vm_ops->open)
		new->vm_ops->open(new);

	down_write(&nommu_region_sem);
	delete_nommu_region(vma->vm_region);
	if (new_below) {
		vma->vm_region->vm_start = vma->vm_start = addr;
		vma->vm_region->vm_pgoff = vma->vm_pgoff += npages;
	} else {
		vma->vm_region->vm_end = vma->vm_end = addr;
		vma->vm_region->vm_top = addr;
	}
	add_nommu_region(vma->vm_region);
	add_nommu_region(new->vm_region);
	up_write(&nommu_region_sem);

	setup_vma_to_mm(vma, mm);
	setup_vma_to_mm(new, mm);
	vma_iter_store(vmi, new);
	mm->map_count++;
	return 0;

err_vmi_preallocate:
	vm_area_free(new);
err_vma_dup:
	kmem_cache_free(vm_region_jar, region);
	return -ENOMEM;
}

 
static int vmi_shrink_vma(struct vma_iterator *vmi,
		      struct vm_area_struct *vma,
		      unsigned long from, unsigned long to)
{
	struct vm_region *region;

	 
	if (from > vma->vm_start) {
		if (vma_iter_clear_gfp(vmi, from, vma->vm_end, GFP_KERNEL))
			return -ENOMEM;
		vma->vm_end = from;
	} else {
		if (vma_iter_clear_gfp(vmi, vma->vm_start, to, GFP_KERNEL))
			return -ENOMEM;
		vma->vm_start = to;
	}

	 
	region = vma->vm_region;
	BUG_ON(region->vm_usage != 1);

	down_write(&nommu_region_sem);
	delete_nommu_region(region);
	if (from > region->vm_start) {
		to = region->vm_top;
		region->vm_top = region->vm_end = from;
	} else {
		region->vm_start = to;
	}
	add_nommu_region(region);
	up_write(&nommu_region_sem);

	free_page_series(from, to);
	return 0;
}

 
int do_munmap(struct mm_struct *mm, unsigned long start, size_t len, struct list_head *uf)
{
	VMA_ITERATOR(vmi, mm, start);
	struct vm_area_struct *vma;
	unsigned long end;
	int ret = 0;

	len = PAGE_ALIGN(len);
	if (len == 0)
		return -EINVAL;

	end = start + len;

	 
	vma = vma_find(&vmi, end);
	if (!vma) {
		static int limit;
		if (limit < 5) {
			pr_warn("munmap of memory not mmapped by process %d (%s): 0x%lx-0x%lx\n",
					current->pid, current->comm,
					start, start + len - 1);
			limit++;
		}
		return -EINVAL;
	}

	 
	if (vma->vm_file) {
		do {
			if (start > vma->vm_start)
				return -EINVAL;
			if (end == vma->vm_end)
				goto erase_whole_vma;
			vma = vma_find(&vmi, end);
		} while (vma);
		return -EINVAL;
	} else {
		 
		if (start == vma->vm_start && end == vma->vm_end)
			goto erase_whole_vma;
		if (start < vma->vm_start || end > vma->vm_end)
			return -EINVAL;
		if (offset_in_page(start))
			return -EINVAL;
		if (end != vma->vm_end && offset_in_page(end))
			return -EINVAL;
		if (start != vma->vm_start && end != vma->vm_end) {
			ret = split_vma(&vmi, vma, start, 1);
			if (ret < 0)
				return ret;
		}
		return vmi_shrink_vma(&vmi, vma, start, end);
	}

erase_whole_vma:
	if (delete_vma_from_mm(vma))
		ret = -ENOMEM;
	else
		delete_vma(mm, vma);
	return ret;
}

int vm_munmap(unsigned long addr, size_t len)
{
	struct mm_struct *mm = current->mm;
	int ret;

	mmap_write_lock(mm);
	ret = do_munmap(mm, addr, len, NULL);
	mmap_write_unlock(mm);
	return ret;
}
EXPORT_SYMBOL(vm_munmap);

SYSCALL_DEFINE2(munmap, unsigned long, addr, size_t, len)
{
	return vm_munmap(addr, len);
}

 
void exit_mmap(struct mm_struct *mm)
{
	VMA_ITERATOR(vmi, mm, 0);
	struct vm_area_struct *vma;

	if (!mm)
		return;

	mm->total_vm = 0;

	 
	mmap_write_lock(mm);
	for_each_vma(vmi, vma) {
		cleanup_vma_from_mm(vma);
		delete_vma(mm, vma);
		cond_resched();
	}
	__mt_destroy(&mm->mm_mt);
	mmap_write_unlock(mm);
}

int vm_brk(unsigned long addr, unsigned long len)
{
	return -ENOMEM;
}

 
static unsigned long do_mremap(unsigned long addr,
			unsigned long old_len, unsigned long new_len,
			unsigned long flags, unsigned long new_addr)
{
	struct vm_area_struct *vma;

	 
	old_len = PAGE_ALIGN(old_len);
	new_len = PAGE_ALIGN(new_len);
	if (old_len == 0 || new_len == 0)
		return (unsigned long) -EINVAL;

	if (offset_in_page(addr))
		return -EINVAL;

	if (flags & MREMAP_FIXED && new_addr != addr)
		return (unsigned long) -EINVAL;

	vma = find_vma_exact(current->mm, addr, old_len);
	if (!vma)
		return (unsigned long) -EINVAL;

	if (vma->vm_end != vma->vm_start + old_len)
		return (unsigned long) -EFAULT;

	if (is_nommu_shared_mapping(vma->vm_flags))
		return (unsigned long) -EPERM;

	if (new_len > vma->vm_region->vm_end - vma->vm_region->vm_start)
		return (unsigned long) -ENOMEM;

	 
	vma->vm_end = vma->vm_start + new_len;
	return vma->vm_start;
}

SYSCALL_DEFINE5(mremap, unsigned long, addr, unsigned long, old_len,
		unsigned long, new_len, unsigned long, flags,
		unsigned long, new_addr)
{
	unsigned long ret;

	mmap_write_lock(current->mm);
	ret = do_mremap(addr, old_len, new_len, flags, new_addr);
	mmap_write_unlock(current->mm);
	return ret;
}

struct page *follow_page(struct vm_area_struct *vma, unsigned long address,
			 unsigned int foll_flags)
{
	return NULL;
}

int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
		unsigned long pfn, unsigned long size, pgprot_t prot)
{
	if (addr != (pfn << PAGE_SHIFT))
		return -EINVAL;

	vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
	return 0;
}
EXPORT_SYMBOL(remap_pfn_range);

int vm_iomap_memory(struct vm_area_struct *vma, phys_addr_t start, unsigned long len)
{
	unsigned long pfn = start >> PAGE_SHIFT;
	unsigned long vm_len = vma->vm_end - vma->vm_start;

	pfn += vma->vm_pgoff;
	return io_remap_pfn_range(vma, vma->vm_start, pfn, vm_len, vma->vm_page_prot);
}
EXPORT_SYMBOL(vm_iomap_memory);

int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
			unsigned long pgoff)
{
	unsigned int size = vma->vm_end - vma->vm_start;

	if (!(vma->vm_flags & VM_USERMAP))
		return -EINVAL;

	vma->vm_start = (unsigned long)(addr + (pgoff << PAGE_SHIFT));
	vma->vm_end = vma->vm_start + size;

	return 0;
}
EXPORT_SYMBOL(remap_vmalloc_range);

vm_fault_t filemap_fault(struct vm_fault *vmf)
{
	BUG();
	return 0;
}
EXPORT_SYMBOL(filemap_fault);

vm_fault_t filemap_map_pages(struct vm_fault *vmf,
		pgoff_t start_pgoff, pgoff_t end_pgoff)
{
	BUG();
	return 0;
}
EXPORT_SYMBOL(filemap_map_pages);

int __access_remote_vm(struct mm_struct *mm, unsigned long addr, void *buf,
		       int len, unsigned int gup_flags)
{
	struct vm_area_struct *vma;
	int write = gup_flags & FOLL_WRITE;

	if (mmap_read_lock_killable(mm))
		return 0;

	 
	vma = find_vma(mm, addr);
	if (vma) {
		 
		if (addr + len >= vma->vm_end)
			len = vma->vm_end - addr;

		 
		if (write && vma->vm_flags & VM_MAYWRITE)
			copy_to_user_page(vma, NULL, addr,
					 (void *) addr, buf, len);
		else if (!write && vma->vm_flags & VM_MAYREAD)
			copy_from_user_page(vma, NULL, addr,
					    buf, (void *) addr, len);
		else
			len = 0;
	} else {
		len = 0;
	}

	mmap_read_unlock(mm);

	return len;
}

 
int access_remote_vm(struct mm_struct *mm, unsigned long addr,
		void *buf, int len, unsigned int gup_flags)
{
	return __access_remote_vm(mm, addr, buf, len, gup_flags);
}

 
int access_process_vm(struct task_struct *tsk, unsigned long addr, void *buf, int len,
		unsigned int gup_flags)
{
	struct mm_struct *mm;

	if (addr + len < addr)
		return 0;

	mm = get_task_mm(tsk);
	if (!mm)
		return 0;

	len = __access_remote_vm(mm, addr, buf, len, gup_flags);

	mmput(mm);
	return len;
}
EXPORT_SYMBOL_GPL(access_process_vm);

 
int nommu_shrink_inode_mappings(struct inode *inode, size_t size,
				size_t newsize)
{
	struct vm_area_struct *vma;
	struct vm_region *region;
	pgoff_t low, high;
	size_t r_size, r_top;

	low = newsize >> PAGE_SHIFT;
	high = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;

	down_write(&nommu_region_sem);
	i_mmap_lock_read(inode->i_mapping);

	 
	vma_interval_tree_foreach(vma, &inode->i_mapping->i_mmap, low, high) {
		 
		if (vma->vm_flags & VM_SHARED) {
			i_mmap_unlock_read(inode->i_mapping);
			up_write(&nommu_region_sem);
			return -ETXTBSY;  
		}
	}

	 
	vma_interval_tree_foreach(vma, &inode->i_mapping->i_mmap, 0, ULONG_MAX) {
		if (!(vma->vm_flags & VM_SHARED))
			continue;

		region = vma->vm_region;
		r_size = region->vm_top - region->vm_start;
		r_top = (region->vm_pgoff << PAGE_SHIFT) + r_size;

		if (r_top > newsize) {
			region->vm_top -= r_top - newsize;
			if (region->vm_end > region->vm_top)
				region->vm_end = region->vm_top;
		}
	}

	i_mmap_unlock_read(inode->i_mapping);
	up_write(&nommu_region_sem);
	return 0;
}

 
static int __meminit init_user_reserve(void)
{
	unsigned long free_kbytes;

	free_kbytes = K(global_zone_page_state(NR_FREE_PAGES));

	sysctl_user_reserve_kbytes = min(free_kbytes / 32, 1UL << 17);
	return 0;
}
subsys_initcall(init_user_reserve);

 
static int __meminit init_admin_reserve(void)
{
	unsigned long free_kbytes;

	free_kbytes = K(global_zone_page_state(NR_FREE_PAGES));

	sysctl_admin_reserve_kbytes = min(free_kbytes / 32, 1UL << 13);
	return 0;
}
subsys_initcall(init_admin_reserve);
