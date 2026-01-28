#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/initrd.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/sort.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/efi.h>
#include <linux/swiotlb.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/kexec.h>
#include <linux/crash_dump.h>
#include <linux/hugetlb.h>
#include <linux/acpi_iort.h>
#include <linux/kmemleak.h>
#include <asm/boot.h>
#include <asm/fixmap.h>
#include <asm/kasan.h>
#include <asm/kernel-pgtable.h>
#include <asm/kvm_host.h>
#include <asm/memory.h>
#include <asm/numa.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <linux/sizes.h>
#include <asm/tlb.h>
#include <asm/alternative.h>
#include <asm/xen/swiotlb-xen.h>
s64 memstart_addr __ro_after_init = -1;
EXPORT_SYMBOL(memstart_addr);
phys_addr_t __ro_after_init arm64_dma_phys_limit;
#define CRASH_ALIGN			SZ_2M
#define CRASH_ADDR_LOW_MAX		arm64_dma_phys_limit
#define CRASH_ADDR_HIGH_MAX		(PHYS_MASK + 1)
#define CRASH_HIGH_SEARCH_BASE		SZ_4G
#define DEFAULT_CRASH_KERNEL_LOW_SIZE	(128UL << 20)
#if defined(CONFIG_ARM64_4K_PAGES)
#define ARM64_MEMSTART_SHIFT		PUD_SHIFT
#elif defined(CONFIG_ARM64_16K_PAGES)
#define ARM64_MEMSTART_SHIFT		CONT_PMD_SHIFT
#else
#define ARM64_MEMSTART_SHIFT		PMD_SHIFT
#endif
#if ARM64_MEMSTART_SHIFT < SECTION_SIZE_BITS
#define ARM64_MEMSTART_ALIGN	(1UL << SECTION_SIZE_BITS)
#else
#define ARM64_MEMSTART_ALIGN	(1UL << ARM64_MEMSTART_SHIFT)
#endif
static int __init reserve_crashkernel_low(unsigned long long low_size)
{
	unsigned long long low_base;
	low_base = memblock_phys_alloc_range(low_size, CRASH_ALIGN, 0, CRASH_ADDR_LOW_MAX);
	if (!low_base) {
		pr_err("cannot allocate crashkernel low memory (size:0x%llx).\n", low_size);
		return -ENOMEM;
	}
	pr_info("crashkernel low memory reserved: 0x%08llx - 0x%08llx (%lld MB)\n",
		low_base, low_base + low_size, low_size >> 20);
	crashk_low_res.start = low_base;
	crashk_low_res.end   = low_base + low_size - 1;
	insert_resource(&iomem_resource, &crashk_low_res);
	return 0;
}
static void __init reserve_crashkernel(void)
{
	unsigned long long crash_low_size = 0, search_base = 0;
	unsigned long long crash_max = CRASH_ADDR_LOW_MAX;
	unsigned long long crash_base, crash_size;
	char *cmdline = boot_command_line;
	bool fixed_base = false;
	bool high = false;
	int ret;
	if (!IS_ENABLED(CONFIG_KEXEC_CORE))
		return;
	ret = parse_crashkernel(cmdline, memblock_phys_mem_size(),
				&crash_size, &crash_base);
	if (ret == -ENOENT) {
		ret = parse_crashkernel_high(cmdline, 0, &crash_size, &crash_base);
		if (ret || !crash_size)
			return;
		ret = parse_crashkernel_low(cmdline, 0, &crash_low_size, &crash_base);
		if (ret == -ENOENT)
			crash_low_size = DEFAULT_CRASH_KERNEL_LOW_SIZE;
		else if (ret)
			return;
		search_base = CRASH_HIGH_SEARCH_BASE;
		crash_max = CRASH_ADDR_HIGH_MAX;
		high = true;
	} else if (ret || !crash_size) {
		return;
	}
	crash_size = PAGE_ALIGN(crash_size);
	if (crash_base) {
		fixed_base = true;
		search_base = crash_base;
		crash_max = crash_base + crash_size;
	}
retry:
	crash_base = memblock_phys_alloc_range(crash_size, CRASH_ALIGN,
					       search_base, crash_max);
	if (!crash_base) {
		if (fixed_base) {
			pr_warn("crashkernel reservation failed - memory is in use.\n");
			return;
		}
		if (!high && crash_max == CRASH_ADDR_LOW_MAX) {
			crash_max = CRASH_ADDR_HIGH_MAX;
			search_base = CRASH_ADDR_LOW_MAX;
			crash_low_size = DEFAULT_CRASH_KERNEL_LOW_SIZE;
			goto retry;
		}
		if (high && crash_max == CRASH_ADDR_HIGH_MAX) {
			crash_max = CRASH_ADDR_LOW_MAX;
			search_base = 0;
			goto retry;
		}
		pr_warn("cannot allocate crashkernel (size:0x%llx)\n",
			crash_size);
		return;
	}
	if ((crash_base >= CRASH_ADDR_LOW_MAX) && crash_low_size &&
	     reserve_crashkernel_low(crash_low_size)) {
		memblock_phys_free(crash_base, crash_size);
		return;
	}
	pr_info("crashkernel reserved: 0x%016llx - 0x%016llx (%lld MB)\n",
		crash_base, crash_base + crash_size, crash_size >> 20);
	kmemleak_ignore_phys(crash_base);
	if (crashk_low_res.end)
		kmemleak_ignore_phys(crashk_low_res.start);
	crashk_res.start = crash_base;
	crashk_res.end = crash_base + crash_size - 1;
	insert_resource(&iomem_resource, &crashk_res);
}
static phys_addr_t __init max_zone_phys(unsigned int zone_bits)
{
	phys_addr_t zone_mask = DMA_BIT_MASK(zone_bits);
	phys_addr_t phys_start = memblock_start_of_DRAM();
	if (phys_start > U32_MAX)
		zone_mask = PHYS_ADDR_MAX;
	else if (phys_start > zone_mask)
		zone_mask = U32_MAX;
	return min(zone_mask, memblock_end_of_DRAM() - 1) + 1;
}
static void __init zone_sizes_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES]  = {0};
	unsigned int __maybe_unused acpi_zone_dma_bits;
	unsigned int __maybe_unused dt_zone_dma_bits;
	phys_addr_t __maybe_unused dma32_phys_limit = max_zone_phys(32);
