
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/kexec.h>
#include <linux/memblock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/ima.h>
#include <crypto/hash.h>
#include <crypto/sha2.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include "kexec_internal.h"

#ifdef CONFIG_KEXEC_SIG
static bool sig_enforce = IS_ENABLED(CONFIG_KEXEC_SIG_FORCE);

void set_kexec_sig_enforced(void)
{
	sig_enforce = true;
}
#endif

static int kexec_calculate_store_digests(struct kimage *image);

 
#define KEXEC_FILE_SIZE_MAX	min_t(s64, 4LL << 30, SSIZE_MAX)

 
int kexec_image_probe_default(struct kimage *image, void *buf,
			      unsigned long buf_len)
{
	const struct kexec_file_ops * const *fops;
	int ret = -ENOEXEC;

	for (fops = &kexec_file_loaders[0]; *fops && (*fops)->probe; ++fops) {
		ret = (*fops)->probe(buf, buf_len);
		if (!ret) {
			image->fops = *fops;
			return ret;
		}
	}

	return ret;
}

static void *kexec_image_load_default(struct kimage *image)
{
	if (!image->fops || !image->fops->load)
		return ERR_PTR(-ENOEXEC);

	return image->fops->load(image, image->kernel_buf,
				 image->kernel_buf_len, image->initrd_buf,
				 image->initrd_buf_len, image->cmdline_buf,
				 image->cmdline_buf_len);
}

int kexec_image_post_load_cleanup_default(struct kimage *image)
{
	if (!image->fops || !image->fops->cleanup)
		return 0;

	return image->fops->cleanup(image->image_loader_data);
}

 
void kimage_file_post_load_cleanup(struct kimage *image)
{
	struct purgatory_info *pi = &image->purgatory_info;

	vfree(image->kernel_buf);
	image->kernel_buf = NULL;

	vfree(image->initrd_buf);
	image->initrd_buf = NULL;

	kfree(image->cmdline_buf);
	image->cmdline_buf = NULL;

	vfree(pi->purgatory_buf);
	pi->purgatory_buf = NULL;

	vfree(pi->sechdrs);
	pi->sechdrs = NULL;

#ifdef CONFIG_IMA_KEXEC
	vfree(image->ima_buffer);
	image->ima_buffer = NULL;
#endif  

	 
	arch_kimage_file_post_load_cleanup(image);

	 
	kfree(image->image_loader_data);
	image->image_loader_data = NULL;
}

#ifdef CONFIG_KEXEC_SIG
#ifdef CONFIG_SIGNED_PE_FILE_VERIFICATION
int kexec_kernel_verify_pe_sig(const char *kernel, unsigned long kernel_len)
{
	int ret;

	ret = verify_pefile_signature(kernel, kernel_len,
				      VERIFY_USE_SECONDARY_KEYRING,
				      VERIFYING_KEXEC_PE_SIGNATURE);
	if (ret == -ENOKEY && IS_ENABLED(CONFIG_INTEGRITY_PLATFORM_KEYRING)) {
		ret = verify_pefile_signature(kernel, kernel_len,
					      VERIFY_USE_PLATFORM_KEYRING,
					      VERIFYING_KEXEC_PE_SIGNATURE);
	}
	return ret;
}
#endif

static int kexec_image_verify_sig(struct kimage *image, void *buf,
				  unsigned long buf_len)
{
	if (!image->fops || !image->fops->verify_sig) {
		pr_debug("kernel loader does not support signature verification.\n");
		return -EKEYREJECTED;
	}

	return image->fops->verify_sig(buf, buf_len);
}

