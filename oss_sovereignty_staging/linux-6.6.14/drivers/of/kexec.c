
 

#include <linux/ima.h>
#include <linux/kernel.h>
#include <linux/kexec.h>
#include <linux/memblock.h>
#include <linux/libfdt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/types.h>

#define RNG_SEED_SIZE		128

 
#define FDT_EXTRA_SPACE 0x1000

 
static int fdt_find_and_del_mem_rsv(void *fdt, unsigned long start, unsigned long size)
{
	int i, ret, num_rsvs = fdt_num_mem_rsv(fdt);

	for (i = 0; i < num_rsvs; i++) {
		u64 rsv_start, rsv_size;

		ret = fdt_get_mem_rsv(fdt, i, &rsv_start, &rsv_size);
		if (ret) {
			pr_err("Malformed device tree.\n");
			return -EINVAL;
		}

		if (rsv_start == start && rsv_size == size) {
			ret = fdt_del_mem_rsv(fdt, i);
			if (ret) {
				pr_err("Error deleting device tree reservation.\n");
				return -EINVAL;
			}

			return 0;
		}
	}

	return -ENOENT;
}

 
static int get_addr_size_cells(int *addr_cells, int *size_cells)
{
	struct device_node *root;

	root = of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	*addr_cells = of_n_addr_cells(root);
	*size_cells = of_n_size_cells(root);

	of_node_put(root);

	return 0;
}

 
static int do_get_kexec_buffer(const void *prop, int len, unsigned long *addr,
			       size_t *size)
{
	int ret, addr_cells, size_cells;

	ret = get_addr_size_cells(&addr_cells, &size_cells);
	if (ret)
		return ret;

	if (len < 4 * (addr_cells + size_cells))
		return -ENOENT;

	*addr = of_read_number(prop, addr_cells);
	*size = of_read_number(prop + 4 * addr_cells, size_cells);

	return 0;
}

#ifdef CONFIG_HAVE_IMA_KEXEC
 
int __init ima_get_kexec_buffer(void **addr, size_t *size)
{
	int ret, len;
	unsigned long tmp_addr;
	unsigned long start_pfn, end_pfn;
	size_t tmp_size;
	const void *prop;

	prop = of_get_property(of_chosen, "linux,ima-kexec-buffer", &len);
	if (!prop)
		return -ENOENT;

	ret = do_get_kexec_buffer(prop, len, &tmp_addr, &tmp_size);
	if (ret)
		return ret;

	 
	if (!tmp_size)
		return -ENOENT;

	 
	start_pfn = PHYS_PFN(tmp_addr);
	end_pfn = PHYS_PFN(tmp_addr + tmp_size - 1);
	if (!page_is_ram(start_pfn) || !page_is_ram(end_pfn)) {
		pr_warn("IMA buffer at 0x%lx, size = 0x%zx beyond memory\n",
			tmp_addr, tmp_size);
		return -EINVAL;
	}

	*addr = __va(tmp_addr);
	*size = tmp_size;

	return 0;
}

 
int __init ima_free_kexec_buffer(void)
{
	int ret;
	unsigned long addr;
	size_t size;
	struct property *prop;

	prop = of_find_property(of_chosen, "linux,ima-kexec-buffer", NULL);
	if (!prop)
		return -ENOENT;

	ret = do_get_kexec_buffer(prop->value, prop->length, &addr, &size);
	if (ret)
		return ret;

	ret = of_remove_property(of_chosen, prop);
	if (ret)
		return ret;

	memblock_free_late(addr, size);
	return 0;
}
#endif

 
static void remove_ima_buffer(void *fdt, int chosen_node)
{
	int ret, len;
	unsigned long addr;
	size_t size;
	const void *prop;

	if (!IS_ENABLED(CONFIG_HAVE_IMA_KEXEC))
		return;

	prop = fdt_getprop(fdt, chosen_node, "linux,ima-kexec-buffer", &len);
	if (!prop)
		return;

	ret = do_get_kexec_buffer(prop, len, &addr, &size);
	fdt_delprop(fdt, chosen_node, "linux,ima-kexec-buffer");
	if (ret)
		return;

	ret = fdt_find_and_del_mem_rsv(fdt, addr, size);
	if (!ret)
		pr_debug("Removed old IMA buffer reservation.\n");
}

#ifdef CONFIG_IMA_KEXEC
 
