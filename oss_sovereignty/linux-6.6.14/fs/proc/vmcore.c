
 

#include <linux/mm.h>
#include <linux/kcore.h>
#include <linux/user.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/printk.h>
#include <linux/memblock.h>
#include <linux/init.h>
#include <linux/crash_dump.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <linux/cc_platform.h>
#include <asm/io.h>
#include "internal.h"

 
static LIST_HEAD(vmcore_list);

 
static char *elfcorebuf;
static size_t elfcorebuf_sz;
static size_t elfcorebuf_sz_orig;

static char *elfnotes_buf;
static size_t elfnotes_sz;
 
static size_t elfnotes_orig_sz;

 
static u64 vmcore_size;

static struct proc_dir_entry *proc_vmcore;

#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
 
static LIST_HEAD(vmcoredd_list);
static DEFINE_MUTEX(vmcoredd_mutex);

static bool vmcoredd_disabled;
core_param(novmcoredd, vmcoredd_disabled, bool, 0);
#endif  

 
static size_t vmcoredd_orig_sz;

static DEFINE_SPINLOCK(vmcore_cb_lock);
DEFINE_STATIC_SRCU(vmcore_cb_srcu);
 
static LIST_HEAD(vmcore_cb_list);
 
static bool vmcore_opened;

void register_vmcore_cb(struct vmcore_cb *cb)
{
	INIT_LIST_HEAD(&cb->next);
	spin_lock(&vmcore_cb_lock);
	list_add_tail(&cb->next, &vmcore_cb_list);
	 
	if (vmcore_opened)
		pr_warn_once("Unexpected vmcore callback registration\n");
	spin_unlock(&vmcore_cb_lock);
}
EXPORT_SYMBOL_GPL(register_vmcore_cb);

void unregister_vmcore_cb(struct vmcore_cb *cb)
{
	spin_lock(&vmcore_cb_lock);
	list_del_rcu(&cb->next);
	 
	if (vmcore_opened)
		pr_warn_once("Unexpected vmcore callback unregistration\n");
	spin_unlock(&vmcore_cb_lock);

	synchronize_srcu(&vmcore_cb_srcu);
}
EXPORT_SYMBOL_GPL(unregister_vmcore_cb);

static bool pfn_is_ram(unsigned long pfn)
{
	struct vmcore_cb *cb;
	bool ret = true;

	list_for_each_entry_srcu(cb, &vmcore_cb_list, next,
				 srcu_read_lock_held(&vmcore_cb_srcu)) {
		if (unlikely(!cb->pfn_is_ram))
			continue;
		ret = cb->pfn_is_ram(cb, pfn);
		if (!ret)
			break;
	}

	return ret;
}

static int open_vmcore(struct inode *inode, struct file *file)
{
	spin_lock(&vmcore_cb_lock);
	vmcore_opened = true;
	spin_unlock(&vmcore_cb_lock);

	return 0;
}

 
ssize_t read_from_oldmem(struct iov_iter *iter, size_t count,
			 u64 *ppos, bool encrypted)
{
	unsigned long pfn, offset;
	ssize_t nr_bytes;
	ssize_t read = 0, tmp;
	int idx;

	if (!count)
		return 0;

	offset = (unsigned long)(*ppos % PAGE_SIZE);
	pfn = (unsigned long)(*ppos / PAGE_SIZE);

	idx = srcu_read_lock(&vmcore_cb_srcu);
	do {
		if (count > (PAGE_SIZE - offset))
			nr_bytes = PAGE_SIZE - offset;
		else
			nr_bytes = count;

		 
		if (!pfn_is_ram(pfn)) {
			tmp = iov_iter_zero(nr_bytes, iter);
		} else {
			if (encrypted)
				tmp = copy_oldmem_page_encrypted(iter, pfn,
								 nr_bytes,
								 offset);
			else
				tmp = copy_oldmem_page(iter, pfn, nr_bytes,
						       offset);
		}
		if (tmp < nr_bytes) {
			srcu_read_unlock(&vmcore_cb_srcu, idx);
			return -EFAULT;
		}

		*ppos += nr_bytes;
		count -= nr_bytes;
		read += nr_bytes;
		++pfn;
		offset = 0;
	} while (count);
	srcu_read_unlock(&vmcore_cb_srcu, idx);

	return read;
}

 
int __weak elfcorehdr_alloc(unsigned long long *addr, unsigned long long *size)
{
	return 0;
}

 
void __weak elfcorehdr_free(unsigned long long addr)
{}

 
ssize_t __weak elfcorehdr_read(char *buf, size_t count, u64 *ppos)
{
	struct kvec kvec = { .iov_base = buf, .iov_len = count };
	struct iov_iter iter;

	iov_iter_kvec(&iter, ITER_DEST, &kvec, 1, count);

	return read_from_oldmem(&iter, count, ppos, false);
}

 
ssize_t __weak elfcorehdr_read_notes(char *buf, size_t count, u64 *ppos)
{
	struct kvec kvec = { .iov_base = buf, .iov_len = count };
	struct iov_iter iter;

	iov_iter_kvec(&iter, ITER_DEST, &kvec, 1, count);

	return read_from_oldmem(&iter, count, ppos,
			cc_platform_has(CC_ATTR_MEM_ENCRYPT));
}

 
int __weak remap_oldmem_pfn_range(struct vm_area_struct *vma,
				  unsigned long from, unsigned long pfn,
				  unsigned long size, pgprot_t prot)
{
	prot = pgprot_encrypted(prot);
	return remap_pfn_range(vma, from, pfn, size, prot);
}

 
ssize_t __weak copy_oldmem_page_encrypted(struct iov_iter *iter,
		unsigned long pfn, size_t csize, unsigned long offset)
{
	return copy_oldmem_page(iter, pfn, csize, offset);
}

