
 

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include "acrn_drv.h"

static int modify_region(struct acrn_vm *vm, struct vm_memory_region_op *region)
{
	struct vm_memory_region_batch *regions;
	int ret;

	regions = kzalloc(sizeof(*regions), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	regions->vmid = vm->vmid;
	regions->regions_num = 1;
	regions->regions_gpa = virt_to_phys(region);

	ret = hcall_set_memory_regions(virt_to_phys(regions));
	if (ret < 0)
		dev_dbg(acrn_dev.this_device,
			"Failed to set memory region for VM[%u]!\n", vm->vmid);

	kfree(regions);
	return ret;
}

 
int acrn_mm_region_add(struct acrn_vm *vm, u64 user_gpa, u64 service_gpa,
		       u64 size, u32 mem_type, u32 mem_access_right)
{
	struct vm_memory_region_op *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = ACRN_MEM_REGION_ADD;
	region->user_vm_pa = user_gpa;
	region->service_vm_pa = service_gpa;
	region->size = size;
	region->attr = ((mem_type & ACRN_MEM_TYPE_MASK) |
			(mem_access_right & ACRN_MEM_ACCESS_RIGHT_MASK));
	ret = modify_region(vm, region);

	dev_dbg(acrn_dev.this_device,
		"%s: user-GPA[%pK] service-GPA[%pK] size[0x%llx].\n",
		__func__, (void *)user_gpa, (void *)service_gpa, size);
	kfree(region);
	return ret;
}

 
int acrn_mm_region_del(struct acrn_vm *vm, u64 user_gpa, u64 size)
{
	struct vm_memory_region_op *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = ACRN_MEM_REGION_DEL;
	region->user_vm_pa = user_gpa;
	region->service_vm_pa = 0UL;
	region->size = size;
	region->attr = 0U;

	ret = modify_region(vm, region);

	dev_dbg(acrn_dev.this_device, "%s: user-GPA[%pK] size[0x%llx].\n",
		__func__, (void *)user_gpa, size);
	kfree(region);
	return ret;
}

int acrn_vm_memseg_map(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	int ret;

	if (memmap->type == ACRN_MEMMAP_RAM)
		return acrn_vm_ram_map(vm, memmap);

	if (memmap->type != ACRN_MEMMAP_MMIO) {
		dev_dbg(acrn_dev.this_device,
			"Invalid memmap type: %u\n", memmap->type);
		return -EINVAL;
	}

	ret = acrn_mm_region_add(vm, memmap->user_vm_pa,
				 memmap->service_vm_pa, memmap->len,
				 ACRN_MEM_TYPE_UC, memmap->attr);
	if (ret < 0)
		dev_dbg(acrn_dev.this_device,
			"Add memory region failed, VM[%u]!\n", vm->vmid);

	return ret;
}

int acrn_vm_memseg_unmap(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	int ret;

	if (memmap->type != ACRN_MEMMAP_MMIO) {
		dev_dbg(acrn_dev.this_device,
			"Invalid memmap type: %u\n", memmap->type);
		return -EINVAL;
	}

	ret = acrn_mm_region_del(vm, memmap->user_vm_pa, memmap->len);
	if (ret < 0)
		dev_dbg(acrn_dev.this_device,
			"Del memory region failed, VM[%u]!\n", vm->vmid);

	return ret;
}

 
int acrn_vm_ram_map(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	struct vm_memory_region_batch *regions_info;
	int nr_pages, i = 0, order, nr_regions = 0;
	struct vm_memory_mapping *region_mapping;
	struct vm_memory_region_op *vm_region;
	struct page **pages = NULL, *page;
	void *remap_vaddr;
	int ret, pinned;
	u64 user_vm_pa;
	unsigned long pfn;
	struct vm_area_struct *vma;

	if (!vm || !memmap)
		return -EINVAL;

	mmap_read_lock(current->mm);
	vma = vma_lookup(current->mm, memmap->vma_base);
	if (vma && ((vma->vm_flags & VM_PFNMAP) != 0)) {
		if ((memmap->vma_base + memmap->len) > vma->vm_end) {
			mmap_read_unlock(current->mm);
			return -EINVAL;
		}

		ret = follow_pfn(vma, memmap->vma_base, &pfn);
		mmap_read_unlock(current->mm);
		if (ret < 0) {
			dev_dbg(acrn_dev.this_device,
				"Failed to lookup PFN at VMA:%pK.\n", (void *)memmap->vma_base);
			return ret;
		}

		return acrn_mm_region_add(vm, memmap->user_vm_pa,
			 PFN_PHYS(pfn), memmap->len,
			 ACRN_MEM_TYPE_WB, memmap->attr);
	}
	mmap_read_unlock(current->mm);

	 
	nr_pages = memmap->len >> PAGE_SHIFT;
	pages = vzalloc(array_size(nr_pages, sizeof(*pages)));
	if (!pages)
		return -ENOMEM;

	 
	pinned = pin_user_pages_fast(memmap->vma_base,
				     nr_pages, FOLL_WRITE | FOLL_LONGTERM,
				     pages);
	if (pinned < 0) {
		ret = pinned;
		goto free_pages;
	} else if (pinned != nr_pages) {
		ret = -EFAULT;
		goto put_pages;
	}

	 
	remap_vaddr = vmap(pages, nr_pages, VM_MAP, PAGE_KERNEL);
	if (!remap_vaddr) {
		ret = -ENOMEM;
		goto put_pages;
	}

	 
	mutex_lock(&vm->regions_mapping_lock);
	region_mapping = &vm->regions_mapping[vm->regions_mapping_count];
	if (vm->regions_mapping_count < ACRN_MEM_MAPPING_MAX) {
		region_mapping->pages = pages;
		region_mapping->npages = nr_pages;
		region_mapping->size = memmap->len;
		region_mapping->service_vm_va = remap_vaddr;
		region_mapping->user_vm_pa = memmap->user_vm_pa;
		vm->regions_mapping_count++;
	} else {
		dev_warn(acrn_dev.this_device,
			"Run out of memory mapping slots!\n");
		ret = -ENOMEM;
		mutex_unlock(&vm->regions_mapping_lock);
		goto unmap_no_count;
	}
	mutex_unlock(&vm->regions_mapping_lock);

	 
	while (i < nr_pages) {
		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		nr_regions++;
		i += 1 << order;
	}

	 
	regions_info = kzalloc(struct_size(regions_info, regions_op,
					   nr_regions), GFP_KERNEL);
	if (!regions_info) {
		ret = -ENOMEM;
		goto unmap_kernel_map;
	}

	 
	vm_region = regions_info->regions_op;
	regions_info->vmid = vm->vmid;
	regions_info->regions_num = nr_regions;
	regions_info->regions_gpa = virt_to_phys(vm_region);
	user_vm_pa = memmap->user_vm_pa;
	i = 0;
	while (i < nr_pages) {
		u32 region_size;

		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		region_size = PAGE_SIZE << order;
		vm_region->type = ACRN_MEM_REGION_ADD;
		vm_region->user_vm_pa = user_vm_pa;
		vm_region->service_vm_pa = page_to_phys(page);
		vm_region->size = region_size;
		vm_region->attr = (ACRN_MEM_TYPE_WB & ACRN_MEM_TYPE_MASK) |
				  (memmap->attr & ACRN_MEM_ACCESS_RIGHT_MASK);

		vm_region++;
		user_vm_pa += region_size;
		i += 1 << order;
	}

	 
	ret = hcall_set_memory_regions(virt_to_phys(regions_info));
	if (ret < 0) {
		dev_dbg(acrn_dev.this_device,
			"Failed to set regions, VM[%u]!\n", vm->vmid);
		goto unset_region;
	}
	kfree(regions_info);

	dev_dbg(acrn_dev.this_device,
		"%s: VM[%u] service-GVA[%pK] user-GPA[%pK] size[0x%llx]\n",
		__func__, vm->vmid,
		remap_vaddr, (void *)memmap->user_vm_pa, memmap->len);
	return ret;

unset_region:
	kfree(regions_info);
unmap_kernel_map:
	mutex_lock(&vm->regions_mapping_lock);
	vm->regions_mapping_count--;
	mutex_unlock(&vm->regions_mapping_lock);
unmap_no_count:
	vunmap(remap_vaddr);
put_pages:
	for (i = 0; i < pinned; i++)
		unpin_user_page(pages[i]);
free_pages:
	vfree(pages);
	return ret;
}

 
void acrn_vm_all_ram_unmap(struct acrn_vm *vm)
{
	struct vm_memory_mapping *region_mapping;
	int i, j;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region_mapping = &vm->regions_mapping[i];
		vunmap(region_mapping->service_vm_va);
		for (j = 0; j < region_mapping->npages; j++)
			unpin_user_page(region_mapping->pages[j]);
		vfree(region_mapping->pages);
	}
	mutex_unlock(&vm->regions_mapping_lock);
}