static int
kimage_validate_signature(struct kimage *image)
{
	int ret;

	ret = kexec_image_verify_sig(image, image->kernel_buf,
				     image->kernel_buf_len);
	if (ret) {

		if (sig_enforce) {
			pr_notice("Enforced kernel signature verification failed (%d).\n", ret);
			return ret;
		}

		 
		if (!ima_appraise_signature(READING_KEXEC_IMAGE) &&
		    security_locked_down(LOCKDOWN_KEXEC))
			return -EPERM;

		pr_debug("kernel signature verification failed (%d).\n", ret);
	}

	return 0;
}
#endif

 
static int
kimage_file_prepare_segments(struct kimage *image, int kernel_fd, int initrd_fd,
			     const char __user *cmdline_ptr,
			     unsigned long cmdline_len, unsigned flags)
{
	ssize_t ret;
	void *ldata;

	ret = kernel_read_file_from_fd(kernel_fd, 0, &image->kernel_buf,
				       KEXEC_FILE_SIZE_MAX, NULL,
				       READING_KEXEC_IMAGE);
	if (ret < 0)
		return ret;
	image->kernel_buf_len = ret;

	 
	ret = arch_kexec_kernel_image_probe(image, image->kernel_buf,
					    image->kernel_buf_len);
	if (ret)
		goto out;

#ifdef CONFIG_KEXEC_SIG
	ret = kimage_validate_signature(image);

	if (ret)
		goto out;
#endif
	 
	if (!(flags & KEXEC_FILE_NO_INITRAMFS)) {
		ret = kernel_read_file_from_fd(initrd_fd, 0, &image->initrd_buf,
					       KEXEC_FILE_SIZE_MAX, NULL,
					       READING_KEXEC_INITRAMFS);
		if (ret < 0)
			goto out;
		image->initrd_buf_len = ret;
		ret = 0;
	}

	if (cmdline_len) {
		image->cmdline_buf = memdup_user(cmdline_ptr, cmdline_len);
		if (IS_ERR(image->cmdline_buf)) {
			ret = PTR_ERR(image->cmdline_buf);
			image->cmdline_buf = NULL;
			goto out;
		}

		image->cmdline_buf_len = cmdline_len;

		 
		if (image->cmdline_buf[cmdline_len - 1] != '\0') {
			ret = -EINVAL;
			goto out;
		}

		ima_kexec_cmdline(kernel_fd, image->cmdline_buf,
				  image->cmdline_buf_len - 1);
	}

	 
	ima_add_kexec_buffer(image);

	 
	ldata = kexec_image_load_default(image);

	if (IS_ERR(ldata)) {
		ret = PTR_ERR(ldata);
		goto out;
	}

	image->image_loader_data = ldata;
out:
	 
	if (ret)
		kimage_file_post_load_cleanup(image);
	return ret;
}

static int
kimage_file_alloc_init(struct kimage **rimage, int kernel_fd,
		       int initrd_fd, const char __user *cmdline_ptr,
		       unsigned long cmdline_len, unsigned long flags)
{
	int ret;
	struct kimage *image;
	bool kexec_on_panic = flags & KEXEC_FILE_ON_CRASH;

	image = do_kimage_alloc_init();
	if (!image)
		return -ENOMEM;

	image->file_mode = 1;

	if (kexec_on_panic) {
		 
		image->control_page = crashk_res.start;
		image->type = KEXEC_TYPE_CRASH;
	}

	ret = kimage_file_prepare_segments(image, kernel_fd, initrd_fd,
					   cmdline_ptr, cmdline_len, flags);
	if (ret)
		goto out_free_image;

	ret = sanity_check_segment_list(image);
	if (ret)
		goto out_free_post_load_bufs;

	ret = -ENOMEM;
	image->control_code_page = kimage_alloc_control_pages(image,
					   get_order(KEXEC_CONTROL_PAGE_SIZE));
	if (!image->control_code_page) {
		pr_err("Could not allocate control_code_buffer\n");
		goto out_free_post_load_bufs;
	}

	if (!kexec_on_panic) {
		image->swap_page = kimage_alloc_control_pages(image, 0);
		if (!image->swap_page) {
			pr_err("Could not allocate swap buffer\n");
			goto out_free_control_pages;
		}
	}

	*rimage = image;
	return 0;
out_free_control_pages:
	kimage_free_page_list(&image->control_pages);
out_free_post_load_bufs:
	kimage_file_post_load_cleanup(image);
out_free_image:
	kfree(image);
	return ret;
}