#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
static int vmcoredd_copy_dumps(struct iov_iter *iter, u64 start, size_t size)
{
	struct vmcoredd_node *dump;
	u64 offset = 0;
	int ret = 0;
	size_t tsz;
	char *buf;

	mutex_lock(&vmcoredd_mutex);
	list_for_each_entry(dump, &vmcoredd_list, list) {
		if (start < offset + dump->size) {
			tsz = min(offset + (u64)dump->size - start, (u64)size);
			buf = dump->buf + start - offset;
			if (copy_to_iter(buf, tsz, iter) < tsz) {
				ret = -EFAULT;
				goto out_unlock;
			}

			size -= tsz;
			start += tsz;

			 
			if (!size)
				goto out_unlock;
		}
		offset += dump->size;
	}

out_unlock:
	mutex_unlock(&vmcoredd_mutex);
	return ret;
}

#ifdef CONFIG_MMU
static int vmcoredd_mmap_dumps(struct vm_area_struct *vma, unsigned long dst,
			       u64 start, size_t size)
{
	struct vmcoredd_node *dump;
	u64 offset = 0;
	int ret = 0;
	size_t tsz;
	char *buf;

	mutex_lock(&vmcoredd_mutex);
	list_for_each_entry(dump, &vmcoredd_list, list) {
		if (start < offset + dump->size) {
			tsz = min(offset + (u64)dump->size - start, (u64)size);
			buf = dump->buf + start - offset;
			if (remap_vmalloc_range_partial(vma, dst, buf, 0,
							tsz)) {
				ret = -EFAULT;
				goto out_unlock;
			}

			size -= tsz;
			start += tsz;
			dst += tsz;

			 
			if (!size)
				goto out_unlock;
		}
		offset += dump->size;
	}

out_unlock:
	mutex_unlock(&vmcoredd_mutex);
	return ret;
}
#endif  
#endif  

 
static ssize_t __read_vmcore(struct iov_iter *iter, loff_t *fpos)
{
	ssize_t acc = 0, tmp;
	size_t tsz;
	u64 start;
	struct vmcore *m = NULL;

	if (!iov_iter_count(iter) || *fpos >= vmcore_size)
		return 0;

	iov_iter_truncate(iter, vmcore_size - *fpos);

	 
	if (*fpos < elfcorebuf_sz) {
		tsz = min(elfcorebuf_sz - (size_t)*fpos, iov_iter_count(iter));
		if (copy_to_iter(elfcorebuf + *fpos, tsz, iter) < tsz)
			return -EFAULT;
		*fpos += tsz;
		acc += tsz;

		 
		if (!iov_iter_count(iter))
			return acc;
	}

	 
	if (*fpos < elfcorebuf_sz + elfnotes_sz) {
		void *kaddr;

		 
#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
		 
		if (*fpos < elfcorebuf_sz + vmcoredd_orig_sz) {
			tsz = min(elfcorebuf_sz + vmcoredd_orig_sz -
				  (size_t)*fpos, iov_iter_count(iter));
			start = *fpos - elfcorebuf_sz;
			if (vmcoredd_copy_dumps(iter, start, tsz))
				return -EFAULT;

			*fpos += tsz;
			acc += tsz;

			 
			if (!iov_iter_count(iter))
				return acc;
		}
#endif  

		 
		tsz = min(elfcorebuf_sz + elfnotes_sz - (size_t)*fpos,
			  iov_iter_count(iter));
		kaddr = elfnotes_buf + *fpos - elfcorebuf_sz - vmcoredd_orig_sz;
		if (copy_to_iter(kaddr, tsz, iter) < tsz)
			return -EFAULT;

		*fpos += tsz;
		acc += tsz;

		 
		if (!iov_iter_count(iter))
			return acc;
	}

	list_for_each_entry(m, &vmcore_list, list) {
		if (*fpos < m->offset + m->size) {
			tsz = (size_t)min_t(unsigned long long,
					    m->offset + m->size - *fpos,
					    iov_iter_count(iter));
			start = m->paddr + *fpos - m->offset;
			tmp = read_from_oldmem(iter, tsz, &start,
					cc_platform_has(CC_ATTR_MEM_ENCRYPT));
			if (tmp < 0)
				return tmp;
			*fpos += tsz;
			acc += tsz;

			 
			if (!iov_iter_count(iter))
				return acc;
		}
	}

	return acc;
}

