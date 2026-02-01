
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/btf.h>
#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kexec.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/ioport.h>
#include <linux/hardirq.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/utsname.h>
#include <linux/numa.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/panic_notifier.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/console.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <linux/syscore_ops.h>
#include <linux/compiler.h>
#include <linux/hugetlb.h>
#include <linux/objtool.h>
#include <linux/kmsg_dump.h>

#include <asm/page.h>
#include <asm/sections.h>

#include <crypto/hash.h>
#include "kexec_internal.h"

atomic_t __kexec_lock = ATOMIC_INIT(0);

 
bool kexec_in_progress = false;


 
struct resource crashk_res = {
	.name  = "Crash kernel",
	.start = 0,
	.end   = 0,
	.flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM,
	.desc  = IORES_DESC_CRASH_KERNEL
};
struct resource crashk_low_res = {
	.name  = "Crash kernel",
	.start = 0,
	.end   = 0,
	.flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM,
	.desc  = IORES_DESC_CRASH_KERNEL
};

int kexec_should_crash(struct task_struct *p)
{
	 
	if (crash_kexec_post_notifiers)
		return 0;
	 
	if (in_interrupt() || !p->pid || is_global_init(p) || panic_on_oops)
		return 1;
	return 0;
}

int kexec_crash_loaded(void)
{
	return !!kexec_crash_image;
}
EXPORT_SYMBOL_GPL(kexec_crash_loaded);

 

 
#define KIMAGE_NO_DEST (-1UL)
#define PAGE_COUNT(x) (((x) + PAGE_SIZE - 1) >> PAGE_SHIFT)

static struct page *kimage_alloc_page(struct kimage *image,
				       gfp_t gfp_mask,
				       unsigned long dest);

int sanity_check_segment_list(struct kimage *image)
{
	int i;
	unsigned long nr_segments = image->nr_segments;
	unsigned long total_pages = 0;
	unsigned long nr_pages = totalram_pages();

	 
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		if (mstart > mend)
			return -EADDRNOTAVAIL;
		if ((mstart & ~PAGE_MASK) || (mend & ~PAGE_MASK))
			return -EADDRNOTAVAIL;
		if (mend >= KEXEC_DESTINATION_MEMORY_LIMIT)
			return -EADDRNOTAVAIL;
	}

	 
	for (i = 0; i < nr_segments; i++) {
		unsigned long mstart, mend;
		unsigned long j;

		mstart = image->segment[i].mem;
		mend   = mstart + image->segment[i].memsz;
		for (j = 0; j < i; j++) {
			unsigned long pstart, pend;

			pstart = image->segment[j].mem;
			pend   = pstart + image->segment[j].memsz;
			 
			if ((mend > pstart) && (mstart < pend))
				return -EINVAL;
		}
	}

	 
	for (i = 0; i < nr_segments; i++) {
		if (image->segment[i].bufsz > image->segment[i].memsz)
			return -EINVAL;
	}

	 
	for (i = 0; i < nr_segments; i++) {
		if (PAGE_COUNT(image->segment[i].memsz) > nr_pages / 2)
			return -EINVAL;

		total_pages += PAGE_COUNT(image->segment[i].memsz);
	}

	if (total_pages > nr_pages / 2)
		return -EINVAL;

	 

	if (image->type == KEXEC_TYPE_CRASH) {
		for (i = 0; i < nr_segments; i++) {
			unsigned long mstart, mend;

			mstart = image->segment[i].mem;
			mend = mstart + image->segment[i].memsz - 1;
			 
			if ((mstart < phys_to_boot_phys(crashk_res.start)) ||
			    (mend > phys_to_boot_phys(crashk_res.end)))
				return -EADDRNOTAVAIL;
		}
	}

	return 0;
}

struct kimage *do_kimage_alloc_init(void)
{
	struct kimage *image;

	 
	image = kzalloc(sizeof(*image), GFP_KERNEL);
	if (!image)
		return NULL;