SYSCALL_DEFINE5(kexec_file_load, int, kernel_fd, int, initrd_fd,
		unsigned long, cmdline_len, const char __user *, cmdline_ptr,
		unsigned long, flags)
{
	int image_type = (flags & KEXEC_FILE_ON_CRASH) ?
			 KEXEC_TYPE_CRASH : KEXEC_TYPE_DEFAULT;
	struct kimage **dest_image, *image;
	int ret = 0, i;

	 
	if (!kexec_load_permitted(image_type))
		return -EPERM;

	 
	if (flags != (flags & KEXEC_FILE_FLAGS))
		return -EINVAL;

	image = NULL;

	if (!kexec_trylock())
		return -EBUSY;

	if (image_type == KEXEC_TYPE_CRASH) {
		dest_image = &kexec_crash_image;
		if (kexec_crash_image)
			arch_kexec_unprotect_crashkres();
	} else {
		dest_image = &kexec_image;
	}

	if (flags & KEXEC_FILE_UNLOAD)
		goto exchange;

	 
	if (flags & KEXEC_FILE_ON_CRASH)
		kimage_free(xchg(&kexec_crash_image, NULL));

	ret = kimage_file_alloc_init(&image, kernel_fd, initrd_fd, cmdline_ptr,
				     cmdline_len, flags);
	if (ret)
		goto out;

	ret = machine_kexec_prepare(image);
	if (ret)
		goto out;

	 
	ret = kimage_crash_copy_vmcoreinfo(image);
	if (ret)
		goto out;

	ret = kexec_calculate_store_digests(image);
	if (ret)
		goto out;

	for (i = 0; i < image->nr_segments; i++) {
		struct kexec_segment *ksegment;

		ksegment = &image->segment[i];
		pr_debug("Loading segment %d: buf=0x%p bufsz=0x%zx mem=0x%lx memsz=0x%zx\n",
			 i, ksegment->buf, ksegment->bufsz, ksegment->mem,
			 ksegment->memsz);

		ret = kimage_load_segment(image, &image->segment[i]);
		if (ret)
			goto out;
	}

	kimage_terminate(image);

	ret = machine_kexec_post_load(image);
	if (ret)
		goto out;

	 
	kimage_file_post_load_cleanup(image);
exchange:
	image = xchg(dest_image, image);
out:
	if ((flags & KEXEC_FILE_ON_CRASH) && kexec_crash_image)
		arch_kexec_protect_crashkres();

	kexec_unlock();
	kimage_free(image);
	return ret;
}

static int locate_mem_hole_top_down(unsigned long start, unsigned long end,
				    struct kexec_buf *kbuf)
{
	struct kimage *image = kbuf->image;
	unsigned long temp_start, temp_end;

	temp_end = min(end, kbuf->buf_max);
	temp_start = temp_end - kbuf->memsz;

	do {
		 
		temp_start = temp_start & (~(kbuf->buf_align - 1));

		if (temp_start < start || temp_start < kbuf->buf_min)
			return 0;

		temp_end = temp_start + kbuf->memsz - 1;

		 
		if (kimage_is_destination_range(image, temp_start, temp_end)) {
			temp_start = temp_start - PAGE_SIZE;
			continue;
		}

		 
		break;
	} while (1);

	 
	kbuf->mem = temp_start;

	 
	return 1;
}

static int locate_mem_hole_bottom_up(unsigned long start, unsigned long end,
				     struct kexec_buf *kbuf)
{
	struct kimage *image = kbuf->image;
	unsigned long temp_start, temp_end;

	temp_start = max(start, kbuf->buf_min);

	do {
		temp_start = ALIGN(temp_start, kbuf->buf_align);
		temp_end = temp_start + kbuf->memsz - 1;

		if (temp_end > end || temp_end > kbuf->buf_max)
			return 0;
		 
		if (kimage_is_destination_range(image, temp_start, temp_end)) {
			temp_start = temp_start + PAGE_SIZE;
			continue;
		}

		 
		break;
	} while (1);

	 
	kbuf->mem = temp_start;

	 
	return 1;
}