static ssize_t read_vmcore(struct kiocb *iocb, struct iov_iter *iter)
{
	return __read_vmcore(iter, &iocb->ki_pos);
}

 
static vm_fault_t mmap_vmcore_fault(struct vm_fault *vmf)
{
#ifdef CONFIG_S390
	struct address_space *mapping = vmf->vma->vm_file->f_mapping;
	pgoff_t index = vmf->pgoff;
	struct iov_iter iter;
	struct kvec kvec;
	struct page *page;
	loff_t offset;
	int rc;

	page = find_or_create_page(mapping, index, GFP_KERNEL);
	if (!page)
		return VM_FAULT_OOM;
	if (!PageUptodate(page)) {
		offset = (loff_t) index << PAGE_SHIFT;
		kvec.iov_base = page_address(page);
		kvec.iov_len = PAGE_SIZE;
		iov_iter_kvec(&iter, ITER_DEST, &kvec, 1, PAGE_SIZE);

		rc = __read_vmcore(&iter, &offset);
		if (rc < 0) {
			unlock_page(page);
			put_page(page);
			return vmf_error(rc);
		}
		SetPageUptodate(page);
	}
	unlock_page(page);
	vmf->page = page;
	return 0;
#else
	return VM_FAULT_SIGBUS;
#endif
}

static const struct vm_operations_struct vmcore_mmap_ops = {
	.fault = mmap_vmcore_fault,
};

 
static inline char *vmcore_alloc_buf(size_t size)
{
#ifdef CONFIG_MMU
	return vmalloc_user(size);
#else
	return vzalloc(size);
#endif
}

 
#ifdef CONFIG_MMU
 
static int remap_oldmem_pfn_checked(struct vm_area_struct *vma,
				    unsigned long from, unsigned long pfn,
				    unsigned long size, pgprot_t prot)
{
	unsigned long map_size;
	unsigned long pos_start, pos_end, pos;
	unsigned long zeropage_pfn = my_zero_pfn(0);
	size_t len = 0;

	pos_start = pfn;
	pos_end = pfn + (size >> PAGE_SHIFT);

	for (pos = pos_start; pos < pos_end; ++pos) {
		if (!pfn_is_ram(pos)) {
			 
			if (pos > pos_start) {
				 
				map_size = (pos - pos_start) << PAGE_SHIFT;
				if (remap_oldmem_pfn_range(vma, from + len,
							   pos_start, map_size,
							   prot))
					goto fail;
				len += map_size;
			}
			 
			if (remap_oldmem_pfn_range(vma, from + len,
						   zeropage_pfn,
						   PAGE_SIZE, prot))
				goto fail;
			len += PAGE_SIZE;
			pos_start = pos + 1;
		}
	}
	if (pos > pos_start) {
		 
		map_size = (pos - pos_start) << PAGE_SHIFT;
		if (remap_oldmem_pfn_range(vma, from + len, pos_start,
					   map_size, prot))
			goto fail;
	}
	return 0;
fail:
	do_munmap(vma->vm_mm, from, len, NULL);
	return -EAGAIN;
}

static int vmcore_remap_oldmem_pfn(struct vm_area_struct *vma,
			    unsigned long from, unsigned long pfn,
			    unsigned long size, pgprot_t prot)
{
	int ret, idx;

	 
	idx = srcu_read_lock(&vmcore_cb_srcu);
	if (!list_empty(&vmcore_cb_list))
		ret = remap_oldmem_pfn_checked(vma, from, pfn, size, prot);
	else
		ret = remap_oldmem_pfn_range(vma, from, pfn, size, prot);
	srcu_read_unlock(&vmcore_cb_srcu, idx);
	return ret;
}

static int mmap_vmcore(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	u64 start, end, len, tsz;
	struct vmcore *m;

	start = (u64)vma->vm_pgoff << PAGE_SHIFT;
	end = start + size;

	if (size > vmcore_size || end > vmcore_size)
		return -EINVAL;

	if (vma->vm_flags & (VM_WRITE | VM_EXEC))
		return -EPERM;

	vm_flags_mod(vma, VM_MIXEDMAP, VM_MAYWRITE | VM_MAYEXEC);
	vma->vm_ops = &vmcore_mmap_ops;

	len = 0;

	if (start < elfcorebuf_sz) {
		u64 pfn;

		tsz = min(elfcorebuf_sz - (size_t)start, size);
		pfn = __pa(elfcorebuf + start) >> PAGE_SHIFT;
		if (remap_pfn_range(vma, vma->vm_start, pfn, tsz,
				    vma->vm_page_prot))
			return -EAGAIN;
		size -= tsz;
		start += tsz;
		len += tsz;

		if (size == 0)
			return 0;
	}

	if (start < elfcorebuf_sz + elfnotes_sz) {
		void *kaddr;

		 
#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
		 
		if (start < elfcorebuf_sz + vmcoredd_orig_sz) {
			u64 start_off;

			tsz = min(elfcorebuf_sz + vmcoredd_orig_sz -
				  (size_t)start, size);
			start_off = start - elfcorebuf_sz;
			if (vmcoredd_mmap_dumps(vma, vma->vm_start + len,
						start_off, tsz))
				goto fail;

			size -= tsz;
			start += tsz;
			len += tsz;

			 
			if (!size)
				return 0;
		}
#endif  

		 
		tsz = min(elfcorebuf_sz + elfnotes_sz - (size_t)start, size);
		kaddr = elfnotes_buf + start - elfcorebuf_sz - vmcoredd_orig_sz;
		if (remap_vmalloc_range_partial(vma, vma->vm_start + len,
						kaddr, 0, tsz))
			goto fail;

		size -= tsz;
		start += tsz;
		len += tsz;

		if (size == 0)
			return 0;
	}

	list_for_each_entry(m, &vmcore_list, list) {
		if (start < m->offset + m->size) {
			u64 paddr = 0;

			tsz = (size_t)min_t(unsigned long long,
					    m->offset + m->size - start, size);
			paddr = m->paddr + start - m->offset;
			if (vmcore_remap_oldmem_pfn(vma, vma->vm_start + len,
						    paddr >> PAGE_SHIFT, tsz,
						    vma->vm_page_prot))
				goto fail;
			size -= tsz;
			start += tsz;
			len += tsz;

			if (size == 0)
				return 0;
		}
	}

	return 0;
fail:
	do_munmap(vma->vm_mm, vma->vm_start, len, NULL);
	return -EAGAIN;
}
#else
static int mmap_vmcore(struct file *file, struct vm_area_struct *vma)
{
	return -ENOSYS;
}
#endif