	image->head = 0;
	image->entry = &image->head;
	image->last_entry = &image->head;
	image->control_page = ~0;  
	image->type = KEXEC_TYPE_DEFAULT;

	 
	INIT_LIST_HEAD(&image->control_pages);

	 
	INIT_LIST_HEAD(&image->dest_pages);

	 
	INIT_LIST_HEAD(&image->unusable_pages);

#ifdef CONFIG_CRASH_HOTPLUG
	image->hp_action = KEXEC_CRASH_HP_NONE;
	image->elfcorehdr_index = -1;
	image->elfcorehdr_updated = false;
#endif

	return image;
}

int kimage_is_destination_range(struct kimage *image,
					unsigned long start,
					unsigned long end)
{
	unsigned long i;

	for (i = 0; i < image->nr_segments; i++) {
		unsigned long mstart, mend;

		mstart = image->segment[i].mem;
		mend = mstart + image->segment[i].memsz;
		if ((end > mstart) && (start < mend))
			return 1;
	}

	return 0;
}

static struct page *kimage_alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *pages;

	if (fatal_signal_pending(current))
		return NULL;
	pages = alloc_pages(gfp_mask & ~__GFP_ZERO, order);
	if (pages) {
		unsigned int count, i;

		pages->mapping = NULL;
		set_page_private(pages, order);
		count = 1 << order;
		for (i = 0; i < count; i++)
			SetPageReserved(pages + i);

		arch_kexec_post_alloc_pages(page_address(pages), count,
					    gfp_mask);

		if (gfp_mask & __GFP_ZERO)
			for (i = 0; i < count; i++)
				clear_highpage(pages + i);
	}

	return pages;
}

static void kimage_free_pages(struct page *page)
{
	unsigned int order, count, i;

	order = page_private(page);
	count = 1 << order;

	arch_kexec_pre_free_pages(page_address(page), count);

	for (i = 0; i < count; i++)
		ClearPageReserved(page + i);
	__free_pages(page, order);
}

void kimage_free_page_list(struct list_head *list)
{
	struct page *page, *next;

	list_for_each_entry_safe(page, next, list, lru) {
		list_del(&page->lru);
		kimage_free_pages(page);
	}
}

static struct page *kimage_alloc_normal_control_pages(struct kimage *image,
							unsigned int order)
{
	 
	struct list_head extra_pages;
	struct page *pages;
	unsigned int count;

	count = 1 << order;
	INIT_LIST_HEAD(&extra_pages);

	 
	do {
		unsigned long pfn, epfn, addr, eaddr;

		pages = kimage_alloc_pages(KEXEC_CONTROL_MEMORY_GFP, order);
		if (!pages)
			break;
		pfn   = page_to_boot_pfn(pages);
		epfn  = pfn + count;
		addr  = pfn << PAGE_SHIFT;
		eaddr = epfn << PAGE_SHIFT;
		if ((epfn >= (KEXEC_CONTROL_MEMORY_LIMIT >> PAGE_SHIFT)) ||
			      kimage_is_destination_range(image, addr, eaddr)) {
			list_add(&pages->lru, &extra_pages);
			pages = NULL;
		}
	} while (!pages);

	if (pages) {
		 
		list_add(&pages->lru, &image->control_pages);

		 
	}
	 
	kimage_free_page_list(&extra_pages);

	return pages;
}