static int locate_mem_hole_callback(struct resource *res, void *arg)
{
	struct kexec_buf *kbuf = (struct kexec_buf *)arg;
	u64 start = res->start, end = res->end;
	unsigned long sz = end - start + 1;

	 

	 
	if (res->flags & IORESOURCE_SYSRAM_DRIVER_MANAGED)
		return 0;

	if (sz < kbuf->memsz)
		return 0;

	if (end < kbuf->buf_min || start > kbuf->buf_max)
		return 0;

	 
	if (kbuf->top_down)
		return locate_mem_hole_top_down(start, end, kbuf);
	return locate_mem_hole_bottom_up(start, end, kbuf);
}

#ifdef CONFIG_ARCH_KEEP_MEMBLOCK
static int kexec_walk_memblock(struct kexec_buf *kbuf,
			       int (*func)(struct resource *, void *))
{
	int ret = 0;
	u64 i;
	phys_addr_t mstart, mend;
	struct resource res = { };

	if (kbuf->image->type == KEXEC_TYPE_CRASH)
		return func(&crashk_res, kbuf);

	 
	if (kbuf->top_down) {
		for_each_free_mem_range_reverse(i, NUMA_NO_NODE, MEMBLOCK_NONE,
						&mstart, &mend, NULL) {
			 
			res.start = mstart;
			res.end = mend - 1;
			ret = func(&res, kbuf);
			if (ret)
				break;
		}
	} else {
		for_each_free_mem_range(i, NUMA_NO_NODE, MEMBLOCK_NONE,
					&mstart, &mend, NULL) {
			 
			res.start = mstart;
			res.end = mend - 1;
			ret = func(&res, kbuf);
			if (ret)
				break;
		}
	}

	return ret;
}
#else
static int kexec_walk_memblock(struct kexec_buf *kbuf,
			       int (*func)(struct resource *, void *))
{
	return 0;
}
#endif

 
static int kexec_walk_resources(struct kexec_buf *kbuf,
				int (*func)(struct resource *, void *))
{
	if (kbuf->image->type == KEXEC_TYPE_CRASH)
		return walk_iomem_res_desc(crashk_res.desc,
					   IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY,
					   crashk_res.start, crashk_res.end,
					   kbuf, func);
	else
		return walk_system_ram_res(0, ULONG_MAX, kbuf, func);
}

 
int kexec_locate_mem_hole(struct kexec_buf *kbuf)
{
	int ret;

	 
	if (kbuf->mem != KEXEC_BUF_MEM_UNKNOWN)
		return 0;

	if (!IS_ENABLED(CONFIG_ARCH_KEEP_MEMBLOCK))
		ret = kexec_walk_resources(kbuf, locate_mem_hole_callback);
	else
		ret = kexec_walk_memblock(kbuf, locate_mem_hole_callback);

	return ret == 1 ? 0 : -EADDRNOTAVAIL;
}

 
int kexec_add_buffer(struct kexec_buf *kbuf)
{
	struct kexec_segment *ksegment;
	int ret;

	 
	if (!kbuf->image->file_mode)
		return -EINVAL;

	if (kbuf->image->nr_segments >= KEXEC_SEGMENT_MAX)
		return -EINVAL;

	 
	if (!list_empty(&kbuf->image->control_pages)) {
		WARN_ON(1);
		return -EINVAL;
	}

	 
	kbuf->memsz = ALIGN(kbuf->memsz, PAGE_SIZE);
	kbuf->buf_align = max(kbuf->buf_align, PAGE_SIZE);

	 
	ret = arch_kexec_locate_mem_hole(kbuf);
	if (ret)
		return ret;

	 
	ksegment = &kbuf->image->segment[kbuf->image->nr_segments];
	ksegment->kbuf = kbuf->buffer;
	ksegment->bufsz = kbuf->bufsz;
	ksegment->mem = kbuf->mem;
	ksegment->memsz = kbuf->memsz;
	kbuf->image->nr_segments++;
	return 0;
}

 
static int kexec_calculate_store_digests(struct kimage *image)
{
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int ret = 0, i, j, zero_buf_sz, sha_region_sz;
	size_t desc_size, nullsz;
	char *digest;
	void *zero_buf;
	struct kexec_sha_region *sha_regions;
	struct purgatory_info *pi = &image->purgatory_info;

	if (!IS_ENABLED(CONFIG_ARCH_SUPPORTS_KEXEC_PURGATORY))
		return 0;

	zero_buf = __va(page_to_pfn(ZERO_PAGE(0)) << PAGE_SHIFT);
	zero_buf_sz = PAGE_SIZE;

	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto out;
	}

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);
	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto out_free_tfm;
	}

	sha_region_sz = KEXEC_SEGMENT_MAX * sizeof(struct kexec_sha_region);
	sha_regions = vzalloc(sha_region_sz);
	if (!sha_regions) {
		ret = -ENOMEM;
		goto out_free_desc;
	}

	desc->tfm   = tfm;

	ret = crypto_shash_init(desc);
	if (ret < 0)
		goto out_free_sha_regions;

	digest = kzalloc(SHA256_DIGEST_SIZE, GFP_KERNEL);
	if (!digest) {
		ret = -ENOMEM;
		goto out_free_sha_regions;
	}

	for (j = i = 0; i < image->nr_segments; i++) {
		struct kexec_segment *ksegment;

#ifdef CONFIG_CRASH_HOTPLUG
		 
		if (j == image->elfcorehdr_index)
			continue;
#endif

		ksegment = &image->segment[i];
		 
		if (ksegment->kbuf == pi->purgatory_buf)
			continue;

		ret = crypto_shash_update(desc, ksegment->kbuf,
					  ksegment->bufsz);
		if (ret)
			break;

		 
		nullsz = ksegment->memsz - ksegment->bufsz;
		while (nullsz) {
			unsigned long bytes = nullsz;

			if (bytes > zero_buf_sz)
				bytes = zero_buf_sz;
			ret = crypto_shash_update(desc, zero_buf, bytes);
			if (ret)
				break;
			nullsz -= bytes;
		}

		if (ret)
			break;

		sha_regions[j].start = ksegment->mem;
		sha_regions[j].len = ksegment->memsz;
		j++;
	}

	if (!ret) {
		ret = crypto_shash_final(desc, digest);
		if (ret)
			goto out_free_digest;
		ret = kexec_purgatory_get_set_symbol(image, "purgatory_sha_regions",
						     sha_regions, sha_region_sz, 0);
		if (ret)
			goto out_free_digest;

		ret = kexec_purgatory_get_set_symbol(image, "purgatory_sha256_digest",
						     digest, SHA256_DIGEST_SIZE, 0);
		if (ret)
			goto out_free_digest;
	}