static const struct proc_ops vmcore_proc_ops = {
	.proc_open	= open_vmcore,
	.proc_read_iter	= read_vmcore,
	.proc_lseek	= default_llseek,
	.proc_mmap	= mmap_vmcore,
};

static struct vmcore* __init get_new_element(void)
{
	return kzalloc(sizeof(struct vmcore), GFP_KERNEL);
}

static u64 get_vmcore_size(size_t elfsz, size_t elfnotesegsz,
			   struct list_head *vc_list)
{
	u64 size;
	struct vmcore *m;

	size = elfsz + elfnotesegsz;
	list_for_each_entry(m, vc_list, list) {
		size += m->size;
	}
	return size;
}

 
static int __init update_note_header_size_elf64(const Elf64_Ehdr *ehdr_ptr)
{
	int i, rc=0;
	Elf64_Phdr *phdr_ptr;
	Elf64_Nhdr *nhdr_ptr;

	phdr_ptr = (Elf64_Phdr *)(ehdr_ptr + 1);
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		void *notes_section;
		u64 offset, max_sz, sz, real_sz = 0;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		max_sz = phdr_ptr->p_memsz;
		offset = phdr_ptr->p_offset;
		notes_section = kmalloc(max_sz, GFP_KERNEL);
		if (!notes_section)
			return -ENOMEM;
		rc = elfcorehdr_read_notes(notes_section, max_sz, &offset);
		if (rc < 0) {
			kfree(notes_section);
			return rc;
		}
		nhdr_ptr = notes_section;
		while (nhdr_ptr->n_namesz != 0) {
			sz = sizeof(Elf64_Nhdr) +
				(((u64)nhdr_ptr->n_namesz + 3) & ~3) +
				(((u64)nhdr_ptr->n_descsz + 3) & ~3);
			if ((real_sz + sz) > max_sz) {
				pr_warn("Warning: Exceeded p_memsz, dropping PT_NOTE entry n_namesz=0x%x, n_descsz=0x%x\n",
					nhdr_ptr->n_namesz, nhdr_ptr->n_descsz);
				break;
			}
			real_sz += sz;
			nhdr_ptr = (Elf64_Nhdr*)((char*)nhdr_ptr + sz);
		}
		kfree(notes_section);
		phdr_ptr->p_memsz = real_sz;
		if (real_sz == 0) {
			pr_warn("Warning: Zero PT_NOTE entries found\n");
		}
	}

	return 0;
}

 
static int __init get_note_number_and_size_elf64(const Elf64_Ehdr *ehdr_ptr,
						 int *nr_ptnote, u64 *sz_ptnote)
{
	int i;
	Elf64_Phdr *phdr_ptr;

	*nr_ptnote = *sz_ptnote = 0;

	phdr_ptr = (Elf64_Phdr *)(ehdr_ptr + 1);
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		*nr_ptnote += 1;
		*sz_ptnote += phdr_ptr->p_memsz;
	}

	return 0;
}

 
static int __init copy_notes_elf64(const Elf64_Ehdr *ehdr_ptr, char *notes_buf)
{
	int i, rc=0;
	Elf64_Phdr *phdr_ptr;

	phdr_ptr = (Elf64_Phdr*)(ehdr_ptr + 1);

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		u64 offset;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		offset = phdr_ptr->p_offset;
		rc = elfcorehdr_read_notes(notes_buf, phdr_ptr->p_memsz,
					   &offset);
		if (rc < 0)
			return rc;
		notes_buf += phdr_ptr->p_memsz;
	}

	return 0;
}

 
static int __init merge_note_headers_elf64(char *elfptr, size_t *elfsz,
					   char **notes_buf, size_t *notes_sz)
{
	int i, nr_ptnote=0, rc=0;
	char *tmp;
	Elf64_Ehdr *ehdr_ptr;
	Elf64_Phdr phdr;
	u64 phdr_sz = 0, note_off;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;

	rc = update_note_header_size_elf64(ehdr_ptr);
	if (rc < 0)
		return rc;

	rc = get_note_number_and_size_elf64(ehdr_ptr, &nr_ptnote, &phdr_sz);
	if (rc < 0)
		return rc;

	*notes_sz = roundup(phdr_sz, PAGE_SIZE);
	*notes_buf = vmcore_alloc_buf(*notes_sz);
	if (!*notes_buf)
		return -ENOMEM;

	rc = copy_notes_elf64(ehdr_ptr, *notes_buf);
	if (rc < 0)
		return rc;

	 
	phdr.p_type    = PT_NOTE;
	phdr.p_flags   = 0;
	note_off = sizeof(Elf64_Ehdr) +
			(ehdr_ptr->e_phnum - nr_ptnote +1) * sizeof(Elf64_Phdr);
	phdr.p_offset  = roundup(note_off, PAGE_SIZE);
	phdr.p_vaddr   = phdr.p_paddr = 0;
	phdr.p_filesz  = phdr.p_memsz = phdr_sz;
	phdr.p_align   = 4;

	 
	tmp = elfptr + sizeof(Elf64_Ehdr);
	memcpy(tmp, &phdr, sizeof(phdr));
	tmp += sizeof(phdr);

	 
	i = (nr_ptnote - 1) * sizeof(Elf64_Phdr);
	*elfsz = *elfsz - i;
	memmove(tmp, tmp+i, ((*elfsz)-sizeof(Elf64_Ehdr)-sizeof(Elf64_Phdr)));
	memset(elfptr + *elfsz, 0, i);
	*elfsz = roundup(*elfsz, PAGE_SIZE);

	 
	ehdr_ptr->e_phnum = ehdr_ptr->e_phnum - nr_ptnote + 1;

	 
	elfnotes_orig_sz = phdr.p_memsz;

	return 0;
}

 
static int __init update_note_header_size_elf32(const Elf32_Ehdr *ehdr_ptr)
{
	int i, rc=0;
	Elf32_Phdr *phdr_ptr;
	Elf32_Nhdr *nhdr_ptr;

	phdr_ptr = (Elf32_Phdr *)(ehdr_ptr + 1);
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		void *notes_section;
		u64 offset, max_sz, sz, real_sz = 0;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		max_sz = phdr_ptr->p_memsz;
		offset = phdr_ptr->p_offset;
		notes_section = kmalloc(max_sz, GFP_KERNEL);
		if (!notes_section)
			return -ENOMEM;
		rc = elfcorehdr_read_notes(notes_section, max_sz, &offset);
		if (rc < 0) {
			kfree(notes_section);
			return rc;
		}
		nhdr_ptr = notes_section;
		while (nhdr_ptr->n_namesz != 0) {
			sz = sizeof(Elf32_Nhdr) +
				(((u64)nhdr_ptr->n_namesz + 3) & ~3) +
				(((u64)nhdr_ptr->n_descsz + 3) & ~3);
			if ((real_sz + sz) > max_sz) {
				pr_warn("Warning: Exceeded p_memsz, dropping PT_NOTE entry n_namesz=0x%x, n_descsz=0x%x\n",
					nhdr_ptr->n_namesz, nhdr_ptr->n_descsz);
				break;
			}
			real_sz += sz;
			nhdr_ptr = (Elf32_Nhdr*)((char*)nhdr_ptr + sz);
		}
		kfree(notes_section);
		phdr_ptr->p_memsz = real_sz;
		if (real_sz == 0) {
			pr_warn("Warning: Zero PT_NOTE entries found\n");
		}
	}

	return 0;
}

 
static int __init get_note_number_and_size_elf32(const Elf32_Ehdr *ehdr_ptr,
						 int *nr_ptnote, u64 *sz_ptnote)
{
	int i;
	Elf32_Phdr *phdr_ptr;

	*nr_ptnote = *sz_ptnote = 0;

	phdr_ptr = (Elf32_Phdr *)(ehdr_ptr + 1);
	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		*nr_ptnote += 1;
		*sz_ptnote += phdr_ptr->p_memsz;
	}

	return 0;
}

 
static int __init copy_notes_elf32(const Elf32_Ehdr *ehdr_ptr, char *notes_buf)
{
	int i, rc=0;
	Elf32_Phdr *phdr_ptr;

	phdr_ptr = (Elf32_Phdr*)(ehdr_ptr + 1);

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		u64 offset;
		if (phdr_ptr->p_type != PT_NOTE)
			continue;
		offset = phdr_ptr->p_offset;
		rc = elfcorehdr_read_notes(notes_buf, phdr_ptr->p_memsz,
					   &offset);
		if (rc < 0)
			return rc;
		notes_buf += phdr_ptr->p_memsz;
	}

	return 0;
}

 
static int __init merge_note_headers_elf32(char *elfptr, size_t *elfsz,
					   char **notes_buf, size_t *notes_sz)
{
	int i, nr_ptnote=0, rc=0;
	char *tmp;
	Elf32_Ehdr *ehdr_ptr;
	Elf32_Phdr phdr;
	u64 phdr_sz = 0, note_off;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;

	rc = update_note_header_size_elf32(ehdr_ptr);
	if (rc < 0)
		return rc;

	rc = get_note_number_and_size_elf32(ehdr_ptr, &nr_ptnote, &phdr_sz);
	if (rc < 0)
		return rc;

	*notes_sz = roundup(phdr_sz, PAGE_SIZE);
	*notes_buf = vmcore_alloc_buf(*notes_sz);
	if (!*notes_buf)
		return -ENOMEM;

	rc = copy_notes_elf32(ehdr_ptr, *notes_buf);
	if (rc < 0)
		return rc;

	 
	phdr.p_type    = PT_NOTE;
	phdr.p_flags   = 0;
	note_off = sizeof(Elf32_Ehdr) +
			(ehdr_ptr->e_phnum - nr_ptnote +1) * sizeof(Elf32_Phdr);
	phdr.p_offset  = roundup(note_off, PAGE_SIZE);
	phdr.p_vaddr   = phdr.p_paddr = 0;
	phdr.p_filesz  = phdr.p_memsz = phdr_sz;
	phdr.p_align   = 4;

	 
	tmp = elfptr + sizeof(Elf32_Ehdr);
	memcpy(tmp, &phdr, sizeof(phdr));
	tmp += sizeof(phdr);

	 
	i = (nr_ptnote - 1) * sizeof(Elf32_Phdr);
	*elfsz = *elfsz - i;
	memmove(tmp, tmp+i, ((*elfsz)-sizeof(Elf32_Ehdr)-sizeof(Elf32_Phdr)));
	memset(elfptr + *elfsz, 0, i);
	*elfsz = roundup(*elfsz, PAGE_SIZE);

	 
	ehdr_ptr->e_phnum = ehdr_ptr->e_phnum - nr_ptnote + 1;

	 
	elfnotes_orig_sz = phdr.p_memsz;

	return 0;
}

 
static int __init process_ptload_program_headers_elf64(char *elfptr,
						size_t elfsz,
						size_t elfnotes_sz,
						struct list_head *vc_list)
{
	int i;
	Elf64_Ehdr *ehdr_ptr;
	Elf64_Phdr *phdr_ptr;
	loff_t vmcore_off;
	struct vmcore *new;