static struct page *kimage_alloc_crash_control_pages(struct kimage *image,
						      unsigned int order)
{
	 
	unsigned long hole_start, hole_end, size;
	struct page *pages;

	pages = NULL;
	size = (1 << order) << PAGE_SHIFT;
	hole_start = (image->control_page + (size - 1)) & ~(size - 1);
	hole_end   = hole_start + size - 1;
	while (hole_end <= crashk_res.end) {
		unsigned long i;

		cond_resched();

		if (hole_end > KEXEC_CRASH_CONTROL_MEMORY_LIMIT)
			break;
		 
		for (i = 0; i < image->nr_segments; i++) {
			unsigned long mstart, mend;

			mstart = image->segment[i].mem;
			mend   = mstart + image->segment[i].memsz - 1;
			if ((hole_end >= mstart) && (hole_start <= mend)) {
				 
				hole_start = (mend + (size - 1)) & ~(size - 1);
				hole_end   = hole_start + size - 1;
				break;
			}
		}
		 
		if (i == image->nr_segments) {
			pages = pfn_to_page(hole_start >> PAGE_SHIFT);
			image->control_page = hole_end;
			break;
		}
	}

	 
	if (pages)
		arch_kexec_post_alloc_pages(page_address(pages), 1 << order, 0);

	return pages;
}


struct page *kimage_alloc_control_pages(struct kimage *image,
					 unsigned int order)
{
	struct page *pages = NULL;

	switch (image->type) {
	case KEXEC_TYPE_DEFAULT:
		pages = kimage_alloc_normal_control_pages(image, order);
		break;
	case KEXEC_TYPE_CRASH:
		pages = kimage_alloc_crash_control_pages(image, order);
		break;
	}

	return pages;
}

int kimage_crash_copy_vmcoreinfo(struct kimage *image)
{
	struct page *vmcoreinfo_page;
	void *safecopy;

	if (image->type != KEXEC_TYPE_CRASH)
		return 0;

	 
	vmcoreinfo_page = kimage_alloc_control_pages(image, 0);
	if (!vmcoreinfo_page) {
		pr_warn("Could not allocate vmcoreinfo buffer\n");
		return -ENOMEM;
	}
	safecopy = vmap(&vmcoreinfo_page, 1, VM_MAP, PAGE_KERNEL);
	if (!safecopy) {
		pr_warn("Could not vmap vmcoreinfo buffer\n");
		return -ENOMEM;
	}

	image->vmcoreinfo_data_copy = safecopy;
	crash_update_vmcoreinfo_safecopy(safecopy);

	return 0;
}

static int kimage_add_entry(struct kimage *image, kimage_entry_t entry)
{
	if (*image->entry != 0)
		image->entry++;

	if (image->entry == image->last_entry) {
		kimage_entry_t *ind_page;
		struct page *page;

		page = kimage_alloc_page(image, GFP_KERNEL, KIMAGE_NO_DEST);
		if (!page)
			return -ENOMEM;

		ind_page = page_address(page);
		*image->entry = virt_to_boot_phys(ind_page) | IND_INDIRECTION;
		image->entry = ind_page;
		image->last_entry = ind_page +
				      ((PAGE_SIZE/sizeof(kimage_entry_t)) - 1);
	}
	*image->entry = entry;
	image->entry++;
	*image->entry = 0;

	return 0;
}

static int kimage_set_destination(struct kimage *image,
				   unsigned long destination)
{
	destination &= PAGE_MASK;

	return kimage_add_entry(image, destination | IND_DESTINATION);
}


static int kimage_add_page(struct kimage *image, unsigned long page)
{
	page &= PAGE_MASK;

	return kimage_add_entry(image, page | IND_SOURCE);
}


static void kimage_free_extra_pages(struct kimage *image)
{
	 
	kimage_free_page_list(&image->dest_pages);

	 
	kimage_free_page_list(&image->unusable_pages);

}

void kimage_terminate(struct kimage *image)
{
	if (*image->entry != 0)
		image->entry++;

	*image->entry = IND_DONE;
}

#define for_each_kimage_entry(image, ptr, entry) \
	for (ptr = &image->head; (entry = *ptr) && !(entry & IND_DONE); \
		ptr = (entry & IND_INDIRECTION) ? \
			boot_phys_to_virt((entry & PAGE_MASK)) : ptr + 1)

static void kimage_free_entry(kimage_entry_t entry)
{
	struct page *page;

	page = boot_pfn_to_page(entry >> PAGE_SHIFT);
	kimage_free_pages(page);
}