#ifdef CONFIG_ZONE_DMA
	acpi_zone_dma_bits = fls64(acpi_iort_dma_get_max_cpu_address());
	dt_zone_dma_bits = fls64(of_dma_get_max_cpu_address(NULL));
	zone_dma_bits = min3(32U, dt_zone_dma_bits, acpi_zone_dma_bits);
	arm64_dma_phys_limit = max_zone_phys(zone_dma_bits);
	max_zone_pfns[ZONE_DMA] = PFN_DOWN(arm64_dma_phys_limit);
#endif
#ifdef CONFIG_ZONE_DMA32
	max_zone_pfns[ZONE_DMA32] = PFN_DOWN(dma32_phys_limit);
	if (!arm64_dma_phys_limit)
		arm64_dma_phys_limit = dma32_phys_limit;
#endif
	if (!arm64_dma_phys_limit)
		arm64_dma_phys_limit = PHYS_MASK + 1;
	max_zone_pfns[ZONE_NORMAL] = max_pfn;
	free_area_init(max_zone_pfns);
}
int pfn_is_map_memory(unsigned long pfn)
{
	phys_addr_t addr = PFN_PHYS(pfn);
	if (PHYS_PFN(addr) != pfn)
		return 0;
	return memblock_is_map_memory(addr);
}
EXPORT_SYMBOL(pfn_is_map_memory);
static phys_addr_t memory_limit __ro_after_init = PHYS_ADDR_MAX;
static int __init early_mem(char *p)
{
	if (!p)
		return 1;
	memory_limit = memparse(p, &p) & PAGE_MASK;
	pr_notice("Memory limited to %lldMB\n", memory_limit >> 20);
	return 0;
}
early_param("mem", early_mem);
void __init arm64_memblock_init(void)
{
	s64 linear_region_size = PAGE_END - _PAGE_OFFSET(vabits_actual);
	if (IS_ENABLED(CONFIG_KVM) && vabits_actual == 52 &&
	    is_hyp_mode_available() && !is_kernel_in_hyp_mode()) {
		pr_info("Capping linear region to 51 bits for KVM in nVHE mode on LVA capable hardware.\n");
		linear_region_size = min_t(u64, linear_region_size, BIT(51));
	}
	memblock_remove(1ULL << PHYS_MASK_SHIFT, ULLONG_MAX);
	memstart_addr = round_down(memblock_start_of_DRAM(),
				   ARM64_MEMSTART_ALIGN);
	if ((memblock_end_of_DRAM() - memstart_addr) > linear_region_size)
		pr_warn("Memory doesn't fit in the linear mapping, VA_BITS too small\n");
	memblock_remove(max_t(u64, memstart_addr + linear_region_size,
			__pa_symbol(_end)), ULLONG_MAX);
	if (memstart_addr + linear_region_size < memblock_end_of_DRAM()) {
		memstart_addr = round_up(memblock_end_of_DRAM() - linear_region_size,
					 ARM64_MEMSTART_ALIGN);
		memblock_remove(0, memstart_addr);
	}
	if (IS_ENABLED(CONFIG_ARM64_VA_BITS_52) && (vabits_actual != 52))
		memstart_addr -= _PAGE_OFFSET(48) - _PAGE_OFFSET(52);
	if (memory_limit != PHYS_ADDR_MAX) {
		memblock_mem_limit_remove_map(memory_limit);
		memblock_add(__pa_symbol(_text), (u64)(_end - _text));
	}
	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) && phys_initrd_size) {
		u64 base = phys_initrd_start & PAGE_MASK;
		u64 size = PAGE_ALIGN(phys_initrd_start + phys_initrd_size) - base;
		if (WARN(base < memblock_start_of_DRAM() ||
			 base + size > memblock_start_of_DRAM() +
				       linear_region_size,
			"initrd not fully accessible via the linear mapping -- please check your bootloader ...\n")) {
			phys_initrd_size = 0;
		} else {
			memblock_add(base, size);
			memblock_clear_nomap(base, size);
			memblock_reserve(base, size);
		}
	}
	if (IS_ENABLED(CONFIG_RANDOMIZE_BASE)) {
		extern u16 memstart_offset_seed;
		u64 mmfr0 = read_cpuid(ID_AA64MMFR0_EL1);
		int parange = cpuid_feature_extract_unsigned_field(
					mmfr0, ID_AA64MMFR0_EL1_PARANGE_SHIFT);
		s64 range = linear_region_size -
			    BIT(id_aa64mmfr0_parange_to_phys_shift(parange));
		if (memstart_offset_seed > 0 && range >= (s64)ARM64_MEMSTART_ALIGN) {
			range /= ARM64_MEMSTART_ALIGN;
			memstart_addr -= ARM64_MEMSTART_ALIGN *
					 ((range * memstart_offset_seed) >> 16);
		}
	}
	memblock_reserve(__pa_symbol(_stext), _end - _stext);
	if (IS_ENABLED(CONFIG_BLK_DEV_INITRD) && phys_initrd_size) {
		initrd_start = __phys_to_virt(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
	early_init_fdt_scan_reserved_mem();
	high_memory = __va(memblock_end_of_DRAM() - 1) + 1;
}
void __init bootmem_init(void)
{
	unsigned long min, max;
	min = PFN_UP(memblock_start_of_DRAM());
	max = PFN_DOWN(memblock_end_of_DRAM());
	early_memtest(min << PAGE_SHIFT, max << PAGE_SHIFT);
	max_pfn = max_low_pfn = max;
	min_low_pfn = min;
	arch_numa_init();
#if defined(CONFIG_HUGETLB_PAGE) && defined(CONFIG_CMA)
	arm64_hugetlb_cma_reserve();
#endif
	kvm_hyp_reserve();
	sparse_init();
	zone_sizes_init();
	dma_contiguous_reserve(arm64_dma_phys_limit);
	reserve_crashkernel();
	memblock_dump_all();
}
void __init mem_init(void)
{
	bool swiotlb = max_pfn > PFN_DOWN(arm64_dma_phys_limit);
	if (IS_ENABLED(CONFIG_DMA_BOUNCE_UNALIGNED_KMALLOC))
		swiotlb = true;
	swiotlb_init(swiotlb, SWIOTLB_VERBOSE);
	memblock_free_all();
#ifdef CONFIG_COMPAT
	BUILD_BUG_ON(TASK_SIZE_32 > DEFAULT_MAP_WINDOW_64);
#endif
	BUILD_BUG_ON(ARM64_HW_PGTABLE_LEVELS(CONFIG_ARM64_VA_BITS) !=
		     CONFIG_PGTABLE_LEVELS);
	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}
void free_initmem(void)
{
	free_reserved_area(lm_alias(__init_begin),
			   lm_alias(__init_end),
			   POISON_FREE_INITMEM, "unused kernel");
	vunmap_range((u64)__init_begin, (u64)__init_end);
}
void dump_mem_limit(void)
{
	if (memory_limit != PHYS_ADDR_MAX) {
		pr_emerg("Memory Limit: %llu MB\n", memory_limit >> 20);
	} else {
		pr_emerg("Memory Limit: none\n");
	}
}