	ehdr_ptr = (Elf64_Ehdr *)elfptr;
	phdr_ptr = (Elf64_Phdr*)(elfptr + sizeof(Elf64_Ehdr));  

	 
	vmcore_off = elfsz + elfnotes_sz;

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		u64 paddr, start, end, size;

		if (phdr_ptr->p_type != PT_LOAD)
			continue;

		paddr = phdr_ptr->p_offset;
		start = rounddown(paddr, PAGE_SIZE);
		end = roundup(paddr + phdr_ptr->p_memsz, PAGE_SIZE);
		size = end - start;

		 
		new = get_new_element();
		if (!new)
			return -ENOMEM;
		new->paddr = start;
		new->size = size;
		list_add_tail(&new->list, vc_list);

		 
		phdr_ptr->p_offset = vmcore_off + (paddr - start);
		vmcore_off = vmcore_off + size;
	}
	return 0;
}

static int __init process_ptload_program_headers_elf32(char *elfptr,
						size_t elfsz,
						size_t elfnotes_sz,
						struct list_head *vc_list)
{
	int i;
	Elf32_Ehdr *ehdr_ptr;
	Elf32_Phdr *phdr_ptr;
	loff_t vmcore_off;
	struct vmcore *new;

	ehdr_ptr = (Elf32_Ehdr *)elfptr;
	phdr_ptr = (Elf32_Phdr*)(elfptr + sizeof(Elf32_Ehdr));  

	 
	vmcore_off = elfsz + elfnotes_sz;