void kimage_free(struct kimage *image)
{
	kimage_entry_t *ptr, entry;
	kimage_entry_t ind = 0;

	if (!image)
		return;

	if (image->vmcoreinfo_data_copy) {
		crash_update_vmcoreinfo_safecopy(NULL);
		vunmap(image->vmcoreinfo_data_copy);
	}

	kimage_free_extra_pages(image);
	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_INDIRECTION) {
			 
			if (ind & IND_INDIRECTION)
				kimage_free_entry(ind);
			 
			ind = entry;
		} else if (entry & IND_SOURCE)
			kimage_free_entry(entry);
	}
	 
	if (ind & IND_INDIRECTION)
		kimage_free_entry(ind);

	 
	machine_kexec_cleanup(image);

	 
	kimage_free_page_list(&image->control_pages);

	 
	if (image->file_mode)
		kimage_file_post_load_cleanup(image);

	kfree(image);
}

static kimage_entry_t *kimage_dst_used(struct kimage *image,
					unsigned long page)
{
	kimage_entry_t *ptr, entry;
	unsigned long destination = 0;

	for_each_kimage_entry(image, ptr, entry) {
		if (entry & IND_DESTINATION)
			destination = entry & PAGE_MASK;
		else if (entry & IND_SOURCE) {
			if (page == destination)
				return ptr;
			destination += PAGE_SIZE;
		}
	}

	return NULL;
}

static struct page *kimage_alloc_page(struct kimage *image,
					gfp_t gfp_mask,
					unsigned long destination)
{
	 
	struct page *page;
	unsigned long addr;

	 
	list_for_each_entry(page, &image->dest_pages, lru) {
		addr = page_to_boot_pfn(page) << PAGE_SHIFT;
		if (addr == destination) {
			list_del(&page->lru);
			return page;
		}
	}
	page = NULL;
	while (1) {
		kimage_entry_t *old;

		 
		page = kimage_alloc_pages(gfp_mask, 0);
		if (!page)
			return NULL;
		 
		if (page_to_boot_pfn(page) >
				(KEXEC_SOURCE_MEMORY_LIMIT >> PAGE_SHIFT)) {
			list_add(&page->lru, &image->unusable_pages);
			continue;
		}
		addr = page_to_boot_pfn(page) << PAGE_SHIFT;

		 
		if (addr == destination)
			break;

		 
		if (!kimage_is_destination_range(image, addr,
						  addr + PAGE_SIZE))
			break;

		 
		old = kimage_dst_used(image, addr);
		if (old) {
			 
			unsigned long old_addr;
			struct page *old_page;

			old_addr = *old & PAGE_MASK;
			old_page = boot_pfn_to_page(old_addr >> PAGE_SHIFT);
			copy_highpage(page, old_page);
			*old = addr | (*old & ~PAGE_MASK);

			 
			if (!(gfp_mask & __GFP_HIGHMEM) &&
			    PageHighMem(old_page)) {
				kimage_free_pages(old_page);
				continue;
			}
			page = old_page;
			break;
		}
		 
		list_add(&page->lru, &image->dest_pages);
	}

	return page;
}

static int kimage_load_normal_segment(struct kimage *image,
					 struct kexec_segment *segment)
{
	unsigned long maddr;
	size_t ubytes, mbytes;
	int result;
	unsigned char __user *buf = NULL;
	unsigned char *kbuf = NULL;

	if (image->file_mode)
		kbuf = segment->kbuf;
	else
		buf = segment->buf;
	ubytes = segment->bufsz;
	mbytes = segment->memsz;
	maddr = segment->mem;

	result = kimage_set_destination(image, maddr);
	if (result < 0)
		goto out;