out_free_digest:
	kfree(digest);
out_free_sha_regions:
	vfree(sha_regions);
out_free_desc:
	kfree(desc);
out_free_tfm:
	kfree(tfm);
out:
	return ret;
}

#ifdef CONFIG_ARCH_SUPPORTS_KEXEC_PURGATORY
 
static int kexec_purgatory_setup_kbuf(struct purgatory_info *pi,
				      struct kexec_buf *kbuf)
{
	const Elf_Shdr *sechdrs;
	unsigned long bss_align;
	unsigned long bss_sz;
	unsigned long align;
	int i, ret;

	sechdrs = (void *)pi->ehdr + pi->ehdr->e_shoff;
	kbuf->buf_align = bss_align = 1;
	kbuf->bufsz = bss_sz = 0;

	for (i = 0; i < pi->ehdr->e_shnum; i++) {
		if (!(sechdrs[i].sh_flags & SHF_ALLOC))
			continue;

		align = sechdrs[i].sh_addralign;
		if (sechdrs[i].sh_type != SHT_NOBITS) {
			if (kbuf->buf_align < align)
				kbuf->buf_align = align;
			kbuf->bufsz = ALIGN(kbuf->bufsz, align);
			kbuf->bufsz += sechdrs[i].sh_size;
		} else {
			if (bss_align < align)
				bss_align = align;
			bss_sz = ALIGN(bss_sz, align);
			bss_sz += sechdrs[i].sh_size;
		}
	}
	kbuf->bufsz = ALIGN(kbuf->bufsz, bss_align);
	kbuf->memsz = kbuf->bufsz + bss_sz;
	if (kbuf->buf_align < bss_align)
		kbuf->buf_align = bss_align;