	for (i = 0; i < ehdr_ptr->e_phnum; i++, phdr_ptr++) {
		u64 paddr, start, end, size;

		if (phdr_ptr->p_type != PT_LOAD)
			continue;

		paddr = phdr_ptr->p_offset;
		start = rounddown(paddr, PAGE_SIZE);
		end = roundup(paddr + phdr_ptr->p_memsz, PAGE_SIZE);
		size = end - start;

		 
		new = get_new_element();
		if (!new)
			return -ENOMEM;
		new->paddr = start;
		new->size = size;
		list_add_tail(&new->list, vc_list);

		 
		phdr_ptr->p_offset = vmcore_off + (paddr - start);
		vmcore_off = vmcore_off + size;
	}
	return 0;
}

 
static void set_vmcore_list_offsets(size_t elfsz, size_t elfnotes_sz,
				    struct list_head *vc_list)
{
	loff_t vmcore_off;
	struct vmcore *m;

	 
	vmcore_off = elfsz + elfnotes_sz;

	list_for_each_entry(m, vc_list, list) {
		m->offset = vmcore_off;
		vmcore_off += m->size;
	}
}

static void free_elfcorebuf(void)
{
	free_pages((unsigned long)elfcorebuf, get_order(elfcorebuf_sz_orig));
	elfcorebuf = NULL;
	vfree(elfnotes_buf);
	elfnotes_buf = NULL;
}