	while (mbytes) {
		struct page *page;
		char *ptr;
		size_t uchunk, mchunk;

		page = kimage_alloc_page(image, GFP_HIGHUSER, maddr);
		if (!page) {
			result  = -ENOMEM;
			goto out;
		}
		result = kimage_add_page(image, page_to_boot_pfn(page)
								<< PAGE_SHIFT);
		if (result < 0)
			goto out;

		ptr = kmap_local_page(page);
		 
		clear_page(ptr);
		ptr += maddr & ~PAGE_MASK;
		mchunk = min_t(size_t, mbytes,
				PAGE_SIZE - (maddr & ~PAGE_MASK));
		uchunk = min(ubytes, mchunk);

		 
		if (image->file_mode)
			memcpy(ptr, kbuf, uchunk);
		else
			result = copy_from_user(ptr, buf, uchunk);
		kunmap_local(ptr);
		if (result) {
			result = -EFAULT;
			goto out;
		}
		ubytes -= uchunk;
		maddr  += mchunk;
		if (image->file_mode)
			kbuf += mchunk;
		else
			buf += mchunk;
		mbytes -= mchunk;

		cond_resched();
	}
out:
	return result;
}

static int kimage_load_crash_segment(struct kimage *image,
					struct kexec_segment *segment)
{
	 
	unsigned long maddr;
	size_t ubytes, mbytes;
	int result;
	unsigned char __user *buf = NULL;
	unsigned char *kbuf = NULL;

	result = 0;
	if (image->file_mode)
		kbuf = segment->kbuf;
	else
		buf = segment->buf;
	ubytes = segment->bufsz;
	mbytes = segment->memsz;
	maddr = segment->mem;
	while (mbytes) {
		struct page *page;
		char *ptr;
		size_t uchunk, mchunk;

		page = boot_pfn_to_page(maddr >> PAGE_SHIFT);
		if (!page) {
			result  = -ENOMEM;
			goto out;
		}
		arch_kexec_post_alloc_pages(page_address(page), 1, 0);
		ptr = kmap_local_page(page);
		ptr += maddr & ~PAGE_MASK;
		mchunk = min_t(size_t, mbytes,
				PAGE_SIZE - (maddr & ~PAGE_MASK));
		uchunk = min(ubytes, mchunk);
		if (mchunk > uchunk) {
			 
			memset(ptr + uchunk, 0, mchunk - uchunk);
		}

		 
		if (image->file_mode)
			memcpy(ptr, kbuf, uchunk);
		else
			result = copy_from_user(ptr, buf, uchunk);
		kexec_flush_icache_page(page);
		kunmap_local(ptr);
		arch_kexec_pre_free_pages(page_address(page), 1);
		if (result) {
			result = -EFAULT;
			goto out;
		}
		ubytes -= uchunk;
		maddr  += mchunk;
		if (image->file_mode)
			kbuf += mchunk;
		else
			buf += mchunk;
		mbytes -= mchunk;

		cond_resched();
	}
out:
	return result;
}

int kimage_load_segment(struct kimage *image,
				struct kexec_segment *segment)
{
	int result = -ENOMEM;

	switch (image->type) {
	case KEXEC_TYPE_DEFAULT:
		result = kimage_load_normal_segment(image, segment);
		break;
	case KEXEC_TYPE_CRASH:
		result = kimage_load_crash_segment(image, segment);
		break;
	}

	return result;
}

struct kexec_load_limit {
	 
	struct mutex mutex;
	int limit;
};

static struct kexec_load_limit load_limit_reboot = {
	.mutex = __MUTEX_INITIALIZER(load_limit_reboot.mutex),
	.limit = -1,
};

static struct kexec_load_limit load_limit_panic = {
	.mutex = __MUTEX_INITIALIZER(load_limit_panic.mutex),
	.limit = -1,
};

struct kimage *kexec_image;
struct kimage *kexec_crash_image;
static int kexec_load_disabled;