static int setup_ima_buffer(const struct kimage *image, void *fdt,
			    int chosen_node)
{
	int ret;

	if (!image->ima_buffer_size)
		return 0;

	ret = fdt_appendprop_addrrange(fdt, 0, chosen_node,
				       "linux,ima-kexec-buffer",
				       image->ima_buffer_addr,
				       image->ima_buffer_size);
	if (ret < 0)
		return -EINVAL;

	ret = fdt_add_mem_rsv(fdt, image->ima_buffer_addr,
			      image->ima_buffer_size);
	if (ret)
		return -EINVAL;

	pr_debug("IMA buffer at 0x%llx, size = 0x%zx\n",
		 image->ima_buffer_addr, image->ima_buffer_size);

	return 0;
}
#else  
static inline int setup_ima_buffer(const struct kimage *image, void *fdt,
				   int chosen_node)
{
	return 0;
}
#endif  

 
void *of_kexec_alloc_and_setup_fdt(const struct kimage *image,
				   unsigned long initrd_load_addr,
				   unsigned long initrd_len,
				   const char *cmdline, size_t extra_fdt_size)
{
	void *fdt;
	int ret, chosen_node, len;
	const void *prop;
	size_t fdt_size;

	fdt_size = fdt_totalsize(initial_boot_params) +
		   (cmdline ? strlen(cmdline) : 0) +
		   FDT_EXTRA_SPACE +
		   extra_fdt_size;
	fdt = kvmalloc(fdt_size, GFP_KERNEL);
	if (!fdt)
		return NULL;

	ret = fdt_open_into(initial_boot_params, fdt, fdt_size);
	if (ret < 0) {
		pr_err("Error %d setting up the new device tree.\n", ret);
		goto out;
	}

	 
	ret = fdt_find_and_del_mem_rsv(fdt, __pa(initial_boot_params),
				       fdt_totalsize(initial_boot_params));
	if (ret == -EINVAL) {
		pr_err("Error removing memory reservation.\n");
		goto out;
	}

	chosen_node = fdt_path_offset(fdt, "/chosen");
	if (chosen_node == -FDT_ERR_NOTFOUND)
		chosen_node = fdt_add_subnode(fdt, fdt_path_offset(fdt, "/"),
					      "chosen");
	if (chosen_node < 0) {
		ret = chosen_node;
		goto out;
	}

	ret = fdt_delprop(fdt, chosen_node, "linux,elfcorehdr");
	if (ret && ret != -FDT_ERR_NOTFOUND)
		goto out;
	ret = fdt_delprop(fdt, chosen_node, "linux,usable-memory-range");
	if (ret && ret != -FDT_ERR_NOTFOUND)
		goto out;

	 
	prop = fdt_getprop(fdt, chosen_node, "linux,initrd-start", &len);
	if (prop) {
		u64 tmp_start, tmp_end, tmp_size;

		tmp_start = of_read_number(prop, len / 4);

		prop = fdt_getprop(fdt, chosen_node, "linux,initrd-end", &len);
		if (!prop) {
			ret = -EINVAL;
			goto out;
		}

		tmp_end = of_read_number(prop, len / 4);

		 
		tmp_size = tmp_end - tmp_start;
		ret = fdt_find_and_del_mem_rsv(fdt, tmp_start, tmp_size);
		if (ret == -ENOENT)
			ret = fdt_find_and_del_mem_rsv(fdt, tmp_start,
						       round_up(tmp_size, PAGE_SIZE));
		if (ret == -EINVAL)
			goto out;
	}

	 
	if (initrd_load_addr) {
		ret = fdt_setprop_u64(fdt, chosen_node, "linux,initrd-start",
				      initrd_load_addr);
		if (ret)
			goto out;

		ret = fdt_setprop_u64(fdt, chosen_node, "linux,initrd-end",
				      initrd_load_addr + initrd_len);
		if (ret)
			goto out;

		ret = fdt_add_mem_rsv(fdt, initrd_load_addr, initrd_len);
		if (ret)
			goto out;

	} else {
		ret = fdt_delprop(fdt, chosen_node, "linux,initrd-start");
		if (ret && (ret != -FDT_ERR_NOTFOUND))
			goto out;

		ret = fdt_delprop(fdt, chosen_node, "linux,initrd-end");
		if (ret && (ret != -FDT_ERR_NOTFOUND))
			goto out;
	}

	if (image->type == KEXEC_TYPE_CRASH) {
		 
		ret = fdt_appendprop_addrrange(fdt, 0, chosen_node,
				"linux,elfcorehdr", image->elf_load_addr,
				image->elf_headers_sz);
		if (ret)
			goto out;

		 
		ret = fdt_add_mem_rsv(fdt, image->elf_load_addr,
				      image->elf_headers_sz);
		if (ret)
			goto out;

		 
		ret = fdt_appendprop_addrrange(fdt, 0, chosen_node,
				"linux,usable-memory-range", crashk_res.start,
				crashk_res.end - crashk_res.start + 1);
		if (ret)
			goto out;

		if (crashk_low_res.end) {
			ret = fdt_appendprop_addrrange(fdt, 0, chosen_node,
					"linux,usable-memory-range",
					crashk_low_res.start,
					crashk_low_res.end - crashk_low_res.start + 1);
			if (ret)
				goto out;
		}
	}

	 
	if (cmdline) {
		ret = fdt_setprop_string(fdt, chosen_node, "bootargs", cmdline);
		if (ret)
			goto out;
	} else {
		ret = fdt_delprop(fdt, chosen_node, "bootargs");
		if (ret && (ret != -FDT_ERR_NOTFOUND))
			goto out;
	}

	 
	ret = fdt_delprop(fdt, chosen_node, "kaslr-seed");
	if (ret == -FDT_ERR_NOTFOUND)
		ret = 0;
	else if (ret)
		goto out;

	if (rng_is_initialized()) {
		u64 seed = get_random_u64();

		ret = fdt_setprop_u64(fdt, chosen_node, "kaslr-seed", seed);
		if (ret)
			goto out;
	} else {
		pr_notice("RNG is not initialised: omitting \"%s\" property\n",
			  "kaslr-seed");
	}

	 
	if (rng_is_initialized()) {
		void *rng_seed;

		ret = fdt_setprop_placeholder(fdt, chosen_node, "rng-seed",
				RNG_SEED_SIZE, &rng_seed);
		if (ret)
			goto out;
		get_random_bytes(rng_seed, RNG_SEED_SIZE);
	} else {
		pr_notice("RNG is not initialised: omitting \"%s\" property\n",
			  "rng-seed");
	}

	ret = fdt_setprop(fdt, chosen_node, "linux,booted-from-kexec", NULL, 0);
	if (ret)
		goto out;

	remove_ima_buffer(fdt, chosen_node);
	ret = setup_ima_buffer(image, fdt, fdt_path_offset(fdt, "/chosen"));

out:
	if (ret) {
		kvfree(fdt);
		fdt = NULL;
	}

	return fdt;
}