static int __init parse_crash_elf64_headers(void)
{
	int rc=0;
	Elf64_Ehdr ehdr;
	u64 addr;

	addr = elfcorehdr_addr;

	 
	rc = elfcorehdr_read((char *)&ehdr, sizeof(Elf64_Ehdr), &addr);
	if (rc < 0)
		return rc;

	 
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
		(ehdr.e_type != ET_CORE) ||
		!vmcore_elf64_check_arch(&ehdr) ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_version != EV_CURRENT ||
		ehdr.e_ehsize != sizeof(Elf64_Ehdr) ||
		ehdr.e_phentsize != sizeof(Elf64_Phdr) ||
		ehdr.e_phnum == 0) {
		pr_warn("Warning: Core image elf header is not sane\n");
		return -EINVAL;
	}

	 
	elfcorebuf_sz_orig = sizeof(Elf64_Ehdr) +
				ehdr.e_phnum * sizeof(Elf64_Phdr);
	elfcorebuf_sz = elfcorebuf_sz_orig;
	elfcorebuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					      get_order(elfcorebuf_sz_orig));
	if (!elfcorebuf)
		return -ENOMEM;
	addr = elfcorehdr_addr;
	rc = elfcorehdr_read(elfcorebuf, elfcorebuf_sz_orig, &addr);
	if (rc < 0)
		goto fail;

	 
	rc = merge_note_headers_elf64(elfcorebuf, &elfcorebuf_sz,
				      &elfnotes_buf, &elfnotes_sz);
	if (rc)
		goto fail;
	rc = process_ptload_program_headers_elf64(elfcorebuf, elfcorebuf_sz,
						  elfnotes_sz, &vmcore_list);
	if (rc)
		goto fail;
	set_vmcore_list_offsets(elfcorebuf_sz, elfnotes_sz, &vmcore_list);
	return 0;
fail:
	free_elfcorebuf();
	return rc;
}

static int __init parse_crash_elf32_headers(void)
{
	int rc=0;
	Elf32_Ehdr ehdr;
	u64 addr;

	addr = elfcorehdr_addr;

	 
	rc = elfcorehdr_read((char *)&ehdr, sizeof(Elf32_Ehdr), &addr);
	if (rc < 0)
		return rc;

	 
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
		(ehdr.e_type != ET_CORE) ||
		!vmcore_elf32_check_arch(&ehdr) ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS32||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_version != EV_CURRENT ||
		ehdr.e_ehsize != sizeof(Elf32_Ehdr) ||
		ehdr.e_phentsize != sizeof(Elf32_Phdr) ||
		ehdr.e_phnum == 0) {
		pr_warn("Warning: Core image elf header is not sane\n");
		return -EINVAL;
	}

	 
	elfcorebuf_sz_orig = sizeof(Elf32_Ehdr) + ehdr.e_phnum * sizeof(Elf32_Phdr);
	elfcorebuf_sz = elfcorebuf_sz_orig;
	elfcorebuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					      get_order(elfcorebuf_sz_orig));
	if (!elfcorebuf)
		return -ENOMEM;
	addr = elfcorehdr_addr;
	rc = elfcorehdr_read(elfcorebuf, elfcorebuf_sz_orig, &addr);
	if (rc < 0)
		goto fail;

	 
	rc = merge_note_headers_elf32(elfcorebuf, &elfcorebuf_sz,
				      &elfnotes_buf, &elfnotes_sz);
	if (rc)
		goto fail;
	rc = process_ptload_program_headers_elf32(elfcorebuf, elfcorebuf_sz,
						  elfnotes_sz, &vmcore_list);
	if (rc)
		goto fail;
	set_vmcore_list_offsets(elfcorebuf_sz, elfnotes_sz, &vmcore_list);
	return 0;
fail:
	free_elfcorebuf();
	return rc;
}

static int __init parse_crash_elf_headers(void)
{
	unsigned char e_ident[EI_NIDENT];
	u64 addr;
	int rc=0;

	addr = elfcorehdr_addr;
	rc = elfcorehdr_read(e_ident, EI_NIDENT, &addr);
	if (rc < 0)
		return rc;
	if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
		pr_warn("Warning: Core image elf header not found\n");
		return -EINVAL;
	}

	if (e_ident[EI_CLASS] == ELFCLASS64) {
		rc = parse_crash_elf64_headers();
		if (rc)
			return rc;
	} else if (e_ident[EI_CLASS] == ELFCLASS32) {
		rc = parse_crash_elf32_headers();
		if (rc)
			return rc;
	} else {
		pr_warn("Warning: Core image elf header is not sane\n");
		return -EINVAL;
	}

	 
	vmcore_size = get_vmcore_size(elfcorebuf_sz, elfnotes_sz,
				      &vmcore_list);

	return 0;
}

#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
 