#ifdef CONFIG_SYSCTL
static int kexec_limit_handler(struct ctl_table *table, int write,
			       void *buffer, size_t *lenp, loff_t *ppos)
{
	struct kexec_load_limit *limit = table->data;
	int val;
	struct ctl_table tmp = {
		.data = &val,
		.maxlen = sizeof(val),
		.mode = table->mode,
	};
	int ret;

	if (write) {
		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
		if (ret)
			return ret;

		if (val < 0)
			return -EINVAL;

		mutex_lock(&limit->mutex);
		if (limit->limit != -1 && val >= limit->limit)
			ret = -EINVAL;
		else
			limit->limit = val;
		mutex_unlock(&limit->mutex);

		return ret;
	}

	mutex_lock(&limit->mutex);
	val = limit->limit;
	mutex_unlock(&limit->mutex);

	return proc_dointvec(&tmp, write, buffer, lenp, ppos);
}

static struct ctl_table kexec_core_sysctls[] = {
	{
		.procname	= "kexec_load_disabled",
		.data		= &kexec_load_disabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		 
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "kexec_load_limit_panic",
		.data		= &load_limit_panic,
		.mode		= 0644,
		.proc_handler	= kexec_limit_handler,
	},
	{
		.procname	= "kexec_load_limit_reboot",
		.data		= &load_limit_reboot,
		.mode		= 0644,
		.proc_handler	= kexec_limit_handler,
	},
	{ }
};

static int __init kexec_core_sysctl_init(void)
{
	register_sysctl_init("kernel", kexec_core_sysctls);
	return 0;
}
late_initcall(kexec_core_sysctl_init);
#endif

bool kexec_load_permitted(int kexec_image_type)
{
	struct kexec_load_limit *limit;

	 
	if (!capable(CAP_SYS_BOOT) || kexec_load_disabled)
		return false;

	 
	limit = (kexec_image_type == KEXEC_TYPE_CRASH) ?
		&load_limit_panic : &load_limit_reboot;
	mutex_lock(&limit->mutex);
	if (!limit->limit) {
		mutex_unlock(&limit->mutex);
		return false;
	}
	if (limit->limit != -1)
		limit->limit--;
	mutex_unlock(&limit->mutex);

	return true;
}

 
void __noclone __crash_kexec(struct pt_regs *regs)
{
	 
	if (kexec_trylock()) {
		if (kexec_crash_image) {
			struct pt_regs fixed_regs;

			crash_setup_regs(&fixed_regs, regs);
			crash_save_vmcoreinfo();
			machine_crash_shutdown(&fixed_regs);
			machine_kexec(kexec_crash_image);
		}
		kexec_unlock();
	}
}
STACK_FRAME_NON_STANDARD(__crash_kexec);

__bpf_kfunc void crash_kexec(struct pt_regs *regs)
{
	int old_cpu, this_cpu;

	 
	this_cpu = raw_smp_processor_id();
	old_cpu = atomic_cmpxchg(&panic_cpu, PANIC_CPU_INVALID, this_cpu);
	if (old_cpu == PANIC_CPU_INVALID) {
		 
		__crash_kexec(regs);

		 
		atomic_set(&panic_cpu, PANIC_CPU_INVALID);
	}
}

static inline resource_size_t crash_resource_size(const struct resource *res)
{
	return !res->end ? 0 : resource_size(res);
}

ssize_t crash_get_memory_size(void)
{
	ssize_t size = 0;

	if (!kexec_trylock())
		return -EBUSY;

	size += crash_resource_size(&crashk_res);
	size += crash_resource_size(&crashk_low_res);

	kexec_unlock();
	return size;
}

static int __crash_shrink_memory(struct resource *old_res,
				 unsigned long new_size)
{
	struct resource *ram_res;

	ram_res = kzalloc(sizeof(*ram_res), GFP_KERNEL);
	if (!ram_res)
		return -ENOMEM;

	ram_res->start = old_res->start + new_size;
	ram_res->end   = old_res->end;
	ram_res->flags = IORESOURCE_BUSY | IORESOURCE_SYSTEM_RAM;
	ram_res->name  = "System RAM";

	if (!new_size) {
		release_resource(old_res);
		old_res->start = 0;
		old_res->end   = 0;
	} else {
		crashk_res.end = ram_res->start - 1;
	}

	crash_free_reserved_phys_range(ram_res->start, ram_res->end);
	insert_resource(&iomem_resource, ram_res);

	return 0;
}