	kbuf->buffer = vzalloc(kbuf->bufsz);
	if (!kbuf->buffer)
		return -ENOMEM;
	pi->purgatory_buf = kbuf->buffer;

	ret = kexec_add_buffer(kbuf);
	if (ret)
		goto out;

	return 0;
out:
	vfree(pi->purgatory_buf);
	pi->purgatory_buf = NULL;
	return ret;
}

 
static int kexec_purgatory_setup_sechdrs(struct purgatory_info *pi,
					 struct kexec_buf *kbuf)
{
	unsigned long bss_addr;
	unsigned long offset;
	size_t sechdrs_size;
	Elf_Shdr *sechdrs;
	int i;

	 
	sechdrs_size = array_size(sizeof(Elf_Shdr), pi->ehdr->e_shnum);
	sechdrs = vzalloc(sechdrs_size);
	if (!sechdrs)
		return -ENOMEM;
	memcpy(sechdrs, (void *)pi->ehdr + pi->ehdr->e_shoff, sechdrs_size);
	pi->sechdrs = sechdrs;

	offset = 0;
	bss_addr = kbuf->mem + kbuf->bufsz;
	kbuf->image->start = pi->ehdr->e_entry;

	for (i = 0; i < pi->ehdr->e_shnum; i++) {
		unsigned long align;
		void *src, *dst;

		if (!(sechdrs[i].sh_flags & SHF_ALLOC))
			continue;

		align = sechdrs[i].sh_addralign;
		if (sechdrs[i].sh_type == SHT_NOBITS) {
			bss_addr = ALIGN(bss_addr, align);
			sechdrs[i].sh_addr = bss_addr;
			bss_addr += sechdrs[i].sh_size;
			continue;
		}

		offset = ALIGN(offset, align);

		 
		if (sechdrs[i].sh_flags & SHF_EXECINSTR &&
		    pi->ehdr->e_entry >= sechdrs[i].sh_addr &&
		    pi->ehdr->e_entry < (sechdrs[i].sh_addr
					 + sechdrs[i].sh_size) &&
		    !WARN_ON(kbuf->image->start != pi->ehdr->e_entry)) {
			kbuf->image->start -= sechdrs[i].sh_addr;
			kbuf->image->start += kbuf->mem + offset;
		}

		src = (void *)pi->ehdr + sechdrs[i].sh_offset;
		dst = pi->purgatory_buf + offset;
		memcpy(dst, src, sechdrs[i].sh_size);

		sechdrs[i].sh_addr = kbuf->mem + offset;
		sechdrs[i].sh_offset = offset;
		offset += sechdrs[i].sh_size;
	}

	return 0;
}