static void vmcoredd_write_header(void *buf, struct vmcoredd_data *data,
				  u32 size)
{
	struct vmcoredd_header *vdd_hdr = (struct vmcoredd_header *)buf;

	vdd_hdr->n_namesz = sizeof(vdd_hdr->name);
	vdd_hdr->n_descsz = size + sizeof(vdd_hdr->dump_name);
	vdd_hdr->n_type = NT_VMCOREDD;

	strncpy((char *)vdd_hdr->name, VMCOREDD_NOTE_NAME,
		sizeof(vdd_hdr->name));
	memcpy(vdd_hdr->dump_name, data->dump_name, sizeof(vdd_hdr->dump_name));
}

 
static void vmcoredd_update_program_headers(char *elfptr, size_t elfnotesz,
					    size_t vmcoreddsz)
{
	unsigned char *e_ident = (unsigned char *)elfptr;
	u64 start, end, size;
	loff_t vmcore_off;
	u32 i;

	vmcore_off = elfcorebuf_sz + elfnotesz;

	if (e_ident[EI_CLASS] == ELFCLASS64) {
		Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elfptr;
		Elf64_Phdr *phdr = (Elf64_Phdr *)(elfptr + sizeof(Elf64_Ehdr));

		 
		for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
			if (phdr->p_type == PT_NOTE) {
				 
				phdr->p_memsz = elfnotes_orig_sz + vmcoreddsz;
				phdr->p_filesz = phdr->p_memsz;
				continue;
			}

			start = rounddown(phdr->p_offset, PAGE_SIZE);
			end = roundup(phdr->p_offset + phdr->p_memsz,
				      PAGE_SIZE);
			size = end - start;
			phdr->p_offset = vmcore_off + (phdr->p_offset - start);
			vmcore_off += size;
		}
	} else {
		Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elfptr;
		Elf32_Phdr *phdr = (Elf32_Phdr *)(elfptr + sizeof(Elf32_Ehdr));

		 
		for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
			if (phdr->p_type == PT_NOTE) {
				 
				phdr->p_memsz = elfnotes_orig_sz + vmcoreddsz;
				phdr->p_filesz = phdr->p_memsz;
				continue;
			}

			start = rounddown(phdr->p_offset, PAGE_SIZE);
			end = roundup(phdr->p_offset + phdr->p_memsz,
				      PAGE_SIZE);
			size = end - start;
			phdr->p_offset = vmcore_off + (phdr->p_offset - start);
			vmcore_off += size;
		}
	}
}

 
static void vmcoredd_update_size(size_t dump_size)
{
	vmcoredd_orig_sz += dump_size;
	elfnotes_sz = roundup(elfnotes_orig_sz, PAGE_SIZE) + vmcoredd_orig_sz;
	vmcoredd_update_program_headers(elfcorebuf, elfnotes_sz,
					vmcoredd_orig_sz);

	 
	set_vmcore_list_offsets(elfcorebuf_sz, elfnotes_sz, &vmcore_list);

	vmcore_size = get_vmcore_size(elfcorebuf_sz, elfnotes_sz,
				      &vmcore_list);
	proc_vmcore->size = vmcore_size;
}

 
int vmcore_add_device_dump(struct vmcoredd_data *data)
{
	struct vmcoredd_node *dump;
	void *buf = NULL;
	size_t data_size;
	int ret;

	if (vmcoredd_disabled) {
		pr_err_once("Device dump is disabled\n");
		return -EINVAL;
	}

	if (!data || !strlen(data->dump_name) ||
	    !data->vmcoredd_callback || !data->size)
		return -EINVAL;

	dump = vzalloc(sizeof(*dump));
	if (!dump) {
		ret = -ENOMEM;
		goto out_err;
	}

	 
	data_size = roundup(sizeof(struct vmcoredd_header) + data->size,
			    PAGE_SIZE);

	 
	buf = vmcore_alloc_buf(data_size);
	if (!buf) {
		ret = -ENOMEM;
		goto out_err;
	}

	vmcoredd_write_header(buf, data, data_size -
			      sizeof(struct vmcoredd_header));

	 
	ret = data->vmcoredd_callback(data, buf +
				      sizeof(struct vmcoredd_header));
	if (ret)
		goto out_err;

	dump->buf = buf;
	dump->size = data_size;

	 
	mutex_lock(&vmcoredd_mutex);
	list_add_tail(&dump->list, &vmcoredd_list);
	mutex_unlock(&vmcoredd_mutex);

	vmcoredd_update_size(data_size);
	return 0;

out_err:
	vfree(buf);
	vfree(dump);

	return ret;
}
EXPORT_SYMBOL(vmcore_add_device_dump);
#endif  

 
static void vmcore_free_device_dumps(void)
{
#ifdef CONFIG_PROC_VMCORE_DEVICE_DUMP
	mutex_lock(&vmcoredd_mutex);
	while (!list_empty(&vmcoredd_list)) {
		struct vmcoredd_node *dump;

		dump = list_first_entry(&vmcoredd_list, struct vmcoredd_node,
					list);
		list_del(&dump->list);
		vfree(dump->buf);
		vfree(dump);
	}
	mutex_unlock(&vmcoredd_mutex);
#endif  
}

 
static int __init vmcore_init(void)
{
	int rc = 0;

	 
	rc = elfcorehdr_alloc(&elfcorehdr_addr, &elfcorehdr_size);
	if (rc)
		return rc;
	 
	if (!(is_vmcore_usable()))
		return rc;
	rc = parse_crash_elf_headers();
	if (rc) {
		elfcorehdr_free(elfcorehdr_addr);
		pr_warn("Kdump: vmcore not initialized\n");
		return rc;
	}
	elfcorehdr_free(elfcorehdr_addr);
	elfcorehdr_addr = ELFCORE_ADDR_ERR;

	proc_vmcore = proc_create("vmcore", S_IRUSR, NULL, &vmcore_proc_ops);
	if (proc_vmcore)
		proc_vmcore->size = vmcore_size;
	return 0;
}
fs_initcall(vmcore_init);

 
void vmcore_cleanup(void)
{
	if (proc_vmcore) {
		proc_remove(proc_vmcore);
		proc_vmcore = NULL;
	}

	 
	while (!list_empty(&vmcore_list)) {
		struct vmcore *m;

		m = list_first_entry(&vmcore_list, struct vmcore, list);
		list_del(&m->list);
		kfree(m);
	}
	free_elfcorebuf();

	 
	vmcore_free_device_dumps();
}