int crash_shrink_memory(unsigned long new_size)
{
	int ret = 0;
	unsigned long old_size, low_size;

	if (!kexec_trylock())
		return -EBUSY;

	if (kexec_crash_image) {
		ret = -ENOENT;
		goto unlock;
	}

	low_size = crash_resource_size(&crashk_low_res);
	old_size = crash_resource_size(&crashk_res) + low_size;
	new_size = roundup(new_size, KEXEC_CRASH_MEM_ALIGN);
	if (new_size >= old_size) {
		ret = (new_size == old_size) ? 0 : -EINVAL;
		goto unlock;
	}

	 
	if (low_size > new_size) {
		ret = __crash_shrink_memory(&crashk_res, 0);
		if (ret)
			goto unlock;

		ret = __crash_shrink_memory(&crashk_low_res, new_size);
	} else {
		ret = __crash_shrink_memory(&crashk_res, new_size - low_size);
	}

	 
	if (!crashk_res.end && crashk_low_res.end) {
		crashk_res.start = crashk_low_res.start;
		crashk_res.end   = crashk_low_res.end;
		release_resource(&crashk_low_res);
		crashk_low_res.start = 0;
		crashk_low_res.end   = 0;
		insert_resource(&iomem_resource, &crashk_res);
	}

unlock:
	kexec_unlock();
	return ret;
}

void crash_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	u32 *buf;

	if ((cpu < 0) || (cpu >= nr_cpu_ids))
		return;

	 
	buf = (u32 *)per_cpu_ptr(crash_notes, cpu);
	if (!buf)
		return;
	memset(&prstatus, 0, sizeof(prstatus));
	prstatus.common.pr_pid = current->pid;
	elf_core_copy_regs(&prstatus.pr_reg, regs);
	buf = append_elf_note(buf, KEXEC_CORE_NOTE_NAME, NT_PRSTATUS,
			      &prstatus, sizeof(prstatus));
	final_note(buf);
}

 
int kernel_kexec(void)
{
	int error = 0;

	if (!kexec_trylock())
		return -EBUSY;
	if (!kexec_image) {
		error = -EINVAL;
		goto Unlock;
	}

#ifdef CONFIG_KEXEC_JUMP
	if (kexec_image->preserve_context) {
		pm_prepare_console();
		error = freeze_processes();
		if (error) {
			error = -EBUSY;
			goto Restore_console;
		}
		suspend_console();
		error = dpm_suspend_start(PMSG_FREEZE);
		if (error)
			goto Resume_console;
		 
		error = dpm_suspend_end(PMSG_FREEZE);
		if (error)
			goto Resume_devices;
		error = suspend_disable_secondary_cpus();
		if (error)
			goto Enable_cpus;
		local_irq_disable();
		error = syscore_suspend();
		if (error)
			goto Enable_irqs;
	} else
#endif
	{
		kexec_in_progress = true;
		kernel_restart_prepare("kexec reboot");
		migrate_to_reboot_cpu();

		 
		cpu_hotplug_enable();
		pr_notice("Starting new kernel\n");
		machine_shutdown();
	}

	kmsg_dump(KMSG_DUMP_SHUTDOWN);
	machine_kexec(kexec_image);

#ifdef CONFIG_KEXEC_JUMP
	if (kexec_image->preserve_context) {
		syscore_resume();
 Enable_irqs:
		local_irq_enable();
 Enable_cpus:
		suspend_enable_secondary_cpus();
		dpm_resume_start(PMSG_RESTORE);
 Resume_devices:
		dpm_resume_end(PMSG_RESTORE);
 Resume_console:
		resume_console();
		thaw_processes();
 Restore_console:
		pm_restore_console();
	}
#endif

 Unlock:
	kexec_unlock();
	return error;
}