static int kexec_apply_relocations(struct kimage *image)
{
	int i, ret;
	struct purgatory_info *pi = &image->purgatory_info;
	const Elf_Shdr *sechdrs;

	sechdrs = (void *)pi->ehdr + pi->ehdr->e_shoff;

	for (i = 0; i < pi->ehdr->e_shnum; i++) {
		const Elf_Shdr *relsec;
		const Elf_Shdr *symtab;
		Elf_Shdr *section;

		relsec = sechdrs + i;

		if (relsec->sh_type != SHT_RELA &&
		    relsec->sh_type != SHT_REL)
			continue;

		 
		if (relsec->sh_info >= pi->ehdr->e_shnum ||
		    relsec->sh_link >= pi->ehdr->e_shnum)
			return -ENOEXEC;

		section = pi->sechdrs + relsec->sh_info;
		symtab = sechdrs + relsec->sh_link;

		if (!(section->sh_flags & SHF_ALLOC))
			continue;

		 
		if (symtab->sh_link >= pi->ehdr->e_shnum)
			 
			continue;

		 
		if (relsec->sh_type == SHT_RELA)
			ret = arch_kexec_apply_relocations_add(pi, section,
							       relsec, symtab);
		else if (relsec->sh_type == SHT_REL)
			ret = arch_kexec_apply_relocations(pi, section,
							   relsec, symtab);
		if (ret)
			return ret;
	}

	return 0;
}

 
int kexec_load_purgatory(struct kimage *image, struct kexec_buf *kbuf)
{
	struct purgatory_info *pi = &image->purgatory_info;
	int ret;

	if (kexec_purgatory_size <= 0)
		return -EINVAL;

	pi->ehdr = (const Elf_Ehdr *)kexec_purgatory;

	ret = kexec_purgatory_setup_kbuf(pi, kbuf);
	if (ret)
		return ret;

	ret = kexec_purgatory_setup_sechdrs(pi, kbuf);
	if (ret)
		goto out_free_kbuf;

	ret = kexec_apply_relocations(image);
	if (ret)
		goto out;

	return 0;
out:
	vfree(pi->sechdrs);
	pi->sechdrs = NULL;
out_free_kbuf:
	vfree(pi->purgatory_buf);
	pi->purgatory_buf = NULL;
	return ret;
}

 
static const Elf_Sym *kexec_purgatory_find_symbol(struct purgatory_info *pi,
						  const char *name)
{
	const Elf_Shdr *sechdrs;
	const Elf_Ehdr *ehdr;
	const Elf_Sym *syms;
	const char *strtab;
	int i, k;

	if (!pi->ehdr)
		return NULL;

	ehdr = pi->ehdr;
	sechdrs = (void *)ehdr + ehdr->e_shoff;

	for (i = 0; i < ehdr->e_shnum; i++) {
		if (sechdrs[i].sh_type != SHT_SYMTAB)
			continue;

		if (sechdrs[i].sh_link >= ehdr->e_shnum)
			 
			continue;
		strtab = (void *)ehdr + sechdrs[sechdrs[i].sh_link].sh_offset;
		syms = (void *)ehdr + sechdrs[i].sh_offset;

		 
		for (k = 0; k < sechdrs[i].sh_size/sizeof(Elf_Sym); k++) {
			if (ELF_ST_BIND(syms[k].st_info) != STB_GLOBAL)
				continue;

			if (strcmp(strtab + syms[k].st_name, name) != 0)
				continue;

			if (syms[k].st_shndx == SHN_UNDEF ||
			    syms[k].st_shndx >= ehdr->e_shnum) {
				pr_debug("Symbol: %s has bad section index %d.\n",
						name, syms[k].st_shndx);
				return NULL;
			}

			 
			return &syms[k];
		}
	}

	return NULL;
}

void *kexec_purgatory_get_symbol_addr(struct kimage *image, const char *name)
{
	struct purgatory_info *pi = &image->purgatory_info;
	const Elf_Sym *sym;
	Elf_Shdr *sechdr;

	sym = kexec_purgatory_find_symbol(pi, name);
	if (!sym)
		return ERR_PTR(-EINVAL);

	sechdr = &pi->sechdrs[sym->st_shndx];

	 
	return (void *)(sechdr->sh_addr + sym->st_value);
}

 
int kexec_purgatory_get_set_symbol(struct kimage *image, const char *name,
				   void *buf, unsigned int size, bool get_value)
{
	struct purgatory_info *pi = &image->purgatory_info;
	const Elf_Sym *sym;
	Elf_Shdr *sec;
	char *sym_buf;

	sym = kexec_purgatory_find_symbol(pi, name);
	if (!sym)
		return -EINVAL;

	if (sym->st_size != size) {
		pr_err("symbol %s size mismatch: expected %lu actual %u\n",
		       name, (unsigned long)sym->st_size, size);
		return -EINVAL;
	}

	sec = pi->sechdrs + sym->st_shndx;

	if (sec->sh_type == SHT_NOBITS) {
		pr_err("symbol %s is in a bss section. Cannot %s\n", name,
		       get_value ? "get" : "set");
		return -EINVAL;
	}

	sym_buf = (char *)pi->purgatory_buf + sec->sh_offset + sym->st_value;

	if (get_value)
		memcpy((void *)buf, sym_buf, size);
	else
		memcpy((void *)sym_buf, buf, size);

	return 0;
}
#endif  
